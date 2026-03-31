// MODULE: explore
// Purpose : Explore dual-panel file explorer using a dedicated console screen buffer
// Exports : struct Entry | struct Panel | struct ExploreState | explore_toggle()
// Depends : common.h, terminal.h, info.h

#include <set>
#include <map>
#include <cstdint>
#include <shellapi.h>

struct Entry {
    std::wstring name;
    bool         is_dir    = false;
    bool         is_hidden = false;
    uint64_t     size      = 0;
    FILETIME     modified  = {};
};

struct Panel {
    std::wstring       cwd;
    std::vector<Entry> all_entries;
    std::vector<Entry> entries;
    std::wstring       filter;
    int                filter_cursor = 0;
    int                cursor = 0;
    int                scroll = 0;
    std::set<std::wstring> selected;
    int                sort_mode = 0;
    bool               active = false;
};

enum EXPLORER_SORT_MODE {
    EXPLORER_SORT_NAME = 0,
    EXPLORER_SORT_EXT,
    EXPLORER_SORT_SIZE,
    EXPLORER_SORT_TIME,
    EXPLORER_SORT_COUNT,
};

enum EXPLORER_FOCUS_MODE {
    EXPLORER_FOCUS_PANEL,
    EXPLORER_FOCUS_FILTER,
    EXPLORER_FOCUS_DIALOG,
};

enum EXPLORER_DIALOG_KIND {
    EXPLORER_DIALOG_NONE,
    EXPLORER_DIALOG_MKDIR,
    EXPLORER_DIALOG_COPY,
    EXPLORER_DIALOG_MOVE,
    EXPLORER_DIALOG_RECYCLE,
    EXPLORER_DIALOG_DELETE,
    EXPLORER_DIALOG_OVERWRITE,
    EXPLORER_DIALOG_INFO,
    EXPLORER_DIALOG_PROGRESS,
};

struct ExploreDialog {
    EXPLORER_DIALOG_KIND kind         = EXPLORER_DIALOG_NONE;
    bool         visible      = false;
    std::wstring title;
    std::wstring summary;
    std::wstring detail;
    std::wstring input_label;
    std::wstring input_value;
    int          input_cursor = 0;
    int          progress_current = 0;
    int          progress_total   = 0;
};

enum EXPLORER_COPY_TASK_KIND {
    EXPLORER_COPY_TASK_MKDIR,
    EXPLORER_COPY_TASK_FILE,
    EXPLORER_COPY_TASK_RMDIR,
};

struct ExploreCopyTask {
    EXPLORER_COPY_TASK_KIND kind = EXPLORER_COPY_TASK_FILE;
    std::wstring       src;
    std::wstring       dst;
    std::wstring       source_key;
};

struct ExploreCopyState {
    bool                     active         = false;
    bool                     move_mode      = false;
    bool                     overwrite_all  = false;
    bool                     overwrite_once = false;
    bool                     source_left    = true;
    std::vector<std::wstring> sources;
    std::vector<ExploreCopyTask> tasks;
    std::map<std::wstring, size_t> pending;
    size_t                   index          = 0;
};

struct ExploreDeleteState {
    bool                     active        = false;
    bool                     recycle_mode  = true;
    bool                     source_left   = true;
    std::vector<std::wstring> sources;
    size_t                   index         = 0;
};

struct ExploreState {
    Panel               left;
    Panel               right;
    EXPLORER_FOCUS_MODE    focus         = EXPLORER_FOCUS_PANEL;
    bool                filter_replace = false;
    std::wstring        jump_buffer;
    ULONGLONG           jump_tick     = 0;
    ExploreDialog           dialog;
    ExploreCopyState        copy;
    ExploreDeleteState      del;
    HANDLE              explore_buf       = NULL;
    HANDLE              shell_buf     = NULL;
    CONSOLE_CURSOR_INFO shell_cursor  = {25, TRUE};
    DWORD               shell_out_mode = 0;
    int                 view_width    = 0;
    int                 view_height   = 0;
    bool                ready         = false;
};

// -- Explore color config -----------------------------------------------------
// Explore uses direct console attributes in its private screen buffer, so these are
// WORD attributes instead of ANSI escape strings.
#define EXPLORER_BORDER_ACTIVE   (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY)
#define EXPLORER_BORDER_INACTIVE (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)
#define EXPLORER_PATH            (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY)
#define EXPLORER_FILTER          (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define EXPLORER_FILTER_BG       (BACKGROUND_BLUE | BACKGROUND_INTENSITY)
#define EXPLORER_CURSOR_BG       (BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_INTENSITY)
#define EXPLORER_CURSOR_SELECTED (EXPLORER_CURSOR_BG | FOREGROUND_BLUE | FOREGROUND_INTENSITY)
#define EXPLORER_SELECTED        (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define EXPLORER_DOTDOT          (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)
#define EXPLORER_STATUSBAR       (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)
#define EXPLORER_STATUS_KEY      (BACKGROUND_BLUE | BACKGROUND_INTENSITY)
#define EXPLORER_STATUS_TEXT     (BACKGROUND_INTENSITY)
#define EXPLORER_BADGE           (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define EXPLORER_DIALOG_BORDER   EXPLORER_BORDER_ACTIVE
#define EXPLORER_DIALOG_FILL     EXPLORER_STATUSBAR
#define EXPLORER_DIALOG_TITLE    EXPLORER_PATH
#define EXPLORER_DIALOG_TEXT     EXPLORER_PATH
#define EXPLORER_DIALOG_LABEL    EXPLORER_BADGE
#define EXPLORER_DIALOG_INPUT    EXPLORER_PATH
#define EXPLORER_DIALOG_CURSOR   (BACKGROUND_BLUE | BACKGROUND_INTENSITY)
#define EXPLORER_COLOR_DIR       (FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY)
#define EXPLORER_COLOR_EXE       (FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define EXPLORER_COLOR_ARCHIVE   (FOREGROUND_RED | FOREGROUND_INTENSITY)
#define EXPLORER_COLOR_IMAGE     (FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY)
#define EXPLORER_COLOR_MEDIA     (FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY)
#define EXPLORER_COLOR_HIDDEN    (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)
#define EXPLORER_PROG_LOW        EXPLORER_COLOR_DIR
#define EXPLORER_PROG_MID        EXPLORER_BADGE
#define EXPLORER_PROG_HIGH       EXPLORER_COLOR_ARCHIVE
// -----------------------------------------------------------------------------

static ExploreState g_explore;

int edit_file(const std::string& path);
int view_file(const std::string& path);

static int explore_total_rows(const Panel& panel);
static int explore_visible_rows();
static void explore_clamp_panel(Panel& panel);
static bool explore_panel_has_filter_row(const Panel& panel);
static bool explore_any_filter_row();
static int explore_separator_y();
static int explore_entries_y();
static void explore_jump_clear();
static void explore_sync_panels_from_shell();
static void explore_focus_entry_name(Panel& panel, const std::wstring& name);
static Panel& explore_active_panel();
static Panel& explore_inactive_panel();
static std::vector<std::wstring> explore_copy_sources();
static bool explore_copy_build_tasks(const std::vector<std::wstring>& sources, const std::wstring& raw_dest,
    std::vector<ExploreCopyTask>& tasks, std::map<std::wstring, size_t>& pending, std::wstring& error);
static bool explore_move_build_tasks(const std::vector<std::wstring>& sources, const std::wstring& raw_dest,
    std::vector<ExploreCopyTask>& tasks, std::map<std::wstring, size_t>& pending, std::wstring& error);
static void explore_copy_run();
static void explore_delete_run();
static void explore_copy_refresh_panels();

static const ULONGLONG EXPLORER_JUMP_TIMEOUT_MS = 1000;

static bool explore_filter_focused() {
    return g_explore.focus == EXPLORER_FOCUS_FILTER;
}

static bool explore_dialog_focused() {
    return g_explore.focus == EXPLORER_FOCUS_DIALOG && g_explore.dialog.visible;
}

static bool explore_read_view_size(HANDLE buf, int& width, int& height) {
    CONSOLE_SCREEN_BUFFER_INFO csbi = {};
    if (!buf || buf == INVALID_HANDLE_VALUE) return false;
    if (!GetConsoleScreenBufferInfo(buf, &csbi)) return false;
    width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    return width > 0 && height > 0;
}

static void explore_cache_view_size() {
    int width = 0, height = 0;
    if (explore_read_view_size(g_explore.explore_buf, width, height)) {
        g_explore.view_width = width;
        g_explore.view_height = height;
    }
}

static bool explore_sync_view() {
    CONSOLE_SCREEN_BUFFER_INFO csbi = {};
    if (!g_explore.explore_buf || g_explore.explore_buf == INVALID_HANDLE_VALUE) return false;
    if (!GetConsoleScreenBufferInfo(g_explore.explore_buf, &csbi)) return false;

    int view_width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    int view_height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    if (view_width <= 0 || view_height <= 0) return false;

    if (view_width > csbi.dwSize.X || view_height > csbi.dwSize.Y) {
        COORD grown = {
            (SHORT)std::max<int>(csbi.dwSize.X, view_width),
            (SHORT)std::max<int>(csbi.dwSize.Y, view_height)
        };
        SetConsoleScreenBufferSize(g_explore.explore_buf, grown);
    }

    bool changed = view_width != g_explore.view_width || view_height != g_explore.view_height;
    g_explore.view_width = view_width;
    g_explore.view_height = view_height;
    return changed;
}

static std::wstring explore_current_dir() {
    wchar_t buf[MAX_PATH] = {};
    GetCurrentDirectoryW(MAX_PATH, buf);
    return buf;
}

static std::wstring explore_display_path(const std::wstring& path) {
    std::wstring out = path;
    std::replace(out.begin(), out.end(), L'\\', L'/');
    return out;
}

static std::wstring explore_lower(const std::wstring& s) {
    std::wstring out = s;
    std::transform(out.begin(), out.end(), out.begin(), ::towlower);
    return out;
}

static std::wstring explore_fit(const std::wstring& s, int width) {
    if (width <= 0) return L"";
    if ((int)s.size() <= width) return s;
    if (width <= 3) return std::wstring(width, L'.');
    return s.substr(0, width - 3) + L"...";
}

static std::wstring explore_trim(const std::wstring& s) {
    size_t start = 0;
    while (start < s.size() && iswspace(s[start]))
        start++;
    size_t end = s.size();
    while (end > start && iswspace(s[end - 1]))
        end--;
    return s.substr(start, end - start);
}

static std::wstring explore_glob_pattern(const std::wstring& dir) {
    if (dir.empty()) return L"*";
    wchar_t last = dir.back();
    if (last == L'\\' || last == L'/') return dir + L"*";
    return dir + L"\\*";
}

static std::wstring explore_join_path(const std::wstring& dir, const std::wstring& name) {
    if (dir.empty()) return name;
    wchar_t last = dir.back();
    if (last == L'\\' || last == L'/') return dir + name;
    return dir + L"\\" + name;
}

static bool explore_is_drive_root(const std::wstring& path) {
    if (path.size() != 3) return false;
    return iswalpha(path[0]) && path[1] == L':' && (path[2] == L'\\' || path[2] == L'/');
}

static std::wstring explore_parent_dir(const std::wstring& path) {
    if (path.empty()) return path;

    std::wstring norm = path;
    std::replace(norm.begin(), norm.end(), L'/', L'\\');
    while (norm.size() > 3 && !norm.empty() && norm.back() == L'\\')
        norm.pop_back();
    if (explore_is_drive_root(norm)) return norm;

    size_t slash = norm.find_last_of(L'\\');
    if (slash == std::wstring::npos) return norm;
    if (slash == 2 && iswalpha(norm[0]) && norm[1] == L':')
        return norm.substr(0, 3);
    if (slash == 0)
        return norm.substr(0, 1);
    return norm.substr(0, slash);
}

static std::wstring explore_leaf_name(const std::wstring& path) {
    if (path.empty()) return L"";
    std::wstring norm = path;
    std::replace(norm.begin(), norm.end(), L'/', L'\\');
    while (norm.size() > 3 && !norm.empty() && norm.back() == L'\\')
        norm.pop_back();
    size_t slash = norm.find_last_of(L'\\');
    if (slash == std::wstring::npos) return norm;
    return norm.substr(slash + 1);
}

static std::wstring explore_entry_ext(const std::wstring& name) {
    size_t dot = name.rfind(L'.');
    if (dot == std::wstring::npos || dot == 0 || dot + 1 >= name.size())
        return L"";
    return explore_lower(name.substr(dot + 1));
}

static std::wstring explore_native_path(const std::wstring& path) {
    std::wstring out = path;
    std::replace(out.begin(), out.end(), L'/', L'\\');
    return out;
}

static bool explore_is_absolute_path(const std::wstring& path) {
    if (path.size() > 1 && path[1] == L':')
        return true;
    if (path.size() > 1 && path[0] == L'\\' && path[1] == L'\\')
        return true;
    if (!path.empty() && (path[0] == L'\\' || path[0] == L'/'))
        return true;
    return false;
}

static bool explore_trailing_slash(const std::wstring& path) {
    return !path.empty() && (path.back() == L'\\' || path.back() == L'/');
}

static DWORD explore_path_attrs(const std::wstring& path) {
    return GetFileAttributesW(path.c_str());
}

static bool explore_path_exists(const std::wstring& path) {
    return explore_path_attrs(path) != INVALID_FILE_ATTRIBUTES;
}

static bool explore_path_is_dir(const std::wstring& path) {
    DWORD attrs = explore_path_attrs(path);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static bool explore_same_path(const std::wstring& a, const std::wstring& b) {
    wchar_t a_buf[MAX_PATH] = {};
    wchar_t b_buf[MAX_PATH] = {};
    DWORD a_len = GetFullPathNameW(a.c_str(), MAX_PATH, a_buf, NULL);
    DWORD b_len = GetFullPathNameW(b.c_str(), MAX_PATH, b_buf, NULL);
    if (a_len == 0 || b_len == 0 || a_len >= MAX_PATH || b_len >= MAX_PATH)
        return explore_lower(explore_native_path(a)) == explore_lower(explore_native_path(b));
    return explore_lower(a_buf) == explore_lower(b_buf);
}

static bool explore_path_starts_with(const std::wstring& child, const std::wstring& parent) {
    std::wstring c = explore_lower(explore_native_path(child));
    std::wstring p = explore_lower(explore_native_path(parent));
    if (!p.empty() && p.back() != L'\\') p += L'\\';
    return c.size() >= p.size() && c.substr(0, p.size()) == p;
}

static bool explore_ensure_dir(const std::wstring& path) {
    if (path.empty()) return false;
    std::wstring norm = explore_native_path(path);
    if (explore_path_is_dir(norm)) return true;

    std::wstring parent = explore_parent_dir(norm);
    if (!parent.empty() && parent != norm && !explore_path_is_dir(parent)) {
        if (!explore_ensure_dir(parent)) return false;
    }

    if (CreateDirectoryW(norm.c_str(), NULL))
        return true;
    return GetLastError() == ERROR_ALREADY_EXISTS && explore_path_is_dir(norm);
}

static std::wstring explore_last_error_text(DWORD code) {
    wchar_t* buf = NULL;
    DWORD len = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, code, 0, (LPWSTR)&buf, 0, NULL);
    std::wstring out = len ? std::wstring(buf, len) : L"Operation failed";
    if (buf) LocalFree(buf);
    while (!out.empty() && (out.back() == L'\r' || out.back() == L'\n' || out.back() == L' '))
        out.pop_back();
    return out;
}

static bool explore_replace_file(const std::wstring& src, const std::wstring& dst) {
    DWORD attrs = explore_path_attrs(dst);
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM))) {
        SetFileAttributesW(dst.c_str(), attrs & ~(FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM));
    }
    if (explore_path_exists(dst) && !DeleteFileW(dst.c_str()))
        return false;
    return CopyFileW(src.c_str(), dst.c_str(), TRUE) != 0;
}

static bool explore_move_file(const std::wstring& src, const std::wstring& dst, bool overwrite) {
    DWORD flags = MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH;
    if (overwrite)
        flags |= MOVEFILE_REPLACE_EXISTING;

    if (MoveFileExW(src.c_str(), dst.c_str(), flags))
        return true;

    if (!overwrite)
        return false;

    DWORD attrs = explore_path_attrs(dst);
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)))
        SetFileAttributesW(dst.c_str(), attrs & ~(FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM));
    if (explore_path_exists(dst) && !DeleteFileW(dst.c_str()))
        return false;
    return MoveFileExW(src.c_str(), dst.c_str(), MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH) != 0;
}

static bool explore_has_glob(const std::wstring& pattern) {
    return pattern.find(L'*') != std::wstring::npos || pattern.find(L'?') != std::wstring::npos;
}

static bool explore_glob_match(const std::wstring& text, const std::wstring& pattern) {
    size_t ti = 0, pi = 0;
    size_t star = std::wstring::npos;
    size_t match = 0;

    while (ti < text.size()) {
        if (pi < pattern.size() && (pattern[pi] == L'?' || pattern[pi] == text[ti])) {
            ti++;
            pi++;
            continue;
        }
        if (pi < pattern.size() && pattern[pi] == L'*') {
            star = pi++;
            match = ti;
            continue;
        }
        if (star != std::wstring::npos) {
            pi = star + 1;
            ti = ++match;
            continue;
        }
        return false;
    }

    while (pi < pattern.size() && pattern[pi] == L'*')
        pi++;
    return pi == pattern.size();
}

static std::wstring explore_entry_key(const Panel& panel, const Entry& entry) {
    return explore_join_path(panel.cwd, entry.name);
}

static std::wstring explore_sort_mode_label(int sort_mode) {
    switch (sort_mode) {
    case EXPLORER_SORT_EXT:  return L"Ext";
    case EXPLORER_SORT_SIZE: return L"Size";
    case EXPLORER_SORT_TIME: return L"Time";
    default:            return L"Name";
    }
}

static bool explore_entry_less(const Entry& a, const Entry& b, int sort_mode) {
    std::wstring la = explore_lower(a.name);
    std::wstring lb = explore_lower(b.name);
    if (sort_mode == EXPLORER_SORT_EXT) {
        std::wstring ea = explore_entry_ext(a.name);
        std::wstring eb = explore_entry_ext(b.name);
        if (ea != eb) return ea < eb;
        if (la != lb) return la < lb;
        return a.name < b.name;
    }
    if (sort_mode == EXPLORER_SORT_SIZE) {
        if (a.size != b.size) return a.size > b.size;
        if (la != lb) return la < lb;
        return a.name < b.name;
    }
    if (sort_mode == EXPLORER_SORT_TIME) {
        LONG hi = CompareFileTime(&a.modified, &b.modified);
        if (hi != 0) return hi > 0;
        if (la != lb) return la < lb;
        return a.name < b.name;
    }
    if (la != lb) return la < lb;
    return a.name < b.name;
}

static void explore_apply_filter(Panel& panel) {
    panel.entries.clear();
    if (panel.filter.empty()) {
        panel.entries = panel.all_entries;
    } else {
        std::wstring needle = explore_lower(panel.filter);
        for (const Entry& entry : panel.all_entries) {
            std::wstring name = explore_lower(entry.name);
            bool match = explore_has_glob(needle)
                ? explore_glob_match(name, needle)
                : name.find(needle) != std::wstring::npos;
            if (match)
                panel.entries.push_back(entry);
        }
    }
}

static void explore_load_entries(Panel& panel, bool reset_cursor, bool clear_selection = true) {
    panel.all_entries.clear();
    panel.entries.clear();
    if (clear_selection)
        panel.selected.clear();

    WIN32_FIND_DATAW fd = {};
    HANDLE h = FindFirstFileW(explore_glob_pattern(panel.cwd).c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        std::vector<Entry> dirs;
        std::vector<Entry> files;
        do {
            std::wstring name = fd.cFileName;
            if (name == L"." || name == L"..") continue;

            bool is_dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            bool hidden = (fd.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) != 0 ||
                          (!name.empty() && name[0] == L'.');
            uint64_t size = is_dir ? 0 : (((uint64_t)fd.nFileSizeHigh << 32) | fd.nFileSizeLow);
            Entry entry = {name, is_dir, hidden, size, fd.ftLastWriteTime};
            (is_dir ? dirs : files).push_back(entry);
        } while (FindNextFileW(h, &fd));
        FindClose(h);

        auto cmp = [&](const Entry& a, const Entry& b) {
            return explore_entry_less(a, b, panel.sort_mode);
        };
        std::sort(dirs.begin(), dirs.end(), cmp);
        std::sort(files.begin(), files.end(), cmp);

        panel.all_entries.reserve(dirs.size() + files.size());
        panel.all_entries.insert(panel.all_entries.end(), dirs.begin(), dirs.end());
        panel.all_entries.insert(panel.all_entries.end(), files.begin(), files.end());
    }

    if (!clear_selection) {
        std::set<std::wstring> valid;
        for (const Entry& entry : panel.all_entries)
            valid.insert(explore_entry_key(panel, entry));

        for (auto it = panel.selected.begin(); it != panel.selected.end();) {
            if (!valid.count(*it)) it = panel.selected.erase(it);
            else ++it;
        }
    }

    explore_apply_filter(panel);
    if (reset_cursor) {
        panel.cursor = 0;
        panel.scroll = 0;
    }
    explore_clamp_panel(panel);
}

static WORD explore_entry_color(const Entry& e) {
    if (e.is_hidden) return EXPLORER_COLOR_HIDDEN;
    if (e.is_dir) return EXPLORER_COLOR_DIR;

    size_t dot = e.name.rfind(L'.');
    if (dot == std::wstring::npos) return EXPLORER_PATH;

    std::wstring ext = explore_lower(e.name.substr(dot));
    if (ext == L".exe" || ext == L".bat" || ext == L".cmd" || ext == L".ps1" || ext == L".msi")
        return EXPLORER_COLOR_EXE;
    if (ext == L".zip" || ext == L".tar" || ext == L".gz" || ext == L".tgz" ||
        ext == L".bz2" || ext == L".xz" || ext == L".7z" || ext == L".rar" ||
        ext == L".cab" || ext == L".iso")
        return EXPLORER_COLOR_ARCHIVE;
    if (ext == L".jpg" || ext == L".jpeg" || ext == L".png" || ext == L".gif" ||
        ext == L".bmp" || ext == L".svg" || ext == L".webp" || ext == L".ico")
        return EXPLORER_COLOR_IMAGE;
    if (ext == L".mp3" || ext == L".wav" || ext == L".ogg" || ext == L".flac" ||
        ext == L".aac" || ext == L".m4a" || ext == L".mp4" || ext == L".mkv" ||
        ext == L".avi" || ext == L".mov" || ext == L".webm")
        return EXPLORER_COLOR_MEDIA;
    return EXPLORER_PATH;
}

static int explore_total_rows(const Panel& panel) {
    return 1 + (int)panel.entries.size(); // ".." + entries
}

static bool explore_panel_has_filter_row(const Panel& panel) {
    return !panel.filter.empty() || (panel.active && explore_filter_focused());
}

static bool explore_any_filter_row() {
    return explore_panel_has_filter_row(g_explore.left) || explore_panel_has_filter_row(g_explore.right);
}

static int explore_separator_y() {
    return explore_any_filter_row() ? 3 : 2;
}

static int explore_entries_y() {
    return explore_separator_y() + 1;
}

static int explore_visible_rows() {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(g_explore.explore_buf, &csbi)) return 0;
    int height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    return std::max(0, height - (explore_entries_y() + 2));
}

static void explore_clamp_panel(Panel& panel) {
    int total = std::max(1, explore_total_rows(panel));
    if (panel.cursor < 0) panel.cursor = 0;
    if (panel.cursor >= total) panel.cursor = total - 1;

    int visible = std::max(1, explore_visible_rows());
    if (panel.scroll > panel.cursor) panel.scroll = panel.cursor;
    if (panel.cursor >= panel.scroll + visible) panel.scroll = panel.cursor - visible + 1;
    int max_scroll = std::max(0, total - visible);
    if (panel.scroll < 0) panel.scroll = 0;
    if (panel.scroll > max_scroll) panel.scroll = max_scroll;
}

static void explore_init_state() {
    if (g_explore.ready) return;

    std::wstring start = explore_current_dir();
    g_explore.left.cwd = start;
    g_explore.right.cwd = start;
    g_explore.left.active = true;
    g_explore.right.active = false;
    explore_load_entries(g_explore.left, true);
    explore_load_entries(g_explore.right, true);
    g_explore.ready = true;
}

static void explore_close_buffer() {
    if (g_explore.explore_buf && g_explore.explore_buf != INVALID_HANDLE_VALUE) {
        CloseHandle(g_explore.explore_buf);
    }
    g_explore.explore_buf = NULL;
    g_explore.view_width = 0;
    g_explore.view_height = 0;
}

static bool explore_resize_buffer(int width, int height) {
    if (width < 20 || height < 8) return false;

    explore_close_buffer();

    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    g_explore.explore_buf = CreateConsoleScreenBuffer(
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        &sa,
        CONSOLE_TEXTMODE_BUFFER,
        NULL
    );
    if (!g_explore.explore_buf || g_explore.explore_buf == INVALID_HANDLE_VALUE) return false;

    SetConsoleMode(g_explore.explore_buf, g_explore.shell_out_mode);

    COORD size = {(SHORT)width, (SHORT)height};
    SetConsoleScreenBufferSize(g_explore.explore_buf, size);

    SMALL_RECT rect = {0, 0, (SHORT)(width - 1), (SHORT)(height - 1)};
    SetConsoleWindowInfo(g_explore.explore_buf, TRUE, &rect);

    CONSOLE_CURSOR_INFO hidden = {1, FALSE};
    SetConsoleCursorInfo(g_explore.explore_buf, &hidden);
    g_explore.view_width = width;
    g_explore.view_height = height;
    return true;
}

static bool explore_ensure_buffer() {
    if (!g_explore.shell_buf) g_explore.shell_buf = out_h;
    if (g_explore.shell_buf == NULL || g_explore.shell_buf == INVALID_HANDLE_VALUE) return false;
    GetConsoleMode(g_explore.shell_buf, &g_explore.shell_out_mode);

    int width = 0, height = 0;
    if (!explore_read_view_size(g_explore.shell_buf, width, height)) return false;
    if (!explore_resize_buffer(width, height)) return false;
    return true;
}

static void explore_put(std::vector<wchar_t>& chars, std::vector<WORD>& attrs,
    int width, int height, int x, int y, wchar_t ch, WORD attr) {
    if (x < 0 || y < 0 || x >= width || y >= height) return;
    size_t idx = (size_t)y * width + x;
    chars[idx] = ch;
    attrs[idx] = attr;
}

static void explore_fill(std::vector<wchar_t>& chars, std::vector<WORD>& attrs,
    int width, int height, int x, int y, int len, wchar_t ch, WORD attr) {
    for (int i = 0; i < len; i++)
        explore_put(chars, attrs, width, height, x + i, y, ch, attr);
}

static void explore_text(std::vector<wchar_t>& chars, std::vector<WORD>& attrs,
    int width, int height, int x, int y, const std::wstring& text, WORD attr) {
    for (int i = 0; i < (int)text.size(); i++)
        explore_put(chars, attrs, width, height, x + i, y, text[i], attr);
}

static std::wstring explore_status_pad(const std::wstring& label) {
    const int width = 4;
    std::wstring padded = L" " + label + L" ";
    if ((int)padded.size() >= width) return padded;
    return padded + std::wstring(width - (int)padded.size(), L' ');
}

static int explore_status_item(std::vector<wchar_t>& chars, std::vector<WORD>& attrs,
    int width, int height, int x, int y, const std::wstring& key, const std::wstring& label) {
    if (x >= width) return x;

    explore_text(chars, attrs, width, height, x, y, key, EXPLORER_STATUS_KEY);
    x += (int)key.size();
    std::wstring padded = explore_status_pad(label);
    explore_text(chars, attrs, width, height, x, y, padded, EXPLORER_STATUS_TEXT);
    x += (int)padded.size();
    if (x < width) {
        explore_put(chars, attrs, width, height, x, y, L' ', EXPLORER_STATUSBAR);
        x++;
    }
    return x;
}

static void explore_box(std::vector<wchar_t>& chars, std::vector<WORD>& attrs,
    int width, int height, int left, int top, int box_w, int box_h, WORD border_attr, WORD fill_attr) {
    if (box_w <= 1 || box_h <= 1) return;
    for (int y = top; y < top + box_h; y++)
        explore_fill(chars, attrs, width, height, left, y, box_w, L' ', fill_attr);

    explore_put(chars, attrs, width, height, left, top, L'\x250C', border_attr);
    explore_put(chars, attrs, width, height, left + box_w - 1, top, L'\x2510', border_attr);
    explore_put(chars, attrs, width, height, left, top + box_h - 1, L'\x2514', border_attr);
    explore_put(chars, attrs, width, height, left + box_w - 1, top + box_h - 1, L'\x2518', border_attr);
    if (box_w > 2) {
        explore_fill(chars, attrs, width, height, left + 1, top, box_w - 2, L'\x2500', border_attr);
        explore_fill(chars, attrs, width, height, left + 1, top + box_h - 1, box_w - 2, L'\x2500', border_attr);
    }
    for (int y = top + 1; y < top + box_h - 1; y++) {
        explore_put(chars, attrs, width, height, left, y, L'\x2502', border_attr);
        explore_put(chars, attrs, width, height, left + box_w - 1, y, L'\x2502', border_attr);
    }
}

static std::wstring explore_dialog_tail(const std::wstring& value, int width, int cursor, int& start_out) {
    start_out = 0;
    if (width <= 0) return L"";
    if ((int)value.size() <= width) return value;
    start_out = std::max(0, cursor - width + 1);
    if (start_out + width > (int)value.size())
        start_out = (int)value.size() - width;
    return value.substr(start_out, width);
}

static void explore_draw_dialog(std::vector<wchar_t>& chars, std::vector<WORD>& attrs, int width, int height) {
    if (!g_explore.dialog.visible) return;

    int dialog_w = std::min(width, std::max(36, std::min(width - 4, 64)));
    bool input_dialog = g_explore.dialog.kind == EXPLORER_DIALOG_COPY ||
        g_explore.dialog.kind == EXPLORER_DIALOG_MOVE ||
        g_explore.dialog.kind == EXPLORER_DIALOG_MKDIR;
    int dialog_h = std::min(height, input_dialog ? 8 : 7);
    if (dialog_w <= 2 || dialog_h <= 2) return;

    int left = std::max(0, (width - dialog_w) / 2);
    int top  = std::max(0, (height - dialog_h) / 2);
    explore_box(chars, attrs, width, height, left, top, dialog_w, dialog_h, EXPLORER_DIALOG_BORDER, EXPLORER_DIALOG_FILL);

    explore_text(chars, attrs, width, height, left + 2, top, explore_fit(g_explore.dialog.title, dialog_w - 4), EXPLORER_DIALOG_TITLE);
    explore_text(chars, attrs, width, height, left + 2, top + 2,
        explore_fit(g_explore.dialog.summary, dialog_w - 4), EXPLORER_DIALOG_TEXT);
    if (!g_explore.dialog.detail.empty()) {
        explore_text(chars, attrs, width, height, left + 2, top + 3,
            explore_fit(g_explore.dialog.detail, dialog_w - 4), EXPLORER_DIALOG_TEXT);
    }

    int footer_x = left + 2;
    int footer_y = top + dialog_h - 2;
    if (input_dialog) {
        explore_text(chars, attrs, width, height, left + 2, top + 3,
            explore_fit(g_explore.dialog.input_label, dialog_w - 4), EXPLORER_DIALOG_LABEL);

        int input_x = left + 2;
        int input_y = top + 4;
        int input_w = std::max(1, dialog_w - 4);
        explore_fill(chars, attrs, width, height, input_x, input_y, input_w, L' ', EXPLORER_DIALOG_INPUT);

        int start = 0;
        std::wstring visible = explore_dialog_tail(g_explore.dialog.input_value, input_w, g_explore.dialog.input_cursor, start);
        explore_text(chars, attrs, width, height, input_x, input_y, visible, EXPLORER_DIALOG_INPUT);

        int cursor_x = input_x + std::max(0, g_explore.dialog.input_cursor - start);
        if (cursor_x >= input_x + input_w) cursor_x = input_x + input_w - 1;
        wchar_t cursor_ch = L' ';
        if (g_explore.dialog.input_cursor >= start &&
            g_explore.dialog.input_cursor < start + (int)visible.size()) {
            cursor_ch = visible[g_explore.dialog.input_cursor - start];
        }
        explore_put(chars, attrs, width, height, cursor_x, input_y, cursor_ch, EXPLORER_DIALOG_CURSOR);

        footer_x = explore_status_item(chars, attrs, width, height, footer_x, footer_y, L"ENTER", L"Ok");
        explore_status_item(chars, attrs, width, height, footer_x, footer_y, L"ESC", L"Cancel");
    } else if (g_explore.dialog.kind == EXPLORER_DIALOG_OVERWRITE) {
        footer_x = explore_status_item(chars, attrs, width, height, footer_x, footer_y, L"ENTER", L"Yes");
        footer_x = explore_status_item(chars, attrs, width, height, footer_x, footer_y, L"CTRL+ENTER", L"All");
        explore_status_item(chars, attrs, width, height, footer_x, footer_y, L"ESC", L"Cancel");
    } else if (g_explore.dialog.kind == EXPLORER_DIALOG_RECYCLE || g_explore.dialog.kind == EXPLORER_DIALOG_DELETE) {
        footer_x = explore_status_item(chars, attrs, width, height, footer_x, footer_y, L"ENTER",
            g_explore.dialog.kind == EXPLORER_DIALOG_RECYCLE ? L"Recycle" : L"Delete");
        explore_status_item(chars, attrs, width, height, footer_x, footer_y, L"ESC", L"Cancel");
    } else if (g_explore.dialog.kind == EXPLORER_DIALOG_PROGRESS) {
        int bar_x = left + 2;
        int bar_y = top + 4;
        int bar_w = std::max(8, dialog_w - 4);
        int total = std::max(1, g_explore.dialog.progress_total);
        int current = std::max(0, std::min(g_explore.dialog.progress_current, total));
        int filled = (bar_w * current) / total;
        explore_fill(chars, attrs, width, height, bar_x, bar_y, bar_w, L' ', EXPLORER_DIALOG_INPUT);
        if (filled > 0)
            explore_fill(chars, attrs, width, height, bar_x, bar_y, filled, L' ', EXPLORER_DIALOG_CURSOR);

        std::wstring percent = std::to_wstring((current * 100) / total) + L"%";
        int percent_x = bar_x + std::max(0, (bar_w - (int)percent.size()) / 2);
        explore_text(chars, attrs, width, height, percent_x, bar_y, percent, EXPLORER_DIALOG_TEXT);
        explore_text(chars, attrs, width, height, left + 2, footer_y,
            explore_fit(L"Working...", dialog_w - 4), EXPLORER_DIALOG_TEXT);
    } else {
        footer_x = explore_status_item(chars, attrs, width, height, footer_x, footer_y, L"ENTER", L"Close");
        explore_status_item(chars, attrs, width, height, footer_x, footer_y, L"ESC", L"Close");
    }
}

static void explore_draw_panel(std::vector<wchar_t>& chars, std::vector<WORD>& attrs,
    int width, int height, const Panel& panel, int left, int right) {
    int inner_x = left + 1;
    int inner_w = std::max(0, right - left - 1);
    if (inner_w <= 0) return;

    std::wstring path = explore_fit(explore_display_path(panel.cwd), inner_w);
    WORD head_attr = panel.active ? EXPLORER_PATH : EXPLORER_BORDER_INACTIVE;
    explore_text(chars, attrs, width, height, inner_x, 1, path, head_attr);
    std::wstring sort_badge = L"[" + explore_sort_mode_label(panel.sort_mode) + L"]";
    sort_badge = explore_fit(sort_badge, inner_w);
    int sort_x = inner_x + std::max(0, inner_w - (int)sort_badge.size());
    explore_text(chars, attrs, width, height, sort_x, 1, sort_badge, panel.active ? EXPLORER_BADGE : EXPLORER_BORDER_INACTIVE);

    if (explore_any_filter_row()) {
        int filter_y = explore_separator_y() - 1;
        WORD filter_attr = panel.active ? EXPLORER_PATH : EXPLORER_STATUSBAR;
        bool filter_focus = panel.active && explore_filter_focused();
        if (filter_focus) {
            int start = 0;
            std::wstring visible = explore_dialog_tail(panel.filter, inner_w, panel.filter_cursor, start);
            explore_text(chars, attrs, width, height, inner_x, filter_y, visible, filter_attr);

            int cursor_x = inner_x + std::max(0, panel.filter_cursor - start);
            if (cursor_x >= inner_x + inner_w) cursor_x = inner_x + inner_w - 1;
            wchar_t cursor_ch = L' ';
            if (panel.filter_cursor >= start &&
                panel.filter_cursor < start + (int)visible.size()) {
                cursor_ch = visible[panel.filter_cursor - start];
            }
            explore_put(chars, attrs, width, height, cursor_x, filter_y, cursor_ch, EXPLORER_DIALOG_CURSOR);
        } else {
            std::wstring filter = panel.filter.empty() ? L"*" : panel.filter;
            explore_text(chars, attrs, width, height, inner_x, filter_y, explore_fit(filter, inner_w), filter_attr);
        }
    }

    int visible = std::max(0, explore_visible_rows());
    for (int row = 0; row < visible; row++) {
        int y = explore_entries_y() + row;
        int idx = panel.scroll + row;
        bool is_cursor = panel.active && idx == panel.cursor;
        if (is_cursor)
            explore_fill(chars, attrs, width, height, inner_x, y, inner_w, L' ', EXPLORER_CURSOR_BG);

        if (idx >= explore_total_rows(panel)) continue;

        std::wstring prefix = L"  ";
        std::wstring name;
        WORD name_attr = EXPLORER_DOTDOT;

        if (idx == 0) {
            name = L"..";
        } else {
            const Entry& e = panel.entries[idx - 1];
            name = e.name + (e.is_dir ? L"/" : L"");
            name_attr = panel.selected.count(explore_entry_key(panel, e)) ? EXPLORER_SELECTED : explore_entry_color(e);
        }

        int text_w = std::max(0, inner_w - 1);
        int prefix_w = std::min(text_w, (int)prefix.size());
        int name_w = std::max(0, text_w - prefix_w);
        std::wstring visible_name = name_w > 0 ? name.substr(0, name_w) : L"";
        if (name_w > 0 && (int)name.size() > name_w)
            visible_name[name_w - 1] = L'…';
        for (int i = 0; i < prefix_w; i++) {
            WORD attr = EXPLORER_PATH;
            if (is_cursor) {
                bool selected_entry = idx > 0 && panel.selected.count(explore_entry_key(panel, panel.entries[idx - 1]));
                attr = selected_entry ? EXPLORER_CURSOR_SELECTED : EXPLORER_CURSOR_BG;
            }
            explore_put(chars, attrs, width, height, inner_x + i, y, prefix[i], attr);
        }
        for (int i = 0; i < (int)visible_name.size(); i++) {
            WORD attr = name_attr;
            if (is_cursor) {
                bool selected_entry = idx > 0 && panel.selected.count(explore_entry_key(panel, panel.entries[idx - 1]));
                attr = selected_entry ? EXPLORER_CURSOR_SELECTED : EXPLORER_CURSOR_BG;
            }
            explore_put(chars, attrs, width, height, inner_x + prefix_w + i, y, visible_name[i], attr);
        }
    }

    if (!panel.selected.empty()) {
        std::wstring badge = L"[" + std::to_wstring(panel.selected.size()) + L" selected]";
        badge = explore_fit(badge, inner_w);
        int badge_x = inner_x + std::max(0, inner_w - (int)badge.size());
        int badge_y = height - 3;
        int badge_idx = panel.scroll + (badge_y - explore_entries_y());
        bool badge_on_cursor = panel.active && badge_idx == panel.cursor;
        WORD badge_attr = badge_on_cursor ? EXPLORER_CURSOR_BG : EXPLORER_BADGE;
        explore_text(chars, attrs, width, height, badge_x, badge_y, badge, badge_attr);
    }
}

static void explore_present(const std::vector<wchar_t>& chars, const std::vector<WORD>& attrs, int width, int height) {
    if (width <= 0 || height <= 0) return;

    std::vector<CHAR_INFO> buffer((size_t)width * height);
    for (size_t i = 0; i < buffer.size(); i++) {
        buffer[i].Char.UnicodeChar = chars[i];
        buffer[i].Attributes = attrs[i];
    }

    COORD buf_size = {(SHORT)width, (SHORT)height};
    COORD buf_pos = {0, 0};
    SMALL_RECT rect = {0, 0, (SHORT)(width - 1), (SHORT)(height - 1)};
    WriteConsoleOutputW(g_explore.explore_buf, buffer.data(), buf_size, buf_pos, &rect);
}

static void explore_draw() {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(g_explore.explore_buf, &csbi)) return;

    int width  = csbi.srWindow.Right  - csbi.srWindow.Left + 1;
    int height = csbi.srWindow.Bottom - csbi.srWindow.Top  + 1;
    if (width <= 0 || height <= 0) return;

    std::vector<wchar_t> chars((size_t)width * height, L' ');
    std::vector<WORD> attrs((size_t)width * height, 0);

    if (width < 20 || height < 8) {
        std::wstring msg = L"Explore needs a larger console";
        int x = std::max(0, (width - (int)msg.size()) / 2);
        int y = std::max(0, height / 2);
        explore_text(chars, attrs, width, height, x, y, explore_fit(msg, width), EXPLORER_PATH);
        explore_present(chars, attrs, width, height);
        return;
    }

    int split = (width - 1) / 2;
    int bottom = height - 2;
    int sep_y = explore_separator_y();
    WORD left_border  = g_explore.left.active  ? EXPLORER_BORDER_ACTIVE : EXPLORER_BORDER_INACTIVE;
    WORD right_border = g_explore.right.active ? EXPLORER_BORDER_ACTIVE : EXPLORER_BORDER_INACTIVE;
    WORD mid_border   = EXPLORER_BORDER_INACTIVE;

    explore_put(chars, attrs, width, height, 0, 0, L'┌', left_border);
    explore_fill(chars, attrs, width, height, 1, 0, std::max(0, split - 1), L'─', left_border);
    explore_put(chars, attrs, width, height, split, 0, L'┬', mid_border);
    explore_fill(chars, attrs, width, height, split + 1, 0, std::max(0, width - split - 2), L'─', right_border);
    explore_put(chars, attrs, width, height, width - 1, 0, L'┐', right_border);

    for (int y = 1; y < bottom; y++) {
        explore_put(chars, attrs, width, height, 0, y, L'│', left_border);
        explore_put(chars, attrs, width, height, split, y, L'│', mid_border);
        explore_put(chars, attrs, width, height, width - 1, y, L'│', right_border);
    }

    explore_put(chars, attrs, width, height, 0, sep_y, L'├', left_border);
    explore_fill(chars, attrs, width, height, 1, sep_y, std::max(0, split - 1), L'─', left_border);
    explore_put(chars, attrs, width, height, split, sep_y, L'┼', mid_border);
    explore_fill(chars, attrs, width, height, split + 1, sep_y, std::max(0, width - split - 2), L'─', right_border);
    explore_put(chars, attrs, width, height, width - 1, sep_y, L'┤', right_border);

    explore_put(chars, attrs, width, height, 0, bottom, L'└', left_border);
    explore_fill(chars, attrs, width, height, 1, bottom, std::max(0, split - 1), L'─', left_border);
    explore_put(chars, attrs, width, height, split, bottom, L'┴', mid_border);
    explore_fill(chars, attrs, width, height, split + 1, bottom, std::max(0, width - split - 2), L'─', right_border);
    explore_put(chars, attrs, width, height, width - 1, bottom, L'┘', right_border);

    explore_clamp_panel(g_explore.left);
    explore_clamp_panel(g_explore.right);
    explore_draw_panel(chars, attrs, width, height, g_explore.left, 0, split);
    explore_draw_panel(chars, attrs, width, height, g_explore.right, split, width - 1);

    explore_fill(chars, attrs, width, height, 0, height - 1, width, L' ', EXPLORER_STATUSBAR);
    int sx = 1;
    sx = explore_status_item(chars, attrs, width, height, sx, height - 1, L"F1", L"Mirror");
    sx = explore_status_item(chars, attrs, width, height, sx, height - 1, L"F2", L"Sort");
    sx = explore_status_item(chars, attrs, width, height, sx, height - 1, L"F3", L"View");
    sx = explore_status_item(chars, attrs, width, height, sx, height - 1, L"F4", L"Edit");
    sx = explore_status_item(chars, attrs, width, height, sx, height - 1, L"F5", L"Copy");
    sx = explore_status_item(chars, attrs, width, height, sx, height - 1, L"F6", L"Move");
    sx = explore_status_item(chars, attrs, width, height, sx, height - 1, L"F7", L"MkDir");
    sx = explore_status_item(chars, attrs, width, height, sx, height - 1, L"F8", L"Del");
    sx = explore_status_item(chars, attrs, width, height, sx, height - 1, L"S+F8", L"Del");
    explore_status_item(chars, attrs, width, height, sx, height - 1, L"CTRL+O", L"Hide");

    explore_draw_dialog(chars, attrs, width, height);
    explore_present(chars, attrs, width, height);
}

static Panel& explore_active_panel() {
    return g_explore.left.active ? g_explore.left : g_explore.right;
}

static Panel& explore_inactive_panel() {
    return g_explore.left.active ? g_explore.right : g_explore.left;
}

static Panel& explore_copy_source_panel() {
    return g_explore.copy.source_left ? g_explore.left : g_explore.right;
}

static std::wstring explore_copy_summary() {
    Panel& panel = explore_active_panel();
    if (!panel.selected.empty())
        return L"From: " + std::to_wstring(panel.selected.size()) + L" selected";
    if (panel.cursor <= 0 || panel.cursor - 1 >= (int)panel.entries.size())
        return L"From: Nothing selected";

    const Entry& entry = panel.entries[panel.cursor - 1];
    return L"From: " + entry.name + (entry.is_dir ? L"/" : L"");
}

static void explore_dialog_open(const std::wstring& title, const std::wstring& summary,
    const std::wstring& detail, const std::wstring& input_label,
    const std::wstring& input_value, EXPLORER_DIALOG_KIND kind) {
    explore_jump_clear();
    g_explore.focus = EXPLORER_FOCUS_DIALOG;
    g_explore.filter_replace = false;
    g_explore.dialog.kind = kind;
    g_explore.dialog.visible = true;
    g_explore.dialog.title = title;
    g_explore.dialog.summary = summary;
    g_explore.dialog.detail = detail;
    g_explore.dialog.input_label = input_label;
    g_explore.dialog.input_value = input_value;
    g_explore.dialog.input_cursor = (int)g_explore.dialog.input_value.size();
}

static void explore_copy_dialog_open() {
    std::wstring target = explore_display_path(explore_inactive_panel().cwd);
    explore_dialog_open(L"Copy", explore_copy_summary(), L"", L"To:", target, EXPLORER_DIALOG_COPY);
}

static void explore_mkdir_dialog_open() {
    explore_dialog_open(L"MkDir",
        L"In: " + explore_display_path(explore_active_panel().cwd),
        L"",
        L"Name:",
        L"",
        EXPLORER_DIALOG_MKDIR);
}

static void explore_move_dialog_open() {
    std::wstring target = explore_display_path(explore_inactive_panel().cwd);
    explore_dialog_open(L"Move", explore_copy_summary(), L"", L"To:", target, EXPLORER_DIALOG_MOVE);
}

static void explore_open_other_same_dir() {
    explore_jump_clear();
    Panel& active = explore_active_panel();
    Panel& inactive = explore_inactive_panel();
    inactive.cwd = active.cwd;
    inactive.filter.clear();
    inactive.filter_cursor = 0;
    explore_load_entries(inactive, true);
}

static void explore_cycle_sort_mode() {
    explore_jump_clear();
    Panel& panel = explore_active_panel();
    std::wstring focus_name;
    bool focus_entry = panel.cursor > 0 && panel.cursor - 1 < (int)panel.entries.size();
    if (focus_entry)
        focus_name = panel.entries[panel.cursor - 1].name;

    panel.sort_mode = (panel.sort_mode + 1) % EXPLORER_SORT_COUNT;
    explore_load_entries(panel, false, false);
    if (focus_entry)
        explore_focus_entry_name(panel, focus_name);
    else
        explore_clamp_panel(panel);
}

static bool explore_open_current_file(bool readonly) {
    Panel& panel = explore_active_panel();
    if (panel.cursor <= 0 || panel.cursor - 1 >= (int)panel.entries.size())
        return false;

    const Entry& entry = panel.entries[panel.cursor - 1];
    if (entry.is_dir)
        return false;

    std::wstring path = explore_join_path(panel.cwd, entry.name);
    SetCurrentDirectoryW(panel.cwd.c_str());
    SetConsoleActiveScreenBuffer(g_explore.shell_buf);
    if (g_explore.shell_buf) {
        DWORD cur_mode = 0;
        if (!GetConsoleMode(g_explore.shell_buf, &cur_mode) || cur_mode != g_explore.shell_out_mode)
            SetConsoleMode(g_explore.shell_buf, g_explore.shell_out_mode);
        SetConsoleCursorInfo(g_explore.shell_buf, &g_explore.shell_cursor);
    }

    if (readonly)
        view_file(to_utf8(path));
    else
        edit_file(to_utf8(path));

    explore_sync_panels_from_shell();
    if (explore_ensure_buffer()) {
        SetConsoleActiveScreenBuffer(g_explore.explore_buf);
        explore_cache_view_size();
    }
    return true;
}

static std::wstring explore_delete_summary(const std::vector<std::wstring>& sources, bool recycle) {
    if (sources.empty())
        return L"Nothing selected";
    if (sources.size() == 1)
        return explore_display_path(sources[0]);
    return std::to_wstring(sources.size()) + L" selected items";
}

static std::wstring explore_delete_detail(const std::vector<std::wstring>& sources, bool recycle) {
    if (sources.empty())
        return L"";
    if (sources.size() == 1)
        return recycle ? L"Move this item to recycle bin?" : L"Permanently delete this item?";
    return recycle ? L"Move selected items to recycle bin?" : L"Permanently delete selected items?";
}

static void explore_delete_dialog_open(bool recycle) {
    std::vector<std::wstring> sources = explore_copy_sources();
    explore_dialog_open(recycle ? L"Recycle" : L"Delete",
        explore_delete_summary(sources, recycle),
        explore_delete_detail(sources, recycle),
        L"", L"", recycle ? EXPLORER_DIALOG_RECYCLE : EXPLORER_DIALOG_DELETE);
}

static void explore_info_dialog_open(const std::wstring& title, const std::wstring& summary, const std::wstring& detail) {
    explore_dialog_open(title, summary, detail, L"", L"", EXPLORER_DIALOG_INFO);
}

static void explore_overwrite_dialog_open(const std::wstring& dst) {
    std::wstring title = g_explore.copy.move_mode ? L"Move" : L"Copy";
    explore_dialog_open(title + L" Overwrite", L"Target file already exists", explore_display_path(dst), L"", L"", EXPLORER_DIALOG_OVERWRITE);
}

static void explore_progress_dialog_open(const std::wstring& title, const std::wstring& summary, const std::wstring& detail, int current, int total) {
    g_explore.focus = EXPLORER_FOCUS_DIALOG;
    g_explore.filter_replace = false;
    g_explore.dialog.kind = EXPLORER_DIALOG_PROGRESS;
    g_explore.dialog.visible = true;
    g_explore.dialog.title = title;
    g_explore.dialog.summary = summary;
    g_explore.dialog.detail = detail;
    g_explore.dialog.input_label.clear();
    g_explore.dialog.input_value.clear();
    g_explore.dialog.input_cursor = 0;
    g_explore.dialog.progress_current = current;
    g_explore.dialog.progress_total = total;
}

static void explore_dialog_close() {
    g_explore.dialog.visible = false;
    g_explore.dialog = ExploreDialog();
    g_explore.focus = EXPLORER_FOCUS_PANEL;
}

static void explore_dialog_confirm() {
    if (g_explore.dialog.kind == EXPLORER_DIALOG_MKDIR) {
        Panel& panel = explore_active_panel();
        std::wstring raw = explore_trim(g_explore.dialog.input_value);
        if (raw.empty()) {
            explore_info_dialog_open(L"MkDir", L"Folder name is empty", L"");
            return;
        }

        std::wstring target = explore_native_path(raw);
        if (!explore_is_absolute_path(target))
            target = explore_join_path(panel.cwd, target);

        if (explore_path_exists(target)) {
            explore_info_dialog_open(L"MkDir", L"Folder already exists", explore_display_path(target));
            return;
        }
        if (!explore_ensure_dir(target)) {
            explore_info_dialog_open(L"MkDir", L"Failed to create folder", explore_last_error_text(GetLastError()));
            return;
        }

        std::wstring created_name = explore_leaf_name(target);
        explore_dialog_close();
        explore_copy_refresh_panels();
        if (explore_same_path(panel.cwd, explore_parent_dir(target)) || explore_same_path(panel.cwd, target)) {
            explore_focus_entry_name(panel, created_name);
        }
        return;
    }

    if (g_explore.dialog.kind == EXPLORER_DIALOG_COPY) {
        std::vector<std::wstring> sources = explore_copy_sources();
        std::vector<ExploreCopyTask> tasks;
        std::map<std::wstring, size_t> pending;
        std::wstring error;
        if (!explore_copy_build_tasks(sources, g_explore.dialog.input_value, tasks, pending, error)) {
            explore_info_dialog_open(L"Copy", error, L"");
            return;
        }

        explore_dialog_close();
        g_explore.copy = ExploreCopyState();
        g_explore.copy.active = true;
        g_explore.copy.source_left = g_explore.left.active;
        g_explore.copy.sources = sources;
        g_explore.copy.tasks = std::move(tasks);
        g_explore.copy.pending = std::move(pending);
        explore_copy_run();
        return;
    }

    if (g_explore.dialog.kind == EXPLORER_DIALOG_MOVE) {
        std::vector<std::wstring> sources = explore_copy_sources();
        std::vector<ExploreCopyTask> tasks;
        std::map<std::wstring, size_t> pending;
        std::wstring error;
        if (!explore_move_build_tasks(sources, g_explore.dialog.input_value, tasks, pending, error)) {
            explore_info_dialog_open(L"Move", error, L"");
            return;
        }

        explore_dialog_close();
        g_explore.copy = ExploreCopyState();
        g_explore.copy.active = true;
        g_explore.copy.move_mode = true;
        g_explore.copy.source_left = g_explore.left.active;
        g_explore.copy.sources = sources;
        g_explore.copy.tasks = std::move(tasks);
        g_explore.copy.pending = std::move(pending);
        explore_copy_run();
        return;
    }

    if (g_explore.dialog.kind == EXPLORER_DIALOG_OVERWRITE) {
        g_explore.copy.overwrite_once = true;
        explore_dialog_close();
        explore_copy_run();
        return;
    }

    if (g_explore.dialog.kind == EXPLORER_DIALOG_RECYCLE || g_explore.dialog.kind == EXPLORER_DIALOG_DELETE) {
        std::vector<std::wstring> sources = explore_copy_sources();
        if (sources.empty()) {
            explore_info_dialog_open(g_explore.dialog.kind == EXPLORER_DIALOG_RECYCLE ? L"Recycle" : L"Delete", L"Nothing selected", L"");
            return;
        }

        bool recycle = g_explore.dialog.kind == EXPLORER_DIALOG_RECYCLE;
        explore_dialog_close();
        g_explore.del = ExploreDeleteState();
        g_explore.del.active = true;
        g_explore.del.recycle_mode = recycle;
        g_explore.del.source_left = g_explore.left.active;
        g_explore.del.sources = std::move(sources);
        explore_delete_run();
        return;
    }

    explore_dialog_close();
}

static void explore_dialog_confirm_all() {
    if (g_explore.dialog.kind != EXPLORER_DIALOG_OVERWRITE) return;
    g_explore.copy.overwrite_all = true;
    explore_dialog_close();
    explore_copy_run();
}

static void explore_dialog_move_cursor(int delta) {
    int next = g_explore.dialog.input_cursor + delta;
    g_explore.dialog.input_cursor = std::max(0, std::min(next, (int)g_explore.dialog.input_value.size()));
}

static void explore_dialog_move_home_end(bool home) {
    g_explore.dialog.input_cursor = home ? 0 : (int)g_explore.dialog.input_value.size();
}

static void explore_dialog_backspace() {
    if (g_explore.dialog.input_cursor <= 0) return;
    g_explore.dialog.input_value.erase(g_explore.dialog.input_cursor - 1, 1);
    g_explore.dialog.input_cursor--;
}

static void explore_dialog_delete_char() {
    if (g_explore.dialog.input_cursor >= (int)g_explore.dialog.input_value.size()) return;
    g_explore.dialog.input_value.erase(g_explore.dialog.input_cursor, 1);
}

static void explore_dialog_insert(wchar_t ch) {
    g_explore.dialog.input_value.insert(g_explore.dialog.input_cursor, 1, ch);
    g_explore.dialog.input_cursor++;
}

static std::vector<std::wstring> explore_copy_sources() {
    Panel& panel = explore_active_panel();
    if (!panel.selected.empty())
        return std::vector<std::wstring>(panel.selected.begin(), panel.selected.end());
    if (panel.cursor <= 0 || panel.cursor - 1 >= (int)panel.entries.size())
        return {};
    return {explore_entry_key(panel, panel.entries[panel.cursor - 1])};
}

static void explore_copy_add_tasks(const std::wstring& src, const std::wstring& dst, const std::wstring& source_key,
    std::vector<ExploreCopyTask>& tasks, std::map<std::wstring, size_t>& pending) {
    if (explore_path_is_dir(src)) {
        tasks.push_back({EXPLORER_COPY_TASK_MKDIR, src, dst, source_key});
        pending[source_key]++;

        WIN32_FIND_DATAW fd = {};
        HANDLE h = FindFirstFileW(explore_glob_pattern(src).c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) return;
        do {
            std::wstring name = fd.cFileName;
            if (name == L"." || name == L"..") continue;
            std::wstring child_src = explore_join_path(src, name);
            std::wstring child_dst = explore_join_path(dst, name);
            explore_copy_add_tasks(child_src, child_dst, source_key, tasks, pending);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
        return;
    }

    tasks.push_back({EXPLORER_COPY_TASK_FILE, src, dst, source_key});
    pending[source_key]++;
}

static bool explore_copy_build_tasks(const std::vector<std::wstring>& sources, const std::wstring& raw_dest,
    std::vector<ExploreCopyTask>& tasks, std::map<std::wstring, size_t>& pending, std::wstring& error) {
    tasks.clear();
    pending.clear();
    if (sources.empty()) {
        error = L"Nothing selected";
        return false;
    }

    std::wstring dest = explore_native_path(raw_dest);
    if (dest.empty()) {
        error = L"Destination is empty";
        return false;
    }

    bool dest_exists = explore_path_exists(dest);
    bool dest_is_dir = explore_path_is_dir(dest);
    bool dest_hint_dir = explore_trailing_slash(raw_dest);

    if (sources.size() > 1) {
        if (dest_exists && !dest_is_dir) {
            error = L"Destination must be a folder for multiple items";
            return false;
        }
        for (const std::wstring& src : sources) {
            std::wstring resolved = explore_join_path(dest, explore_leaf_name(src));
            if (explore_same_path(src, resolved)) {
                error = L"Source and destination are the same";
                return false;
            }
            explore_copy_add_tasks(src, resolved, src, tasks, pending);
        }
        return true;
    }

    std::wstring src = sources[0];
    bool src_is_dir = explore_path_is_dir(src);
    std::wstring resolved = dest;
    if (dest_exists && dest_is_dir)
        resolved = explore_join_path(dest, explore_leaf_name(src));
    else if (dest_hint_dir)
        resolved = explore_join_path(dest, explore_leaf_name(src));
    else if (src_is_dir && dest_exists && !dest_is_dir) {
        error = L"Cannot copy folder onto a file";
        return false;
    }

    if (explore_same_path(src, resolved)) {
        error = L"Source and destination are the same";
        return false;
    }

    explore_copy_add_tasks(src, resolved, src, tasks, pending);
    return true;
}

static void explore_move_add_tasks(const std::wstring& src, const std::wstring& dst, const std::wstring& source_key,
    std::vector<ExploreCopyTask>& tasks, std::map<std::wstring, size_t>& pending) {
    if (explore_path_is_dir(src)) {
        tasks.push_back({EXPLORER_COPY_TASK_MKDIR, src, dst, source_key});
        pending[source_key]++;

        WIN32_FIND_DATAW fd = {};
        HANDLE h = FindFirstFileW(explore_glob_pattern(src).c_str(), &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                std::wstring name = fd.cFileName;
                if (name == L"." || name == L"..") continue;
                std::wstring child_src = explore_join_path(src, name);
                std::wstring child_dst = explore_join_path(dst, name);
                explore_move_add_tasks(child_src, child_dst, source_key, tasks, pending);
            } while (FindNextFileW(h, &fd));
            FindClose(h);
        }

        tasks.push_back({EXPLORER_COPY_TASK_RMDIR, src, dst, source_key});
        pending[source_key]++;
        return;
    }

    tasks.push_back({EXPLORER_COPY_TASK_FILE, src, dst, source_key});
    pending[source_key]++;
}

static bool explore_move_build_tasks(const std::vector<std::wstring>& sources, const std::wstring& raw_dest,
    std::vector<ExploreCopyTask>& tasks, std::map<std::wstring, size_t>& pending, std::wstring& error) {
    tasks.clear();
    pending.clear();
    if (sources.empty()) {
        error = L"Nothing selected";
        return false;
    }

    std::wstring dest = explore_native_path(raw_dest);
    if (dest.empty()) {
        error = L"Destination is empty";
        return false;
    }

    bool dest_exists = explore_path_exists(dest);
    bool dest_is_dir = explore_path_is_dir(dest);
    bool dest_hint_dir = explore_trailing_slash(raw_dest);

    if (sources.size() > 1) {
        if (dest_exists && !dest_is_dir) {
            error = L"Destination must be a folder for multiple items";
            return false;
        }
        for (const std::wstring& src : sources) {
            std::wstring resolved = explore_join_path(dest, explore_leaf_name(src));
            if (explore_same_path(src, resolved)) {
                error = L"Source and destination are the same";
                return false;
            }
            if (explore_path_is_dir(src) && explore_path_starts_with(resolved, src)) {
                error = L"Cannot move a folder into itself";
                return false;
            }
            explore_move_add_tasks(src, resolved, src, tasks, pending);
        }
        return true;
    }

    std::wstring src = sources[0];
    bool src_is_dir = explore_path_is_dir(src);
    std::wstring resolved = dest;
    if (dest_exists && dest_is_dir)
        resolved = explore_join_path(dest, explore_leaf_name(src));
    else if (dest_hint_dir)
        resolved = explore_join_path(dest, explore_leaf_name(src));
    else if (src_is_dir && dest_exists && !dest_is_dir) {
        error = L"Cannot move folder onto a file";
        return false;
    }

    if (explore_same_path(src, resolved)) {
        error = L"Source and destination are the same";
        return false;
    }
    if (src_is_dir && explore_path_starts_with(resolved, src)) {
        error = L"Cannot move a folder into itself";
        return false;
    }

    explore_move_add_tasks(src, resolved, src, tasks, pending);
    return true;
}

static void explore_copy_refresh_panels() {
    explore_load_entries(g_explore.left, false, false);
    explore_load_entries(g_explore.right, false, false);
}

static int explore_copy_total_steps() {
    return std::max(1, (int)g_explore.copy.tasks.size());
}

static Panel& explore_delete_source_panel() {
    return g_explore.del.source_left ? g_explore.left : g_explore.right;
}

static int explore_delete_total_steps() {
    return std::max(1, (int)g_explore.del.sources.size());
}

static std::wstring explore_delete_name() {
    return g_explore.del.recycle_mode ? L"Recycle" : L"Delete";
}

static std::wstring explore_delete_progress_name() {
    return g_explore.del.recycle_mode ? L"Recycling" : L"Deleting";
}

static std::wstring explore_fileop_error_text(int code, bool aborted) {
    if (aborted)
        return L"Operation canceled";
    if (code == 0)
        return L"Operation failed";

    std::wstring text = explore_last_error_text((DWORD)code);
    if (text.empty() || text == L"The operation completed successfully.")
        return L"Operation failed (code " + std::to_wstring(code) + L")";
    return text;
}

static bool explore_delete_path(const std::wstring& path, bool recycle, std::wstring& error) {
    std::wstring from = explore_native_path(path);
    if (from.empty()) {
        error = L"Nothing selected";
        return false;
    }

    from.push_back(L'\0');
    from.push_back(L'\0');

    SHFILEOPSTRUCTW op = {};
    op.wFunc = FO_DELETE;
    op.pFrom = from.c_str();
    op.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
    if (recycle)
        op.fFlags |= FOF_ALLOWUNDO;

    int rc = SHFileOperationW(&op);
    if (rc == 0 && !op.fAnyOperationsAborted)
        return true;

    error = explore_fileop_error_text(rc, op.fAnyOperationsAborted != FALSE);
    return false;
}

static std::wstring explore_transfer_name() {
    return g_explore.copy.move_mode ? L"Move" : L"Copy";
}

static void explore_copy_finish() {
    Panel& source = explore_copy_source_panel();
    for (const std::wstring& src : g_explore.copy.sources)
        source.selected.erase(src);
    if (g_explore.dialog.kind == EXPLORER_DIALOG_PROGRESS)
        explore_dialog_close();
    g_explore.copy = ExploreCopyState();
    explore_copy_refresh_panels();
}

static void explore_copy_cancel() {
    if (g_explore.dialog.kind == EXPLORER_DIALOG_PROGRESS)
        explore_dialog_close();
    g_explore.copy = ExploreCopyState();
}

static void explore_copy_run() {
    while (g_explore.copy.active && g_explore.copy.index < g_explore.copy.tasks.size()) {
        const ExploreCopyTask& task = g_explore.copy.tasks[g_explore.copy.index];
        explore_progress_dialog_open(
            explore_transfer_name() + L"ing",
            L"From: " + explore_display_path(task.source_key),
            L"To: " + explore_display_path(task.dst),
            (int)g_explore.copy.index,
            explore_copy_total_steps());
        explore_draw();

        if (task.kind == EXPLORER_COPY_TASK_MKDIR) {
            if (!explore_ensure_dir(task.dst)) {
                DWORD code = GetLastError();
                explore_copy_cancel();
                explore_info_dialog_open(explore_transfer_name(), L"Failed to create folder", explore_last_error_text(code));
                return;
            }
            g_explore.copy.overwrite_once = false;
            auto it = g_explore.copy.pending.find(task.source_key);
            if (it != g_explore.copy.pending.end()) {
                if (it->second > 0) it->second--;
                if (it->second == 0) {
                    explore_copy_source_panel().selected.erase(task.source_key);
                    g_explore.copy.pending.erase(it);
                }
            }
            g_explore.copy.index++;
            continue;
        }

        if (task.kind == EXPLORER_COPY_TASK_RMDIR) {
            if (!RemoveDirectoryW(task.src.c_str())) {
                DWORD code = GetLastError();
                if (code != ERROR_FILE_NOT_FOUND && code != ERROR_PATH_NOT_FOUND) {
                    explore_copy_cancel();
                    explore_info_dialog_open(explore_transfer_name(), L"Failed to remove source folder", explore_last_error_text(code));
                    return;
                }
            }
            g_explore.copy.overwrite_once = false;
            auto it = g_explore.copy.pending.find(task.source_key);
            if (it != g_explore.copy.pending.end()) {
                if (it->second > 0) it->second--;
                if (it->second == 0) {
                    explore_copy_source_panel().selected.erase(task.source_key);
                    g_explore.copy.pending.erase(it);
                }
            }
            g_explore.copy.index++;
            continue;
        }

        std::wstring parent = explore_parent_dir(task.dst);
        if (!parent.empty() && !explore_ensure_dir(parent)) {
            DWORD code = GetLastError();
            explore_copy_cancel();
            explore_info_dialog_open(explore_transfer_name(), L"Failed to create target folder", explore_last_error_text(code));
            return;
        }

        bool exists = explore_path_exists(task.dst);
        if (exists && !g_explore.copy.overwrite_all && !g_explore.copy.overwrite_once) {
            explore_overwrite_dialog_open(task.dst);
            return;
        }

        bool overwrite = exists && (g_explore.copy.overwrite_all || g_explore.copy.overwrite_once);
        bool ok = g_explore.copy.move_mode
            ? explore_move_file(task.src, task.dst, overwrite)
            : (overwrite
                ? explore_replace_file(task.src, task.dst)
                : (CopyFileW(task.src.c_str(), task.dst.c_str(), TRUE) != 0));
        if (!ok) {
            DWORD code = GetLastError();
            explore_copy_cancel();
            explore_info_dialog_open(explore_transfer_name(), explore_transfer_name() + L" failed", explore_last_error_text(code));
            return;
        }

        g_explore.copy.overwrite_once = false;
        auto it = g_explore.copy.pending.find(task.source_key);
        if (it != g_explore.copy.pending.end()) {
            if (it->second > 0) it->second--;
            if (it->second == 0) {
                explore_copy_source_panel().selected.erase(task.source_key);
                g_explore.copy.pending.erase(it);
            }
        }
        g_explore.copy.index++;
    }

    if (g_explore.copy.active && g_explore.copy.index >= g_explore.copy.tasks.size()) {
        explore_progress_dialog_open(explore_transfer_name() + L" complete", L"", L"",
            explore_copy_total_steps(), explore_copy_total_steps());
        explore_draw();
        explore_copy_finish();
    }
}

static void explore_delete_finish() {
    if (g_explore.dialog.kind == EXPLORER_DIALOG_PROGRESS)
        explore_dialog_close();
    g_explore.del = ExploreDeleteState();
    explore_copy_refresh_panels();
}

static void explore_delete_cancel() {
    if (g_explore.dialog.kind == EXPLORER_DIALOG_PROGRESS)
        explore_dialog_close();
    g_explore.del = ExploreDeleteState();
}

static void explore_delete_run() {
    while (g_explore.del.active && g_explore.del.index < g_explore.del.sources.size()) {
        const std::wstring& src = g_explore.del.sources[g_explore.del.index];
        explore_progress_dialog_open(
            explore_delete_progress_name(),
            L"From: " + explore_display_path(src),
            g_explore.del.recycle_mode ? L"To: recycle bin" : L"To: permanent delete",
            (int)g_explore.del.index,
            explore_delete_total_steps());
        explore_draw();

        std::wstring error;
        if (!explore_delete_path(src, g_explore.del.recycle_mode, error)) {
            explore_delete_cancel();
            explore_info_dialog_open(explore_delete_name(), explore_delete_name() + L" failed", error);
            return;
        }

        explore_delete_source_panel().selected.erase(src);
        g_explore.del.index++;
    }

    if (g_explore.del.active && g_explore.del.index >= g_explore.del.sources.size()) {
        explore_progress_dialog_open(explore_delete_name() + L" complete", L"", L"",
            explore_delete_total_steps(), explore_delete_total_steps());
        explore_draw();
        explore_delete_finish();
    }
}

static bool explore_jump_expired() {
    return !g_explore.jump_buffer.empty() &&
        (GetTickCount64() - g_explore.jump_tick) > EXPLORER_JUMP_TIMEOUT_MS;
}

static void explore_jump_clear() {
    g_explore.jump_buffer.clear();
    g_explore.jump_tick = 0;
}

static void explore_jump_reset_if_expired() {
    if (explore_jump_expired())
        explore_jump_clear();
}

static void explore_jump_focus_match(Panel& panel) {
    if (g_explore.jump_buffer.empty()) return;

    std::wstring prefix = explore_lower(g_explore.jump_buffer);
    for (int i = 0; i < (int)panel.entries.size(); i++) {
        std::wstring name = explore_lower(panel.entries[i].name);
        if (name.rfind(prefix, 0) == 0) {
            panel.cursor = i + 1;
            explore_clamp_panel(panel);
            return;
        }
    }
}

static void explore_jump_append(wchar_t ch) {
    explore_jump_reset_if_expired();
    g_explore.jump_buffer.push_back((wchar_t)::towlower(ch));
    g_explore.jump_tick = GetTickCount64();
    explore_jump_focus_match(explore_active_panel());
}

static void explore_jump_backspace() {
    explore_jump_reset_if_expired();
    if (g_explore.jump_buffer.empty()) return;
    g_explore.jump_buffer.pop_back();
    g_explore.jump_tick = g_explore.jump_buffer.empty() ? 0 : GetTickCount64();
    if (!g_explore.jump_buffer.empty())
        explore_jump_focus_match(explore_active_panel());
}

static void explore_sync_panels_from_shell() {
    std::wstring shell_cwd = explore_current_dir();
    if (shell_cwd.empty()) return;

    Panel& active = explore_active_panel();
    if (active.cwd != shell_cwd) {
        active.cwd = shell_cwd;
        explore_load_entries(active, true);
    } else {
        explore_load_entries(active, false, false);
    }

    Panel& inactive = explore_inactive_panel();
    if (inactive.cwd == shell_cwd)
        explore_load_entries(inactive, false, false);
}

static void explore_focus_filter_result(Panel& panel) {
    if (!panel.filter.empty() && !panel.entries.empty()) panel.cursor = 1;
    explore_clamp_panel(panel);
}

static void explore_focus_entry_name(Panel& panel, const std::wstring& name) {
    if (name.empty()) {
        explore_clamp_panel(panel);
        return;
    }
    for (int i = 0; i < (int)panel.entries.size(); i++) {
        if (panel.entries[i].name == name) {
            panel.cursor = i + 1;
            explore_clamp_panel(panel);
            return;
        }
    }
    explore_clamp_panel(panel);
}

static void explore_filter_apply_active(bool focus_first_match) {
    Panel& panel = explore_active_panel();
    panel.filter_cursor = std::max(0, std::min(panel.filter_cursor, (int)panel.filter.size()));
    explore_apply_filter(panel);
    if (focus_first_match && !panel.filter.empty())
        explore_focus_filter_result(panel);
    else
        explore_clamp_panel(panel);
}

static void explore_filter_begin() {
    Panel& panel = explore_active_panel();
    explore_jump_clear();
    g_explore.focus = EXPLORER_FOCUS_FILTER;
    g_explore.filter_replace = !panel.filter.empty();
    panel.filter_cursor = (int)panel.filter.size();
}

static void explore_filter_append(wchar_t ch) {
    Panel& panel = explore_active_panel();
    if (g_explore.filter_replace) {
        panel.filter.clear();
        panel.filter_cursor = 0;
        g_explore.filter_replace = false;
    }
    panel.filter.insert(panel.filter_cursor, 1, ch);
    panel.filter_cursor++;
    explore_filter_apply_active(true);
}

static void explore_filter_backspace() {
    Panel& panel = explore_active_panel();
    g_explore.filter_replace = false;
    if (panel.filter_cursor <= 0 || panel.filter.empty()) return;
    panel.filter.erase(panel.filter_cursor - 1, 1);
    panel.filter_cursor--;
    explore_filter_apply_active(false);
}

static void explore_filter_delete() {
    Panel& panel = explore_active_panel();
    g_explore.filter_replace = false;
    if (panel.filter_cursor >= (int)panel.filter.size()) return;
    panel.filter.erase(panel.filter_cursor, 1);
    explore_filter_apply_active(false);
}

static void explore_filter_move_cursor(int delta) {
    Panel& panel = explore_active_panel();
    g_explore.filter_replace = false;
    int next = panel.filter_cursor + delta;
    panel.filter_cursor = std::max(0, std::min(next, (int)panel.filter.size()));
}

static void explore_filter_move_home_end(bool home) {
    Panel& panel = explore_active_panel();
    g_explore.filter_replace = false;
    panel.filter_cursor = home ? 0 : (int)panel.filter.size();
}

static void explore_filter_end() {
    g_explore.focus = EXPLORER_FOCUS_PANEL;
    g_explore.filter_replace = false;
}

static void explore_filter_clear_and_exit() {
    Panel& panel = explore_active_panel();
    panel.filter.clear();
    panel.filter_cursor = 0;
    explore_filter_end();
    explore_filter_apply_active(false);
}

static void explore_switch_panel() {
    g_explore.focus = EXPLORER_FOCUS_PANEL;
    g_explore.filter_replace = false;
    explore_jump_clear();
    g_explore.left.active = !g_explore.left.active;
    g_explore.right.active = !g_explore.right.active;
    explore_clamp_panel(g_explore.left);
    explore_clamp_panel(g_explore.right);
}

static void explore_move_cursor(int delta) {
    explore_jump_clear();
    Panel& panel = explore_active_panel();
    panel.cursor += delta;
    explore_clamp_panel(panel);
}

static void explore_move_page(int delta) {
    explore_jump_clear();
    Panel& panel = explore_active_panel();
    int step = std::max(1, explore_visible_rows());
    panel.cursor += delta * step;
    explore_clamp_panel(panel);
}

static void explore_move_home_end(bool home) {
    explore_jump_clear();
    Panel& panel = explore_active_panel();
    panel.cursor = home ? 0 : std::max(0, explore_total_rows(panel) - 1);
    explore_clamp_panel(panel);
}

static void explore_toggle_selection() {
    explore_jump_clear();
    Panel& panel = explore_active_panel();
    if (panel.cursor <= 0) return;

    int idx = panel.cursor - 1;
    if (idx >= (int)panel.entries.size()) return;

    std::wstring key = explore_entry_key(panel, panel.entries[idx]);
    if (panel.selected.count(key))
        panel.selected.erase(key);
    else
        panel.selected.insert(key);

    if (panel.cursor < explore_total_rows(panel) - 1)
        panel.cursor++;
    explore_clamp_panel(panel);
}

static bool explore_clear_selection() {
    Panel& panel = explore_active_panel();
    if (panel.selected.empty()) return false;
    panel.selected.clear();
    return true;
}

static void explore_go_parent() {
    explore_jump_clear();
    Panel& panel = explore_active_panel();
    std::wstring child_name = explore_leaf_name(panel.cwd);
    std::wstring parent = explore_parent_dir(panel.cwd);
    if (!parent.empty() && parent != panel.cwd) {
        panel.cwd = parent;
        explore_load_entries(panel, true);
        explore_focus_entry_name(panel, child_name);
    } else {
        panel.cursor = 0;
        panel.scroll = 0;
    }
}

static void explore_enter() {
    explore_jump_clear();
    Panel& panel = explore_active_panel();
    if (panel.cursor == 0) {
        explore_go_parent();
        return;
    }
    if (panel.cursor - 1 >= (int)panel.entries.size()) return;

    const Entry& entry = panel.entries[panel.cursor - 1];
    std::wstring path = explore_join_path(panel.cwd, entry.name);
    if (entry.is_dir) {
        panel.cwd = path;
        explore_load_entries(panel, true);
    }
}

void explore_toggle() {
    explore_init_state();
    explore_jump_clear();
    explore_sync_panels_from_shell();
    if (!explore_ensure_buffer()) return;

    if (g_explore.shell_buf) {
        GetConsoleCursorInfo(g_explore.shell_buf, &g_explore.shell_cursor);
        GetConsoleMode(g_explore.shell_buf, &g_explore.shell_out_mode);
    }

    SetConsoleActiveScreenBuffer(g_explore.explore_buf);
    explore_cache_view_size();
    explore_draw();

    while (true) {
        DWORD wait = WaitForSingleObject(in_h, 50);
        if (wait != WAIT_OBJECT_0) continue;

        INPUT_RECORD ir;
        DWORD count = 0;
        if (!ReadConsoleInputW(in_h, &ir, 1, &count)) break;

        if (ir.EventType == WINDOW_BUFFER_SIZE_EVENT) {
            explore_cache_view_size();
            explore_draw();
            continue;
        }
        if (ir.EventType != KEY_EVENT || !ir.Event.KeyEvent.bKeyDown) continue;

        WORD vk = ir.Event.KeyEvent.wVirtualKeyCode;
        wchar_t ch = ir.Event.KeyEvent.uChar.UnicodeChar;
        DWORD state = ir.Event.KeyEvent.dwControlKeyState;
        bool ctrl = (state & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
        bool shift = (state & SHIFT_PRESSED) != 0;

        if ((ctrl && vk == 'O') || vk == VK_F10) break;
        if (explore_dialog_focused()) {
            if (!ctrl && vk == VK_ESCAPE) {
                if (g_explore.dialog.kind == EXPLORER_DIALOG_OVERWRITE)
                    explore_copy_cancel();
                explore_dialog_close();
                explore_draw();
                continue;
            }
            if (!ctrl && vk == VK_RETURN) { explore_dialog_confirm(); explore_draw(); continue; }
            if (ctrl && vk == VK_RETURN && g_explore.dialog.kind == EXPLORER_DIALOG_OVERWRITE) {
                explore_dialog_confirm_all();
                explore_draw();
                continue;
            }
            if (g_explore.dialog.kind == EXPLORER_DIALOG_COPY || g_explore.dialog.kind == EXPLORER_DIALOG_MOVE || g_explore.dialog.kind == EXPLORER_DIALOG_MKDIR) {
                if (!ctrl && vk == VK_LEFT)   { explore_dialog_move_cursor(-1); explore_draw(); continue; }
                if (!ctrl && vk == VK_RIGHT)  { explore_dialog_move_cursor(+1); explore_draw(); continue; }
                if (!ctrl && vk == VK_HOME)   { explore_dialog_move_home_end(true); explore_draw(); continue; }
                if (!ctrl && vk == VK_END)    { explore_dialog_move_home_end(false); explore_draw(); continue; }
                if (!ctrl && vk == VK_BACK)   { explore_dialog_backspace(); explore_draw(); continue; }
                if (!ctrl && vk == VK_DELETE) { explore_dialog_delete_char(); explore_draw(); continue; }
                if (!ctrl && ch >= 32 && ch != 127) { explore_dialog_insert(ch); explore_draw(); continue; }
            }
            continue;
        }
        if (explore_filter_focused()) {
            if (vk == VK_UP)     { explore_filter_end(); explore_move_cursor(-1); explore_draw(); continue; }
            if (vk == VK_DOWN)   { explore_filter_end(); explore_move_cursor(+1); explore_draw(); continue; }
            if (!ctrl && vk == VK_RETURN) { explore_filter_end(); explore_draw(); continue; }
            if (!ctrl && vk == VK_ESCAPE) { explore_filter_clear_and_exit(); explore_draw(); continue; }
            if (!ctrl && vk == VK_LEFT)   { explore_filter_move_cursor(-1); explore_draw(); continue; }
            if (!ctrl && vk == VK_RIGHT)  { explore_filter_move_cursor(+1); explore_draw(); continue; }
            if (!ctrl && vk == VK_HOME)   { explore_filter_move_home_end(true); explore_draw(); continue; }
            if (!ctrl && vk == VK_END)    { explore_filter_move_home_end(false); explore_draw(); continue; }
            if (!ctrl && vk == VK_BACK)   { explore_filter_backspace(); explore_draw(); continue; }
            if (!ctrl && vk == VK_DELETE) { explore_filter_delete(); explore_draw(); continue; }
            if (!ctrl && ch >= 32 && ch != 127) { explore_filter_append(ch); explore_draw(); continue; }
            continue;
        }
        explore_jump_reset_if_expired();
        if (!ctrl && ch == L'/')         { explore_filter_begin(); explore_draw(); continue; }
        if (!ctrl && vk == VK_ESCAPE)    {
            if (explore_clear_selection() || !g_explore.jump_buffer.empty()) {
                explore_jump_clear();
                explore_draw();
            }
            continue;
        }
        if (vk == VK_F1)       { explore_open_other_same_dir(); explore_draw(); continue; }
        if (vk == VK_F2)       { explore_cycle_sort_mode(); explore_draw(); continue; }
        if (vk == VK_TAB)      { explore_switch_panel(); explore_draw(); continue; }
        if (vk == VK_UP)       { explore_move_cursor(-1); explore_draw(); continue; }
        if (vk == VK_DOWN)     { explore_move_cursor(+1); explore_draw(); continue; }
        if (vk == VK_LEFT)     { explore_move_page(-1); explore_draw(); continue; }
        if (vk == VK_RIGHT)    { explore_move_page(+1); explore_draw(); continue; }
        if (vk == VK_PRIOR)    { explore_move_page(-1); explore_draw(); continue; }
        if (vk == VK_NEXT)     { explore_move_page(+1); explore_draw(); continue; }
        if (vk == VK_HOME)     { explore_move_home_end(true); explore_draw(); continue; }
        if (vk == VK_END)      { explore_move_home_end(false); explore_draw(); continue; }
        if (vk == VK_F3)       { if (explore_open_current_file(true)) explore_draw(); continue; }
        if (vk == VK_F4)       { if (explore_open_current_file(false)) explore_draw(); continue; }
        if (vk == VK_F5)       { explore_copy_dialog_open(); explore_draw(); continue; }
        if (vk == VK_F6)       { explore_move_dialog_open(); explore_draw(); continue; }
        if (vk == VK_F7)       { explore_mkdir_dialog_open(); explore_draw(); continue; }
        if (vk == VK_F8 && shift) { explore_delete_dialog_open(false); explore_draw(); continue; }
        if (vk == VK_F8)       { explore_delete_dialog_open(true); explore_draw(); continue; }
        if (vk == VK_INSERT)   { explore_toggle_selection(); explore_draw(); continue; }
        if (vk == VK_RETURN)   { explore_enter(); explore_draw(); continue; }
        if (!ctrl && vk == VK_BACK && !g_explore.jump_buffer.empty()) { explore_jump_backspace(); explore_draw(); continue; }
        if (vk == VK_BACK)     { explore_go_parent(); explore_draw(); continue; }
        if (!ctrl && ch >= 32 && ch != 127) { explore_jump_append(ch); explore_draw(); continue; }
    }

    Panel& active = explore_active_panel();
    if (!active.cwd.empty())
        SetCurrentDirectoryW(active.cwd.c_str());

    SetConsoleActiveScreenBuffer(g_explore.shell_buf);
    if (g_explore.shell_buf) {
        DWORD cur_mode = 0;
        if (!GetConsoleMode(g_explore.shell_buf, &cur_mode) || cur_mode != g_explore.shell_out_mode)
            SetConsoleMode(g_explore.shell_buf, g_explore.shell_out_mode);
        SetConsoleCursorInfo(g_explore.shell_buf, &g_explore.shell_cursor);
    }
    explore_close_buffer();
}





