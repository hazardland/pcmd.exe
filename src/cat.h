// MODULE: cat
// Purpose : file printer with syntax highlighting — delegates to highlight.h, image.h, video.h
// Exports : cat()
// Depends : common.h, highlight.h, signal.h, stb_image.h (included earlier by zcmd.cpp)

static bool cat_is_image_ext(const std::string& path) {
    size_t dot = path.rfind('.');
    if (dot == std::string::npos) return false;
    std::string ext = path.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext==".jpg"||ext==".jpeg"||ext==".png"||ext==".bmp"||ext==".gif"||ext==".tga"||ext==".psd";
}

static bool cat_is_video_ext(const std::string& path) {
    size_t dot = path.rfind('.');
    if (dot == std::string::npos) return false;
    std::string ext = path.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext==".mp4"||ext==".mkv"||ext==".avi"||ext==".mov"||ext==".webm"||ext==".flv"||ext==".wmv";
}

struct CatPixel { uint8_t r, g, b; };

static CatPixel cat_sample(const uint8_t* img, int w, int x, int y) {
    const uint8_t* p = img + (y * w + x) * 3;
    return { p[0], p[1], p[2] };
}

static char* cat_write_u8(char* p, uint8_t v) {
    if (v >= 100) { *p++ = '0' + v/100; *p++ = '0' + (v/10)%10; }
    else if (v >= 10) { *p++ = '0' + v/10; }
    *p++ = '0' + v%10;
    return p;
}

static constexpr const char* cat_quad[16] = {
    " ","\xE2\x96\x98","\xE2\x96\x9D","\xE2\x96\x80",
    "\xE2\x96\x96","\xE2\x96\x8C","\xE2\x96\x9E","\xE2\x96\x9B",
    "\xE2\x96\x97","\xE2\x96\x9A","\xE2\x96\x90","\xE2\x96\x9C",
    "\xE2\x96\x84","\xE2\x96\x99","\xE2\x96\x9F","\xE2\x96\x88",
};

static float cat_luma(CatPixel p) { return 0.299f*p.r + 0.587f*p.g + 0.114f*p.b; }
static float cat_dist2(CatPixel a, CatPixel b) {
    float dr=a.r-b.r, dg=a.g-b.g, db=a.b-b.b;
    return dr*dr + dg*dg + db*db;
}

static char* cat_push_cell(char* p, CatPixel tl, CatPixel tr, CatPixel bl, CatPixel br) {
    CatPixel px[4] = { tl, tr, bl, br };
    float lu[4] = { cat_luma(px[0]), cat_luma(px[1]), cat_luma(px[2]), cat_luma(px[3]) };
    int lo=0, hi=0;
    for (int i=1; i<4; ++i) {
        if (lu[i] < lu[lo]) lo = i;
        if (lu[i] > lu[hi]) hi = i;
    }
    CatPixel c0=px[lo], c1=px[hi];
    int mask = 0;
    for (int iter=0; iter<3; ++iter) {
        mask = 0;
        for (int i=0; i<4; ++i)
            if (cat_dist2(px[i], c1) < cat_dist2(px[i], c0))
                mask |= (1 << i);
        int r0=0,g0=0,b0=0,n0=0, r1=0,g1=0,b1=0,n1=0;
        for (int i=0; i<4; ++i) {
            if (mask & (1<<i)) { r1+=px[i].r; g1+=px[i].g; b1+=px[i].b; ++n1; }
            else               { r0+=px[i].r; g0+=px[i].g; b0+=px[i].b; ++n0; }
        }
        if (n0) c0 = { (uint8_t)(r0/n0), (uint8_t)(g0/n0), (uint8_t)(b0/n0) };
        if (n1) c1 = { (uint8_t)(r1/n1), (uint8_t)(g1/n1), (uint8_t)(b1/n1) };
    }
    *p++='\033'; *p++='['; *p++='3'; *p++='8'; *p++=';'; *p++='2'; *p++=';';
    p=cat_write_u8(p,c1.r); *p++=';'; p=cat_write_u8(p,c1.g); *p++=';'; p=cat_write_u8(p,c1.b); *p++='m';
    *p++='\033'; *p++='['; *p++='4'; *p++='8'; *p++=';'; *p++='2'; *p++=';';
    p=cat_write_u8(p,c0.r); *p++=';'; p=cat_write_u8(p,c0.g); *p++=';'; p=cat_write_u8(p,c0.b); *p++='m';
    for (const char* g=cat_quad[mask]; *g;) *p++=*g++;
    return p;
}

static int cat_image(const std::string& path) {
    int img_w, img_h;
    uint8_t* img = stbi_load(path.c_str(), &img_w, &img_h, nullptr, 3);
    if (!img) { out("cat: cannot load image '" + path + "'\r\n"); return 1; }

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    int term_w = 80, term_h = 24;
    if (GetConsoleScreenBufferInfo(out_h, &csbi)) {
        term_w = csbi.srWindow.Right  - csbi.srWindow.Left + 1;
        term_h = csbi.srWindow.Bottom - csbi.srWindow.Top  + 1;
    }
    --term_h;

    double cell_aspect = term_cell_aspect();
    if (cell_aspect <= 0.0) cell_aspect = 0.60;

    int out_w = term_w;
    int out_h2 = (int)((double)img_h / img_w * out_w * cell_aspect + 0.5);
    if (out_h2 > term_h) {
        out_h2 = term_h;
        out_w = (int)((double)img_w / img_h * out_h2 / cell_aspect + 0.5);
    }
    if (out_w < 1) out_w = 1;
    if (out_h2 < 1) out_h2 = 1;

    std::vector<char> buf((size_t)out_h2 * (out_w * 41 + 6));
    char* p = buf.data();

    const double x_scale = (double)img_w / (out_w * 2);
    const double y_scale = (double)img_h / (out_h2 * 2);

    for (int row=0; row<out_h2; ++row) {
        int y0 = (int)((2*row)   * y_scale); if (y0 >= img_h) y0 = img_h-1;
        int y1 = (int)((2*row+1) * y_scale); if (y1 >= img_h) y1 = img_h-1;
        for (int col=0; col<out_w; ++col) {
            int x0 = (int)((2*col)   * x_scale); if (x0 >= img_w) x0 = img_w-1;
            int x1 = (int)((2*col+1) * x_scale); if (x1 >= img_w) x1 = img_w-1;
            p = cat_push_cell(p,
                cat_sample(img, img_w, x0, y0), cat_sample(img, img_w, x1, y0),
                cat_sample(img, img_w, x0, y1), cat_sample(img, img_w, x1, y1));
        }
        *p++='\033'; *p++='['; *p++='0'; *p++='m'; *p++='\r'; *p++='\n';
    }

    DWORD written;
    WriteConsoleA(out_h, buf.data(), (DWORD)(p - buf.data()), &written, nullptr);
    stbi_image_free(img);
    return 0;
}

static int cat_video(const std::string& path) {
    std::wstring wpath = to_wide(path);
    int vid_w = 0, vid_h = 0;
    double duration = 0.0;
    {
        wchar_t probe[2048];
        swprintf(probe, 2048,
            L"ffprobe -v error -select_streams v:0"
            L" -show_entries stream=width,height -of csv=p=0 \"%ls\"",
            wpath.c_str());
        FILE* fp = _wpopen(probe, L"r");
        if (fp) { fscanf(fp, "%d,%d", &vid_w, &vid_h); _pclose(fp); }
    }
    {
        wchar_t probe[2048];
        swprintf(probe, 2048,
            L"ffprobe -v error -show_entries format=duration -of csv=p=0 \"%ls\"",
            wpath.c_str());
        FILE* fp = _wpopen(probe, L"r");
        if (fp) { fscanf(fp, "%lf", &duration); _pclose(fp); }
    }
    if (vid_w <= 0 || vid_h <= 0) {
        out("cat: cannot read video dimensions (is ffmpeg installed?)\r\n");
        return 1;
    }

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    int tw = 80, th = 24;
    if (GetConsoleScreenBufferInfo(out_h, &csbi)) {
        tw = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        th = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    }
    --th;

    double cell_aspect = term_cell_aspect();
    if (cell_aspect <= 0.0) cell_aspect = 0.60;

    int out_w = tw;
    int out_h2 = (int)((double)vid_h / vid_w * out_w * cell_aspect + 0.5);
    if (out_h2 > th) {
        out_h2 = th;
        out_w = (int)((double)vid_w / vid_h * out_h2 / cell_aspect + 0.5);
    }
    if (out_w < 1) out_w = 1;
    if (out_h2 < 1) out_h2 = 1;

    int frame_w = out_w * 2, frame_h = out_h2 * 2;
    double fps = 24.0;
    long long frame_ms = (long long)(1000.0 / fps);

    wchar_t cmd[2048];
    swprintf(cmd, 2048,
        L"ffmpeg -hide_banner -loglevel quiet -i \"%ls\""
        L" -vf fps=%.3f,scale=%d:%d -f rawvideo -pix_fmt rgb24 -",
        wpath.c_str(), fps, frame_w, frame_h);

    FILE* pipe = _wpopen(cmd, L"rb");
    if (!pipe) { out("cat: failed to open video pipe\r\n"); return 1; }

    size_t frame_bytes = (size_t)frame_w * frame_h * 3;
    std::vector<uint8_t> rawframe(frame_bytes);
    std::vector<char> vbuf((size_t)out_h2 * (out_w * 41 + 6) + 8);

    size_t slash = path.find_last_of("/\\");
    std::string fname = (slash == std::string::npos) ? path : path.substr(slash + 1);

    auto fmt_ts = [](double secs) -> std::string {
        int s = (int)secs;
        char buf[16]; snprintf(buf, sizeof(buf), "%d:%02d", s/60, s%60);
        return buf;
    };
    std::string total_ts = fmt_ts(duration);

    char orig_title[512] = {};
    GetConsoleTitleA(orig_title, sizeof(orig_title));

    out("\033[?25l\033[2J\033[H");
    ctrl_c_fired = false;

    bool stop = false;
    int frame_no = 0;
    while (!stop && fread(rawframe.data(), 1, frame_bytes, pipe) == frame_bytes) {
        ULONGLONG t0 = GetTickCount64();

        {
            std::string ts = fmt_ts(frame_no / fps);
            std::string title = ts + "/" + total_ts + " - " + fname;
            SetConsoleTitleA(title.c_str());
        }
        ++frame_no;

        char* p = vbuf.data();
        *p++='\033'; *p++='['; *p++='H';
        for (int row = 0; row < out_h2; ++row) {
            for (int col = 0; col < out_w; ++col) {
                auto px = [&](int x, int y) -> CatPixel {
                    const uint8_t* d = rawframe.data() + (y * frame_w + x) * 3;
                    return { d[0], d[1], d[2] };
                };
                p = cat_push_cell(p, px(col*2,row*2),   px(col*2+1,row*2),
                                    px(col*2,row*2+1), px(col*2+1,row*2+1));
            }
            *p++='\033'; *p++='['; *p++='0'; *p++='m'; *p++='\r'; *p++='\n';
        }
        DWORD wr;
        WriteFile(out_h, vbuf.data(), (DWORD)(p - vbuf.data()), &wr, nullptr);

        if (ctrl_c_fired) stop = true;
        DWORD nevents = 0;
        GetNumberOfConsoleInputEvents(in_h, &nevents);
        while (nevents-- > 0) {
            INPUT_RECORD ir; DWORD rd;
            ReadConsoleInput(in_h, &ir, 1, &rd);
            if (ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown) {
                WORD vk = ir.Event.KeyEvent.wVirtualKeyCode;
                DWORD ctrl = ir.Event.KeyEvent.dwControlKeyState;
                if (vk == VK_ESCAPE || (vk == 'C' && (ctrl & (LEFT_CTRL_PRESSED|RIGHT_CTRL_PRESSED))))
                    stop = true;
            }
        }

        ULONGLONG elapsed = GetTickCount64() - t0;
        if ((long long)elapsed < frame_ms) Sleep((DWORD)(frame_ms - elapsed));
    }

    _pclose(pipe);
    out("\033[0m\033[?25h\r\n");
    SetConsoleTitleA(orig_title);
    return 0;
}

// Reads a file and prints it with syntax highlighting. filter is an optional case-insensitive
// substring — only lines containing it are shown (like cat file | grep word).
int cat(const std::string& path, const std::string& filter = "") {
    std::string p = normalize_path(path);
    if (cat_is_image_ext(p)) return cat_image(p);
    if (cat_is_video_ext(p)) return cat_video(p);
    std::ifstream f(p);
    if (!f.is_open()) { out("cat: cannot open '" + path + "'\r\n"); return 1; }
    lang l = detect_lang(p);
    std::string flt = filter;
    std::transform(flt.begin(), flt.end(), flt.begin(), ::tolower);
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back()=='\r') line.pop_back();
        if (!flt.empty()) {
            std::string ll = line;
            std::transform(ll.begin(), ll.end(), ll.begin(), ::tolower);
            if (ll.find(flt) == std::string::npos) continue;
        }
        out(colorize_line(line, l) + "\r\n");
    }
    return 0;
}
