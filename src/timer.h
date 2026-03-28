// MODULE: timer
// Purpose : stopwatch counting up with centiseconds, big block digits
// Exports : timer_cmd()
// Depends : common.h, terminal.h, clock.h (digit_px, colon_px)

// dot: only bottom-center cell lit
static const uint8_t dot_px[5] = { 0, 0, 0, 0, 1 };

// draw time string centered — digits 6w, colon/dot 2w, gap 1
static void draw_timer(const std::string& t, int term_cols, int term_rows) {
    // width: each digit=6, colon/dot=2, gaps=(symbols-1)*1
    int nsyms = (int)t.size();
    int ndigits = 0, nsep = 0;
    for (char c : t) (c == ':' || c == '.') ? nsep++ : ndigits++;
    int timer_w = ndigits * 6 + nsep * 2 + (nsyms - 1);
    const int timer_h = 5;
    int sc = (term_cols - timer_w) / 2;
    int sr = (term_rows - timer_h) / 2;

    std::string frame;
    frame.reserve(768);

    int col = sc;
    for (size_t si = 0; si < t.size(); si++) {
        char ch = t[si];
        bool is_colon = (ch == ':');
        bool is_dot   = (ch == '.');
        int sym_w = (is_colon || is_dot) ? 2 : 6;

        for (int r = 0; r < 5; r++) {
            char buf[32];
            snprintf(buf, sizeof(buf), "\x1b[%d;%dH", sr + r + 1, col + 1);
            frame += buf;
            frame += BLUE;
            if (is_colon) {
                frame += colon_px[r] ? "\xe2\x96\x88\xe2\x96\x88" : "  ";
            } else if (is_dot) {
                frame += dot_px[r]   ? "\xe2\x96\x88\xe2\x96\x88" : "  ";
            } else {
                int d = ch - '0';
                for (int b = 2; b >= 0; b--)
                    frame += (digit_px[d][r] >> b) & 1 ? "\xe2\x96\x88\xe2\x96\x88" : "  ";
            }
            frame += RESET;
        }
        col += sym_w + 1;
    }
    out(frame);
}

static void timer_cmd() {
    out("\x1b[?25l\x1b[2J");

    ULONGLONG start = GetTickCount64();
    std::string prev;
    int prev_cols = 0, prev_rows = 0;

    while (true) {
        // non-blocking keypress — any key stops
        DWORD n = 0;
        GetNumberOfConsoleInputEvents(in_h, &n);
        while (n-- > 0) {
            INPUT_RECORD ir; DWORD rd;
            ReadConsoleInputW(in_h, &ir, 1, &rd);
            if (ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown)
                goto done;
        }

        {
            ULONGLONG elapsed = GetTickCount64() - start;
            int cs   = (int)((elapsed / 10)  % 100);
            int secs = (int)((elapsed / 1000) % 60);
            int mins = (int)((elapsed / 60000) % 60);
            int hrs  = (int)( elapsed / 3600000);

            int cols = term_width(), rows = term_height();
            char buf[16];
            if (cols >= 64)
                snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%02d", hrs, mins, secs, cs);
            else
                snprintf(buf, sizeof(buf), "%02d:%02d.%02d", mins, secs, cs);
            std::string ts(buf);
            if (ts != prev || cols != prev_cols || rows != prev_rows) {
                if (cols != prev_cols || rows != prev_rows) out("\x1b[2J");
                draw_timer(ts, cols, rows);
                prev = ts; prev_cols = cols; prev_rows = rows;
            }
        }

        Sleep(10);
    }
done:
    out("\x1b[0m\x1b[2J\x1b[H\x1b[?25h");
    out("Stopped at " + prev + "\r\n");
}
