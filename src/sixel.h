// MODULE: sixel
// Purpose : shared low-level SIXEL helpers for the future img/vid commands
// Exports : sixel_supported() sixel_fit() sixel_render() | SixelFrame SixelFit
// Depends : common.h, terminal.h

struct SixelFrame {
    const uint8_t* rgb;
    int width;
    int height;
};

struct SixelFit {
    int term_w;
    int term_h;
    int cell_w;
    int cell_h;
    int pixel_w;
    int pixel_h;
};

struct SixelColor {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

struct SixelHistColor {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    int count;
};

struct SixelBox {
    int start;
    int end;
    int count;
    int rmin;
    int rmax;
    int gmin;
    int gmax;
    int bmin;
    int bmax;
};

struct SixelScratch {
    std::vector<uint8_t> pixels;
    std::vector<uint8_t> used;
    std::vector<float> work;
    std::vector<char> line;
    std::vector<uint8_t> band_used;
    std::vector<int> band_colors;
    std::string buf;
};

static void sixel_push_num(std::string& buf, int value) {
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "%d", value);
    buf += tmp;
}

static void sixel_push_run(std::string& buf, int count, char ch) {
    if (count <= 3) {
        buf.append((size_t)count, ch);
        return;
    }
    buf += '!';
    sixel_push_num(buf, count);
    buf += ch;
}

static uint8_t sixel_clamp_u8(float v) {
    if (v < 0.0f) return 0;
    if (v > 255.0f) return 255;
    return (uint8_t)(v + 0.5f);
}

static void sixel_box_update(SixelBox& box, const std::vector<SixelHistColor>& colors) {
    box.count = 0;
    box.rmin = box.gmin = box.bmin = 255;
    box.rmax = box.gmax = box.bmax = 0;
    for (int i = box.start; i < box.end; ++i) {
        const SixelHistColor& c = colors[i];
        box.count += c.count;
        if (c.r < box.rmin) box.rmin = c.r;
        if (c.r > box.rmax) box.rmax = c.r;
        if (c.g < box.gmin) box.gmin = c.g;
        if (c.g > box.gmax) box.gmax = c.g;
        if (c.b < box.bmin) box.bmin = c.b;
        if (c.b > box.bmax) box.bmax = c.b;
    }
}

static int sixel_box_channel(const SixelBox& box) {
    int rr = box.rmax - box.rmin;
    int gr = box.gmax - box.gmin;
    int br = box.bmax - box.bmin;
    if (gr >= rr && gr >= br) return 1;
    if (br >= rr && br >= gr) return 2;
    return 0;
}

static int sixel_box_score(const SixelBox& box) {
    int span = (box.rmax - box.rmin) + (box.gmax - box.gmin) + (box.bmax - box.bmin);
    return span * std::max(1, box.count);
}

static std::vector<SixelColor> sixel_build_palette(const SixelFrame& frame, int target_colors) {
    const int hist_size = 32 * 32 * 32;
    std::vector<uint32_t> count(hist_size, 0);
    std::vector<uint32_t> rsum(hist_size, 0);
    std::vector<uint32_t> gsum(hist_size, 0);
    std::vector<uint32_t> bsum(hist_size, 0);

    for (int y = 0; y < frame.height; ++y) {
        for (int x = 0; x < frame.width; ++x) {
            const uint8_t* src = frame.rgb + ((size_t)y * frame.width + x) * 3;
            int idx = ((src[0] >> 3) << 10) | ((src[1] >> 3) << 5) | (src[2] >> 3);
            ++count[idx];
            rsum[idx] += src[0];
            gsum[idx] += src[1];
            bsum[idx] += src[2];
        }
    }

    std::vector<SixelHistColor> colors;
    colors.reserve(hist_size);
    for (int i = 0; i < hist_size; ++i) {
        if (!count[i]) continue;
        colors.push_back({
            (uint8_t)(rsum[i] / count[i]),
            (uint8_t)(gsum[i] / count[i]),
            (uint8_t)(bsum[i] / count[i]),
            (int)count[i]
        });
    }

    if (colors.empty())
        return { {0, 0, 0} };

    if ((int)colors.size() <= target_colors) {
        std::vector<SixelColor> palette;
        palette.reserve(colors.size());
        for (const SixelHistColor& c : colors)
            palette.push_back({ c.r, c.g, c.b });
        return palette;
    }

    std::vector<SixelBox> boxes;
    boxes.push_back({ 0, (int)colors.size(), 0, 0, 0, 0, 0, 0, 0 });
    sixel_box_update(boxes[0], colors);

    while ((int)boxes.size() < target_colors) {
        int best = -1;
        int best_score = -1;
        for (int i = 0; i < (int)boxes.size(); ++i) {
            if (boxes[i].end - boxes[i].start <= 1) continue;
            int score = sixel_box_score(boxes[i]);
            if (score > best_score) {
                best_score = score;
                best = i;
            }
        }
        if (best < 0) break;

        SixelBox box = boxes[best];
        int channel = sixel_box_channel(box);
        auto begin = colors.begin() + box.start;
        auto end = colors.begin() + box.end;
        if (channel == 0) {
            std::sort(begin, end, [](const SixelHistColor& a, const SixelHistColor& b) { return a.r < b.r; });
        } else if (channel == 1) {
            std::sort(begin, end, [](const SixelHistColor& a, const SixelHistColor& b) { return a.g < b.g; });
        } else {
            std::sort(begin, end, [](const SixelHistColor& a, const SixelHistColor& b) { return a.b < b.b; });
        }

        int total = 0;
        for (int i = box.start; i < box.end; ++i) total += colors[i].count;
        int half = total / 2;
        int split = box.start;
        int accum = 0;
        while (split < box.end - 1) {
            accum += colors[split].count;
            ++split;
            if (accum >= half) break;
        }
        if (split <= box.start || split >= box.end) break;

        boxes[best] = { box.start, split, 0, 0, 0, 0, 0, 0, 0 };
        sixel_box_update(boxes[best], colors);

        SixelBox right = { split, box.end, 0, 0, 0, 0, 0, 0, 0 };
        sixel_box_update(right, colors);
        boxes.push_back(right);
    }

    std::vector<SixelColor> palette;
    palette.reserve(boxes.size());
    for (const SixelBox& box : boxes) {
        uint64_t r = 0, g = 0, b = 0, total = 0;
        for (int i = box.start; i < box.end; ++i) {
            uint64_t n = (uint64_t)colors[i].count;
            total += n;
            r += (uint64_t)colors[i].r * n;
            g += (uint64_t)colors[i].g * n;
            b += (uint64_t)colors[i].b * n;
        }
        if (!total) continue;
        palette.push_back({
            (uint8_t)(r / total),
            (uint8_t)(g / total),
            (uint8_t)(b / total)
        });
    }

    if (palette.empty())
        palette.push_back({ 0, 0, 0 });
    return palette;
}

static int sixel_color_distance(float r, float g, float b, const SixelColor& c) {
    int dr = (int)r - c.r;
    int dg = (int)g - c.g;
    int db = (int)b - c.b;
    int rmean = ((int)r + c.r) / 2;
    return (((512 + rmean) * dr * dr) >> 8) + 4 * dg * dg + (((767 - rmean) * db * db) >> 8);
}

static uint8_t sixel_nearest_color(float r, float g, float b, const std::vector<SixelColor>& palette) {
    int best = 0;
    int best_dist = sixel_color_distance(r, g, b, palette[0]);
    for (int i = 1; i < (int)palette.size(); ++i) {
        int dist = sixel_color_distance(r, g, b, palette[i]);
        if (dist < best_dist) {
            best = i;
            best_dist = dist;
        }
    }
    return (uint8_t)best;
}

static std::vector<uint8_t> sixel_build_lookup(const std::vector<SixelColor>& palette) {
    const int dim = 32;
    std::vector<uint8_t> lut((size_t)dim * dim * dim);
    for (int r = 0; r < dim; ++r) {
        for (int g = 0; g < dim; ++g) {
            for (int b = 0; b < dim; ++b) {
                float rr = (float)(r * 255) / (dim - 1);
                float gg = (float)(g * 255) / (dim - 1);
                float bb = (float)(b * 255) / (dim - 1);
                lut[(r << 10) | (g << 5) | b] = sixel_nearest_color(rr, gg, bb, palette);
            }
        }
    }
    return lut;
}

static bool sixel_supported() {
    char buf[256] = {};
    if (GetEnvironmentVariableA("WT_SESSION", buf, sizeof(buf)) > 0)
        return true;

    DWORD n = GetEnvironmentVariableA("TERM", buf, sizeof(buf));
    if (n > 0 && n < sizeof(buf)) {
        std::string term = buf;
        std::transform(term.begin(), term.end(), term.begin(), ::tolower);
        if (term.find("sixel") != std::string::npos)
            return true;
    }

    n = GetEnvironmentVariableA("TERM_PROGRAM", buf, sizeof(buf));
    if (n > 0 && n < sizeof(buf)) {
        std::string term_program = buf;
        std::transform(term_program.begin(), term_program.end(), term_program.begin(), ::tolower);
        if (term_program.find("windows terminal") != std::string::npos)
            return true;
    }

    return false;
}

static SixelFit sixel_fit(int src_w, int src_h, int reserve_rows = 1) {
    SixelFit fit = {};
    fit.term_w = term_width();
    fit.term_h = term_height() - reserve_rows;
    if (fit.term_w < 1) fit.term_w = 1;
    if (fit.term_h < 1) fit.term_h = 1;

    if (src_w < 1) src_w = 1;
    if (src_h < 1) src_h = 1;

    int cell_px_w = 8;
    int cell_px_h = 16;
    CONSOLE_FONT_INFOEX cfi = {};
    cfi.cbSize = sizeof(cfi);
    if (GetCurrentConsoleFontEx(out_h, FALSE, &cfi)) {
        if (cfi.dwFontSize.X > 0) cell_px_w = cfi.dwFontSize.X;
        if (cfi.dwFontSize.Y > 0) cell_px_h = cfi.dwFontSize.Y;
    }

    int max_pixel_w = fit.term_w * cell_px_w;
    int max_pixel_h = fit.term_h * cell_px_h;
    if (max_pixel_w < 1) max_pixel_w = 1;
    if (max_pixel_h < 1) max_pixel_h = 1;

    double scale_x = (double)max_pixel_w / src_w;
    double scale_y = (double)max_pixel_h / src_h;
    double scale = std::min(1.0, std::min(scale_x, scale_y));
    if (scale <= 0.0) scale = 1.0;

    fit.pixel_w = (int)(src_w * scale + 0.5);
    fit.pixel_h = (int)(src_h * scale + 0.5);
    if (fit.pixel_w < 1) fit.pixel_w = 1;
    if (fit.pixel_h < 1) fit.pixel_h = 1;

    fit.cell_w = std::max(1, (fit.pixel_w + cell_px_w - 1) / cell_px_w);
    fit.cell_h = std::max(1, (fit.pixel_h + cell_px_h - 1) / cell_px_h);
    return fit;
}

static int sixel_render(const SixelFrame& frame, const SixelFit& fit, bool home = false) {
    (void)fit;
    if (!sixel_supported()) return 1;
    if (!frame.rgb || frame.width < 1 || frame.height < 1) return 1;

    static thread_local SixelScratch scratch;
    std::vector<SixelColor> palette = sixel_build_palette(frame, 256);
    std::vector<uint8_t> lut = sixel_build_lookup(palette);
    const int palette_size = (int)palette.size();
    size_t pixel_count = (size_t)frame.width * frame.height;
    scratch.pixels.resize(pixel_count);
    scratch.used.assign((size_t)palette_size, 0);
    scratch.work.resize(pixel_count * 3);
    for (size_t i = 0; i < scratch.work.size(); ++i)
        scratch.work[i] = frame.rgb[i];

    std::vector<uint8_t>& pixels = scratch.pixels;
    std::vector<uint8_t>& used = scratch.used;
    std::vector<float>& work = scratch.work;

    for (int y = 0; y < frame.height; ++y) {
        for (int x = 0; x < frame.width; ++x) {
            size_t pos = ((size_t)y * frame.width + x);
            size_t off = pos * 3;
            float r = sixel_clamp_u8(work[off + 0]);
            float g = sixel_clamp_u8(work[off + 1]);
            float b = sixel_clamp_u8(work[off + 2]);
            work[off + 0] = r;
            work[off + 1] = g;
            work[off + 2] = b;
            int ri = (int)r >> 3;
            int gi = (int)g >> 3;
            int bi = (int)b >> 3;
            uint8_t idx = lut[(ri << 10) | (gi << 5) | bi];
            pixels[pos] = idx;
            used[idx] = 1;

            float er = r - palette[idx].r;
            float eg = g - palette[idx].g;
            float eb = b - palette[idx].b;

            auto diffuse = [&](int nx, int ny, float factor) {
                if (nx < 0 || nx >= frame.width || ny < 0 || ny >= frame.height) return;
                size_t noff = (((size_t)ny * frame.width + nx) * 3);
                work[noff + 0] += er * factor;
                work[noff + 1] += eg * factor;
                work[noff + 2] += eb * factor;
            };

            diffuse(x + 1, y    , 7.0f / 16.0f);
            diffuse(x - 1, y + 1, 3.0f / 16.0f);
            diffuse(x    , y + 1, 5.0f / 16.0f);
            diffuse(x + 1, y + 1, 1.0f / 16.0f);
        }
    }

    scratch.buf.clear();
    scratch.buf.reserve((size_t)frame.width * frame.height * 2 + palette_size * 24 + 1024);
    std::string& buf = scratch.buf;

    if (home) buf += "\033[H";
    buf += "\033Pq";
    buf += "\"1;1;";
    sixel_push_num(buf, frame.width);
    buf += ';';
    sixel_push_num(buf, frame.height);

    for (int idx = 0; idx < palette_size; ++idx) {
        if (!used[idx]) continue;
        buf += '#';
        sixel_push_num(buf, idx);
        buf += ";2;";
        sixel_push_num(buf, (palette[idx].r * 100 + 127) / 255);
        buf += ';';
        sixel_push_num(buf, (palette[idx].g * 100 + 127) / 255);
        buf += ';';
        sixel_push_num(buf, (palette[idx].b * 100 + 127) / 255);
    }

    scratch.line.resize((size_t)frame.width);
    scratch.band_used.assign((size_t)palette_size, 0);
    scratch.band_colors.clear();
    scratch.band_colors.reserve(palette_size);
    std::vector<char>& line = scratch.line;
    std::vector<uint8_t>& band_used = scratch.band_used;
    std::vector<int>& band_colors = scratch.band_colors;
    for (int y0 = 0; y0 < frame.height; y0 += 6) {
        std::fill(band_used.begin(), band_used.end(), 0);
        band_colors.clear();
        for (int dy = 0; dy < 6; ++dy) {
            int y = y0 + dy;
            if (y >= frame.height) break;
            const uint8_t* row = pixels.data() + (size_t)y * frame.width;
            for (int x = 0; x < frame.width; ++x) {
                uint8_t color = row[x];
                if (!band_used[color]) {
                    band_used[color] = 1;
                    band_colors.push_back(color);
                }
            }
        }

        bool wrote_color = false;
        for (int color : band_colors) {
            if (!used[color]) continue;

            int last = -1;
            for (int x = 0; x < frame.width; ++x) {
                int bits = 0;
                for (int dy = 0; dy < 6; ++dy) {
                    int y = y0 + dy;
                    if (y >= frame.height) break;
                    if (pixels[(size_t)y * frame.width + x] == color)
                        bits |= (1 << dy);
                }
                line[x] = (char)(63 + bits);
                if (bits) last = x;
            }
            if (last < 0) continue;

            if (wrote_color) buf += '$';
            wrote_color = true;

            buf += '#';
            sixel_push_num(buf, color);

            int run_count = 0;
            char run_ch = 0;
            for (int x = 0; x <= last; ++x) {
                char ch = line[x];
                if (run_count == 0) {
                    run_ch = ch;
                    run_count = 1;
                } else if (ch == run_ch) {
                    ++run_count;
                } else {
                    sixel_push_run(buf, run_count, run_ch);
                    run_ch = ch;
                    run_count = 1;
                }
            }
            if (run_count > 0)
                sixel_push_run(buf, run_count, run_ch);
        }
        buf += '-';
    }

    buf += "\033\\";
    DWORD written = 0;
    WriteFile(out_h, buf.data(), (DWORD)buf.size(), &written, nullptr);
    return 0;
}
