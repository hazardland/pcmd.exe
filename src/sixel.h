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

struct SixelRenderOptions {
    int colors;
    bool dither;
    bool home;
    int palette_interval;
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
    std::vector<uint32_t> hist_count;
    std::vector<uint32_t> hist_rsum;
    std::vector<uint32_t> hist_gsum;
    std::vector<uint32_t> hist_bsum;
    std::vector<int> hist_used_bins;
    std::vector<SixelHistColor> hist_colors;
    std::vector<SixelBox> hist_boxes;
    std::vector<SixelColor> cached_palette;
    int cached_palette_colors = 0;
    int cached_palette_interval = 0;
    int cached_palette_frames_left = 0;
    std::vector<uint8_t> pixels;
    std::vector<uint8_t> used;
    std::vector<uint16_t> lut;
    std::vector<float> err_curr;
    std::vector<float> err_next;
    std::vector<char> line;
    std::vector<uint8_t> band_used;
    std::vector<int> band_colors;
    std::vector<uint8_t> band_masks;
    std::vector<int> touched_slots;
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

static std::vector<SixelColor> sixel_build_palette(const SixelFrame& frame, int target_colors, SixelScratch& scratch) {
    const int hist_size = 32 * 32 * 32;
    if ((int)scratch.hist_count.size() != hist_size) {
        scratch.hist_count.assign(hist_size, 0);
        scratch.hist_rsum.assign(hist_size, 0);
        scratch.hist_gsum.assign(hist_size, 0);
        scratch.hist_bsum.assign(hist_size, 0);
    } else {
        for (int idx : scratch.hist_used_bins) {
            scratch.hist_count[idx] = 0;
            scratch.hist_rsum[idx] = 0;
            scratch.hist_gsum[idx] = 0;
            scratch.hist_bsum[idx] = 0;
        }
    }
    scratch.hist_used_bins.clear();

    for (int y = 0; y < frame.height; ++y) {
        for (int x = 0; x < frame.width; ++x) {
            const uint8_t* src = frame.rgb + ((size_t)y * frame.width + x) * 3;
            int idx = ((src[0] >> 3) << 10) | ((src[1] >> 3) << 5) | (src[2] >> 3);
            if (!scratch.hist_count[idx])
                scratch.hist_used_bins.push_back(idx);
            ++scratch.hist_count[idx];
            scratch.hist_rsum[idx] += src[0];
            scratch.hist_gsum[idx] += src[1];
            scratch.hist_bsum[idx] += src[2];
        }
    }

    std::vector<SixelHistColor>& colors = scratch.hist_colors;
    colors.clear();
    colors.reserve(scratch.hist_used_bins.size());
    for (int idx : scratch.hist_used_bins) {
        uint32_t count = scratch.hist_count[idx];
        colors.push_back({
            (uint8_t)(scratch.hist_rsum[idx] / count),
            (uint8_t)(scratch.hist_gsum[idx] / count),
            (uint8_t)(scratch.hist_bsum[idx] / count),
            (int)count
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

    std::vector<SixelBox>& boxes = scratch.hist_boxes;
    boxes.clear();
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

static uint8_t sixel_lookup_color(uint8_t r, uint8_t g, uint8_t b, const std::vector<SixelColor>& palette, std::vector<uint16_t>& lut) {
    int ri = r >> 3;
    int gi = g >> 3;
    int bi = b >> 3;
    size_t key = (size_t)(ri << 10) | (size_t)(gi << 5) | (size_t)bi;
    uint16_t& cached = lut[key];
    if (cached == 0xFFFF) {
        float rr = (float)(ri * 255) / 31.0f;
        float gg = (float)(gi * 255) / 31.0f;
        float bb = (float)(bi * 255) / 31.0f;
        cached = sixel_nearest_color(rr, gg, bb, palette);
    }
    return (uint8_t)cached;
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

static double sixel_width_scale() {
    return term_sixel_width_scale();
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

    double width_scale = sixel_width_scale();
    double scale_x = (double)max_pixel_w / (src_w * width_scale);
    double scale_y = (double)max_pixel_h / src_h;
    double scale = std::min(1.0, std::min(scale_x, scale_y));
    if (scale <= 0.0) scale = 1.0;

    fit.pixel_w = (int)(src_w * scale * width_scale + 0.5);
    fit.pixel_h = (int)(src_h * scale + 0.5);
    if (fit.pixel_w < 1) fit.pixel_w = 1;
    if (fit.pixel_h < 1) fit.pixel_h = 1;

    fit.cell_w = std::max(1, (fit.pixel_w + cell_px_w - 1) / cell_px_w);
    fit.cell_h = std::max(1, (fit.pixel_h + cell_px_h - 1) / cell_px_h);
    return fit;
}

static int sixel_render(const SixelFrame& frame, const SixelFit& fit, const SixelRenderOptions& opts) {
    (void)fit;
    if (!sixel_supported()) return 1;
    if (!frame.rgb || frame.width < 1 || frame.height < 1) return 1;

    static thread_local SixelScratch scratch;
    int target_colors = opts.colors;
    if (target_colors < 2) target_colors = 2;
    if (target_colors > 256) target_colors = 256;
    int palette_interval = opts.palette_interval;
    if (palette_interval < 1) palette_interval = 1;

    bool rebuild_palette =
        scratch.cached_palette.empty() ||
        scratch.cached_palette_colors != target_colors ||
        scratch.cached_palette_interval != palette_interval ||
        scratch.cached_palette_frames_left <= 0;

    if (rebuild_palette) {
        scratch.cached_palette = sixel_build_palette(frame, target_colors, scratch);
        scratch.cached_palette_colors = target_colors;
        scratch.cached_palette_interval = palette_interval;
        scratch.cached_palette_frames_left = palette_interval - 1;
    } else {
        --scratch.cached_palette_frames_left;
    }

    const std::vector<SixelColor>& palette = scratch.cached_palette;
    const int palette_size = (int)palette.size();
    size_t pixel_count = (size_t)frame.width * frame.height;
    scratch.pixels.resize(pixel_count);
    scratch.used.assign((size_t)palette_size, 0);
    scratch.lut.assign(32 * 32 * 32, 0xFFFF);

    std::vector<uint8_t>& pixels = scratch.pixels;
    std::vector<uint8_t>& used = scratch.used;
    std::vector<uint16_t>& lut = scratch.lut;
    if (opts.dither) {
        scratch.err_curr.assign((size_t)(frame.width + 2) * 3, 0.0f);
        scratch.err_next.assign((size_t)(frame.width + 2) * 3, 0.0f);
        std::vector<float>& err_curr = scratch.err_curr;
        std::vector<float>& err_next = scratch.err_next;

        for (int y = 0; y < frame.height; ++y) {
            if (y > 0) {
                err_curr.swap(err_next);
                std::fill(err_next.begin(), err_next.end(), 0.0f);
            }
            const uint8_t* src_row = frame.rgb + (size_t)y * frame.width * 3;
            for (int x = 0; x < frame.width; ++x) {
                size_t pos = ((size_t)y * frame.width + x);
                size_t src_off = (size_t)x * 3;
                size_t err_off = (size_t)(x + 1) * 3;
                float r = (float)sixel_clamp_u8((float)src_row[src_off + 0] + err_curr[err_off + 0]);
                float g = (float)sixel_clamp_u8((float)src_row[src_off + 1] + err_curr[err_off + 1]);
                float b = (float)sixel_clamp_u8((float)src_row[src_off + 2] + err_curr[err_off + 2]);
                uint8_t idx = sixel_lookup_color((uint8_t)r, (uint8_t)g, (uint8_t)b, palette, lut);
                pixels[pos] = idx;
                used[idx] = 1;

                float er = r - palette[idx].r;
                float eg = g - palette[idx].g;
                float eb = b - palette[idx].b;
                err_curr[err_off + 3 + 0] += er * (7.0f / 16.0f);
                err_curr[err_off + 3 + 1] += eg * (7.0f / 16.0f);
                err_curr[err_off + 3 + 2] += eb * (7.0f / 16.0f);

                err_next[err_off - 3 + 0] += er * (3.0f / 16.0f);
                err_next[err_off - 3 + 1] += eg * (3.0f / 16.0f);
                err_next[err_off - 3 + 2] += eb * (3.0f / 16.0f);

                err_next[err_off + 0] += er * (5.0f / 16.0f);
                err_next[err_off + 1] += eg * (5.0f / 16.0f);
                err_next[err_off + 2] += eb * (5.0f / 16.0f);

                err_next[err_off + 3 + 0] += er * (1.0f / 16.0f);
                err_next[err_off + 3 + 1] += eg * (1.0f / 16.0f);
                err_next[err_off + 3 + 2] += eb * (1.0f / 16.0f);
            }
        }
    } else {
        for (int y = 0; y < frame.height; ++y) {
            const uint8_t* src_row = frame.rgb + (size_t)y * frame.width * 3;
            for (int x = 0; x < frame.width; ++x) {
                size_t pos = ((size_t)y * frame.width + x);
                size_t src_off = (size_t)x * 3;
                uint8_t idx = sixel_lookup_color(src_row[src_off + 0], src_row[src_off + 1], src_row[src_off + 2], palette, lut);
                pixels[pos] = idx;
                used[idx] = 1;
            }
        }
    }

    scratch.buf.clear();
    scratch.buf.reserve((size_t)frame.width * frame.height * 2 + palette_size * 24 + 1024);
    std::string& buf = scratch.buf;

    if (opts.home) buf += "\033[H";
    buf += "\033P7;1q";
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
    scratch.band_masks.resize((size_t)palette_size * frame.width);
    scratch.touched_slots.clear();
    scratch.touched_slots.reserve((size_t)frame.width * 6);
    std::vector<char>& line = scratch.line;
    std::vector<uint8_t>& band_used = scratch.band_used;
    std::vector<int>& band_colors = scratch.band_colors;
    std::vector<uint8_t>& band_masks = scratch.band_masks;
    std::vector<int>& touched_slots = scratch.touched_slots;
    for (int y0 = 0; y0 < frame.height; y0 += 6) {
        std::fill(band_used.begin(), band_used.end(), 0);
        band_colors.clear();
        touched_slots.clear();
        for (int x = 0; x < frame.width; ++x) {
            uint8_t col_colors[6];
            uint8_t col_bits[6];
            int col_count = 0;
            for (int dy = 0; dy < 6; ++dy) {
                int y = y0 + dy;
                if (y >= frame.height) break;
                uint8_t color = pixels[(size_t)y * frame.width + x];
                uint8_t bit = (uint8_t)(1 << dy);

                int hit = -1;
                for (int i = 0; i < col_count; ++i) {
                    if (col_colors[i] == color) {
                        hit = i;
                        break;
                    }
                }
                if (hit >= 0) {
                    col_bits[hit] |= bit;
                } else {
                    col_colors[col_count] = color;
                    col_bits[col_count] = bit;
                    ++col_count;
                }
            }

            for (int i = 0; i < col_count; ++i) {
                uint8_t color = col_colors[i];
                size_t slot = (size_t)color * frame.width + x;
                if (!band_masks[slot])
                    touched_slots.push_back((int)slot);
                band_masks[slot] |= col_bits[i];
                if (!band_used[color]) {
                    band_used[color] = 1;
                    band_colors.push_back(color);
                }
            }
        }

        bool wrote_color = false;
        for (int color : band_colors) {
            if (!used[color]) continue;

            uint8_t* mask_row = band_masks.data() + (size_t)color * frame.width;
            int last = -1;
            for (int x = 0; x < frame.width; ++x) {
                int bits = mask_row[x];
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
        for (int slot : touched_slots)
            band_masks[(size_t)slot] = 0;
        buf += '-';
    }

    buf += "\033\\";
    DWORD written = 0;
    WriteFile(out_h, buf.data(), (DWORD)buf.size(), &written, nullptr);
    return 0;
}

static int sixel_render(const SixelFrame& frame, const SixelFit& fit, bool home = false) {
    SixelRenderOptions opts = { 256, true, home, 1 };
    return sixel_render(frame, fit, opts);
}
