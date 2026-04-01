// MODULE: vid
// Purpose : SIXEL-first video command
// Exports : vid_cmd()
// Depends : common.h, sixel.h

static double vid_pick_fps(int pixel_w, int pixel_h) {
    int pixels = pixel_w * pixel_h;
    if (pixels >= 900 * 700) return 5.0;
    if (pixels >= 700 * 500) return 6.0;
    if (pixels >= 520 * 380) return 7.0;
    if (pixels >= 400 * 300) return 8.0;
    return 9.0;
}

static int vid_pick_colors(int pixel_w, int pixel_h) {
    int pixels = pixel_w * pixel_h;
    if (pixels >= 900 * 700) return 48;
    if (pixels >= 700 * 500) return 64;
    if (pixels >= 520 * 380) return 72;
    return 96;
}

static int vid_pick_palette_interval(int pixel_w, int pixel_h) {
    int pixels = pixel_w * pixel_h;
    if (pixels >= 900 * 700) return 4;
    if (pixels >= 700 * 500) return 3;
    return 2;
}

static std::string vid_fmt_ts(double secs) {
    int s = (int)secs;
    char buf[16];
    snprintf(buf, sizeof(buf), "%d:%02d", s / 60, s % 60);
    return buf;
}

int vid_cmd(const std::string& line) {
    std::string path = line.size() > 3 ? line.substr(3) : "";
    while (!path.empty() && path.front() == ' ') path.erase(path.begin());
    while (!path.empty() && path.back()  == ' ') path.pop_back();

    if (path.empty()) {
        out("vid: usage: vid <path>\r\n");
        return 1;
    }
    if (!sixel_supported()) {
        out("vid: SIXEL is not supported in this terminal\r\n");
        return 1;
    }

    std::string norm = normalize_path(path);
    std::wstring wpath = to_wide(norm);
    int src_w = 0, src_h = 0;
    double duration = 0.0;

    {
        wchar_t probe[2048];
        swprintf(probe, 2048,
            L"ffprobe -v error -select_streams v:0"
            L" -show_entries stream=width,height -of csv=p=0 \"%ls\"",
            wpath.c_str());
        FILE* fp = _wpopen(probe, L"r");
        if (fp) { fscanf(fp, "%d,%d", &src_w, &src_h); _pclose(fp); }
    }
    {
        wchar_t probe[2048];
        swprintf(probe, 2048,
            L"ffprobe -v error -show_entries format=duration -of csv=p=0 \"%ls\"",
            wpath.c_str());
        FILE* fp = _wpopen(probe, L"r");
        if (fp) { fscanf(fp, "%lf", &duration); _pclose(fp); }
    }

    if (src_w <= 0 || src_h <= 0) {
        out("vid: cannot read video dimensions (is ffmpeg installed?)\r\n");
        return 1;
    }

    SixelFit fit = sixel_fit(src_w, src_h);
    double fps = vid_pick_fps(fit.pixel_w, fit.pixel_h);
    int colors = vid_pick_colors(fit.pixel_w, fit.pixel_h);
    int palette_interval = vid_pick_palette_interval(fit.pixel_w, fit.pixel_h);
    long long frame_ms = (long long)(1000.0 / fps);

    wchar_t cmd[2048];
    swprintf(cmd, 2048,
        L"ffmpeg -hide_banner -loglevel quiet -i \"%ls\""
        L" -vf fps=%.3f,scale=%d:%d -f rawvideo -pix_fmt rgb24 -",
        wpath.c_str(), fps, fit.pixel_w, fit.pixel_h);

    FILE* pipe = _wpopen(cmd, L"rb");
    if (!pipe) {
        out("vid: failed to open video pipe\r\n");
        return 1;
    }

    size_t frame_bytes = (size_t)fit.pixel_w * fit.pixel_h * 3;
    std::vector<uint8_t> rawframe(frame_bytes);
    SixelRenderOptions opts = { colors, false, true, palette_interval };

    size_t slash = path.find_last_of("/\\");
    std::string fname = (slash == std::string::npos) ? path : path.substr(slash + 1);
    std::string total_ts = vid_fmt_ts(duration);

    char orig_title[512] = {};
    GetConsoleTitleA(orig_title, sizeof(orig_title));

    out("\033[?25l\033[2J\033[H");
    ctrl_c_fired = false;

    bool stop = false;
    int frame_no = 0;
    while (!stop && fread(rawframe.data(), 1, frame_bytes, pipe) == frame_bytes) {
        ULONGLONG t0 = GetTickCount64();

        std::string ts = vid_fmt_ts(frame_no / fps);
        std::string title = ts + "/" + total_ts + " - " + fname;
        SetConsoleTitleA(title.c_str());

        SixelFrame frame = { rawframe.data(), fit.pixel_w, fit.pixel_h };
        if (sixel_render(frame, fit, opts) != 0) {
            stop = true;
            break;
        }

        ++frame_no;
        if (ctrl_c_fired) stop = true;

        DWORD nevents = 0;
        GetNumberOfConsoleInputEvents(in_h, &nevents);
        while (nevents-- > 0) {
            INPUT_RECORD ir; DWORD rd;
            ReadConsoleInput(in_h, &ir, 1, &rd);
            if (ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown) {
                WORD vk = ir.Event.KeyEvent.wVirtualKeyCode;
                DWORD ctrl = ir.Event.KeyEvent.dwControlKeyState;
                if (vk == VK_ESCAPE || (vk == 'C' && (ctrl & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED))))
                    stop = true;
            }
        }

        ULONGLONG elapsed = GetTickCount64() - t0;
        if ((long long)elapsed < frame_ms)
            Sleep((DWORD)(frame_ms - elapsed));
    }

    _pclose(pipe);
    out("\033[0m\033[?25h\r\n");
    SetConsoleTitleA(orig_title);
    return 0;
}
