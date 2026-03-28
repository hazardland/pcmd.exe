// MODULE: clock
// Purpose : fullscreen live clock with big block digits
// Exports : clock_cmd()
// Depends : common.h, terminal.h

#include <ctime>

// 3-wide x 5-tall bitmaps for digits 0-9 (bits: col2 col1 col0)
static const uint8_t digit_px[10][5] = {
    {0b111, 0b101, 0b101, 0b101, 0b111}, // 0
    {0b010, 0b110, 0b010, 0b010, 0b111}, // 1
    {0b111, 0b001, 0b111, 0b100, 0b111}, // 2
    {0b111, 0b001, 0b111, 0b001, 0b111}, // 3
    {0b101, 0b101, 0b111, 0b001, 0b001}, // 4
    {0b111, 0b100, 0b111, 0b001, 0b111}, // 5
    {0b111, 0b100, 0b111, 0b101, 0b111}, // 6
    {0b111, 0b001, 0b001, 0b001, 0b001}, // 7
    {0b111, 0b101, 0b111, 0b101, 0b111}, // 8
    {0b111, 0b101, 0b111, 0b001, 0b111}, // 9
};
// colon: dot on rows 1 and 3
static const uint8_t colon_px[5] = {0, 1, 0, 1, 0};

// draw HH:MM:SS centered on screen using 2-wide block cells
// each digit: 6 chars wide, colon: 2 chars wide, gap: 1 char
// total width: 6*6 + 2*2 + 7*1 = 47
static void draw_clock(const std::string& t, int term_cols, int term_rows) {
    const int clock_w = 47, clock_h = 5;
    int sc = (term_cols - clock_w) / 2;
    int sr = (term_rows - clock_h) / 2;

    std::string frame;
    frame.reserve(512);

    int col = sc;
    for (size_t si = 0; si < t.size(); si++) {
        char ch = t[si];
        bool is_colon = (ch == ':');
        int sym_w = is_colon ? 2 : 6;

        for (int r = 0; r < 5; r++) {
            char buf[32];
            snprintf(buf, sizeof(buf), "\x1b[%d;%dH", sr + r + 1, col + 1);
            frame += buf;
            frame += BLUE;
            if (is_colon) {
                frame += colon_px[r] ? "\xe2\x96\x88\xe2\x96\x88" : "  ";
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

static void clock_cmd() {
    out("\x1b[?25l\x1b[2J");
    std::string prev;
    int prev_cols = 0, prev_rows = 0;

    while (true) {
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
            time_t now = time(nullptr);
            struct tm* tm = localtime(&now);
            char buf[9];
            snprintf(buf, sizeof(buf), "%02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);
            std::string ts(buf);
            int cols = term_width(), rows = term_height();
            if (ts != prev || cols != prev_cols || rows != prev_rows) {
                if (cols != prev_cols || rows != prev_rows) out("\x1b[2J");
                draw_clock(ts, cols, rows);
                prev = ts; prev_cols = cols; prev_rows = rows;
            }
        }

        Sleep(100);
    }
done:
    out("\x1b[0m\x1b[2J\x1b[H\x1b[?25h");
}
