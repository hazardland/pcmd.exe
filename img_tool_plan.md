# Img / Vid Handoff

## Current Architecture

This is the intended structure and it should stay this way:

- `src/cat.h`
  - legacy image and video rendering
  - ANSI / truecolor block-cell path
  - should remain the old `cat` behavior
- `src/img.h`
  - new still-image command
  - SIXEL-based
- `src/vid.h`
  - new video command
  - SIXEL-based
- `src/sixel.h`
  - shared low-level SIXEL fitting / quantization / encoding
- `src/terminal.h`
  - shared terminal geometry assumptions used by both legacy `cat` and SIXEL tools

Boundary rule:

- do not move SIXEL logic into `cat`
- do not make `cat` silently switch to SIXEL
- `cat` must stay the legacy renderer
- `img` and `vid` are the explicit SIXEL tools

## What Exists Now

Implemented and working:

- `img <path>`
  - loads with `stb_image`
  - fits to terminal
  - renders through internal SIXEL encoder
  - good image quality after adaptive palette + dithering work
- `vid <path>`
  - probes video with `ffprobe`
  - decodes scaled RGB frames through `ffmpeg`
  - renders frames through a faster SIXEL profile
  - works, but frame rate is still the weak point
- `cat`
  - still renders images/videos the old way
  - now shares the same terminal-geometry assumptions as the new tools

Integrated shell UX:

- help text updated in `zcmd.cpp`
- built-in command recognition updated in `src/commands.h`
- path hinting exists for `img`
- `vid` command wiring exists

## Files To Read First

If another agent takes over, read these in this order:

1. `src/sixel.h`
2. `src/img.h`
3. `src/vid.h`
4. `src/terminal.h`
5. `src/cat.h`
6. `zcmd.cpp`
7. `src/commands.h`

Reason:

- `src/sixel.h` is the core technology and the most important file
- `src/img.h` shows the high-quality still-image path
- `src/vid.h` shows the current fast-video attempt
- `src/terminal.h` explains the current aspect correction model
- `src/cat.h` matters because we must not regress the legacy path

## Technology Behind It

Image side:

- `stb_image` for still-image decode
- internal nearest-neighbor resize in `src/img.h`
- internal SIXEL encoder in `src/sixel.h`

Video side:

- `ffprobe` to read dimensions / duration
- `ffmpeg` to decode and scale raw RGB frames
- internal SIXEL frame renderer reused from `src/sixel.h`

Rendering approach:

- adaptive palette quantization
- median-cut style palette generation from a 32x32x32 histogram
- nearest-color lookup cache
- Floyd-Steinberg dithering for `img`
- no dithering for `vid`
- SIXEL row-band encoder with reusable scratch buffers

## Terminal Geometry / Aspect Work

This was a major part of the session.

Important reality:

- Windows Terminal does not expose reliable app-side pixel introspection for this
- SIXEL aspect hints alone were not enough
- terminal `cell width` and `line height` settings visibly affect rendered aspect

Current model:

- temporary hardcoded terminal settings live in `src/terminal.h`
- `term_cell_width_setting()`
- `term_line_height_setting()`
- `term_cell_aspect()`
- `term_sixel_width_scale()`

Current calibrated values:

- `cell width = 0.60`
- `line height = 1.00`
- calibrated SIXEL baseline constant in `term_sixel_width_scale()` is `0.52`

Why this matters:

- `img` depends on it for correct SIXEL width fitting
- `cat_image` and `cat_video` now also depend on the shared geometry model
- later, when a real config/runtime value is added, both old and new paths should move together

## What Was Tried And Worked

For `img` quality:

- moved away from a fixed cube palette
- adaptive palette + dithering made a big visible difference
- current `img` quality is considered good and should be preserved

For `img` speed:

- reusable scratch buffers
- rolling-row dithering instead of a full float image buffer
- lazy nearest-color lookup cache
- reusable histogram scratch
- improved SIXEL band encoder that avoids repeated rescans

Result:

- `img` became much faster
- fullscreen still-image rendering is around acceptable territory
- do not casually replace the current `img` pipeline

For aspect correction:

- explicit SIXEL square-pixel header alone was not enough
- calibration against terminal `cell width` / `line height` worked
- tested combinations visually:
  - `cell width 0.6 / line height 1.0`
  - `cell width 1.0 / line height 1.0`
  - `cell width 0.6 / line height 0.6`

For `vid`:

- separate fast render options in `src/sixel.h`
- fewer colors than `img`
- no dithering
- palette reuse across nearby frames
- command is usable now

## What Was Tried And Did Not Help

Important failed or weak directions:

- forcing SIXEL support everywhere
  - VS Code terminal still did not render it
  - keep real support checks
- relying only on SIXEL header aspect metadata
  - Windows Terminal still looked stretched
- making `vid` quality much worse by dropping colors too far
  - quality got bad
  - frame rate did not really improve
- shrinking `vid` below available free space
  - this was rejected by the user as cheating
  - `vid` must use available free space

Conclusion:

- the remaining `vid` bottleneck is probably not just palette quality
- likely major costs are SIXEL encode size and terminal write throughput

## What Must Not Be Damaged

Very important:

- do not ruin the current `img` color quality
- do not remove adaptive palette + dithering from `img`
- do not break the current aspect calibration model
- do not make `cat` depend on SIXEL
- do not change `cat` semantics
- do not silently shrink `vid` below the available free space
- do not degrade `img` just to help `vid`

Practical rule:

- `img` is the high-quality still-image path
- `vid` should be a separate performance-tuned profile
- shared code in `src/sixel.h` must support both without forcing one compromise onto the other

## Current Weak Point

The main remaining problem is `vid` frame rate.

Current state:

- works
- quality is acceptable again
- frame rate is still too low

Most likely next honest step:

- add timing instrumentation for `vid` only
- measure:
  - ffmpeg decode
  - quantize
  - SIXEL encode
  - terminal write
  - sleep

Without this, further changes are too guessy.

## Recommended Next Task For Claude

Suggested objective:

- improve `vid` frame rate without degrading `img`

Recommended approach:

1. read the files listed above
2. understand that `img` quality is considered solved
3. add optional profiling/timing for `vid`
4. identify whether the real wall is:
   - quantization
   - SIXEL encoding
   - terminal output volume
5. optimize only the measured bottleneck

Possible future directions, but only after measurement:

- frame differencing / changed-region updates
- lower-overhead video palette strategy
- better write batching / output reduction
- reuse more state frame-to-frame

## Short Instruction To Future Agent

The project now has a working high-quality `img` pipeline and a working but still-slow `vid` pipeline.

Protect these invariants:

- `img` quality should stay visually close to current output
- `cat` stays legacy-rendered
- `vid` should not cheat by shrinking below free space
- terminal aspect correction lives in shared helpers and should stay shared

If you optimize next, optimize `vid` by measurement, not by making `img` worse.

Best success criteria:
- `img some.png` renders correctly in Windows Terminal
- prompt remains usable afterward
- unsupported terminals fall back cleanly

## Notes

Relevant current modules:
- `src/image.h`
- `src/cat.h`
- `zcmd.cpp`

Relevant external references to revisit:
- Windows Terminal discussion about SIXEL support
- Windows Terminal 1.22 release notes mentioning SIXEL support
