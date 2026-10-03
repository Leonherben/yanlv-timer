# Yanlv-timer

<p align="center">
  <b>Minimalist, Lightweight & Reliable Windows Study & Break Floating Timer</b>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Platform-Windows%2010%2B-blue" alt="Platform">
  <img src="https://img.shields.io/badge/Language-C%2B%2B17-00599C" alt="Language">
  <img src="https://img.shields.io/badge/License-MIT-green" alt="License">
  <img src="https://img.shields.io/badge/Memory-%3C20MB-brightgreen" alt="Memory">
  <img src="https://img.shields.io/badge/Binary-%3C5MB-orange" alt="Binary Size">
</p>

---

## 📖 Introduction

**Yanlv-timer (言律时钟)** is a lightweight Windows desktop study clock crafted for deep focus and healthy eye-break habits. Inspired by Catime's clean, borderless floating digital display, Yanlv-timer is engineered for minimal resource consumption (<20MB RAM, ~0% CPU), extreme time-tracking reliability, and restorative 5-minute full-screen break experiences.

## ✨ Highlights

- 🕒 **Minimal Floating Countdown**: Smooth Direct2D rendering with subpixel antialiasing. Freely draggable, click to pause/resume, and hide/restore at will.
- 🏷️ **Category Management**: Organize study into distinct categories (e.g., "Paper Reading", "Coding", "Vocabulary", default "Uncategorized").
- 📊 **Robust Statistics & History**:
  - High-precision hardware monotonic clock (`QueryPerformanceCounter`), immune to system clock drifts and UI lags.
  - Tracks planned vs. actual effective duration (excluding pauses and downtime).
  - Modify record categories or delete invalid entries with instant data recalculation.
  - 15-second heartbeat crash-recovery: automatically recovers unclosed sessions as interrupted records upon relaunch.
- 🌿 **5-Minute Full-Screen Eye Break**:
  - Auto-break or prompt-before-break modes.
  - Displays soothing default backgrounds or imported local images/videos.
  - Subtle top-right countdown (`05:00` → `00:00`). Press `Esc` to minimize to floating clock while keeping the timer running.
  - Completely frees decoding and graphical resources upon break exit.
- 🪶 **Zero Bloatware**: Standalone executable, statically linked, zero external runtime prerequisites (no VC++ Redistributable needed). Built with native Win32, Direct2D, and embedded SQLite.

---

## 🛠️ Build & Compilation

### Prerequisites
- Windows 10 / 11 64-bit
- C++17 compatible compiler (MSVC or MinGW-w64)
- CMake 3.20+

### Build Steps

```bash
git clone https://github.com/Leonherben/Yanlv-timer.git
cd Yanlv-timer
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The output standalone binary will be located in `build/bin/YanlvTimer.exe`.

---

## 📄 License

This project is licensed under the [MIT License](LICENSE).
