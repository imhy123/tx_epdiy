# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

---

## Project Goal / 项目目标

An e-paper photo frame based on ESP32-S3 + the [epdiy](https://github.com/vroland/epdiy) open-source e-ink driver. Planned features: clock, calendar, photo display, and to-do list.

基于 ESP32-S3 + [epdiy](https://github.com/vroland/epdiy) 开源墨水屏驱动的电子相框，计划功能：时钟、日历、照片展示、待办事项。

---

## Development Environment / 开发环境

- **IDF version / IDF 版本**: v6.0.1 — epdiy now supports IDF v6.x / epdiy 已支持 IDF v6.x（之前固定在 v5.5.4 是因为旧版 epdiy 不支持 v6）
- **Target chip / 目标芯片**: ESP32-S3 (N16R8: 16 MB flash, 8 MB Octal PSRAM)
- **Activate IDF in a shell / 在 shell 中激活 IDF**: `source /Users/heyong/.espressif/tools/activate_idf_v6.0.1.sh` (needed before using `idf.py` directly / 直接使用 `idf.py` 前需先执行)
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
  epdiy_main.c          # Main logic: idf_setup()/idf_loop() run the epdiy demo
                        # 主业务逻辑：idf_setup()/idf_loop() 跑 epdiy demo
  Kconfig.projbuild     # "Photo Frame Configuration" menu: e-paper display model
                        # menuconfig 配置菜单：墨水屏型号选择（默认 ES108FC）
  idf_component.yml     # Component dependency: epdiy fork (see note below)
                        # 组件依赖：epdiy fork（见下方说明）
  firasans_*.h          # Generated font arrays — skip when reading code
                        # 工具生成的字体数组，读代码时可跳过
  img_*.h               # Generated image arrays — skip when reading code
                        # 工具生成的图片数组，读代码时可跳过
partitions/
  esp32s3_n16r8.csv     # Partition table: 3 MB app + ~12.9 MB spiffs storage
                        # 分区表：3MB app + ~12.9MB spiffs 存储
managed_components/
  epdiy/                # epdiy driver — fetched from our fork (imhy123/epdiy,
                        # branch feature/disable_pca9555_tps65185)
                        # epdiy 驱动（来自我们的 fork）
sdkconfig.defaults          # Common defaults: flash size, partition table / 通用默认配置
sdkconfig.defaults.esp32s3  # S3-specific: octal PSRAM + 64 KB/64 B data cache
                            # S3 专属：八线 PSRAM + 64KB/64B 数据 cache
```

**epdiy fork / epdiy 分支**: this board has no PCA9555 IO expander and no
software-controlled TPS65185 (power via GPIO46, VCOM via potentiometer), and the
generic 17 MHz bus speed overruns it. The board-level fixes (gate PCA9555/TPS
behind `EPD_BOARD_V7_DISABLE_IO_EXPANDER`, drive GPIO46 power-enable) live in the
fork `https://github.com/imhy123/epdiy.git` @ `feature/disable_pca9555_tps65185`;
panel/bus_speed live in `epdiy_main.c`. The manifest-level `overrides:` field is
NOT supported by the bundled component manager, so `idf_component.yml` points the
`epdiy` dependency straight at the fork. After pushing a new fork commit, delete
`dependencies.lock` + `managed_components/epdiy` to force a re-fetch.
本板无 PCA9555、TPS65185 不受软件控制（GPIO46 控电源、电位器控 VCOM）；板级修改在 fork 里，
面板/总线速度在 epdiy_main.c。组件管理器不支持 overrides，故直接用 git 依赖指向 fork。

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

Blink demo code is fully removed. `epdiy_main.c` runs the epdiy demo: `app_main()`
calls `idf_setup()` then loops `idf_loop()` (loading bar → sample images → deep
sleep). The display now refreshes correctly on the custom V7 board.

blink 代码已全部移除。`epdiy_main.c` 现在跑 epdiy demo：`app_main()` 调用
`idf_setup()` 后循环 `idf_loop()`（加载条 → 示例图片 → 休眠），屏幕已能正常刷新。

Next step / 下一步: split out clock / calendar / photo / to-do modules — each module
draws into the framebuffer; the main loop wraps refreshes with
`epd_poweron()`/`epd_poweroff()`. / 逐步拆分 时钟/日历/照片/待办 模块。

Known issue / 已知问题: a faint black vertical line on the left edge (deferred —
likely `line_front_porch` edge timing). / 屏幕左边一条淡黑竖线（暂缓，疑似 line_front_porch 边缘时序）。

---

## Notes / 注意事项

- `firasans_*.h` and `img_*.h` are large tool-generated arrays — skip them when reading code / 是工具生成的大型数组，读代码时跳过
- Every screen refresh must be wrapped with `epd_poweron()` … `epd_poweroff()`; keep it off otherwise to save power / 每次刷新需用 poweron/poweroff 包裹，平时保持 poweroff 省电
- Framebuffer is 4bpp (nibble per pixel): `0x0` = black, `0xF` = white / 帧缓冲为 4bpp：`0x0` = 黑，`0xF` = 白
- Call `epd_deinit()` before entering deep sleep / 进入 deep sleep 前须调用 `epd_deinit()`
- `epd_ambient_temperature()` must be called after `epd_poweron()`; temperature affects waveform selection / 须在 poweron 后调用，温度影响波形选择
