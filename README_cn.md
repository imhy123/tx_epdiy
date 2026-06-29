## 编译

### 使用`idf.py`

```shell
source ${HOME}/.espressif/tools/activate_idf_v6.0.1.sh

idf.py fullclean
idf.py all
idf.py flash
```

## 开发基础知识

### epdiy相关

#### 字体

epdiy目录(managed_components/epdiy/scripts/)下有字体转换脚本，可以把 TTF 字体转成 epdiy 的 EpdFont 格式头文件，参考文档：[Fonts, Images, Waveforms](https://epdiy.readthedocs.io/en/latest/filegen.html)。

```shell
./fontconvert.py FiraSans 96 /tmp/FiraSans-Regular.ttf > ../../../main/font/firasans_96.h

# with uv
uv run --with freetype-py python ./fontconvert.py FiraSans 96 /tmp/FiraSans-Regular.ttf > ../../../main/font/firasans_96.h
```

生成后记得手动修改头文件最后的`FiraSans`变量名为`FiraSans_96`。

### Kconfig

**Kconfig 是什么？**

Kconfig 是源自 Linux 内核的构建配置系统，ESP-IDF 直接沿用了它。`idf.py menuconfig` 打开的文本菜单界面就是由 Kconfig 文件驱动的。每个组件可以放一个 `Kconfig` 文件来声明自己的配置项，用户选择后写入 `sdkconfig`，代码里用 `CONFIG_XXX` 宏读取。

**`Kconfig.projbuild` vs `Kconfig`**

普通 `Kconfig` 文件的菜单项会嵌套在组件自己的子菜单下；`Kconfig.projbuild` 的内容会被提升到 menuconfig 的顶层，所以 `main/Kconfig.projbuild` 里定义的选项会直接出现在一级菜单里，而不是藏在某个组件子菜单里。

**`orsource` 是什么？**

`orsource` 是 Kconfig 的"可选 source"指令：

```
source "path/to/file"   # 文件不存在时报错
orsource "path/to/file" # 文件不存在时静默跳过
```

`Kconfig.projbuild` 第一行：

```kconfig
orsource "$IDF_PATH/examples/common_components/env_caps/$IDF_TARGET/Kconfig.env_caps"
```

作用是按当前目标芯片（`$IDF_TARGET`，如 `esp32s3`）引入对应的 GPIO 范围常量（`ENV_GPIO_RANGE_MIN`、`ENV_GPIO_OUT_RANGE_MAX`），供后续 `config BLINK_GPIO` 限定合法 GPIO 编号范围。用 `orsource` 是因为并非所有芯片都有这个文件，不存在时跳过即可。

**Kconfig 配置项在 C 源码里怎么用？**

以 `bool` 类型为例：

```kconfig
config BLINK_LED_GPIO
    bool "GPIO"
config BLINK_LED_STRIP
    bool "LED strip"
```

`bool` 类型被选中时宏**存在**（值为 `1`），未选中时宏**不存在**，惯用 `#ifdef` 判断：

```c
#ifdef CONFIG_BLINK_LED_STRIP
    // 用户在 menuconfig 里选了 "LED strip"
#elif CONFIG_BLINK_LED_GPIO
    // 用户在 menuconfig 里选了 "GPIO"
#else
    #error "unsupported LED type"
#endif
```

这是一个 `choice`（单选组），两个 bool 互斥，所以用 `#ifdef / #elif` 链处理所有分支，最后加 `#error` 兜底。

其他类型对比：

| 类型 | 用法 |
|------|------|
| `bool` | `#ifdef CONFIG_XXX` — 判断宏是否存在 |
| `int` | `CONFIG_BLINK_PERIOD` — 直接读取数值 |
| `string` | `CONFIG_XXX` — 直接读取字符串 |