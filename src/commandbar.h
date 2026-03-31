#pragma once

// MODULE: commandbar
// Purpose : shared VT command-bar styling and rendering for fullscreen tools
// Exports : CommandPalette CommandItem | command_item() commandbar_draw() commandbar_overlay_draw() commandbar_style_vt()
// Depends : common.h

enum COMMAND_BAR_STYLE : WORD {
    COMMAND_BAR_STYLE_NONE = 0x5000,
    COMMAND_BAR_STYLE_FILL,
    COMMAND_BAR_STYLE_KEY,
    COMMAND_BAR_STYLE_TEXT,
};

struct CommandPalette {
    int fill_fg = -1;
    int fill_bg = -1;
    int key_fg = 16;
    int key_bg = 75;
    int text_fg = 250;
    int text_bg = -1;
};

struct CommandItem {
    std::wstring key;
    std::wstring text;
};

static CommandItem command_item(const std::wstring& key, const std::wstring& text) {
    CommandItem item;
    item.key = key;
    item.text = text;
    return item;
}

static CommandPalette command_palette_default() {
    return CommandPalette();
}

static bool commandbar_is_style(WORD attr) {
    return attr >= COMMAND_BAR_STYLE_NONE && attr <= COMMAND_BAR_STYLE_TEXT;
}

static std::string commandbar_vt_fg_bg(int fg, int bg) {
    std::string vt;
    if (bg >= 0) {
        vt += "\x1b[48;5;";
        vt += std::to_string(bg);
        vt += 'm';
    } else {
        vt += "\x1b[49m";
    }
    if (fg >= 0) {
        vt += "\x1b[38;5;";
        vt += std::to_string(fg);
        vt += 'm';
    } else {
        vt += "\x1b[39m";
    }
    return vt;
}

static std::string commandbar_style_vt(WORD attr, const CommandPalette& palette) {
    switch (attr) {
    case COMMAND_BAR_STYLE_FILL: return commandbar_vt_fg_bg(palette.fill_fg, palette.fill_bg);
    case COMMAND_BAR_STYLE_KEY:  return commandbar_vt_fg_bg(palette.key_fg, palette.key_bg);
    case COMMAND_BAR_STYLE_TEXT: return commandbar_vt_fg_bg(palette.text_fg, palette.text_bg);
    default:                     return "";
    }
}

static void commandbar_put(std::vector<wchar_t>& chars, std::vector<WORD>& attrs,
    int width, int height, int x, int y, wchar_t ch, WORD attr) {
    if (x < 0 || y < 0 || x >= width || y >= height) return;
    size_t idx = (size_t)y * width + x;
    chars[idx] = ch;
    attrs[idx] = attr;
}

static void commandbar_fill(std::vector<wchar_t>& chars, std::vector<WORD>& attrs,
    int width, int height, int x, int y, int len, wchar_t ch, WORD attr) {
    for (int i = 0; i < len; i++)
        commandbar_put(chars, attrs, width, height, x + i, y, ch, attr);
}

static void commandbar_text(std::vector<wchar_t>& chars, std::vector<WORD>& attrs,
    int width, int height, int x, int y, const std::wstring& text, WORD attr) {
    for (int i = 0; i < (int)text.size(); i++)
        commandbar_put(chars, attrs, width, height, x + i, y, text[i], attr);
}

static int commandbar_item_draw(std::vector<wchar_t>& chars, std::vector<WORD>& attrs,
    int width, int height, int x, int y, const CommandItem& item) {
    if (x >= width) return x;

    std::wstring tail = ui_text_tail(item.key, item.text);
    commandbar_text(chars, attrs, width, height, x, y, item.key, COMMAND_BAR_STYLE_KEY);
    x += (int)item.key.size();
    commandbar_text(chars, attrs, width, height, x, y, tail, COMMAND_BAR_STYLE_TEXT);
    x += (int)tail.size();
    if (x < width) {
        commandbar_put(chars, attrs, width, height, x, y, L' ', COMMAND_BAR_STYLE_FILL);
        x++;
    }
    return x;
}

static int commandbar_span(const std::vector<CommandItem>& items) {
    int width = 0;
    for (const CommandItem& item : items)
        width += (int)item.key.size() + (int)ui_text_tail(item.key, item.text).size() + 1;
    return width;
}

static void commandbar_draw(std::vector<wchar_t>& chars, std::vector<WORD>& attrs, int width, int height, int y,
    const std::vector<CommandItem>& items, int start_x, bool fill_row);

static std::string commandbar_row_vt(const std::vector<CommandItem>& items, int width, int start_x = 1, bool fill_row = true) {
    if (width <= 0) return "";

    std::vector<wchar_t> chars((size_t)width, L' ');
    std::vector<WORD> attrs((size_t)width, 0);
    commandbar_draw(chars, attrs, width, 1, 0, items, start_x, fill_row);

    int first = fill_row ? 0 : std::max(0, start_x);
    int last = fill_row ? width - 1 : std::min(width - 1, start_x + commandbar_span(items) - 1);
    if (first > last || first >= width) return "";

    std::string row;
    WORD style = (WORD)-1;
    CommandPalette palette = command_palette_default();
    std::wstring run;
    for (int x = first; x <= last; x++) {
        WORD next = attrs[x];
        if (next != style) {
            if (!run.empty()) {
                row += to_utf8(run);
                run.clear();
            }
            row += commandbar_style_vt(next, palette);
            style = next;
        }
        run.push_back(chars[x]);
    }
    if (!run.empty())
        row += to_utf8(run);
    row += RESET;
    return row;
}

static void commandbar_draw(std::vector<wchar_t>& chars, std::vector<WORD>& attrs, int width, int height, int y,
    const std::vector<CommandItem>& items, int start_x = 1, bool fill_row = true) {
    if (width <= 0 || height <= 0 || y < 0 || y >= height) return;

    if (fill_row)
        commandbar_fill(chars, attrs, width, height, 0, y, width, L' ', COMMAND_BAR_STYLE_FILL);

    int x = std::max(0, start_x);
    for (const CommandItem& item : items) {
        x = commandbar_item_draw(chars, attrs, width, height, x, y, item);
        if (x >= width) break;
    }
}

static bool commandbar_overlay_read_size(int& width, int& height) {
    CONSOLE_SCREEN_BUFFER_INFO csbi = {};
    if (!GetConsoleScreenBufferInfo(out_h, &csbi)) return false;
    width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    return width > 0 && height > 0;
}

static void commandbar_overlay_append_cursor_move(std::string& outbuf, int x, int y) {
    outbuf += "\x1b[";
    outbuf += std::to_string(y + 1);
    outbuf += ';';
    outbuf += std::to_string(x + 1);
    outbuf += 'H';
}

static void commandbar_overlay_draw(const std::vector<CommandItem>& items, int y = -1, int start_x = 1, bool fill_row = true) {
    int width = 0, height = 0;
    if (!commandbar_overlay_read_size(width, height)) return;

    if (y < 0) y = height - 1;
    if (y < 0 || y >= height) return;

    CONSOLE_SCREEN_BUFFER_INFO csbi = {};
    if (!GetConsoleScreenBufferInfo(out_h, &csbi)) return;
    int cursor_x = csbi.dwCursorPosition.X;
    int cursor_y = csbi.dwCursorPosition.Y - csbi.srWindow.Top;
    if (cursor_y < 0 || cursor_y >= height)
        cursor_y = 0;

    std::vector<wchar_t> chars((size_t)width, L' ');
    std::vector<WORD> attrs((size_t)width, 0);
    commandbar_draw(chars, attrs, width, 1, 0, items, start_x, fill_row);

    int first = fill_row ? 0 : std::max(0, start_x);
    int last = fill_row ? width - 1 : std::min(width - 1, start_x + commandbar_span(items) - 1);
    if (first > last || first >= width)
        return;

    std::string frame;
    commandbar_overlay_append_cursor_move(frame, first, y);
    WORD style = (WORD)-1;
    std::wstring run;
    CommandPalette palette = command_palette_default();
    for (int x = first; x <= last; x++) {
        WORD next = attrs[x];
        if (next != style) {
            if (!run.empty()) {
                frame += to_utf8(run);
                run.clear();
            }
            frame += commandbar_style_vt(next, palette);
            style = next;
        }
        run.push_back(chars[x]);
    }
    if (!run.empty())
        frame += to_utf8(run);
    frame += RESET;
    commandbar_overlay_append_cursor_move(frame, cursor_x, cursor_y);
    out(frame);
}
