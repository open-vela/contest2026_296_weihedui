# 显示模组与 LCM 接口说明

本文说明本作品所用的显示屏、它在黄山派上的连接方式，以及**为什么本作品不需要 LCM 转接板**。
同时也记录将来若要改接第三方屏幕时，转接板要做哪些事。

> 资料来源：思澈官方 wiki《思澈开发板LCM转接板制作指南》
> <https://wiki.sifli.com/board/sf32lb52x/SF-DevKit-LCM-Adapter.html>
> 与《立创·黄山派开发板使用指南》。文中接口定义表均照抄 wiki 原文，未作推断。

---

## 1. 本作品的显示方案

| 项目 | 值 |
|---|---|
| 开发板 | SF32LB52-DevKit-ULP（立创·黄山派） |
| 板载屏幕接口 | **22p QSPI FPC**（板上丝印 `QSPI LCM`） |
| 屏模组 | 思澈 1.85inch AMOLED Module |
| 裸屏型号 | ZC-A1D85W-010 |
| 分辨率 | 390 × 450 |
| 亮度 | 800 cd/m² |
| 显示接口 | QSPI |
| 触摸接口 | I2C |
| OLED Driver IC | **CO5300AF-01** |
| Power IC | BV6802W |
| TP IC | FT6146-M00 |
| 连接排线 | 22p-0.5mm-5cm-**反向(0.5B)** FPC |

屏与板之间是**直连**：一条 22p 排线，一头插黄山派 22p 座，另一头插 AMOLED 模组板上的
22p 座。模组板本身已经把 22p 的信号分配到它自己的多个连接器上（板上丝印有 `CN2
52-Nano-Core`、`CN3 56/8-LCD`、`CN4`），所以它同时也能插到别的思澈开发板上——这是
模组自带的兼容设计，不是本项目另外做的转接板。

## 2. 为什么本作品不需要 LCM 转接板

思澈那份《LCM转接板制作指南》开篇写明它的用途是「**用来调试第三方显示屏**」。
本作品用的是思澈原厂 LCM 模组，模组与开发板的接口本就配套，**直接插线即可，无需任何
自制转接板**。

反过来，只有当你要把一棵**非原厂**的屏接到思澈开发板上时，才需要按第 5 节做转接板。

### 一个容易踩的坑：FPC 排线的「同向 / 反向」

FPC 排线按两端金手指是否在**同一面**分为两类：

| 型号 | 俗称 | 说明 |
|---|---|---|
| 0.5A | 同向 | 两端金手指在同一面 |
| 0.5B | **反向** | 两端金手指在相背面 |

wiki 的「开发板支持列表」明确给出每块板要用的排线：

| 开发板 | 排线 |
|---|---|
| SF32LB52-DevKit-Nano | 16p-0.5mm-5cm-反向 |
| SF32LB52-DevKit-Core-**** | 16p-0.5mm-5cm-反向 |
| **SF32LB52-DevKit-ULP（立创·黄山派）** | **22p-0.5mm-5cm-反向** |
| SF32LB52-DevKit-LCD | 22p-0.5mm-5cm-反向 |
| SF32LB56-DevKit-LCD | 40p-0.5mm-5cm-反向 |
| SF32LB58-DevKit-LCD | 40p-0.5mm-5cm-正向 |

**买错方向就是信号交叉、屏不亮**，而且外观上很难一眼看出差别——排查显示不亮时，
先确认排线是同向还是反向。

## 3. 22p QSPI FPC 接口信号定义

照抄 wiki 原文（`DevKit FPC CON PIN-Name` / `Descriptions` / `LCM PIN-Name`）：

| PIN | DevKit 侧信号 | 说明 | LCM 侧信号 |
|---|---|---|---|
| 1 | LEDK | LED cathode | LEDK |
| 2 | LEDA | LED anode | LEDA |
| 3 | D2/DB0 | LCD QSPI data 2, 8080 data 0 | DB0 |
| 4 | D3/DB1 | LCD QSPI data 3, 8080 data 1 | DB1 |
| 5 | DB2 | 8080 data 2 | DB2 |
| 6 | DB3 | 8080 data 3 | DB3 |
| 7 | DB4 | 8080 data 4 | DB4 |
| 8 | DB5 | 8080 data 5 | DB5 |
| 9 | DB6 | 8080 data 6 | DB6 |
| 10 | DB7 | 8080 data 7 | DB7 |
| 11 | TE | Tearing effect input | TE |
| 12 | LCD_RST | LCD reset output, Active low | RESX |
| 13 | CLK | LCD QSPI clock output | CLK/WRx |
| 14 | D0/RD | LCD QSPI data 0 inout | D0/RDx |
| 15 | CS | LCD QSPI chip select output | CSx |
| 16 | D1/DC | LCD QSPI data 1 output | D1/DCx |
| 17 | 3.3V | DC 3.3V Power Supply | VCI |
| 18 | TP_INT | TP Interrupt signal inout | TP-INT |
| 19 | TP_SDA | TP I2C data signal | TP-SDA |
| 20 | TP_SCL | TP I2C clock signal | TP-SCL |
| 21 | TP_RST | TP Reset | TP-RTN |
| 22 | GND | Power Supply Ground | GND |

两点值得注意：

1. **这个座上没有 `BL_PWM`。** BL_PWM 只出现在 40p RGB FPC 接口（第 34 脚）。
   这从硬件上印证了本文档第 4 节的结论：黄山派的 22p 座根本没引出背光调光线。
2. **触摸是引出来的**（18~21 脚，I2C + 中断 + 复位），模组的 TP IC 是 FT6146-M00。
   本作品没有使用触摸，见第 6 节。

## 4. AMOLED 为什么没有「背光」，以及它和亮度模块的关系

wiki 的《转接板背光电路 · AMOLED屏背光电路》一节原文：

> AMOLED是采用像素级自发光技术，没有像TFT屏一样的背光源，亮度调节一般是调节像素点的
> 颜色，例如黑色为不发光，白色为最亮，所以无需专门提供PWM信号来调整背光亮度。
> AMOLED屏一般自带PMIC芯片，给面板提供VGH、VGL和VCOM电压，屏驱芯片通过1W接口来调整
> 相应的电压值。PMIC的供电范围一般支持2.7~5.5V，所以可以引开发板FPC接口上的5V或3.3V
> 给屏上PMIC供电。

这段是**官方对「本作品亮度方案」的独立佐证**，与 `docs/communication/agent_guide.md`
第 2.6 节、`app/smart_lock/src/panel_brightness.c` 的注释所述一致：

- AMOLED 没有背光电路，**不存在可调的背光 PWM**；
- 屏自带 PMIC（本模组为 BV6802W）产生 VGH/VGL/VCOM，靠 1W 接口调压；
- 因此「调光」只能写**屏驱自己的亮度寄存器** —— 本作品写的是 CO5300 的
  `0x51`（Write Display Brightness），并同时写 `0x53` 的 bit5 使能亮度控制。

第 3 节的引脚表进一步印证：22p 座上既没有背光驱动输出，也没有 BL_PWM，
`LEDA`/`LEDK` 是给 TFT 屏的背光 LED 串用的，AMOLED 模组不需要它们。

## 5. 若将来要接第三方屏：转接板设计要点

以下摘自 wiki《转接板信号连接》与《FPC软排线的选择》，供改屏时参考。

**必须注意的几条（wiki 标为「重要」）：**

- 开发板 IO 是 **3.3V 电平**。若 LCD 转接板上驱动芯片的 IO 是 1.8V，**必须加 level shift**。
- QSPI、8080、RGB 的**数据和时钟线要串 22~47Ω 电阻**。
- 「-Nano 和 -Core 开发板上 I2C 信号没有加上拉电阻，需要在屏转接板上加 I2C 上拉电阻。」
  —— 本作品用的是 ULP（黄山派），**这条不适用**。
- **FPC 座的 1 脚方向**：开发板 FPC 座是「上翻下接触」型。若转接板直接照抄开发板原理图的
  线序和 1 脚位置，用软排线对接时会造成**信号交叉**而无法使用。解决办法二选一：
  1. 改转接板 PCB 库中插座的 1 脚位置（从相反方向编号），原理图符号不变；
  2. 改转接板原理图符号库的 1 脚位置（从相反方向编号），PCB 库不变。
- 建议转接板上的 FPC 座采用**上翻-上下双侧接触**类型（16p / 22p / 30p / 40p，0.5mm）。
  排线用同面或异面接触点均可。

**背光（仅 TFT 屏需要）：**

- 单个 LED 并联的背光：`LEDA` 可直接接 3.3V，`LEDK` 直接接 GND。
- 多个 LED 串联的背光：5V/3.3V 不够 VF，需**升压 + 恒流**驱动。
  驱动电流 = 并联路数 × 15mA；`IF = VFB / Rset`（芯片 FB 一般为 200mV）。
  常见恒流驱动芯片电路见 wiki 的 AW9962EDNR 示例（Rset = 10Ω → IF ≈ 20mA）。
- 常见 TFT 背光组成：1.3" 2并 / 2.4" 4并 / 2.8" 4并 / 3.5" 6并（VF≈3.3V，可 3.3V 直驱）；
  4.2" 8串（VF≈25.6V）、5.5" 2串×7并、7" 3串×6并、10.1" 4串×7并（需 boost 恒流）。
- 墨水屏背光更复杂，用专用 PMIC（TPS65185 / SY7637A）。

## 6. 模组上本作品未使用的资源

如实记录，避免复现时误以为漏了什么：

| 资源 | 位置 | 本作品 |
|---|---|---|
| 触摸 FT6146-M00 | 22p 的 18~21 脚（I2C） | **未使用**（本作品无触摸交互需求） |
| LEDA / LEDK | 22p 的 1、2 脚 | **未使用**（AMOLED 自发光，无背光 LED 串） |
| 屏上 PMIC BV6802W | 模组板 | 由模组自行管理，本作品不直接控制 |

## 7. 参考资料

- [思澈开发板LCM转接板制作指南](https://wiki.sifli.com/board/sf32lb52x/SF-DevKit-LCM-Adapter.html)
- [立创·黄山派开发板使用指南](https://wiki.sifli.com/board/sf32lb52x/SF32LB52-%E9%BB%84%E5%B1%B1%E6%B4%BE.html)
- 本作品亮度实现：`docs/communication/agent_guide.md` 第 2.6 节、
  `app/smart_lock/src/panel_brightness.c`
