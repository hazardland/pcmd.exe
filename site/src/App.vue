<script setup>
import { onBeforeUnmount, onMounted, ref } from 'vue'

const heroVideo = '/videos/header.mp4'
const heroRatio = '16 / 9'
const heroTopTrim = '5.8%'
const heroChromeHeight = '42px'
const headerCondensed = ref(false)

const updateHeaderState = () => {
  headerCondensed.value = window.scrollY > 28
}

onMounted(() => {
  updateHeaderState()
  window.addEventListener('scroll', updateHeaderState, { passive: true })
})

onBeforeUnmount(() => {
  window.removeEventListener('scroll', updateHeaderState)
})

const signals = [
  'single executable',
  'portable',
  'zero config',
  'performance first',
  'windows native',
  'built-in tools',
]

const spotlights = [
  {
    cmd: 'ls',
    title: 'Readable file navigation',
    body: 'Color, sorting, hidden files, and grep-friendly output make ordinary file listing feel worth doing in the shell.',
    shot: 'ls.png',
    accent: '#74ade8',
  },
  {
    cmd: 'cat',
    title: 'A terminal that can actually show things',
    body: 'Syntax-highlighted text, inline image rendering, and even video playback when ffmpeg is available.',
    shot: 'cat.png',
    accent: '#e5c07b',
  },
  {
    cmd: 'explore',
    title: 'Two-panel file work without leaving the shell',
    body: 'Browse, filter, select, copy, move, recycle, and delete inside a full-screen explorer built into Zcmd.',
    shot: 'explore.png',
    accent: '#98c379',
  },
  {
    cmd: 'top',
    title: 'Instant process watching and task killing',
    body: 'Opens fast, updates fast, and kills fast. It feels more like a reflex than a detour.',
    shot: 'top.png',
    accent: '#b477cf',
  },
  {
    cmd: 'resmon',
    title: 'Live system graphs in the terminal',
    body: 'CPU, GPU, RAM, battery, and network history without bouncing out to separate Windows tools.',
    shot: 'resmon.png',
    accent: '#56b6c2',
  },
  {
    cmd: 'play',
    title: 'An MP3 player that belongs in Zcmd',
    body: 'Play a file or a folder, shuffle, skip, pause, tweak volume, and keep a lightweight now-playing UI right in the shell.',
    shot: 'zcmd_play.gif',
    accent: '#ffd885',
  },
]

const extras = [
  'edit',
  'view',
  'yt',
  'json',
  'clip',
  'calc',
  'ip',
  'clock',
  'stopw',
  'notes',
]
</script>

<template>
  <div class="site-shell" id="top">
    <div class="atmosphere">
      <div class="halo halo-blue"></div>
      <div class="halo halo-gold"></div>
      <div class="shape-ribbon"></div>
      <div class="grid-fade"></div>
    </div>

    <header class="site-header" :class="{ 'is-condensed': headerCondensed }">
      <div class="site-header-shell">
        <nav class="topbar" aria-label="Primary">
          <a class="brand-lockup" href="#top" aria-label="Zcmd homepage">
            <span class="brand-mark">
              <span class="brand-bracket">[</span>zcmd<span class="brand-bracket">]</span><span class="brand-chevron">&gt;</span>
            </span>
            <span class="brand-copy">
              <strong>Zcmd</strong>
              <span>Portable Windows shell</span>
            </span>
          </a>

          <div class="topbar-nav">
            <a href="#tools">Tools</a>
            <a href="#install">Install</a>
            <a href="#extras">More</a>
          </div>

          <div class="topbar-actions">
            <span class="topbar-status">single executable / zero config</span>
            <a
              class="topbar-cta"
              href="https://github.com/hazardland/zcmd/releases/latest"
              target="_blank"
              rel="noreferrer"
            >
              Download
            </a>
          </div>
        </nav>
      </div>
    </header>

    <main>
      <section class="hero">
        <div class="hero-intro">
          <div class="hero-copy">
            <p class="eyebrow">Windows shell replacement</p>
            <h1>
              <span>Zcmd is a portable, single executable,</span>
              <span>zero config shell replacement for</span>
              <span><code>cmd.exe</code> and PowerShell.</span>
            </h1>
          </div>

          <div class="hero-side">
            <p class="lede">
              It keeps the Windows command world intact, fixes the parts that feel neglected, and adds built-in tools worth keeping open.
            </p>

            <div class="signal-row">
              <span v-for="signal in signals" :key="signal">{{ signal }}</span>
            </div>

            <div class="hero-actions">
              <a class="button button-primary" href="https://github.com/hazardland/zcmd/releases/latest" target="_blank" rel="noreferrer">
                <span>Download Latest</span>
              </a>
              <a class="button button-secondary" href="https://github.com/hazardland/zcmd" target="_blank" rel="noreferrer">
                <svg class="link-icon" viewBox="0 0 24 24" aria-hidden="true">
                  <path d="M12 .5C5.65.5.5 5.65.5 12c0 5.08 3.29 9.39 7.86 10.91.58.11.79-.25.79-.56v-1.97c-3.2.7-3.88-1.35-3.88-1.35-.52-1.33-1.28-1.68-1.28-1.68-1.04-.72.08-.71.08-.71 1.15.08 1.75 1.18 1.75 1.18 1.03 1.75 2.69 1.25 3.34.95.1-.74.4-1.25.73-1.54-2.55-.29-5.24-1.28-5.24-5.68 0-1.25.45-2.27 1.18-3.07-.12-.29-.51-1.46.11-3.02 0 0 .97-.31 3.17 1.17.92-.26 1.9-.39 2.88-.39s1.96.13 2.88.39c2.2-1.48 3.17-1.17 3.17-1.17.62 1.56.23 2.73.12 3.02.73.8 1.18 1.82 1.18 3.07 0 4.41-2.69 5.38-5.26 5.66.41.35.77 1.04.77 2.1v3.11c0 .31.21.68.8.56A11.5 11.5 0 0 0 23.5 12C23.5 5.65 18.35.5 12 .5Z" />
                </svg>
                <span>View Repo</span>
              </a>
            </div>
          </div>
        </div>

        <div class="hero-stage">
          <div
            class="video-frame hero-video-shell"
            :style="{
              aspectRatio: heroRatio,
              '--video-top-trim': heroTopTrim,
              '--video-chrome-height': heroChromeHeight,
            }"
          >
            <div class="video-chrome">
              <div class="video-chrome-title">zcmd.exe</div>
              <div class="window-controls video-controls">
                <span aria-hidden="true"></span>
                <span aria-hidden="true"></span>
                <span aria-hidden="true"></span>
              </div>
            </div>
            <video
              class="feature-demo"
              :src="heroVideo"
              autoplay
              muted
              loop
              playsinline
              preload="metadata"
            ></video>
          </div>
        </div>
      </section>

      <section class="summary-strip">
        <div class="summary-card">
          <p class="eyebrow">Why people care</p>
          <h2>Put it somewhere and run it. Then keep it open because the shell itself gets better.</h2>
          <p>
            Fast prompt. Better history. Better completion. Better file navigation. And a set of built-in tools that makes Zcmd feel less like a wrapper and more like a workspace.
          </p>
        </div>
      </section>

      <section class="spotlights" id="tools">
        <div class="section-heading">
          <p class="eyebrow">Tool spotlights</p>
          <h2>The commands that prove the point fastest.</h2>
        </div>

        <article
          v-for="tool in spotlights"
          :key="tool.cmd"
          class="spotlight-card"
          :style="{ '--tool-accent': tool.accent }"
        >
          <div class="spotlight-copy">
            <p class="spotlight-kicker"><code>{{ tool.cmd }}</code></p>
            <h3>{{ tool.title }}</h3>
            <p>{{ tool.body }}</p>
          </div>

          <div class="spotlight-media">
            <div class="shot-frame">
              <div class="shot-topline">
                <span>{{ tool.cmd }}</span>
                <span>drop screenshot later</span>
              </div>
              <div class="shot-placeholder">
                <span>Screenshot placeholder</span>
                <code>{{ tool.shot }}</code>
              </div>
            </div>
          </div>
        </article>
      </section>

      <section class="install-strip" id="install">
        <div class="install-card">
          <p class="eyebrow">Install</p>
          <h2>Download the single executable, point your terminal at it, and you are in.</h2>

          <div class="install-list">
            <div class="install-row">
              <span class="install-label">1</span>
              <code>Download zcmd.exe from the latest GitHub release</code>
            </div>
            <div class="install-row">
              <span class="install-label">2</span>
              <code>Set Windows Terminal or VS Code to launch zcmd.exe</code>
            </div>
            <div class="install-row">
              <span class="install-label">3</span>
              <code>Optional: add ffmpeg and yt-dlp if you want media tools</code>
            </div>
          </div>
        </div>
      </section>

      <section class="more-tools" id="extras">
        <p class="eyebrow">More built-ins</p>
        <h2>There is plenty more in the box.</h2>
        <div class="tools-grid">
          <div v-for="tool in extras" :key="tool" class="tools-chip">
            <code>{{ tool }}</code>
          </div>
        </div>
      </section>
    </main>

    <footer class="site-footer">
      <p>Zcmd is a performance-first Windows shell replacement with the kind of built-in tools that make a terminal session feel unusually complete.</p>
      <div class="footer-links">
        <a href="https://github.com/hazardland/zcmd" target="_blank" rel="noreferrer">
          <svg class="link-icon" viewBox="0 0 24 24" aria-hidden="true">
            <path d="M12 .5C5.65.5.5 5.65.5 12c0 5.08 3.29 9.39 7.86 10.91.58.11.79-.25.79-.56v-1.97c-3.2.7-3.88-1.35-3.88-1.35-.52-1.33-1.28-1.68-1.28-1.68-1.04-.72.08-.71.08-.71 1.15.08 1.75 1.18 1.75 1.18 1.03 1.75 2.69 1.25 3.34.95.1-.74.4-1.25.73-1.54-2.55-.29-5.24-1.28-5.24-5.68 0-1.25.45-2.27 1.18-3.07-.12-.29-.51-1.46.11-3.02 0 0 .97-.31 3.17 1.17.92-.26 1.9-.39 2.88-.39s1.96.13 2.88.39c2.2-1.48 3.17-1.17 3.17-1.17.62 1.56.23 2.73.12 3.02.73.8 1.18 1.82 1.18 3.07 0 4.41-2.69 5.38-5.26 5.66.41.35.77 1.04.77 2.1v3.11c0 .31.21.68.8.56A11.5 11.5 0 0 0 23.5 12C23.5 5.65 18.35.5 12 .5Z" />
          </svg>
          <span>GitHub</span>
        </a>
        <a href="https://github.com/hazardland/zcmd/releases/latest" target="_blank" rel="noreferrer">Latest release</a>
      </div>
    </footer>
  </div>
</template>
