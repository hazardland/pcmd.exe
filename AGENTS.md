# Zcmd Rules

## Git
- Never commit directly to `master`; use a branch for work.
- Only commit when explicitly asked by the user.
- Never delete files without permission.

## Project Boundaries
- `zcmd.cpp` and the `src/*.h` modules are the C++ side of the project.

## Implementation
- Keep changes simple and focused; avoid over-engineering.
- No breaking changes without checking with the user first.
- Prefer Windows SDK / standard Windows APIs over extra dependencies.
- Prefer native Windows APIs and standard library solutions whenever possible.
- Avoid third-party library dependencies unless there is a clear need and the user agrees.
- Build through `build.bat`.

## Naming
- Use `snake_case` for variables and functions.
- Prefer single-word function and variable names where there is no ambiguity.
- Prefer single-word module / command names when possible.
- Use command-style names for user-facing tools when it fits the project style.
  Examples: `play`, `edit`, `explore`.
- Use PascalCase / camel-cased type names for structs and other types.
  Examples: `ExploreState`, `ExploreDialog`.
- Use all-caps enum types and enum values for explorer-style mode constants.
  Examples: `EXPLORER_SORT_MODE`, `EXPLORER_SORT_NAME`.
- Use a clear module prefix for related helpers.
  Example: `explore_toggle()`, `explore_draw()`, `explore_load_entries()`.

## UI And Paths
- Tool name is `Zcmd`.
- Prompt format is `[time]folder[branch*]> `.
- Display paths with `/` separators everywhere in UI output.
- Keep built-in command UX keyboard-friendly and consistent.

## Color Rules
- Keep the built-in color language consistent.
- Directories: blue `75`
- Executables / commands: green `114`
- Archives: red `203`
- Images: magenta `38;5;170`
- Audio/video: cyan `38;5;51`
- Hidden files: gray `240`
- Shared macros stay the source of truth: `GRAY`, `BLUE`, `RED`, `YELLOW`, `GREEN`, `RESET`

## Versioning
- Version format is `0.0.X`.
- Only the third number is auto-bumped during normal builds.
- `version.txt` stores the current patch version.
- `build.bat` increments on successful build and rolls back on failure.
- Major/minor version jumps are manual.

## Notes
- `build.bat` already kills running `zcmd.exe` before building.
- Tab completion should stay focused on file/folder navigation; PATH executable completion is postponed by design.
