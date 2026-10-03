# 言律时钟 (Yanlv-timer)

<p align="center">
  <b>极简 · 轻量 · 可靠的 Windows 桌面学习与休息悬浮时钟</b>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Platform-Windows%2010%2B-blue" alt="Platform">
  <img src="https://img.shields.io/badge/Language-C%2B%2B17-00599C" alt="Language">
  <img src="https://img.shields.io/badge/License-MIT-green" alt="License">
  <img src="https://img.shields.io/badge/Memory-%3C20MB-brightgreen" alt="Memory">
  <img src="https://img.shields.io/badge/Binary-%3C5MB-orange" alt="Binary Size">
</p>

---

## 📖 项目简介

**言律时钟 (Yanlv-timer)** 是一款专为高效专注与科学护眼打造的 Windows 桌面学习计时工具。灵感源自 Catime 的极简无边框悬浮体验，致力于在超低资源占用（常驻内存 < 20MB、CPU 接近 0%）的前提下，提供极其可靠的专注时间统计与 5 分钟沉浸式全屏休息。

## ✨ 核心特性

- 🕒 **极简悬浮倒计时**：无缝悬浮桌面，亚像素平滑渲染，支持自由拖动、单击暂停/继续、随心隐藏与重新呼出。
- 🏷️ **多维度学习分类**：支持自定义学习类别（如“读论文”、“英语背诵”、“专业课”，内置“未分类”），每次专注前一键选择。
- 📊 **严谨可靠的数据统计**：
  - 基于高精度硬件单调时钟（`QueryPerformanceCounter`），无惧系统时间调整或界面卡顿。
  - 精确记录计划时长、实际有效时长（排除暂停时间与关机时间）。
  - 支持历史记录类别修正、无效记录删除与统计数据秒级联动同步。
  - 15 秒心跳容灾机制，遇到异常断电、系统崩溃或睡眠挂起，下次启动自动恢复为中断记录，绝不丢数据。
- 🌿 **5 分钟全屏护眼休息**：
  - 支持“自动休息”与“提醒休息”两种模式。
  - 学习结束后全屏展示静谧舒缓壁纸或本地导入的视频。
  - 右上角轻量倒计时（05:00 → 00:00），按 `Esc` 可平滑切回桌面悬浮倒计时，按需跳过。
  - 休息结束立即释放全部图形解码资源，杜绝内存膨胀。
- 🪶 **极致轻量**：单文件绿色运行，零外部运行时依赖（免安装 VC++ 运行库），纯原生 Win32 + Direct2D + 内嵌 SQLite。

---

## 🛠️ 构建与编译

### 前置要求
- Windows 10 / 11 64位系统
- 支持 C++17 的编译工具链（MSVC 或 MinGW-w64）
- CMake 3.20+

### 本地编译步骤

```bash
# 1. 克隆代码仓库
git clone https://github.com/Leonherben/Yanlv-timer.git
cd Yanlv-timer

# 2. 生成构建文件
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 3. 编译发布包
cmake --build build --config Release
```

编译产物将生成在 `build/bin/YanlvTimer.exe`（单文件可执行程序，体积 < 5MB）。

---

## 📁 目录结构

```text
Yanlv-timer/
├── .github/workflows/          # GitHub Actions 自动化 CI 构建脚本
├── assets/                     # 内置图标与默认舒缓壁纸资源
├── src/
│   ├── app/                    # 应用程序入口、单实例互斥、系统托盘
│   ├── core/                   # 高精度单调计时器、状态机、系统电源事件监听
│   ├── db/                     # 内嵌 SQLite 引擎与数据仓储层
│   └── ui/                     # Direct2D 悬浮窗、启动气泡、全屏休息窗口与管理面板
├── CMakeLists.txt              # CMake 项目构建定义
├── LICENSE                     # MIT 开源许可证
└── README.md                   # 项目中文文档
```

---

## 📄 开源许可证

本项目采用 [MIT License](LICENSE) 开源许可证。
