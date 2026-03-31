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

## Releases
- Build the release through `build.bat` from the repo root.
- A successful build produces `zcmd.exe` and bumps `version.txt` to the release patch version.
- Commit the release version change on a branch first, then merge to `master`.
- Push `master` before creating the GitHub release so the release tag points at the intended commit.
- Create and push the tag explicitly before calling `gh release create`.
- Use release tags in `v0.0.X` format and release titles in `zcmd v0.0.X` format.
- Always attach `zcmd.exe` as the GitHub release asset.
- Safe release flow:
  `build.bat`
  `git add version.txt`
  `git commit -m "Release v0.0.X"`
  `git checkout master`
  `git merge <release-branch>`
  `git push origin master`
  `git tag v0.0.X`
  `git push origin v0.0.X`
  `gh release create v0.0.X .\zcmd.exe --verify-tag --title "zcmd v0.0.X" --notes "Release v0.0.X"`
- If the asset needs to be replaced on an existing release, use:
  `gh release upload v0.0.X .\zcmd.exe --clobber`
- Do not rely on `gh release create` to auto-create the tag from local-only commits; without a pushed tag it can publish from the current remote `master` tip instead.

## Notes
- `build.bat` already kills running `zcmd.exe` before building.
- Tab completion should stay focused on file/folder navigation; PATH executable completion is postponed by design.
