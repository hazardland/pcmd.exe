# YT Plan

## Goals
- Add a small built-in YouTube downloader tool to Zcmd.
- Command shape:
  - `yt mp3 <url> [folder]`
  - `yt mp4 <url> [folder]`
- Keep Zcmd itself dependency-light.
- Rely on external tools already made for this job instead of reimplementing site logic.

## External Tools
- Require `yt-dlp` for both `yt mp3` and `yt mp4`.
- Require `ffmpeg` for both commands:
  - `yt mp3` needs conversion to MP3.
  - `yt mp4` may need merge/remux for best video+audio output.
- If either tool is missing, print a friendly install message using `winget`.

## Command Behavior

### `yt mp3`
- Download the best available YouTube audio source.
- Convert it to `.mp3`.
- Save into the current folder by default.
- If folder argument is `./`, also save into the current folder.

### `yt mp4`
- Download the best available video+audio result.
- Prefer MP4 output when possible.
- Save into the current folder by default.
- If folder argument is `./`, also save into the current folder.

## Path Rules
- Zcmd displays and accepts `/` separators everywhere.
- The optional folder argument should follow the same rule.
- Internally convert `/` to Windows paths only when calling Win32 APIs or child processes.

## UX Rules
- Keep help text short and obvious.
- Print the exact install command when dependency checks fail.
- Preserve child tool output so users can see downloader progress directly.
- Do not over-wrap or hide `yt-dlp` output in the first version.

## Autocomplete
- URL is treated as a normal text argument, not a path.
- Only the optional folder argument should use filesystem completion.
- Completion is position-based:
  - token 1: `yt`
  - token 2: `mp3` or `mp4`
  - token 3: URL
  - token 4: folder path
- Folder completion should be directories-only.
- `./` and nested folder paths like `./music/` should work.

## Parsing Rules
- Accept quoted URL and quoted folder arguments.
- First version can assume a simple shape:
  - one mode
  - one URL
  - optional one folder path
- Reject missing mode or missing URL with help text.

## Internal Shape
- Add a new module under `src/`, for example `src/yt.h`.
- Expose one command entry:
  - `int yt_cmd(const std::string& line);`
- Keep process spawning simple and local to this module.

## Suggested Commands

### MP3
```text
yt-dlp -x --audio-format mp3 --audio-quality 0 <url>
```

### MP4
```text
yt-dlp -f "bestvideo+bestaudio/best" --merge-output-format mp4 <url>
```

## Output Folder Shape
- Use `-P <folder>` for the target directory.
- If folder is missing or `./`, resolve to the current directory.

## Validation Checklist
- `yt mp3 <url>` downloads into current folder.
- `yt mp3 <url> ./` also downloads into current folder.
- `yt mp4 <url>` downloads into current folder.
- Missing `yt-dlp` prints install help.
- Missing `ffmpeg` prints install help.
- Invalid command shape prints help.
- Optional folder argument autocompletes as directories only.

## Implementation Order
1. Add the `yt` command module with dependency checks.
2. Implement command parsing for `mp3` and `mp4`.
3. Spawn `yt-dlp` with the needed flags and let it stream output.
4. Wire help text and command dispatch.
5. Add `which` built-in recognition.
6. Add optional folder autocomplete for the 4th token.
7. Build and test command parsing and completion.
