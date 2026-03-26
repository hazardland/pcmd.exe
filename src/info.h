// MODULE: info
// Purpose : runtime shell info — elevation check, current time, cwd, folder name, git branch and dirty flag
// Exports : elevated() cur_time() cwd() folder() branch() dirty()
// Depends : common.h

// Returns true if the process has admin elevation; used to switch prompt color from blue to red.
bool elevated() {
    BOOL result = FALSE;
    HANDLE token = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        TOKEN_ELEVATION e = {};
        DWORD size = sizeof(e);
        if (GetTokenInformation(token, TokenElevation, &e, sizeof(e), &size))
            result = e.TokenIsElevated;
        CloseHandle(token);
    }
    return result;
}

// Returns the current local time as "HH:MM:SS.cs" (centiseconds) for the prompt timestamp.
std::string cur_time() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    char buf[32];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%02d",
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds / 10);
    return buf;
}

// Returns the current directory with all backslashes replaced by forward slashes.
std::string cwd() {
    wchar_t buf[MAX_PATH];
    GetCurrentDirectoryW(MAX_PATH, buf);
    std::string s = to_utf8(buf);
    std::replace(s.begin(), s.end(), '\\', '/');
    return s;
}

// Extracts the last path component for display in the prompt; handles trailing slash by returning the component before it.
std::string folder(const std::string& path) {
    size_t pos = path.find_last_of("\\/");
    if (pos == std::string::npos) return path;
    std::string name = path.substr(pos + 1);
    return name.empty() ? path.substr(0, pos) : name;
}

// Walks up from cwd looking for .git/HEAD to find the current branch name.
// Also sets root_out to the repo root (parent of .git) when found.
// Returns the branch name, a 7-char SHA for detached HEAD, or empty string if not in a git repo.
std::string branch(std::wstring& root_out) {
    wchar_t dir[MAX_PATH];
    GetCurrentDirectoryW(MAX_PATH, dir);
    std::wstring path = dir;
    while (!path.empty()) {
        HANDLE f = CreateFileW((path + L"\\.git\\HEAD").c_str(),
            GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, NULL);
        if (f != INVALID_HANDLE_VALUE) {
            char buf[256] = {};
            DWORD read = 0;
            ReadFile(f, buf, sizeof(buf) - 1, &read, NULL);
            CloseHandle(f);
            root_out = path;
            std::string s(buf);
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
                s.pop_back();
            const std::string ref = "ref: refs/heads/";
            if (s.substr(0, ref.size()) == ref) return s.substr(ref.size());
            if (s.size() >= 7) return s.substr(0, 7);
            return s;
        }
        size_t sep = path.find_last_of(L"\\/");
        if (sep == std::wstring::npos) break;
        path = path.substr(0, sep);
    }
    return "";
}

// Big-endian helpers for reading git's binary index format (no winsock/ntohl needed).
static inline uint32_t be32(const char* p) {
    return ((uint8_t)p[0] << 24) | ((uint8_t)p[1] << 16) | ((uint8_t)p[2] << 8) | (uint8_t)p[3];
}
static inline uint16_t be16(const char* p) {
    return ((uint8_t)p[0] << 8) | (uint8_t)p[1];
}

// Reads .git/index directly and compares each tracked file's cached mtime+size to the real file.
// Returns true if any tracked file has been modified since it was last staged.
// No git process is spawned. Staged-only changes (same size/mtime, different content) are not detected.
bool dirty(const std::wstring& root) {
    std::wstring index_path = root + L"\\.git\\index";
    HANDLE f = CreateFileW(index_path.c_str(), GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return false;

    LARGE_INTEGER sz;
    if (!GetFileSizeEx(f, &sz) || sz.QuadPart < 12 || sz.QuadPart > 64 * 1024 * 1024) {
        CloseHandle(f); return false;
    }
    std::vector<char> data((size_t)sz.QuadPart);
    DWORD n = 0;
    bool ok = ReadFile(f, data.data(), (DWORD)sz.QuadPart, &n, NULL) && n == (DWORD)sz.QuadPart;
    CloseHandle(f);
    if (!ok) return false;

    const char* d = data.data();
    if (memcmp(d, "DIRC", 4) != 0) return false;
    uint32_t ver   = be32(d + 4);
    uint32_t count = be32(d + 8);
    if (ver < 2 || ver > 3) return false; // v4 uses path compression; skip

    size_t pos = 12;
    for (uint32_t i = 0; i < count; i++) {
        if (pos + 62 > data.size()) break;

        const char* e       = d + pos;
        uint32_t mtime_s    = be32(e + 8);
        uint32_t cached_sz  = be32(e + 36);
        uint16_t flags      = be16(e + 60);
        bool extended       = (ver >= 3) && (flags & 0x4000);
        size_t name_off     = pos + 62 + (extended ? 2 : 0);

        size_t name_end = name_off;
        while (name_end < data.size() && d[name_end] != '\0') name_end++;

        size_t hdr   = 62 + (extended ? 2 : 0);
        size_t nlen  = name_end - name_off;
        size_t entry = hdr + nlen + 1;
        pos += (entry + 7) & ~(size_t)7;

        int wlen = MultiByteToWideChar(CP_UTF8, 0, d + name_off, (int)nlen, NULL, 0);
        if (wlen <= 0) continue;
        std::wstring rel(wlen, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, d + name_off, (int)nlen, &rel[0], wlen);
        for (auto& c : rel) if (c == L'/') c = L'\\';

        WIN32_FILE_ATTRIBUTE_DATA attr;
        if (!GetFileAttributesExW((root + L"\\" + rel).c_str(), GetFileExInfoStandard, &attr))
            return true; // deleted tracked file → dirty

        uint64_t cur_sz = ((uint64_t)attr.nFileSizeHigh << 32) | attr.nFileSizeLow;
        if (cur_sz != cached_sz) return true;

        ULARGE_INTEGER ft = { attr.ftLastWriteTime.dwLowDateTime, attr.ftLastWriteTime.dwHighDateTime };
        if (ft.QuadPart >= 116444736000000000ULL) {
            uint32_t cur_s = (uint32_t)((ft.QuadPart - 116444736000000000ULL) / 10000000ULL);
            if (cur_s != mtime_s) return true;
        }
    }
    return false;
}
