// MODULE: note
// Purpose : open ~/notes.md in the built-in editor
// Exports : note_cmd()
// Depends : common.h, edit.h

static int note_cmd() {
    char home[MAX_PATH] = {};
    GetEnvironmentVariableA("USERPROFILE", home, MAX_PATH);
    return edit_file(std::string(home) + "/notes.md");
}
