# Img Tool Plan

## Goal

Add a built-in `img` command that can display real images directly in supported terminals.

Main target:
- Windows Terminal with SIXEL support

Secondary target:
- fallback gracefully in terminals that do not support SIXEL

## Current Opinion

Best first version:
- add a new `img` command
- keep existing `cat` image behavior unchanged for now
- reuse the current decode + fit logic from `src/image.h`
- add a SIXEL output path for Windows Terminal
- fall back to the existing block-based renderer when SIXEL is not available

Why this seems best:
- low risk to existing `cat`
- easiest way to compare old vs new rendering
- Zcmd already has image loading and terminal-fit logic
- Windows Terminal now supports SIXEL, so this is a real feature path, not a hack

## Why Not Replace `cat` Immediately

Reasons to keep `cat` unchanged at first:
- `cat` already has established behavior
- terminal support is uneven across hosts
- fallback and capability detection should be proven first
- `img` gives a clearer user-facing command for image display

If the new path works well later, we can decide whether:
- `cat` should delegate to `img`
- or `cat` should keep the current ANSI block renderer

## Known Technical Basis

Current image implementation:
- `src/image.h`
- `cat_image(...)` already decodes images with `stb_image`
- current renderer uses 2x2 block quantization with truecolor ANSI

Useful existing pieces:
- image decode
- terminal size query
- aspect-ratio fitting
- direct output to `out_h`

Missing piece:
- SIXEL encoder / writer

## Proposed First Implementation

### Command

Add a new built-in:
- `img <path>`

Possible future flags:
- `img --fit <path>`
- `img --width N <path>`
- `img --no-fallback <path>`

But first version should stay minimal:
- just `img <path>`

### Behavior

Preferred behavior:
1. load image
2. fit it to terminal size
3. if SIXEL is supported, render with SIXEL
4. otherwise fall back to current block renderer

Alternative stricter behavior:
- if SIXEL is unavailable, print a message and exit

Current preference:
- fallback is better for first version

## Architecture Suggestion

Keep this split:

- `src/image.h`
  - shared decode / fit helpers
  - existing block renderer

- new module, likely `src/img.h`
  - `img_cmd(...)`
  - SIXEL-specific rendering path
  - host/capability checks

Possible helper split:
- `img_load(...)`
- `img_fit(...)`
- `img_render_blocks(...)`
- `img_render_sixel(...)`

This keeps the output strategies separate while reusing image preparation.

## Capability Detection Ideas

This part needs care.

Possible approaches:

1. optimistic Windows Terminal path
- detect likely Windows Terminal session through environment variables
- if detected, try SIXEL
- otherwise use fallback

2. explicit flag
- require something like `img --sixel`
- simplest and safest
- less convenient

3. hybrid
- try host detection first
- keep an override later if needed

Current opinion:
- start with a simple Windows Terminal detection heuristic
- still keep fallback

Possible things to inspect later:
- `WT_SESSION`
- terminal-identifying env vars
- whether a better Windows host capability signal exists

## SIXEL Encoder Scope

There are two paths:

### Simple internal encoder

Pros:
- no new dependency
- fully native to project
- easiest to ship

Cons:
- more implementation work
- palette/compression quality may be basic at first

### External tool integration

Use a tool like:
- `img2sixel`
- `chafa`

Pros:
- faster to prototype
- mature encoding quality

Cons:
- adds external dependency expectations
- conflicts with current project preference to avoid extra dependencies

Current opinion:
- internal encoder is the right long-term fit
- but researching external output first could help verify expected SIXEL behavior and escape format

## Research Questions

Things worth researching in a future session:

1. What is the smallest useful SIXEL encoder we can write ourselves?
- palette size
- row encoding
- run-length usage
- image quality tradeoffs

2. What terminals we should officially support?
- Windows Terminal
- conhost
- VSCode integrated terminal
- SSH / remote sessions

3. What is the correct host detection strategy?
- environment variables
- terminal responses
- opt-in flag

4. Should Zcmd keep truecolor block rendering as permanent fallback?
- likely yes

5. Should `img` support animation later?
- probably not first
- static images first

6. Should `play` / `video` ever use SIXEL frames?
- possible in theory
- probably too heavy for first pass

## Suggested Research Steps

Recommended order:

1. confirm Windows Terminal SIXEL behavior with a known-good encoder
- example: `img2sixel`
- validate sizing, scrolling, and cursor behavior

2. inspect escape output from a known-good SIXEL tool
- understand structure of emitted stream

3. implement a very small internal prototype
- fixed palette
- simple row output
- no fancy compression first

4. compare:
- image quality
- speed
- terminal behavior

5. only then integrate into a real `img` command

## Risks

Main risks:
- terminal support inconsistency
- performance on large images
- poor palette/quantization quality in a naive encoder
- scrolling/cursor placement weirdness after image output

Things to test carefully:
- prompt restoration after image output
- resize behavior
- large image scaling
- narrow terminal widths
- fallback path correctness

## Recommendation For Next Session

Best next session goal:
- do research + prototype only
- do not fully integrate into `cat`
- aim for a minimal `img` command proof of concept

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
