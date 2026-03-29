// MODULE: yt
// Purpose : thin YouTube downloader wrapper around yt-dlp + ffmpeg
// Exports : yt_cmd()
// Depends : common.h

static std::string yt_trim(std::string s) {
    while (!s.empty() && s.front() == ' ') s.erase(s.begin());
    while (!s.empty() && s.back()  == ' ') s.pop_back();
    return s;
}

static std::string yt_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

static std::string yt_quote(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else out += c;
    }
    out += "\"";
    return out;
}

static bool yt_has_tool(const wchar_t* name) {
    wchar_t buf[MAX_PATH] = {};
    if (SearchPathW(NULL, name, NULL, MAX_PATH, buf, NULL) != 0) return true;

    std::wstring base(name);
    if (base.find(L'.') == std::wstring::npos) {
        static const wchar_t* exts[] = {L".exe", L".cmd", L".bat", L".com"};
        for (auto ext : exts) {
            if (SearchPathW(NULL, base.c_str(), ext, MAX_PATH, buf, NULL) != 0)
                return true;
        }
    }
    return false;
}

static std::string yt_current_dir() {
    char buf[MAX_PATH] = {};
    DWORD n = GetCurrentDirectoryA(MAX_PATH, buf);
    if (n == 0 || n >= MAX_PATH) return ".";
    std::string path(buf);
    std::replace(path.begin(), path.end(), '\\', '/');
    return path;
}

static bool yt_is_dir(const std::string& path) {
    DWORD attr = GetFileAttributesW(to_wide(path).c_str());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static std::vector<std::string> yt_split_args(const std::string& s) {
    std::vector<std::string> args;
    std::string cur;
    char quote = 0;
    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        if (quote) {
            if (c == '\\' && i + 1 < s.size() && s[i + 1] == quote) {
                cur += s[i + 1];
                i++;
            } else if (c == quote) {
                quote = 0;
            } else {
                cur += c;
            }
            continue;
        }
        if (c == '"' || c == '\'') {
            quote = c;
            continue;
        }
        if (c == ' ' || c == '\t') {
            if (!cur.empty()) {
                args.push_back(cur);
                cur.clear();
            }
            continue;
        }
        cur += c;
    }
    if (!cur.empty()) args.push_back(cur);
    return args;
}

static void yt_help() {
    out(
        GREEN "yt mp3" RESET " <url> [folder]   Download best audio and convert to mp3\r\n"
        GREEN "yt mp4" RESET " <url> [folder]   Download best video/audio as mp4 when possible\r\n"
        "        Default folder is current directory. " GRAY "./" RESET " also means current directory.\r\n"
    );
}

static int yt_run_child(const std::string& cmdline) {
    std::wstring cmd = to_wide(cmdline);
    std::vector<wchar_t> buf(cmd.begin(), cmd.end());
    buf.push_back(0);
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(NULL, buf.data(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        err("yt: failed to start yt-dlp\r\n");
        return 1;
    }
    SetConsoleMode(in_h, orig_in_mode);
    WaitForSingleObject(pi.hProcess, INFINITE);
    SetConsoleMode(in_h, ENABLE_EXTENDED_FLAGS | ENABLE_QUICK_EDIT_MODE | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)code;
}

static int yt_cmd(const std::string& line) {
    std::string args = line.size() > 2 ? yt_trim(line.substr(2)) : "";
    if (!args.empty() && args.front() == ' ') args = yt_trim(args);
    if (args.empty() || args == "help" || args == "--help" || args == "-h") {
        yt_help();
        return 0;
    }

    std::vector<std::string> parts = yt_split_args(args);
    if (parts.empty()) {
        yt_help();
        return 1;
    }

    std::string mode = yt_lower(parts[0]);
    if (mode != "mp3" && mode != "mp4") {
        err("yt: expected mp3 or mp4\r\n");
        yt_help();
        return 1;
    }
    if (parts.size() < 2) {
        err("yt: missing url\r\n");
        yt_help();
        return 1;
    }
    if (parts.size() > 3) {
        err("yt: expected yt mp3 <url> [folder] or yt mp4 <url> [folder]\r\n");
        return 1;
    }

    if (!yt_has_tool(L"yt-dlp")) {
        err("yt: yt-dlp is required\r\n");
        out("Install: winget install yt-dlp.yt-dlp\r\n");
        return 1;
    }
    if (!yt_has_tool(L"ffmpeg")) {
        err("yt: ffmpeg is required\r\n");
        out("Install: winget install Gyan.FFmpeg\r\n");
        return 1;
    }

    std::string url = parts[1];
    std::string folder = parts.size() >= 3 ? parts[2] : "./";
    if (folder.empty() || folder == "." || folder == "./") folder = yt_current_dir();
    folder = normalize_path(folder);
    if (!yt_is_dir(folder)) {
        err("yt: folder not found\r\n");
        return 1;
    }

    std::string cmd = "yt-dlp ";
    if (mode == "mp3")
        cmd += "-x --audio-format mp3 --audio-quality 0 ";
    else
        cmd += "-f \"bestvideo+bestaudio/best\" --merge-output-format mp4 ";
    cmd += "-P " + yt_quote(folder) + " -- " + yt_quote(url);
    return yt_run_child(cmd);
}
