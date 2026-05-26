# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

---

## Project Goal / 项目目标

An e-paper photo frame based on ESP32-S3 + the [epdiy](https://github.com/vroland/epdiy) open-source e-ink driver. Planned features: clock, calendar, photo display, and to-do list.

基于 ESP32-S3 + [epdiy](https://github.com/vroland/epdiy) 开源墨水屏驱动的电子相框，计划功能：时钟、日历、照片展示、待办事项。

---

## Development Environment / 开发环境

- **IDF version / IDF 版本**: v5.5.4 — pinned; epdiy does not yet support IDF v6.x / 固定版本，epdiy 尚不支持 IDF v6.x
- **Target chip / 目标芯片**: ESP32-S3
- **IDF path / IDF 路径**: `~/.espressif/v5.5.4/esp-idf` (see `.vscode/settings.json`)
- **Toolchain / 工具链**: xtensa-esp-elf (`~/.espressif/tools/`)

---

## Common Commands / 常用命令

```bash
# Set target chip (first time or when switching) / 设置目标芯片（首次或切换时）
idf.py set-target esp32s3

# Build / 构建
idf.py build

# Flash and open serial monitor / 烧录并监视串口
idf.py -p PORT flash monitor

# Serial monitor only / 仅监视串口
idf.py -p PORT monitor

# Exit monitor: Ctrl-]  /  退出监视: Ctrl-]

# Project configuration menu / 项目配置菜单
idf.py menuconfig

# Clean build artifacts / 清理构建产物
idf.py fullclean
```

---

## Project Structure / 项目结构

```
main/
  epdiy_main.c          # Main logic (still contains blink demo code pending cleanup)
                        # 主业务逻辑（仍混有 blink demo 代码待清理）
  blink_example_main.c  # Original blink demo — to be deleted / 原始 blink demo，待删除
  Kconfig.projbuild     # "Example Configuration" menu in menuconfig (blink leftover)
                        # menuconfig 配置菜单（blink 遗留）
  idf_component.yml     # Component dependency declaration / 组件依赖声明
  firasans_*.h          # Generated font arrays — skip when reading code
                        # 工具生成的字体数组，读代码时可跳过
  img_*.h               # Generated image arrays — skip when reading code
                        # 工具生成的图片数组，读代码时可跳过
managed_components/
  epdiy/                # epdiy driver (from git, pinned to a specific commit)
                        # epdiy 驱动（来自 git，锁定到特定 commit）
  espressif__led_strip/ # LED strip driver (blink demo leftover dependency)
                        # LED strip 驱动（blink demo 遗留依赖）
sdkconfig               # Current build config (target: esp32s3)
sdkconfig.defaults.esp32s3  # ESP32-S3 config overrides / ESP32-S3 默认配置覆盖
dependencies.lock       # Locks epdiy commit hash for reproducible builds
                        # 锁定 epdiy commit hash，保证可重现构建
```

---

## epdiy Architecture & API / epdiy 架构与 API

epdiy exposes two API layers / epdiy 分为两层 API：

**Low-level API / 底层 API** (`epdiy.h`) — controls display timing and framebuffer directly / 直接操作显示时序和帧缓冲
- `epd_init(&board, &display, options)` — initialize driver / 初始化驱动
- `epd_poweron()` / `epd_poweroff()` — must wrap every refresh / 每次刷新前后必须调用
- `epd_clear()` — clear screen (call while powered on) / 清屏（需在 poweron 状态下调用）
- `epd_hl_update_screen(&hl, mode, temp)` — full-screen refresh / 全屏刷新
- `epd_hl_update_area(&hl, mode, temp, rect)` — partial refresh / 局部刷新
- `epd_deinit()` — release resources before deep sleep / 释放资源（进入 deep sleep 前调用）

**High-level API / 高层 API** (`EpdiyHighlevelState hl`) — manages framebuffer / 管理帧缓冲
- `epd_hl_init(waveform)` — init, returns state object / 初始化，返回高层状态对象
- `epd_hl_get_framebuffer(&hl)` — get framebuffer pointer (4bpp) / 获取帧缓冲指针（4bpp）
- `epd_hl_set_all_white(&hl)` — fill framebuffer with white / 将帧缓冲置全白

**Drawing functions / 绘图函数** (write to framebuffer, do not trigger a refresh / 操作帧缓冲，不立即刷屏)
- `epd_fill_rect(rect, color, fb)` / `epd_draw_rect(rect, color, fb)`
- `epd_fill_circle(x, y, r, color, fb)`
- `epd_draw_rotated_image(area, data, fb)` — draw 4bpp image / 绘制 4bpp 图片
- `epd_write_string(font, text, &x, &y, fb, &props)` — text rendering / 文字渲染

**Refresh modes / 刷新模式** (`EpdDrawMode`)
- `MODE_GC16` — high-quality 16-gray with flash / 高质量 16 灰度，有闪烁
- `MODE_GL16` — 16-gray without flash, preferred for partial updates / 无闪烁 16 灰度，局部更新首选
- `MODE_DU` — fast monochrome, for animations / 快速单色，用于动画/进度条

**Current display config / 当前屏幕配置** (`epdiy_main.c`):
```c
const EpdDisplay_t ES108FC = {
    .width = 1920, .height = 1080,
    .bus_width = 16, .bus_speed = 17,
    .default_waveform = &epdiy_ED047TC1,
};
#define DEMO_BOARD epd_board_v7  // ESP32-S3 uses v7 board / ESP32-S3 对应 v7 板
```

---

## Current Code Status / 当前代码状态

`epdiy_main.c` contains two mixed layers that need cleanup / `epdiy_main.c` 混有两套逻辑待清理：

1. **epdiy demo code / epdiy 演示代码**: `idf_setup()` + `idf_loop()` — the starting point for real features / 正式功能的起点
2. **Blink demo leftovers / blink demo 遗留**: LED blink logic in `app_main()`, plus `blink_example_main.c` and the blink menu in `Kconfig.projbuild` — to be deleted / 待删除

Next step: remove all blink code, rewrite `app_main()` to call `idf_setup()` and `idf_loop()`, then split out clock / calendar / photo / to-do modules.

下一步：删除所有 blink 相关代码，将 `app_main()` 改为调用 `idf_setup()` 和 `idf_loop()`，再逐步拆分各功能模块。

---

## Notes / 注意事项

- `firasans_*.h` and `img_*.h` are large tool-generated arrays — skip them when reading code / 是工具生成的大型数组，读代码时跳过
- Every screen refresh must be wrapped with `epd_poweron()` … `epd_poweroff()`; keep it off otherwise to save power / 每次刷新需用 poweron/poweroff 包裹，平时保持 poweroff 省电
- Framebuffer is 4bpp (nibble per pixel): `0x0` = black, `0xF` = white / 帧缓冲为 4bpp：`0x0` = 黑，`0xF` = 白
- Call `epd_deinit()` before entering deep sleep / 进入 deep sleep 前须调用 `epd_deinit()`
- `epd_ambient_temperature()` must be called after `epd_poweron()`; temperature affects waveform selection / 须在 poweron 后调用，温度影响波形选择
