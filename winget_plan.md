# Winget Plan

## Goals
- Let users install `zcmd` with `winget install zcmd`.
- Keep distribution simple: one `zcmd.exe` binary, no config, no extra runtime.
- Target Windows 11 only.
- Make `Zcmd` available as a command after install.
- Add a `Zcmd` profile to Windows Terminal during install.

## Release Shape
- Publish a versioned GitHub release asset named `zcmd.exe`.
- Keep the binary self-contained and stable across machines.
- Do not require a separate installer for the first release if Winget portable packaging is enough.
- If the portable package cannot create the Terminal profile cleanly, switch to a tiny installer package that still only drops `zcmd.exe` and the profile fragment.

## Expected Install Result
- `winget install zcmd` downloads the released `zcmd.exe`.
- The package places `zcmd.exe` in a stable installed location.
- The command is callable globally as `zcmd`.
- Windows Terminal shows a profile named `Zcmd`.

## Terminal Profile
- Use Windows Terminal JSON fragment extensions instead of editing the user's `settings.json` directly.
- Install a fragment file for `Zcmd` under the Windows Terminal fragments location.
- The profile should launch `zcmd.exe`.
- The profile should use the name `Zcmd`.
- Add icon later only if we ship one; first release can skip it.

## Packaging Options

### Option A: portable package
- Best if Winget portable install can place `zcmd.exe` and make it globally callable.
- Lowest maintenance and closest to the current single-file app shape.
- Need to confirm whether the package format can also place the Terminal fragment file cleanly.

### Option B: tiny installer package
- Use a very small installer only to copy `zcmd.exe` and the Terminal fragment.
- Better fit if Terminal profile installation needs explicit file placement logic.
- Still preserve the "single binary app" philosophy from the user's point of view.

## Recommended Direction
- Start with research/validation for portable packaging.
- Use portable packaging if it can satisfy both goals:
  - command available globally
  - Terminal profile installed automatically
- If not, use a minimal installer package and keep the payload tiny.

## Package Identity
- Package name should stay aligned everywhere:
  - executable: `zcmd.exe`
  - tool name: `Zcmd`
  - Winget package id: choose once and keep stable
- Release titles should continue to use `zcmd v0.0.X`.

## Files To Prepare
- `zcmd.exe` release asset
- Winget manifest files
- Windows Terminal fragment JSON
- Release notes with install and verify commands

## Terminal Fragment Draft
- Profile name: `Zcmd`
- Command line: full installed path to `zcmd.exe`
- Starting directory: user's home folder or default Terminal behavior
- Hidden: false
- Color scheme: default for now

## Validation Checklist
- `winget install zcmd` succeeds on clean Windows 11
- `zcmd` runs from a new PowerShell window
- Windows Terminal shows `Zcmd` without manual settings edits
- Launching the `Zcmd` profile starts the shell correctly
- `winget upgrade zcmd` replaces the binary cleanly
- `winget uninstall zcmd` removes the command and Terminal profile cleanly

## Risks
- Portable package support may not be enough for Terminal fragment installation.
- Global command availability may depend on install scope and Winget behavior.
- Terminal fragment paths may differ by install scope: user vs machine.
- A release asset URL or hash change will require manifest updates every version.

## Implementation Order
1. Confirm the exact Winget package type needed for command + Terminal profile support.
2. Draft the Windows Terminal fragment JSON for `Zcmd`.
3. Prepare release asset naming and versioned GitHub release flow.
4. Create the initial Winget manifest set.
5. Test install on Windows 11 from a clean environment.
6. Test upgrade and uninstall behavior.
7. Submit the package to `microsoft/winget-pkgs`.

## Nice Later Improvements
- Add a custom `Zcmd` icon for the Terminal profile.
- Add an installer option for machine-wide vs user-only install if needed.
- Add a `winget` section to `readme.md` after the packaging flow is proven.
