# zcmd.dev Site Plan

## Goal
- Build a polished landing page for `zcmd.dev`.
- Show Zcmd through short autoplay demo videos instead of long explanations.
- Make visitors quickly understand what Zcmd is, why it exists, and how to install it.

## Core Message
- Zcmd is a fast, native Windows shell written in C++.
- It is a single executable with zero config.
- It brings Linux-shell-style quality-of-life features into a Windows-first workflow.

## Audience
- Windows terminal users.
- Developers who want a faster and more expressive shell experience.
- People who enjoy terminal tools with strong visual identity and built-in convenience.

## Site Style
- Modern, bold, and motion-driven.
- Dark terminal-inspired look.
- Strong use of Zcmd palette: blue, bright yellow, red, gray, magenta.
- Premium feel through scroll motion, parallax/sticky sections, and short looping videos.

## Site Structure
1. Hero
- Headline.
- Short pitch.
- Download button.
- GitHub button.
- Install commands near the top.

2. About
- Very short explanation of why Zcmd exists.
- Mention Windows-first focus.
- Mention PowerShell/cmd.exe motivation in a concise way.

3. Feature Stream
- Vertical scroll sections.
- Each section contains:
  - title
  - short description
  - autoplay looping muted video
- Alternating layout:
  - odd sections: text left, video right
  - even sections: video left, text right

4. Install
- Direct download.
- GitHub releases link.
- `winget` install command once available.

5. Footer
- GitHub link.
- Release link.
- Short tagline.

## Feature Sections
- Prompt and shell feel.
- History, ghost hints, and completion.
- Colorful built-ins and file listing.
- Linux-style `top`.
- Inline image and video rendering.
- MP3 playback with terminal visualizer.
- Single executable, zero config.

## Feature Data Model
```ts
type Feature = {
  id: string
  title: string
  body: string
  video: string
  poster?: string
}
```

## Hero Copy Direction
- Keep it short and direct.
- Mention:
  - custom Windows shell
  - C++
  - single executable
  - zero config
  - fast/native feel

## Install Copy
- Show direct download first.
- Reserve space for:
  - `winget install zcmd`
- If `winget` is not ready yet, show it as “coming soon” or hide it until published.

## Media Asset Plan
- Feature demos will be provided as short local video files.
- For the first version, keep demo videos in the repo as static site assets.
- Prefer short optimized MP4 clips.
- Target roughly `2-8 MB` per clip; around `5 MB` per feature is acceptable.
- If the media library grows too much later, move videos to external asset storage and keep the site code in the repo.

## Technical Plan
- Create website in `site/`.
- Use `Vue + Vite`.
- Build static output to `site/dist`.
- Deploy with Cloudflare Pages.
- Keep the site data-driven so feature sections are rendered from an array.

## Suggested Folder Structure
- `site/`
- `site/src/`
- `site/src/components/`
- `site/src/data/`
- `site/src/assets/`
- `site/public/`

## Components
- `HeroSection`
- `AboutSection`
- `FeatureSection`
- `InstallSection`
- `SiteHeader`
- `SiteFooter`

## Motion Plan
- Subtle parallax or sticky behavior during feature scroll.
- Fade/slide reveals for text.
- Videos autoplay, muted, loop, and play inline.
- Keep motion smooth and not excessive.

## First Implementation Phase
1. Scaffold Vue + Vite site in `site/`.
2. Create the main page layout.
3. Add a feature array with placeholder video URLs.
4. Build alternating feature sections.
5. Add install/download area.
6. Refine visual direction and motion.

## Homepage Feature Selection
- Keep the homepage selective; do not try to show every built-in.
- Target roughly `6-8` main feature sections with video.
- Add a smaller supporting section for extra built-ins later if needed.

## Homepage Feature Shortlist
- Prompt and shell feel.
- Smart history and ghost hints.
- Tab completion.
- Colorful file navigation and `ls`.
- Linux-style `top`.
- Inline image rendering.
- Inline video rendering.
- MP3 playback with terminal visualizer.
- Single executable and zero config.

## Secondary Features
- Aliases.
- Calculator.
- Clock and stopwatch.
- JSON pretty print.
- Notes.
- IP command.
- Editor.
- Clipboard helpers.
- These are better as a compact “more built-ins included” section than full hero sections.

## Later Improvements
- Real feature videos and posters.
- Auto-fetch latest release info from GitHub.
- Add docs or changelog pages if needed.
- Add `winget` install command when published.
