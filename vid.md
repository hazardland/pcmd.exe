# Vid Tool Notes

## Goal

Improve the `vid` command so video playback feels meaningfully smoother while preserving:

- terminal free-space fitting
- acceptable image quality
- the already-working `img` pipeline

This file is specifically about `vid` problems and current constraints.

## Current Status

`vid` exists and works:

- command: `vid <path>`
- reads dimensions / duration with `ffprobe`
- decodes scaled RGB frames with `ffmpeg`
- renders frames through the shared SIXEL renderer
- stops on `Esc` or `Ctrl+C`

Main problem:

- frame rate is still too low

Current user assessment:

- quality is acceptable again
- frame rate is weak

## Files To Read

Read these first:

1. `src/vid.h`
2. `src/sixel.h`
3. `src/terminal.h`
4. `src/img.h`
5. `src/cat.h`
6. `zcmd.cpp`

Why:

- `src/vid.h` is the tool entry point and current policy
- `src/sixel.h` is where most performance-sensitive work happens
- `src/img.h` matters because `img` quality must not be damaged
- `src/cat.h` matters because `cat` should remain separate and legacy-rendered

## Current Vid Design

In `src/vid.h`:

- `vid_pick_fps(...)`
- `vid_pick_colors(...)`
- `vid_pick_palette_interval(...)`
- `vid_cmd(...)`

Current render choices for `vid`:

- lower color count than `img`
- no dithering
- palette reuse across nearby frames
- renders at full available fitted size

Important:

- `vid` must use the available free space
- do not secretly shrink it below the fit size just to fake better FPS

That was tried and explicitly rejected.

## Shared Renderer Context

`src/sixel.h` now supports a render-options split:

- `img`
  - high-quality path
  - adaptive palette
  - dithering
  - `256` colors
- `vid`
  - faster path
  - fewer colors
  - no dithering
  - palette reuse interval

This split is intentional and should stay.

## What Was Tried

### Things that helped for still images

These improved the shared renderer and should generally stay:

- reusable scratch buffers
- rolling-row dithering
- lazy nearest-color lookup
- reusable histogram scratch
- more efficient SIXEL band encoding

These are already in `src/sixel.h`.

### Things tried for `vid`

Tried:

- lower color counts
- no dithering
- palette reuse across multiple frames
- different FPS heuristics

Result:

- some of this helped a little conceptually
- but the biggest aggressive quality cuts did not improve playback enough

### What did not help enough

These are important lessons:

- dropping video colors too far made quality visibly worse
- frame rate did not improve enough to justify the loss
- shrinking `vid` below free space was rejected as cheating

So:

- do not solve this by making `vid` tiny
- do not solve this by making `img` quality worse
- do not blindly crush palette quality

## Likely Bottlenecks

The remaining bottleneck is probably one or more of:

- `ffmpeg` decode / pipe throughput
- quantization cost per frame
- SIXEL encoding cost per frame
- terminal write volume / terminal render throughput

We do not yet have timing data to prove which is dominant.

This is the biggest missing piece.

## Recommended Next Step

Before more tuning:

- add optional profiling for `vid`

Suggested timings:

- video decode read
- quantize
- SIXEL encode
- terminal write
- frame sleep

Only after timing:

- optimize the hottest stage

Without that, changes are mostly guesswork.

## Things That Must Not Be Damaged

Do not damage:

- `img` visual quality
- current `img` adaptive palette + dithering path
- current terminal aspect correction model
- legacy `cat` behavior

Do not merge concerns:

- `img` and `vid` can share low-level SIXEL code
- but `vid` optimizations must not drag `img` quality down

## Terminal Geometry Constraints

Current aspect correction is shared through `src/terminal.h`.

Important helper functions:

- `term_cell_width_setting()`
- `term_line_height_setting()`
- `term_cell_aspect()`
- `term_sixel_width_scale()`

Current temporary calibrated values:

- cell width `0.60`
- line height `1.00`

Do not bypass this logic in `vid`.

`vid` should use the same general geometry model as `img`.

## What Success Looks Like

A good next improvement would be:

- noticeably smoother playback
- no obvious aspect regression
- no major quality collapse
- no cheating by shrinking under free-space fit
- no regressions to `img`

## Short Handoff Summary

`vid` works but is still slow.

The next agent should:

1. profile the frame pipeline
2. find the real bottleneck
3. optimize `vid` specifically
4. leave `img` quality alone
5. keep `vid` using available free space
