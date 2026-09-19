# 智锁卫士 — 基于毫米波雷达的 AI 全自动关门锁门系统

> 队伍：**296 / weihedui**　开发板：**思澈 SF32LB52 黄山派（lckfb_huangshan_pi）**　系统：**openvela**

## 一、作品简介

智锁卫士是一套装在门上的「全自动关门锁门」系统：毫米波雷达持续感知门口有没有人，
门磁开关判断门的开合与锁止状态，两者结合后由主控决定何时自动关门、何时锁门，并在
关门过程中用防夹保护兜底。整套逻辑跑在 openvela 上，同时开放一条 AI Agent 工具命令
界面（`agent ...`），让上层 AI 助手能直接查询状态、下发开关锁指令。

解决的问题：日常「开门后忘了关」「手上拿东西没法关门」「关门夹到人/物」三类场景。

亮点：

- **雷达 + 门磁双传感器融合**，而不是单纯靠延时或红外——只有「门开着」且「雷达范围内无人」同时成立才触发自动关门，避免把还站在门口的人关在外面。
- **自动关门超时可运行时调整**（`agent set_timeout <1-300>`），不用重新烧录。
- **AI Agent 工具化**：六个工具函数 + `agent` NSH 命令，把设备能力暴露成结构化 JSON，便于 AI 助手直接调用。
- **板载 AMOLED 状态面板**：把锁定状态、门磁、雷达、防夹、自动关门倒计时直接画在
  1.85 吋屏上（`app/smart_lock/src/display.c`），现场不用接串口也能看出设备在做什么。

## 二、选题方向

**AI 硬件产品创新**。

选这个方向是因为作品的核心价值在「传感器融合的自动决策 + 可被 AI 调用的设备能力接口」，
而不是单纯的板级适配，也不是一个 UI 类快应用。黄山派本身已由厂商完成 openvela 适配，
因此团队的工作重心放在应用逻辑、Agent 工具界面与板级配置上。

## 三、目录结构

```text
contest2026_296_weihedui/
├── app/smart_lock/                 # 作品主体（应用）
│   ├── CMakeLists.txt              # 注册三个 NSH 命令：smart_lock、agent、lcdtest
│   ├── Kconfig                     # CONFIG_SMART_LOCK* 选项
│   ├── Make.defs / Makefile        # 传统 Make 构建入口
│   ├── include/
│   │   ├── smart_lock.h            # 应用统一头文件，汇总各模块接口
│   │   ├── radar_driver.h          # 底层雷达驱动接口
│   │   ├── door_sensor.h           # 门磁传感器接口
│   │   ├── door_control.h          # 电机/蜂鸣器控制接口
│   │   ├── display.h               # AMOLED 状态面板接口
│   │   └── safety.h                # 防夹安全接口
│   └── src/
│       ├── smart_lock_main.c       # 应用入口、初始化顺序、自动关门主循环
│       ├── radar_driver.c          # 毫米波雷达驱动
│       ├── door_sensor.c           # 门磁传感器驱动
│       ├── door_control.c          # 电机与蜂鸣器控制（GPIO + 状态机）
│       ├── display.c               # AMOLED 状态面板（帧缓冲 + 5x7 点阵字库）
│       ├── lcdtest.c               # 显示诊断工具（见第五节第 3 条）
│       ├── safety.c                # 防夹保护
│       ├── ble_service.c           # BLE GATT 服务（见「已知限制」）
│       ├── agent_tools.c           # 六个 AI Agent 工具函数（输出 JSON）
│       └── agent_main.c            # agent 命令的解析与分发
├── board/contest_board/
│   └── configs/nsh/defconfig       # 板级配置：黄山派原始 defconfig + CONFIG_SMART_LOCK*
├── docs/                           # 各模块设计文档（驱动/控制/通信）
├── logs/                           # AI Coding 日志
├── quickapp/hello_quickapp/        # 组委会骨架，本作品未使用
└── contest2026_296_weihedui.xml    # repo manifest，用 <linkfile> 把上面目录软链进编译树
```

### 关于板级配置的一个说明

大赛规则要求**生产仓库（`packages/` / `nuttx/` / `vendor/`）零改动**。黄山派在 openvela 中
本身就是一个 NuttX "custom board"：它的 `CONFIG_ARCH_BOARD_CUSTOM_DIR` 是一个可配置的
相对路径（默认指向只读的 `vendor/sifli/boards/sf32lb52/lckfb_huangshan_pi`），因此**板级
defconfig 是自描述的**——板子代码目录可以被放在任何位置。

据此，本作品**没有改动任何生产仓库**，只在自己的仓里新增了一个文件
`board/contest_board/configs/nsh/defconfig`（黄山派原始配置 + 本作品的 `CONFIG_SMART_LOCK*`），
并在 manifest 里补了一条 `<linkfile>` 映射。板子的驱动代码以只读方式被引用。

相对于厂商原始 defconfig，本作品的改动只有两处：

1. 追加 `CONFIG_SMART_LOCK*` 及本作品需要的 `CONFIG_DEBUG_*` 若干行；
2. `# CONFIG_QUICKAPP_VAPP is not set` —— 关掉厂商预置的 Lyra 演示快应用，
   避免它与本作品的状态面板抢占同一块屏。

## 四、运行方式

### 1. 拉取工程

```bash
repo init -u https://github.com/open-vela/contest2026_296_weihedui \
  -b dev-ai-contest-2026 -m contest2026_296_weihedui.xml
repo sync -c -j8
```

### 2. 编译

在 openvela 工作区**根目录**（即本仓的上一级）执行：

```bash
cd ..
./build.sh vendor/openvela/boards/contest2026_296_board/configs/nsh --cmake -j$(nproc)
```

产物：`cmake_out/contest2026_296_board_nsh/nuttx.bin`（实测 4616140 B，约 4.4 MB）。

> manifest 里的 `<linkfile>` 会把本仓的 `app/smart_lock` 软链到
> `packages/demos/contest2026_296_smart_lock`、`board/contest_board` 软链到
> `vendor/openvela/boards/contest2026_296_board`，所以不需要手动拷贝。

### 3. 烧录

把 `nuttx.bin` 烧到黄山派（具体烧录工具与接线见
[openvela AI 硬件赛道教程导航](https://github.com/open-vela/docs/blob/dev-ai-contest-2026/zh-cn/contest_2026/ai_hardware/ai_hardware_guide_index.md)）。

> 注意：`0x12000000` 处是出厂 bootloader，**不要覆盖**；应用镜像写到 `0x12010000`。

### 4. 运行

上电后进入 NSH 命令行：

```text
nsh> smart_lock &
```

**请先以 `&` 后台方式启动 `smart_lock`**：它负责按顺序初始化雷达、门磁、电机、防夹、
BLE 各模块，然后进入自动关门主循环。这一步做完，各模块才算就绪。

启动后板载 AMOLED 会立刻显示状态面板，并以 1 Hz 刷新（内容不变时每 10 秒补推一次），
无需任何额外命令。

随后可用 `agent` 命令查询与控制（模块未初始化时电机类命令会返回
`[MOTOR] Error: not initialized`，属预期行为）：

```text
nsh> agent system_status                       # 完整系统状态（JSON）
nsh> agent radar_check                         # 雷达检测状态
nsh> agent door_sensor_read                    # 门磁状态
nsh> agent motor_control open                  # 开门（也可 close/lock/unlock/stop）
nsh> agent set_timeout 30                      # 自动关门超时改为 30 秒（1-300）
nsh> agent brightness 50                     # 屏幕亮度调至 50%（0-100）
nsh> agent help                                # 用法
```

命令界面与各工具的 JSON 返回格式见 `docs/communication/agent_guide.md`。

## 五、显示模块与一个驱动层缺陷

状态面板由 `app/smart_lock/src/display.c` 实现：它打开 `/dev/fb0`，`FBIOGET_VIDEOINFO`
取几何信息后 `mmap` 帧缓冲，自带一套 5×7 点阵字库，把标题栏、锁定状态大字、七行
标签/数值、彩色状态底栏画进显存，然后**每帧做一次全屏 `FBIO_UPDATE`**。

这里有一个实测出来的驱动层缺陷，值得单独说明，因为它决定了这个模块的写法：

- 面板为 390×450、RGB565，`stride == row_bytes`（390×2 = 780），即**全宽**。
- 当 `FBIO_UPDATE` 的区域是**全宽**时，驱动按 24 行一块下发，实测稳定成功；
  用 `app/smart_lock/src/lcdtest.c` 连续做 6 次全屏更新（红/绿/蓝/白/黑 + 嵌套矩形），
  全部返回 0，分块序列从 `[0,0,389,23]` 一直到 `[0,432,389,449]`，无一次超时，画面正确。
- 当更新区域是**局部宽度**（`stride != row_bytes`）时，驱动退化成一次只传 1 行，
  实测**更新不会到达屏幕**——函数返回成功，但玻璃上没有任何变化。

因此本模块**只做全宽更新**，不追求局部刷新。这个缺陷位于 `vendor/sifli/`（受「生产仓库
零改动」约束，不能修改），好在规避方式完全落在本仓自己的代码里。

另外，厂商 defconfig 默认打开 `CONFIG_DEBUG_LCD_INFO`，会让 LCD 驱动在每次 `set_window`
时打印一行日志，把串口控制台刷得无法使用。本作品在板级 defconfig 里关掉了它
（`# CONFIG_DEBUG_LCD_INFO is not set`），启动日志随之干净。

## 六、已知限制

以下几项已在代码中标注，如实说明，避免评委复现时困惑：

1. **电机/蜂鸣器 GPIO 引脚号是占位值。** `src/smart_lock_main.c` 中的
   `g_default_motor_config` 四个引脚号目前均为 `0`。真实接线确定后必须改成黄山派上
   实际使用的引脚号，否则电机与蜂鸣器不会动作。

   另需说明：`src/door_control.c` 中的 `gpio_write()` 是一个**只打印日志的桩函数**
   （见该文件顶部「GPIO模拟定义」注释），并不真正操作硬件。因此电机状态机、超时保护、
   防夹联动这些逻辑可以在没有电机的情况下被完整演示和验证，但**不会驱动真实电机**
   ——这一点比「引脚号是占位值」更根本。

2. **门磁驱动无法工作（Linux sysfs 接口误移植到 NuttX，本次未修复）。** 实机启动
   `smart_lock` 时门磁初始化必然失败：

   ```text
   Failed to open GPIO export: Error 2
   Failed to initialize GPIO
   [MAIN] door_sensor_init failed: -2
   ```

   原因是 `src/door_sensor.c` 通过 `/sys/class/gpio/{export,direction,value,edge}`
   操作 GPIO——这是 **Linux sysfs 接口，NuttX 没有实现**。板上 `ls /sys` 直接返回
   `stat failed: 2`，`/sys` 整个路径都不存在。因此它**无论是否接线都不可能工作**，
   与「接线后即可生效」是不同性质的问题。

   正确做法是改用 NuttX 的 GPIO 字符驱动：板上实测存在 `/dev/gpio0`、`/dev/gpio1`、
   `/dev/gpio2`（对应 `CONFIG_DEV_GPIO=y`），配合 `GPIOIOC_*` ioctl 完成配置、读写
   与边沿事件等待。

   **本次未改写的原因**：`include/door_sensor.h` 中的 `DOOR_SENSOR_GPIO_PIN` 目前
   同样是无意义的占位值 `0`，且没有实物门磁可接。改写之后无法在硬件上验证其正确性，
   只会把一个「确定失败」换成一个「未经验证」；这与本作品其余部分「每一步都要有可
   复核的证据」的做法不一致，故如实记录，留待接线后连同引脚号一并处理。
   `agent door_sensor_read` 当前返回 `{"error":"door sensor read failed"}` 即源于此，
   而非接口约定问题。

3. **BLE 不随镜像出厂。** `src/ble_service.c` 中完整的 BLE GATT 服务（服务 `0x1820`、
   门状态特征 `0x2B20` 读+通知、控制特征 `0x2B21` 写）代码是完整的，但被
   `#if defined(CONFIG_BT) && defined(CONFIG_UART_BTH4)` 门控；出厂 defconfig 中
   **`CONFIG_UART_BTH4` 未开启**，因此实际编译进去的是占位实现。

   原因：黄山派上 BLE 的 HCI 传输层走的是芯片侧 `sf32lb52_bth4.c`
   （`sf32lb52_bt_initialize()` → 注册 `/dev/ttyHCI0`），而 zblue 侧
   `apps/external/zblue/zblue/port/` 的手写移植层要求一个 Zephyr 风格的 HCI 设备对象
   （`__device_dts_ord_DT_N_INST_0_zephyr_bt_hci_ttyHCI0_ORD`）。打开 `CONFIG_UART_BTH4`
   后该符号在整棵源码树中无定义者，链接失败。补齐这段集成还需要为 zblue 移植层提供
   该设备对象，并且它涉及另一颗 LCPU 的固件加载，**在无硬件的情况下无法验证**——
   为避免一个无法验证的部分影响整机启动，出厂默认关闭。

   如需在硬件就绪后启用：在 `board/contest_board/configs/nsh/defconfig` 中加
   `CONFIG_UART_BTH4=y`，然后按上文重新编译（改了 defconfig 建议先 `rm -rf
   cmake_out/contest2026_296_board_nsh` 全新构建，避免 CMake 配置缓存不刷新）。
   注意需先解决上述设备对象缺失问题。

4. **触摸屏当前不可用（I2C 无器件应答）。** 板载触摸控制器为 FT6146，接在 I2C 上。
   实测两条 I2C 总线都能正常注册（`i2c bus` 显示 Bus 0 / Bus 1 均为 YES），但总线上
   **没有任何器件应答**：

   ```text
   nsh> i2c get -b 0 -a 0x38 -r 0xa0      # FT6146 触摸
   i2ctool: get: Transfer failed: 1
   nsh> i2c get -b 1 -a 0x6a -r 0x0f      # LSM6DS3 六轴
   i2ctool: get: Transfer failed: 1
   ```

   启动日志中对应 `ERROR: ft6146_touch_initialize failed: -5`。

   已排查的部分：驱动侧的初始化路径是**完整**的——`ft6146_hw_init()` 会调用
   `BSP_TP_PowerUp()`（复用 PA09/PA41/PA37/PA33 四个引脚）并执行标准的复位时序
   （`BSP_TP_Reset(0)` → 5 ms → `BSP_TP_Reset(1)` → 80 ms），总线句柄也拿到了正确的
   I2C 实例。**问题不在软件层**：两颗分属不同总线、其中一颗还在主板上的器件同时不应答，
   指向硬件侧（触摸经 LCM 转接板引出，转接板可能未连通 I2C/INT/RESET 这几根线）。
   相关代码位于 `vendor/sifli/`（生产仓库，受零改动约束），本作品无法从应用侧修复，
   因此如实记录，未做规避性伪装。

5. **`lcdtest` 是诊断命令，不是产品功能。** 它（`src/lcdtest.c`）用于在硬件上验证
   第五节所述的驱动缺陷：依次全屏刷红/绿/蓝/白/黑、画嵌套矩形、再做一次全屏更新，
   每步之间暂停 2 秒；`lcdtest hold` 则停在最后一帧 60 秒便于拍照。保留它是因为
   它是上面那条结论的**可复现证据**，评委可用它独立验证「全宽更新可靠、局部更新失效」。

## 七、AI Coding 使用说明

本作品在**需求拆解 → 方案设计 → 编码 → 调试 → 文档**全流程中借助 AI 编程助手
（Claude Code）协作，完整对话日志见 `logs/` 目录。

几个具体的协作点：

- **需求与接口先行**：先由 AI 阅读 `docs/` 下的模块设计文档，梳理出各模块间的
  接口契约，再据此核对代码实现，发现并补齐了多处缺失的模块（统一头文件
  `smart_lock.h`、应用入口 `smart_lock_main.c`、构建脚本 `CMakeLists.txt` 等都属此类）。
- **死代码排查**：AI 通过分析调用图发现 `agent_tools.c` 的六个工具函数在全仓库
  **没有任何调用者**，并据此补写了 `agent_main.c` 把 `docs/communication/agent_guide.md`
  中约定的 `agent <tool>` 命令真正实现出来——这五个函数由此从「被链接器
  `--gc-sections` 丢弃」变为真实进入镜像（可用 `System.map` 前后对比验证）。
- **构建系统排障**：定位并解决了 CMake 下应用未被识别、头文件搜索路径缺失、
  defconfig 改动不生效（CMake 配置缓存不刷新）等一系列集成问题。
- **显示模块的从 0 到 1**：显示这条线是 AI 协作最密集的一段。AI 先写了一个独立诊断
  工具（`lcdtest`）把「驱动到底能不能刷新」这个问题变成可观测的——依次全屏刷纯色、
  画嵌套矩形、每步只做一次更新。在硬件上跑出的日志显示 6 次全屏更新全部成功且分块
  序列完整，从而**推翻了此前「局部刷新只是慢」的错误判断**，定位到真正的缺陷是
  「非全宽区域的更新不会到达屏幕」。随后 AI 据此把规避策略（只做全宽更新）写进正式的
  `display.c`，并调好了刷新节奏（1 Hz + 10 秒心跳）。整个过程中 AI 的假设被硬件实测
  否定过不止一次（例如曾误判引脚复用宏的桥接问题），这些弯路都保留在日志里。
- **上机实测推翻文档里的乐观结论**：README 早期版本在「已知限制」里写着「雷达、门磁
  各驱动层逻辑完整，接线后即可生效」。AI 在实机上跑 `smart_lock &` 后，启动日志直接
  打出 `radar_init failed: -2` 与 `door_sensor_init failed: -2`，并进一步定位到两者
  根因**并不相同**：雷达是把设备路径写死成本板不存在的 `/dev/ttyS1`（改一行即可，
  已修复并重新烧录验证），门磁则是把 Linux sysfs 接口用在了 NuttX 上（需重新移植）。
  据此改正了文档中被高估的那句话、新增一条已知限制。这类「文档说得好、硬件不认账」
  的偏差，只有真实上机才会暴露。
- **规则合规性核查**：由 AI 通读大赛《参赛代码提交指南》，确认「生产仓库零改动」的
  边界，设计出只用**一个新增板级配置文件**完成板级配置、且不触碰任何生产仓库的方案，
  并用 `git status` 验证了 `vendor/sifli`、`packages`、`nuttx`、`apps/external/zblue`
  四个生产仓库全部干净。

AI 带来的实际帮助：把「读文档 → 建接口 → 补实现 → 验证链接产物」这条链路压缩到
可在一两天内完成，且每一步都有可复核的证据（构建日志、`System.map` 符号表、
`builtin_list.h` 内置命令表、以及实机串口日志），而不是停留在「看起来对」。
