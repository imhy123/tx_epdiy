# tx_epdiy

An e-paper photo frame based on ESP32-S3 + the [epdiy](https://github.com/vroland/epdiy)
open-source e-ink driver. Planned features: clock, calendar, photo display, and to-do list.

> 中文版见 [README_cn.md](README_cn.md).

## Build

### Using `idf.py`

```shell
source ${HOME}/.espressif/tools/activate_idf_v6.0.1.sh

idf.py fullclean
idf.py all
idf.py flash
```

## Development Notes

### epdiy

#### Fonts

The epdiy directory (`managed_components/epdiy/scripts/`) ships a font-conversion
script that turns a TTF font into an epdiy `EpdFont` header. See
[Fonts, Images, Waveforms](https://epdiy.readthedocs.io/en/latest/filegen.html).

```shell
./fontconvert.py FiraSans 96 /tmp/FiraSans-Regular.ttf > ../../../main/font/firasans_96.h

# with uv
uv run --with freetype-py python ./fontconvert.py FiraSans 96 /tmp/FiraSans-Regular.ttf > ../../../main/font/firasans_96.h
```

After generating, remember to manually rename the trailing `FiraSans` variable in
the header to `FiraSans_96`.

### Kconfig

**What is Kconfig?**

Kconfig is the build-configuration system that originated in the Linux kernel;
ESP-IDF adopts it directly. The text-menu interface opened by `idf.py menuconfig`
is driven by Kconfig files. Each component can ship a `Kconfig` file declaring its
configuration options; the user's choices are written to `sdkconfig`, and the code
reads them through `CONFIG_XXX` macros.

**`Kconfig.projbuild` vs `Kconfig`**

Menu entries from a regular `Kconfig` file are nested under the component's own
submenu; the contents of `Kconfig.projbuild` are promoted to the top level of
menuconfig. So options defined in `main/Kconfig.projbuild` show up directly in the
top-level menu instead of being buried inside a component submenu.

**What is `orsource`?**

`orsource` is Kconfig's "optional source" directive:

```
source "path/to/file"   # error if the file does not exist
orsource "path/to/file" # silently skipped if the file does not exist
```

For example:

```kconfig
orsource "$IDF_PATH/examples/common_components/env_caps/$IDF_TARGET/Kconfig.env_caps"
```

This pulls in the GPIO-range constants (`ENV_GPIO_RANGE_MIN`,
`ENV_GPIO_OUT_RANGE_MAX`) for the current target chip (`$IDF_TARGET`, e.g.
`esp32s3`), so a later `config XXX_GPIO` can constrain the valid GPIO range.
`orsource` is used because not every chip has this file — if it is missing, it is
simply skipped.

**How are Kconfig options used in C source?**

Take a `bool` option as an example:

```kconfig
config BLINK_LED_GPIO
    bool "GPIO"
config BLINK_LED_STRIP
    bool "LED strip"
```

When a `bool` option is selected the macro **exists** (value `1`); when not
selected the macro **does not exist**, so `#ifdef` is the idiomatic check:

```c
#ifdef CONFIG_BLINK_LED_STRIP
    // user selected "LED strip" in menuconfig
#elif CONFIG_BLINK_LED_GPIO
    // user selected "GPIO" in menuconfig
#else
    #error "unsupported LED type"
#endif
```

This is a `choice` (a single-select group): the two bools are mutually exclusive,
so an `#ifdef / #elif` chain handles every branch, with a trailing `#error` as a
fallback.

Comparison with other types:

| Type     | Usage |
|----------|-------|
| `bool`   | `#ifdef CONFIG_XXX` — test whether the macro exists |
| `int`    | `CONFIG_BLINK_PERIOD` — read the value directly |
| `string` | `CONFIG_XXX` — read the string directly |
