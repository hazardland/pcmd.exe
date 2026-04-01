// MODULE: terminal
// Purpose : terminal dimension queries and shared VT UI notes
// Exports : term_width() term_height()
// Depends : common.h
//
// VT UI technique for fullscreen tools:
// - Prefer the VT alternate screen over CreateConsoleScreenBuffer for new full-screen UIs.
// - Enter with ESC[?1049h and leave with ESC[?1049l so the original shell view comes back cleanly.
// - Hide/show cursor with ESC[?25l and ESC[?25h while the UI owns the screen.
// - Render with the same VT color sequences the shell already uses, instead of legacy WORD attributes.
// - Keep an in-memory frame buffer (chars + styles) and diff against the previous frame so redraws stay fast.
// - Query visible size from the active terminal window, then move the cursor with ESC[row;colH when painting.
// - This is the preferred path for future full-screen tools such as top, resmon, and edit because it keeps
//   colors consistent with the normal terminal and avoids classic console buffer palette mismatches.

// Returns the visible column count of the terminal window; falls back to 80 if not a real console.
int term_width() {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(out_h, &csbi))
        return csbi.srWindow.Right - csbi.srWindow.Left + 1;
    return 80;
}

int term_height() {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(out_h, &csbi))
        return csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    return 24;
}

static double term_cell_width_setting() {
    // Temporary hardcoded Windows Terminal profile value until we add a real
    // Zcmd-side setting / calibration flow.
    return 0.60;
}

static double term_line_height_setting() {
    // Temporary hardcoded Windows Terminal profile value.
    return 1.00;
}

static double term_cell_aspect() {
    double cell_width = term_cell_width_setting();
    double line_height = term_line_height_setting();
    if (cell_width <= 0.0) cell_width = 0.60;
    if (line_height <= 0.0) line_height = 1.00;
    return cell_width / line_height;
}

static double term_sixel_width_scale() {
    double cell_width = term_cell_width_setting();
    double line_height = term_line_height_setting();
    if (cell_width <= 0.0) cell_width = 0.60;
    if (line_height <= 0.0) line_height = 1.00;

    // Calibrated neutral baseline for Windows Terminal SIXEL output.
    double scale = (0.52 * line_height) / cell_width;
    if (scale < 0.25) scale = 0.25;
    if (scale > 2.0) scale = 2.0;
    return scale;
}
