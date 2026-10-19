# 工具链与工程配置

本文件只讲**平台相关**的工程配置：EIDE、Keil MDK（AC6）、GCC。
工程结构与并发约束见 `SKILL.md`，移植点见 [porting.md](./porting.md)。

配置前先确认平台契约（[platform-profile.md](./platform-profile.md)），尤其是：

- 工程来源决定工具链：Keil / MDK-ARM → AC6；CubeMX Makefile 或已有 `.ld` → GCC；
- 启动文件语法必须与工具链一致；
- 链接脚本 / scatter 文件必须与芯片真实内存一致。

**工具链必须沿用工程现状。** 从 Keil 导入的工程改成 GCC，等于同时换掉启动文件、
链接脚本和 HAL 编译选项；用户还要继续用 Keil 维护时不要这样做。只有用户明确要求
切换工具链时才换，并且启动文件、链接脚本、验证方式要一起换。

## 1. EIDE 工程配置

EIDE 工程目录固定为 `<project>/EIDE`，`app/`、`bsp/`、`port/` 与厂商生成的
`Core/`、`Drivers/` 平级。所有配置都在 `<project>/EIDE/.eide/eide.yml` 里，
**直接改这个文件**，不要只靠 UI 点击。

### 1.1 工具链与工程来源

| 工程来源（判断依据） | `toolchain` | 启动文件 | 链接脚本 |
| --- | --- | --- | --- |
| Keil MDK / CubeMX 生成的 `MDK-ARM/*.uvprojx` | `AC6`（Keil armclang） | `../MDK-ARM/startup_*.s`（ARM 语法） | `useCustomScatterFile: false`，EIDE 按 `storageLayout` 生成 `.sct` |
| CubeMX 生成 Makefile，或工程里已有 `*.ld` | `GCC`（arm-none-eabi-gcc） | CMSIS `Source/Templates/gcc/startup_*.s` | 厂商 `.ld`，`useCustomScatterFile: true` |

`storageLayout` 按芯片真实内存填写（例：STM32F407ZG 为
`IRAM 0x20000000 + 0x20000`、`IROM 0x08000000 + 0x100000`），不要照抄模板里的近似值。

### 1.2 源目录与排除

```yaml
srcDirs:
  - ../app
  - ../bsp
  - ../port
  - ../non-os-mcu-cpp-utility                 # 仓库根目录：header-only，只为了让头文件出现在工程树
targets:
  Debug:
    excludeList:
      - ../non-os-mcu-cpp-utility/examples    # 必须排除，否则范例的 .cpp 会被一起编译
```

- 仓库根目录可以放进 `srcDirs`，但**必须同时用 `excludeList` 排除 `examples/**`**；
  不想这样做就只保留在 `incList`。
- `Core/`、`Drivers/`、`startup_*.s` 用 `virtualFolder` 引用，不要复制。
- `port/` 里只有 `mcu_cmsis.h`，加进 `srcDirs` 只是方便查看，不会有编译产物。

### 1.3 include 路径与宏

```yaml
targets:
  Debug:
    cppPreprocessAttrs:
      defineList:
        - PY32F002Bx5
        - USE_HAL_DRIVER
      incList:
        - ../Core/INC
        - ../Drivers/CMSIS/Include
        - ../Drivers/PY32F002B_HAL_Driver/Inc
        - ../Drivers/CMSIS/Device/PY32F0xx/Include
        - ../app
        - ../bsp
        - ../non-os-mcu-cpp-utility
        - ../port
```

`../port` 不能漏：`irq_lock.hpp` 通过它找到 `mcu_cmsis.h`。

### 1.4 编译与链接选项（AC6 / Keil armclang）

| 字段 | 值 | 原因 |
| --- | --- | --- |
| `toolchain` | `AC6` | 与 MDK-ARM 工程保持一致 |
| `toolchainConfigMap.AC6.cpuType` / `floatingPointHardware` | `Cortex-M4` / `single` | 按器件填写 |
| `c/cpp-compiler.language-c` | `c99` | 与 CubeMX 生成时保持一致 |
| `c/cpp-compiler.language-cpp` | `c++17` | `timers.hpp` 使用 inline 静态成员 |
| `c/cpp-compiler.CXX_FLAGS` | `-fno-exceptions -fno-rtti` | 裸机不引入异常与 RTTI |
| `c/cpp-compiler.one-elf-section-per-function` | `true` | 配合链接期裁剪 |
| `c/cpp-compiler.warnings` | `ac5-like-warnings` | 容忍厂商 HAL 的告警 |
| `linker.misc-controls` | `--diag_suppress=L6329` | 抑制 armlink 的 RWX 段告警 |
| `useCustomScatterFile` | `false` | 由 EIDE 按 `storageLayout` 生成 `.sct` |

**不要**把 GCC 专用参数（`--specs=nosys.specs`、`-Wl,...`、`-Wno-register`）或 GCC 的
配置键（`global.no-exceptions`、`global.misc-control` 等）套到 AC6 上；反过来，
AC6 的 `CXX_FLAGS` / `misc-controls` 也不要写进 GCC 工程——两套参数互不识别。

### 1.5 编译与链接选项（GCC）

| 字段 | 值 | 原因 |
| --- | --- | --- |
| `toolchain` | `GCC` | 仅用于 GCC 工程 |
| `c/cpp-compiler.language-c` | `c11` | 厂商 HAL 源码 |
| `c/cpp-compiler.language-cpp` | `c++17` | `timers.hpp` 使用 inline 静态成员 |
| `global.no-exceptions` / `global.no-rtti` | `true` | 裸机不引入异常与 RTTI |
| `global.one-elf-section-per-function/-data` | `true` | 配合链接期裁剪 |
| `global.misc-control` | `--specs=nosys.specs --specs=nano.specs` | 无系统调用、精简 libc |
| `global.LD_FLAGS` | `-Wl,--no-warn-rwx-segments` | 消除 binutils ≥ 2.39 的 RWX 段告警 |
| `linker.remove-unused-input-sections` | `true` | `--gc-sections` |
| `useCustomScatterFile` / `scatterFilePath` | `true` / 厂商 `.ld` | 由 MCU Studio 生成，不要手写 |

GCC 的 `no-exceptions` / `no-rtti` / `one-elf-section-*` 属于 `global` 组，
放在 `c/cpp-compiler` 下会被 EIDE 忽略。

`uploadConfigMap` 与 `.vscode/tasks.json` 里的 ELF 路径必须与 EIDE 输出名
（`build/<target>/<name>.elf`）一致。

## 2. 完整 eide.yml 模板

EIDE 工程目录固定为 `<project>/EIDE`，`.eide/eide.yml` 的关键字段如下（路径相对 `EIDE/`；范例位于 `examples/py32f002b_start_kit`，所以仓库根目录是 `../../..`，普通工程通常是 `../non-os-mcu-cpp-utility`）。

> **先选对工具链**：Keil / CubeMX 导入的 `MDK-ARM` 工程用 AC6 + EIDE 生成的 `.sct`；GCC 工程才用下面的 GCC 模板 + 厂商 `.ld`。不要把两者混用。

```yaml
version: "4.1"
name: <project>
type: ARM
deviceName: PY32F002Bx5
srcDirs:
  - ../app
  - ../bsp
  - ../port
  - ../non-os-mcu-cpp-utility
virtualFolder:            # Core / Drivers / startup 用虚拟目录引用，不复制
  name: <virtual_root>
  files: []
  folders:
    - name: Application
      files: []
      folders:
        - name: EIDE
          files:
            - path: startup_py32f002bxx.s
          folders: []
        - name: User
          files: []
          folders:
            - name: Core
              files:
                - path: ../Core/SRC/main.c
                # … gpio.c / tim.c / usart.c / iwdg.c / rcc.c /
                #   py32f002b_it.c / py32f002b_hal_msp.c
              folders: []
    - name: Drivers
      files: []
      folders:
        - name: CMSIS
          files:
            - path: ../Core/SRC/system_py32f002b.c
          folders: []
        - name: PY32F002B_HAL_Driver
          files:
            # 只列用到的 HAL 模块，必须与 py32f002b_hal_conf.h 打开的模块一致
            - path: ../Drivers/PY32F002B_HAL_Driver/Src/py32f002b_hal.c
          folders: []
outDir: build
targets:
  Debug:
    cppPreprocessAttrs:
      defineList:
        - PY32F002Bx5
        - USE_HAL_DRIVER
      incList:
        - ../Core/INC
        - ../Drivers/CMSIS/Include
        - ../Drivers/PY32F002B_HAL_Driver/Inc
        - ../Drivers/CMSIS/Device/PY32F0xx/Include
        - ../app
        - ../bsp
        - ../non-os-mcu-cpp-utility
        - ../port
    excludeList:
      - ../non-os-mcu-cpp-utility/examples
    toolchain: GCC
    toolchainConfigMap:
      GCC:
        cpuType: Cortex-M0+
        floatingPointHardware: none
        options:
          c/cpp-compiler:
            CXX_FLAGS: -Wno-register
            language-c: c11
            language-cpp: c++17
            no-exceptions: true
            no-rtti: true
            one-elf-section-per-data: true
            one-elf-section-per-function: true
            optimization: level-2
            warnings: all-warnings
          global:
            $float-abi-type: softfp
            arm-thumb-mode: thumb
            misc-control: --specs=nosys.specs --specs=nano.specs
            output-debug-info: enable
          linker:
            $toolName: auto
            LD_FLAGS: -Wl,--no-warn-rwx-segments
            LIB_FLAGS: -lm
            output-format: elf
            remove-unused-input-sections: true
        scatterFilePath: PY32F002BF15U6.ld
        useCustomScatterFile: true
    uploader: pyOCD
```

如果工程是从 Keil MDK / CubeMX 的 `MDK-ARM` 工程导入的，**不要用上面的 GCC 模板**，改用 AC6：

```yaml
srcDirs:
  - ../app
  - ../bsp
  - ../port
  - ../non-os-mcu-cpp-utility
virtualFolder:
  name: <virtual_root>
  files: []
  folders:
    - name: Application
      files: []
      folders:
        - name: EIDE
          files:
            - path: ../MDK-ARM/startup_stm32f407xx.s     # ARM 语法启动文件
          folders: []
        # … Core / Drivers 同上
targets:
  Debug:
    cppPreprocessAttrs:
      defineList: [USE_HAL_DRIVER, STM32F407xx]
      incList:
        - ../Core/Inc
        - ../Drivers/STM32F4xx_HAL_Driver/Inc
        - ../Drivers/CMSIS/Device/ST/STM32F4xx/Include
        - ../Drivers/CMSIS/Include
        - ../app
        - ../bsp
        - ../non-os-mcu-cpp-utility
        - ../port
    excludeList:
      - ../non-os-mcu-cpp-utility/examples
    toolchain: AC6
    toolchainConfigMap:
      AC6:
        cpuType: Cortex-M4
        floatingPointHardware: single
        options:
          asm-compiler:
            $use: asm-auto
          c/cpp-compiler:
            CXX_FLAGS: -fno-exceptions -fno-rtti
            language-c: c99
            language-cpp: c++17
            one-elf-section-per-function: true
            optimization: level-2
            plain-char-is-signed: true
            warnings: ac5-like-warnings
          linker:
            misc-controls: --diag_suppress=L6329
            output-format: elf
        scatterFilePath: ""
        storageLayout:            # 按芯片真实内存填写
          RAM:
            - { id: 1, isChecked: true, mem: { size: "0x20000", startAddr: "0x20000000" }, noInit: false, tag: IRAM }
          ROM:
            - { id: 1, isChecked: true, isStartup: true, mem: { size: "0x100000", startAddr: "0x08000000" }, tag: IROM }
        useCustomScatterFile: false   # EIDE 按 storageLayout 生成 .sct
    uploader: STLink
```

检查点：

- **工具链与工程来源一致**：`MDK-ARM/*.uvprojx` 工程用 AC6 + `../MDK-ARM/startup_*.s` + `useCustomScatterFile: false`；GCC 工程用 GCC + CMSIS `Templates/gcc/startup_*.s` + 厂商 `.ld`（`useCustomScatterFile: true`）。
- AC6 的 `CXX_FLAGS` 只能写 armclang 参数（`-fno-exceptions -fno-rtti`），不要混入 `--specs=nosys.specs`、`-Wl,...` 等 GCC 选项。
- `storageLayout` 的 RAM/ROM 按芯片数据手册填写，构建后核对生成的 `.sct`。
- `srcDirs` 放进仓库根目录时，**必须**同步排除 `examples/**`；否则范例的 `.cpp` 会被编译进当前工程。
- `../port` 必须同时出现在 `incList`，否则 `irq_lock.hpp` 找不到 `mcu_cmsis.h`。
- `language-cpp` 必须是 `c++17`；`-fno-exceptions -fno-rtti` 必须打开。
- `py32f002b_hal_conf.h` / `stm32f4xx_hal_conf.h` 打开的每个 HAL 模块都要在 `virtualFolder` 的 `Src` 列表里有对应 `.c`。
- `.vscode/tasks.json` 与 `uploadConfigMap` 的 ELF 路径要和 `outDir/<target>/<name>.elf` 一致。

## 3. Keil MDK 工程配置（AC6）

从 Keil 导入的工程通常**同时保留 `MDK-ARM/*.uvprojx`**，改代码后用户还要用 Keil
打开，所以这份工程必须一起改好，不能只改 EIDE。

### 3.1 确认真的切到了 AC6

编译器选择写在 `<Target>` 下、`<TargetOption>` **之外**：

```xml
<Target>
  <TargetName>stm32f407_start_kit</TargetName>
  <ToolsetNumber>0x4</ToolsetNumber>
  <ToolsetName>ARM-ADS</ToolsetName>
  <pCCUsed>6140001::V6.14.1::ARMCLANG</pCCUsed>
  <uAC6>1</uAC6>
  <TargetOption>
    <TargetCommonOption>
      ...
```

- `<uAC6>1</uAC6>` 放进 `<TargetCommonOption>` **无效**——µVision 仍会用 AC5，
  随后因 `-fno-exceptions` 报 `C3900U: Unrecognized option`。
- `<pCCUsed>` 的版本号与本机 AC6 不一致时，µVision 首次打开会自动修正，不必手改。

### 3.2 把 C++ 源文件加进工程

`Groups` 里新建一个组，C++ 源文件用 `FileType 8`：

```xml
<Group>
  <GroupName>Application/App</GroupName>
  <Files>
    <File>
      <FileName>application.cpp</FileName>
      <FileType>8</FileType>
      <FilePath>../app/application.cpp</FilePath>
    </File>
    <File>
      <FileName>board.cpp</FileName>
      <FileType>8</FileType>
      <FilePath>../bsp/board.cpp</FilePath>
    </File>
  </Files>
</Group>
```

### 3.3 include 路径与编译选项

```xml
<Cads>
  <v6Lang>5</v6Lang>            <!-- C++ 标准：沿用 CubeMX 生成值；AC6 6.14 下 C++17 inline 静态成员可编译 -->
  <v6LangP>3</v6LangP>
  <v6Rtti>0</v6Rtti>
  <VariousControls>
    <MiscControls>-fno-exceptions</MiscControls>
    <Define>USE_HAL_DRIVER,STM32F407xx</Define>
    <IncludePath>../Core/Inc;../Drivers/STM32F4xx_HAL_Driver/Inc;../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy;../Drivers/CMSIS/Device/ST/STM32F4xx/Include;../Drivers/CMSIS/Include;../app;../bsp;../port;../non-os-mcu-cpp-utility</IncludePath>
  </VariousControls>
</Cads>
```

- `IncludePath` 必须追加 `../app;../bsp;../port;../non-os-mcu-cpp-utility`，
  否则 `board.hpp`、`queue.hpp`、`mcu_cmsis.h` 找不到。
- `<v6Rtti>0</v6Rtti>` 关闭 RTTI；异常用 `<MiscControls>-fno-exceptions</MiscControls>` 关闭。
- 工程里若还有 `-Wno-register`、`--specs=*`、`-Wl,*` 等 GCC 选项，一并删掉。

## 4. 验证

**用工程自己的工具链验证**，不要拿另一套工具链的编译结果代替。

### 4.1 EIDE / AC6

在 EIDE 里执行 build（或直接跑
`unify_builder -p build/<target>/builder.params --rebuild`），日志必须出现
`build successfully !`；同时核对 `build/<target>/<name>.sct` 的 RAM/ROM 与芯片一致。

手动单文件验证：

```powershell
armclang --target=arm-arm-none-eabi -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard `
  -c -std=c++17 -fno-exceptions -fno-rtti -DUSE_HAL_DRIVER -DSTM32F407xx `
  -I../Core/Inc -I../Drivers/CMSIS/Include -I../Drivers/STM32F4xx_HAL_Driver/Inc `
  -I../app -I../bsp -I../non-os-mcu-cpp-utility -I../port `
  ../app/application.cpp -o application.o
```

### 4.2 EIDE / GCC

按 `eide.yml` 的同一组参数手动编译一次：

```powershell
arm-none-eabi-g++ -std=c++17 -fno-exceptions -fno-rtti -Wno-register `
  -mcpu=cortex-m0plus -mthumb -DPY32F002Bx5 -DUSE_HAL_DRIVER `
  -I../Core/INC -I../Drivers/CMSIS/Include -I../Drivers/PY32F002B_HAL_Driver/Inc `
  -I../Drivers/CMSIS/Device/PY32F0xx/Include -I../app -I../bsp `
  -I../non-os-mcu-cpp-utility -I../port `
  -c ../app/application.cpp -o application.o
```

链接阶段再补
`-T<链接脚本> --specs=nosys.specs --specs=nano.specs -Wl,--gc-sections`。

### 4.3 Keil MDK

```powershell
& "C:\Keil_v5\UV4\UV4.exe" -b <project>\MDK-ARM\<name>.uvprojx -j0 -o build.log
```

构建日志必须满足：

- 首部 `C Compiler: Armclang.exe ...`（不是 `Armcc.exe`）；
- 结尾 `0 Error(s)`，且生成 `MDK-ARM/<name>/<name>.axf`。

`UV4.exe -b` 的退出码为 0 不一定代表构建成功（有 warning 时也是 0），
**以日志里的 `n Error(s)` 为准**。

## 5. 工具链相关反模式

| 反模式 | 正确做法 |
| --- | --- |
| 从 Keil / CubeMX(MDK-ARM) 导入的工程把 EIDE 工具链改成 GCC（或反向） | 沿用原工具链：AC6 用 `../MDK-ARM/startup_*.s` + EIDE 生成的 `.sct`，GCC 才用 CMSIS gcc 启动文件和 `.ld` |
| `<uAC6>1</uAC6>` 写进 `<TargetCommonOption>` | 必须写在 `<Target>` 下（与 `<pCCUsed>` 同级），否则 µVision 继续用 AC5 |
| 只改 EIDE 不管 `MDK-ARM/*.uvprojx` | Keil 工程也要加 `app/`、`bsp/`、`port/` 源文件与 include 路径，否则用户用 Keil 打开直接编译失败 |
| EIDE 把仓库根目录加进 `srcDirs` 却不排除 `examples/**` | 加 `excludeList`，或只把仓库根目录放进 `incList` |
| AC6 工程里出现 `--specs=*` / `-Wl,*` / `-Wno-register` | 删除 GCC 参数 |
| GCC 工程里出现 `CXX_FLAGS` / `misc-controls` 等 AC6 键 | 使用 `global.*` 配置组 |
