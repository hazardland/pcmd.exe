// MODULE: vid
// Purpose : future SIXEL-first video command
// Exports : vid_cmd()
// Depends : common.h, sixel.h

int vid_cmd(const std::string& line) {
    (void)line;
    out("vid: not implemented yet\r\n");
    return 1;
}
