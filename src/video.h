// MODULE: video
// Purpose : inline video playback — ffmpeg pipe, frame rendering via imgpush_cell, Esc/Ctrl+C stop
// Exports : is_video_ext() cat_video()
// Depends : common.h, signal.h (ctrl_c_fired), image.h (imgpush_cell imgpixel)

static bool is_video_ext(const std::string& path) {
    size_t dot = path.rfind('.');
    if (dot == std::string::npos) return false;
    std::string ext = path.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext==".mp4"||ext==".mkv"||ext==".avi"||ext==".mov"||ext==".webm"||ext==".flv"||ext==".wmv";
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

    int out_w = tw;
    int out_h2 = (int)((double)vid_h / vid_w * out_w / 2.0 + 0.5);
    if (out_h2 > th) {
        out_h2 = th;
        out_w = (int)((double)vid_w / vid_h * out_h2 * 2.0 + 0.5);
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
                auto px = [&](int x, int y) -> imgpixel {
                    const uint8_t* d = rawframe.data() + (y * frame_w + x) * 3;
                    return { d[0], d[1], d[2] };
                };
                p = imgpush_cell(p, px(col*2,row*2),   px(col*2+1,row*2),
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
