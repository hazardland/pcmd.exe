// MODULE: signal
// Purpose : Ctrl+C/Break flag, console close/shutdown handler, forward declarations for handler's dependencies
// Exports : ctrl_c_fired | g_editor | ctrl_handler()
// Depends : common.h

// Set to true by ctrl_handler on Ctrl+C; checked after child exits to suppress exit-code display and emit a newline.
static volatile bool ctrl_c_fired = false;

// Forward declarations so ctrl_handler can save history on close/shutdown.
struct editor;
void save_prev_dir();
void write_alias(const std::string&, const std::string&);
void append_history(const std::wstring&);
void compact_history();

// Globals so ctrl_handler (which has no parameters) can reach the live editor state on unexpected exit.
static editor* g_editor = nullptr;

// Ctrl+C/Break: set flag and suppress for our process (child still receives the signal).
// Close/Logoff/Shutdown: flush history to disk then let Windows terminate us.
BOOL WINAPI ctrl_handler(DWORD type) {
    if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT) {
        ctrl_c_fired = true;
        return TRUE;
    }
    if (type == CTRL_CLOSE_EVENT || type == CTRL_LOGOFF_EVENT || type == CTRL_SHUTDOWN_EVENT) {
        if (g_editor) { compact_history(); save_prev_dir(); }
        return FALSE;
    }
    return FALSE;
}
