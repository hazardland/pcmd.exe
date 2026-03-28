// MODULE: snow
// Purpose : falling snow screensaver with parallax and drift
// Exports : snow_cmd()
// Depends : common.h, terminal.h

static void snow_cmd() {
    srand((unsigned)time(nullptr));

    int cols = term_width();
    int rows = term_height();

    // type 0=· (small/far/slow), 1=* (medium), 2=❄ (large/close/fast)
    static const char* flakes[] = { "\xc2\xb7", "*", "\xe2\x9d\x84" };
    static const int speeds[]   = { 5, 3, 1 };  // ticks per move: slow→fast
    static const int drifts[]   = { 8, 5, 3 };  // 1-in-N chance to drift per move

    struct flake {
        int col, row;
        int type;
        int tick;
    };

    auto make_flake = [&](bool random_row) -> flake {
        int t = rand() % 3;
        return { rand() % cols, random_row ? rand() % rows : 0, t, rand() % speeds[t] };
    };

    int count = cols / 2;
    std::vector<flake> snow(count);
    for (int i = 0; i < count; i++) snow[i] = make_flake(true);

    auto pos = [](std::string& s, int r, int c) {
        char buf[32];
        snprintf(buf, sizeof(buf), "\x1b[%d;%dH", r + 1, c + 1);
        s += buf;
    };

    out("\x1b[?25l\x1b[2J");

    while (true) {
        // handle resize
        int new_cols = term_width(), new_rows = term_height();
        if (new_cols != cols || new_rows != rows) {
            cols = new_cols; rows = new_rows;
            count = cols / 2;
            snow.resize(count);
            for (int i = 0; i < count; i++) snow[i] = make_flake(true);
            out("\x1b[2J");
        }

        // non-blocking keypress — any key exits
        DWORD n = 0;
        GetNumberOfConsoleInputEvents(in_h, &n);
        while (n-- > 0) {
            INPUT_RECORD ir; DWORD rd;
            ReadConsoleInputW(in_h, &ir, 1, &rd);
            if (ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown)
                goto done;
        }

        {
            std::string frame;
            frame.reserve(count * 24);

            for (auto& f : snow) {
                if (++f.tick < speeds[f.type]) continue;
                f.tick = 0;

                // erase old position
                pos(frame, f.row, f.col);
                frame += " ";

                // horizontal drift
                if (rand() % drifts[f.type] == 0)
                    f.col += (rand() % 2) ? 1 : -1;
                if (f.col < 0)    f.col = 0;
                if (f.col >= cols) f.col = cols - 1;

                // move down
                f.row++;
                if (f.row >= rows) f = make_flake(false);

                // color by type: ❄ bright white, * white, · gray
                pos(frame, f.row, f.col);
                if      (f.type == 2) frame += "\x1b[97m";
                else if (f.type == 1) frame += "\x1b[37m";
                else                  frame += "\x1b[38;5;240m";
                frame += flakes[f.type];
                frame += RESET;
            }

            out(frame);
        }

        Sleep(40);
    }
done:
    out("\x1b[0m\x1b[2J\x1b[H\x1b[?25h");
}
