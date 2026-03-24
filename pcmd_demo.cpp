// Demo mode — compiled in only when built with -DDEMO (e.g. "build demo").
// All functions here are named demo_* so they can be removed without affecting anything else.
// This file is #included at the end of pcmd.cpp, so it has full access to all internals.

// Pause between demo actions to give the viewer time to read.
static void demo_pause(int ms) { Sleep(ms); }

// Print text character-by-character with a delay to simulate a human typing.
static void demo_type(const std::string& text, int delay_ms = 70) {
    for (char c : text) { out(std::string(1, c)); Sleep(delay_ms); }
}

// Print gray ghost hint after cursor (save/restore), then accept or erase.
static void demo_hint(const std::string& h, int hold_ms, bool accept) {
    out("\x1b[s" GRAY + h + RESET);
    Sleep(hold_ms);
    out("\x1b[u");
    if (accept) out(h);
    else { out(std::string(h.size(), ' ')); out("\x1b[u"); }
}

// Overwrite the current input line (prompt + buf) — used to simulate Tab / UP.
static void demo_overwrite(const std::string& prompt_str, const std::string& buf,
                            const std::string& hint_sfx = "") {
    out("\r\x1b[K" + prompt_str + buf);
    if (!hint_sfx.empty()) out(GRAY + hint_sfx + RESET "\x1b[u");
}

// Main demo entry point: seeds history, navigates pcmd-demo/, and runs scripted scenes.
// All filesystem navigation is real; Tab and UP key effects are visually simulated via demo_overwrite.
// Restores the original working directory when done.
void demo_run(editor& e) {
    // Seed history for reliable hint / UP-nav demonstrations
    for (const wchar_t* s : {L"ping 8.8.8.8", L"ping github.com", L"ping google.com"}) {
        std::wstring ws(s);
        e.hist.erase(std::remove(e.hist.begin(), e.hist.end(), ws), e.hist.end());
        e.hist.push_back(ws);
    }

    const std::string demo_root = "D:\\src\\pcmd-demo";
    std::string start_dir = cwd();
    bool elev = elevated();

    // Helper: print a fresh prompt reflecting current state
    // Branch is computed once (we stay in the same repo the whole demo)
    std::string br = branch();
    auto ppt = [&](int exit_code = 0) -> std::string {
        auto p = make_prompt(elev, cur_time(), folder(cwd()), br, false, exit_code);
        out(p.str);
        return p.str; // caller saves for demo_overwrite
    };

    // ── Scene 1: cd with parent-dir hinting ───────────────────────────────
    // Navigate into pcmd-demo/src so we can show "cd ../" parent hints
    SetCurrentDirectoryW(to_wide(demo_root + "\\src").c_str());

    out("\r\n"); auto p1 = ppt();
    demo_type("cd ../");
    demo_pause(350);
    // Hint: first alphabetical sibling dir = "archive/"
    out("\x1b[s");  // save cursor (after "cd ../")
    demo_hint("archive/", 900, false);  // show, then erase — we'll Tab through instead
    demo_pause(200);

    // Simulate Tab cycling through parent dirs
    demo_overwrite(p1, "cd ../archive/"); demo_pause(700);
    demo_overwrite(p1, "cd ../assets/");  demo_pause(700);
    demo_overwrite(p1, "cd ../dist/");    demo_pause(700);
    demo_overwrite(p1, "cd ../docs/");    demo_pause(700);
    // Accept docs — navigate there
    out("\r\n");
    SetCurrentDirectoryW(to_wide(demo_root + "\\docs").c_str());
    demo_pause(500);

    // Show we're in docs, cd back to root with cd -
    out("\r\n"); auto p2 = ppt();
    demo_type("cd -"); demo_pause(450);
    out("\r\n");
    SetCurrentDirectoryW(to_wide(demo_root + "\\src").c_str());
    demo_pause(400);

    // Show narrowing: typing "cd ../a" hints "archive/"
    out("\r\n"); auto p3 = ppt();
    demo_type("cd ../", 80); demo_pause(300);
    demo_type("a", 120);     demo_pause(300);
    demo_hint("rchive/", 1000, true);  // accept
    demo_pause(300); out("\r\n");
    SetCurrentDirectoryW(to_wide(demo_root + "\\archive").c_str());
    demo_pause(600);

    // ── Scene 2: history hinting and UP navigation ─────────────────────────
    SetCurrentDirectoryW(to_wide(demo_root).c_str());

    // Type "ping g" → hint "oogle.com"
    out("\r\n"); auto p4 = ppt();
    demo_type("ping g", 80); demo_pause(350);
    out("\x1b[s");  // save cursor position for UP sim
    demo_hint("oogle.com", 800, false);  // show hint, then erase for UP sim

    // Simulate UP: filtered nav, "ping g" stays in buf, hint cycles
    demo_overwrite(p4, "ping g", "ithub.com");  demo_pause(900);
    demo_overwrite(p4, "ping g", "oogle.com");  demo_pause(900);
    // Accept "ping google.com" via right-arrow (overwrite without gray)
    demo_overwrite(p4, "ping google.com"); demo_pause(400);
    out("\r\n");
    {
        ULONGLONG t0 = GetTickCount64();
        run("ping -n 3 google.com");
        ULONGLONG elapsed = GetTickCount64() - t0;
        if (elapsed >= 2000) {
            char tbuf[32];
            snprintf(tbuf, sizeof(tbuf), "%.1fs", elapsed / 1000.0);
            out(std::string(GRAY) + "[" + tbuf + "]" + RESET + "\r\n");
        }
    }
    demo_pause(1200);

    // ── Scene 3: ls with colors ────────────────────────────────────────────
    out("\r\n"); ppt();
    demo_type("ls"); demo_pause(450); out("\r\n");
    do_ls("");
    demo_pause(2000);

    // ls inside images subfolder to show only images (magenta)
    out("\r\n"); ppt();
    demo_type("ls assets/images"); demo_pause(450); out("\r\n");
    do_ls("assets/images");
    demo_pause(1800);

    // ── Scene 4: which ─────────────────────────────────────────────────────
    out("\r\n"); ppt();
    demo_type("which ls"); demo_pause(450); out("\r\n");
    out("ls: pcmd built-in\r\n");
    demo_pause(900);

    out("\r\n"); ppt();
    demo_type("which git"); demo_pause(450); out("\r\n");
    {
        char path_env[32768] = {};
        GetEnvironmentVariableA("PATH", path_env, sizeof(path_env));
        std::stringstream ps(path_env);
        std::string d; bool found = false;
        while (std::getline(ps, d, ';') && !found) {
            if (!d.empty() && d.back() != '\\') d += '\\';
            std::string full = d + "git.exe";
            if (GetFileAttributesA(full.c_str()) != INVALID_FILE_ATTRIBUTES) {
                std::replace(full.begin(), full.end(), '\\', '/');
                out(full + "\r\n"); found = true;
            }
        }
        if (!found) out("git: not found\r\n");
    }
    demo_pause(1200);

    // ── Scene 5: pwd ───────────────────────────────────────────────────────
    out("\r\n"); ppt();
    demo_type("pwd"); demo_pause(450); out("\r\n");
    out(cwd() + "\r\n");
    demo_pause(1000);

    // ── Scene 6: help ──────────────────────────────────────────────────────
    out("\r\n"); ppt();
    demo_type("help"); demo_pause(450); out("\r\n");
    out(GREEN "pwd" RESET "    Print current directory\r\n"
        GREEN "ls" RESET "     Colored directory listing\r\n"
        GREEN "cd" RESET "     Change directory  ~ home  - prev dir\r\n"
        GREEN "which" RESET "  Locate a command in PATH or identify built-ins\r\n"
        GREEN "help" RESET "   Show this help\r\n"
        GREEN "exit" RESET "   Exit pcmd\r\n"
        GRAY "All other commands are passed to cmd.exe" RESET "\r\n");
    demo_pause(2000);

    // ── Done ───────────────────────────────────────────────────────────────
    out(GRAY "\r\n── demo complete ──" RESET "\r\n\r\n");
    SetCurrentDirectoryW(to_wide(start_dir).c_str());
}
