/**
 * Compile the single generated stylesheet used by every window.
 *
 * The hand-written CSS is gone: all rules below are Tailwind component
 * utilities (@apply) plus the dynamic theme variables that theme.js writes at
 * runtime. The input file is temporary; only src/tailwind.css is kept.
 */

const fs = require("fs");
const os = require("os");
const path = require("path");
const { execFileSync } = require("child_process");

const root = path.resolve(__dirname, "..");
const input = path.join(os.tmpdir(), "kotoba-tailwind-input.css");
const output = path.join(root, "src", "tailwind.css");

const css = `
@tailwind base;
@tailwind components;
@tailwind utilities;

@layer components {
  .window-body { @apply overflow-hidden antialiased; font-family:var(--font-body); color:var(--text-body); background-color:var(--bg); background-image:var(--bg-image); background-repeat:no-repeat; background-size:100% 100%; }
  .card { background:var(--card); border:var(--border-width) solid var(--card-border); }
  .heading { text-transform:var(--heading-transform,uppercase); }
  .screen { @apply absolute inset-0 flex items-center justify-center; }
  .romaji-text { @apply italic; font-family:var(--font-body); color:var(--text-muted); letter-spacing:.02em; }

  .quiz-card { @apply flex min-w-[420px] max-w-[min(720px,calc(100vw_-_100px))] flex-col items-center px-[60px] py-[44px]; }
  .brand { @apply mb-[10px] font-bold tracking-[.12em]; font-family:var(--font-body); font-size:calc(14px*var(--scale)); color:var(--accent-soft); }
  .word { @apply mb-1 break-words text-center font-bold leading-[1.12]; font-family:var(--font-display); font-size:calc(56px*var(--scale)); color:var(--text-primary); }
  .reading { @apply mb-[2px] min-h-[1.2em] text-center; font-family:var(--font-body); font-size:calc(26px*var(--scale)); color:var(--accent-soft); }
  .romaji-hint { @apply mb-[6px] min-h-[1.2em] text-center tracking-[.03em]; font-size:calc(15px*var(--scale)); color:var(--text-muted); }
  .hint { @apply mb-4 text-center; font-size:calc(18px*var(--scale)); color:var(--text-muted); }
  .answer-input { @apply h-[58px] w-[440px] max-w-full rounded-none px-[14px] text-center outline-none; font-family:var(--font-body); font-size:calc(28px*var(--scale)); color:var(--text-primary); background:var(--entry-bg); border:var(--input-border-width) solid var(--accent); caret-color:var(--accent-soft); }
  .feedback { @apply my-[6px] min-h-[26px] text-center; font-size:calc(18px*var(--scale)); color:var(--error); }

  .btn { @apply h-10 cursor-pointer whitespace-nowrap rounded-none px-[18px] disabled:cursor-default disabled:opacity-55; font-family:var(--font-body); font-size:calc(15px*var(--scale)); color:var(--secondary-text); background:var(--secondary-bg); border:var(--border-width) solid var(--secondary-border); }
  .btn:hover { background:var(--secondary-hover); }
  .btn:disabled:hover { background:var(--secondary-bg); }
  .btn-outline,.btn-ghost { color:var(--text-muted); background:transparent; border-color:var(--card-border); }
  .btn-outline:hover,.btn-ghost:hover { background:var(--chip-hover); }
  .btn-small { @apply h-[42px] px-[18px] font-bold; font-size:calc(15px*var(--scale)); }
  .btn-primary { @apply font-bold; color:var(--primary-text); background:var(--primary-bg); border-color:var(--primary-bg); }
  .btn-primary:hover { background:var(--primary-hover); border-color:var(--primary-hover); }

  .info-card { @apply flex w-[min(980px,calc(100vw_-_128px))] max-h-[calc(100vh_-_88px)] flex-col px-[64px] pb-[24px] pt-[44px]; }
  .info-scroll { @apply min-h-0 overflow-x-hidden overflow-y-auto pr-[14px]; }
  .info-header { @apply flex items-start justify-between gap-6; }
  .info-title { @apply min-w-0; }
  .info-word { @apply break-words font-bold leading-[1.1]; font-family:var(--font-display); font-size:calc(54px*var(--scale)); color:var(--text-primary); }
  .info-romaji { @apply mt-[6px] min-h-[1.3em] break-words tracking-[.03em]; font-family:var(--font-body); font-size:calc(18px*var(--scale)); color:var(--accent-soft); }
  .header-actions { @apply flex shrink-0 items-center gap-[10px] pt-3; }
  .info-meta { @apply mt-1 flex min-h-[1.2em] flex-wrap items-baseline gap-[18px]; }
  .meta-pos { @apply font-bold; font-size:calc(15px*var(--scale)); color:var(--accent); }
  .meta-text { font-size:calc(18px*var(--scale)); color:var(--text-muted); }
  .divider { @apply my-4 min-h-px; height:var(--border-width); background:var(--card-border); }
  .section-title { @apply mb-[6px] mt-[18px] font-bold; font-size:calc(13px*var(--scale)); color:var(--accent-soft); }
  .def-jp { @apply mb-1 leading-[1.45]; font-size:calc(22px*var(--scale)); color:var(--text-body); }
  .def-romaji { @apply mb-[6px] mt-[-1px] leading-[1.4]; font-size:calc(15px*var(--scale)); }
  .def-vi { @apply mb-2 font-bold leading-[1.45]; font-size:calc(22px*var(--scale)); color:var(--text-primary); }
  .example-row { @apply mb-[2px] flex items-start gap-2; }
  .example-jp { @apply flex-1 leading-[1.45]; font-size:calc(20px*var(--scale)); color:var(--text-body); }
  .example-romaji { @apply mb-1 ml-7 mt-[-2px] leading-[1.4]; font-size:calc(14px*var(--scale)); }
  .example-vi { @apply mb-2 ml-7 leading-[1.45]; font-size:calc(18px*var(--scale)); color:var(--text-muted); }
  .speak-btn { @apply mt-[3px] h-[26px] shrink-0 px-[14px] font-bold; font-size:calc(11px*var(--scale)); }
  .chips-row { @apply mt-[6px] flex flex-wrap items-center gap-[6px]; }
  .chips-label { @apply mr-[2px]; font-size:calc(16px*var(--scale)); color:var(--text-muted); }
  .chip { @apply inline-block px-2 py-[2px] font-bold; font-size:calc(14px*var(--scale)); color:var(--accent-soft); background:var(--chip-bg); }
  .chip.error { color:var(--error); }
  .chip.muted { @apply font-normal; color:var(--text-muted); }
  .chip-romaji { @apply font-normal italic; color:var(--text-muted); }
  .info-footer { @apply shrink-0 pt-1; }
  .info-footer .divider { @apply my-[13px]; }
  .enter-exit { @apply text-center font-bold; font-size:calc(26px*var(--scale)); color:var(--accent-soft); }

  .settings-body { @apply overflow-hidden; font-family:var(--font-body); color:var(--text-body); background:var(--bg); }
  .settings-card { @apply flex h-screen w-screen flex-col; background:var(--card); border:var(--border-width) solid var(--card-border); }
  .settings-scroll { @apply min-h-0 flex-1 overflow-x-hidden overflow-y-auto px-6 pb-1 pt-6; }
  .settings-title { @apply mb-[2px] mt-0 font-bold tracking-[.1em]; font-family:var(--font-body); font-size:calc(14px*var(--scale)); color:var(--accent-soft); }
  .settings-path { @apply mb-1 break-all; font-size:calc(11px*var(--scale)); color:var(--text-muted); }
  .settings-note { @apply mb-[18px] leading-[1.35]; font-size:calc(11px*var(--scale)); color:var(--text-muted); }
  .field-label { @apply mt-[2px] block font-bold; font-size:calc(14px*var(--scale)); color:var(--text-body); }
  .field-hint { @apply mb-4 mt-1 leading-[1.35]; font-size:calc(12px*var(--scale)); color:var(--text-muted); }
  .text-input { @apply mt-2 h-[42px] w-full rounded-none px-3 outline-none; font-family:var(--font-body); font-size:calc(18px*var(--scale)); color:var(--text-primary); background:var(--entry-bg); border:var(--input-border-width) solid var(--accent); }
  .text-input:focus,.ignore-box:focus { border-color:var(--accent-hover); }
  .slider-header { @apply mt-1 flex items-baseline justify-between; }
  .volume-value { @apply font-bold; font-size:calc(14px*var(--scale)); color:var(--accent-soft); }
  .range { @apply my-2 h-[22px] w-full cursor-pointer; accent-color:var(--accent); background:transparent; }
  .switch-row { @apply my-[8px_0_2px] flex cursor-pointer items-center gap-[10px]; font-size:calc(16px*var(--scale)); color:var(--text-body); }
  .switch-input { @apply pointer-events-none absolute h-px w-px opacity-0; }
  .switch { @apply relative h-6 w-11 shrink-0; background:var(--chip-bg); border:var(--border-width) solid var(--secondary-border); }
  .switch::after { content:""; @apply absolute left-[2px] top-[2px] h-4 w-4 transition-[left] duration-150; background:var(--accent); }
  .switch-input:checked + .switch { background:var(--accent-soft); }
  .switch-input:checked + .switch::after { left:22px; }
  .switch-text { @apply leading-[1.35]; }
  .ignore-box { @apply my-2 block h-[210px] w-full resize-none rounded-none px-3 py-[10px] outline-none; font-family:var(--font-body); font-size:calc(16px*var(--scale)); line-height:1.4; color:var(--text-primary); background:var(--entry-bg); border:var(--input-border-width) solid var(--card-border); }
  .settings-footer { @apply shrink-0 px-7 pb-6 pt-2; }
  .status { @apply min-h-[20px] whitespace-pre-wrap leading-[1.35]; font-size:calc(14px*var(--scale)); color:var(--error); }
  .settings-actions { @apply mt-3 flex items-center justify-between gap-4; }
  .settings-actions .btn { @apply h-12 px-5; }
  .settings-actions .btn-ghost { @apply w-[140px]; color:var(--text-body); background:transparent; border-color:var(--card-border); }
  .settings-actions .btn-ghost:hover { background:var(--chip-hover); }
  .settings-actions .btn-primary { @apply w-[180px] font-bold; }
  .settings-actions .btn-primary:hover { background:var(--primary-hover); border-color:var(--primary-hover); }

  .info-scroll::-webkit-scrollbar,.settings-scroll::-webkit-scrollbar { width:12px; }
  .info-scroll::-webkit-scrollbar-track,.settings-scroll::-webkit-scrollbar-track { background:transparent; }
  .info-scroll::-webkit-scrollbar-thumb,.settings-scroll::-webkit-scrollbar-thumb { background:var(--scrollbar-color); border:3px solid var(--card); }
  .info-scroll::-webkit-scrollbar-thumb:hover,.settings-scroll::-webkit-scrollbar-thumb:hover { background:var(--scrollbar-hover); }

  .overlay-body { @apply h-full w-full overflow-hidden; background:transparent; font-family:var(--font-body); }
  .overlay-card { @apply relative h-[40px] w-[140px] overflow-hidden rounded-none; background:var(--card); border:1px solid var(--card-border); }
  .overlay-label { @apply absolute left-[9px] top-[4px] whitespace-nowrap font-bold leading-[1.2] tracking-[.04em]; font-size:8px; color:var(--text-muted); }
  .overlay-time { @apply absolute left-[9px] top-[14px] whitespace-nowrap font-bold leading-[1.2] tabular-nums; font-size:15px; color:var(--text-primary); }
  .overlay-clock { @apply absolute right-[9px] top-[13px]; color:var(--accent-soft); }

  @media (max-width:720px) {
    .quiz-card { @apply px-[26px] py-8; }
    .answer-input { @apply w-full; }
    .info-card { @apply w-[calc(100vw_-_40px)] px-6 pb-[18px] pt-7; }
  }
}
`;

fs.writeFileSync(input, css, "utf8");
try {
  execFileSync(
    process.execPath,
    [
      require.resolve("tailwindcss/lib/cli"),
      "-c",
      path.join(root, "tailwind.config.js"),
      "-i",
      input,
      "-o",
      output,
      "--minify",
    ],
    { cwd: root, stdio: "inherit" },
  );
} finally {
  fs.rmSync(input, { force: true });
}
