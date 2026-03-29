# MP3 Plan

## Goals
- Add a small built-in MP3 player to Zcmd.
- Keep dependencies tiny and local.
- Avoid destabilizing prompt, input, hint, completion, and screen redraw behavior.

## Library Choice
- Use `minimp3.h` for MP3 decoding.
- Keep Windows audio output native via `waveOut` for the first version.
- Do not add large media frameworks.

## MVP Commands
- `mp3 <file>`: play one MP3 file.
- `mp3 pause`: pause playback.
- `mp3 resume`: resume playback.
- `mp3 stop`: stop playback and clear current track.
- `mp3 vol <0-100>`: set playback volume.
- `mp3 status`: print current file, state, time, and volume.

## Later Commands
- `mp3 <folder>`: collect `.mp3` files recursively, shuffle once, and play the queue.
- `mp3 next`: skip to the next queued track.
- `mp3 ui`: enter a temporary now-playing display mode with a one-line visualizer; any key exits the display mode while playback continues.

## Threading Model
- Playback should run on a separate worker thread.
- The worker thread should decode MP3 data and feed PCM buffers to the Windows audio device.
- The main Zcmd thread should remain responsible for prompt rendering, input, and built-in command execution.
- The worker thread must never write directly to the console.

## Why Separate Thread
- Zcmd's input loop is interactive and sensitive to redraw issues.
- Blocking playback work on the main thread would freeze the shell.
- A background worker lets playback continue while the prompt stays responsive.

## Safety Rules
- Keep all console output on the main thread.
- Share only small playback state across threads: file path, state, elapsed time, total time, volume, stop/pause flags.
- Protect shared state with a mutex or atomics.
- On shell exit, stop playback cleanly and join the worker thread.

## Integration Shape
- Add a small MP3 module under `src/` rather than growing `zcmd.cpp` further.
- Expose a compact command-style API, for example:
  - `int mp3_cmd(const std::string& line);`
  - `void mp3_shutdown();`
- Hook `mp3_shutdown()` into shell exit cleanup.

## Visualizer Note
- Do not implement a permanent overlay in the normal prompt.
- If a visualizer is added later, keep it as a temporary full-control display mode started by `mp3 ui`.
- The visualizer should render from main-thread UI code using playback stats gathered from the worker thread.

## Implementation Order
1. Decode one MP3 file with `minimp3`.
2. Play decoded PCM through `waveOut`.
3. Add `pause`, `resume`, `stop`, `vol`, and `status`.
4. Add clean shutdown on exit.
5. Add folder queue and shuffle later.
6. Add display mode / one-line visualizer after playback is stable.
