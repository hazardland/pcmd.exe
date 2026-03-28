// MODULE: json
// Purpose : pretty-print JSON with syntax colors
// Exports : json_fmt()
// Depends : common.h

static int json_fmt(const std::string& path) {
    std::ifstream f(path);
    if (!f) { err("Cannot open: " + path + "\r\n"); return 1; }
    std::string src((std::istreambuf_iterator<char>(f)), {});

    std::string res;
    res.reserve(src.size() * 2);
    int indent = 0;
    bool expect_key = false;
    std::vector<bool> in_obj;

    auto nl = [&]() {
        res += "\r\n";
        for (int i = 0; i < indent; i++) res += "  ";
    };

    size_t i = 0;
    while (i < src.size()) {
        char c = src[i];
        if (isspace((unsigned char)c)) { i++; continue; }

        if (c == '{' || c == '[') {
            bool obj = (c == '{');
            res += GRAY; res += c; res += RESET;
            in_obj.push_back(obj);
            expect_key = obj;
            indent++;
            // peek ahead — skip whitespace, check if empty
            size_t j = i + 1;
            while (j < src.size() && isspace((unsigned char)src[j])) j++;
            if (j < src.size() && src[j] != (obj ? '}' : ']')) nl();
            i++;

        } else if (c == '}' || c == ']') {
            indent--;
            nl();
            res += GRAY; res += c; res += RESET;
            if (!in_obj.empty()) in_obj.pop_back();
            expect_key = false;
            i++;

        } else if (c == ',') {
            res += GRAY ","; res += RESET;
            nl();
            if (!in_obj.empty() && in_obj.back()) expect_key = true;
            i++;

        } else if (c == ':') {
            res += GRAY ": "; res += RESET;
            expect_key = false;
            i++;

        } else if (c == '"') {
            // scan to closing quote
            size_t j = i + 1;
            while (j < src.size() && src[j] != '"') {
                if (src[j] == '\\') j++;
                j++;
            }
            std::string str = src.substr(i, j - i + 1);
            res += expect_key ? BLUE : YELLOW;
            res += str;
            res += RESET;
            i = j + 1;

        } else {
            // number, bool, null
            size_t j = i;
            while (j < src.size() && src[j] != ',' && src[j] != '}' &&
                   src[j] != ']' && !isspace((unsigned char)src[j])) j++;
            res += GREEN;
            res += src.substr(i, j - i);
            res += RESET;
            i = j;
        }
    }
    res += "\r\n";
    out(res);
    return 0;
}
