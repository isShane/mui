# MUI 编译期配置头 + 可切换输出后端

## Context（为什么要做）

MUI 目前是"零缓冲直绘"：所有绘制最终落到 `mui_port.h` 的 3 个移植函数，直接写屏。
这在目标板（YC3121 + 128×128 ST7735S，8080 并口 GPIO 模拟、只写不读、无 DMA，RAM 仅 64KB）
上是合理默认，但缺少"按硬件资源换方案"的能力：

* 想要"绘制期间屏上不出现半成品"时，需要一份显存（32KB）；

* RAM 紧张时，需要只缓几行的条带方案（4KB）或干脆直绘（0 字节）；

* 换个板子（RAM 更大 / 有 DMA）时，希望能直接开关而不改库代码。

本次目标：新增一个**编译期配置头**，由使用方按自身硬件选择输出后端；
未选中的后端完全不编译（零 Flash / 零 RAM 开销），默认配置（直绘）行为与现在**完全一致**。
只配显示相关，不做触摸 / 字体 / 控件模块裁剪。

已确认的改动前提：全库像素输出只汇聚在 `src/mui_gfx.c` 的 7 个调用点
（L67 `mui_port_init`、L87 `mui_port_draw_pixel`、L121 `mui_port_fill_rect`、
L523/567/574 `mui_port_draw_bitmap`、L618 alpha 逐像素），别处不直接调用移植层。

## 一、新增文件

| 文件                 | 作用                                 |
| ------------------ | ---------------------------------- |
| `src/mui_config.h` | 全部配置宏（`#ifndef` 默认值，-DMUI_CFG_XXX 覆盖） |
| `src/mui_out.h`    | 内部输出层接口；直绘模式下用宏直连 `mui_port_*`     |
| `src/mui_out.c`    | 三种缓冲后端实现；直绘模式下该编译单元为空              |

选独立 `mui_out.c` 而不是并入 `mui_gfx.c`：缓冲状态 + 4 套策略自成一域，
直绘时整个 TU 为空（字节级等价于现状），后续换后端不影响图元代码。

## 二、配置宏（`src/mui_config.h`）

```c
#define MUI_OUTPUT_DIRECT        0
#define MUI_OUTPUT_STRIP         1
#define MUI_OUTPUT_FULL          2
#define MUI_OUTPUT_FULL_DOUBLE   3

#ifndef MUI_CFG_OUTPUT_MODE   /* 默认：直绘（0 字节 RAM，行为同现状） */
#define MUI_CFG_OUTPUT_MODE   MUI_OUTPUT_DIRECT
#endif
#ifndef MUI_CFG_BUF_MAX_W     /* 显存支持的最大分辨率（缓冲按它静态分配） */
#define MUI_CFG_BUF_MAX_W     128
#endif
#ifndef MUI_CFG_BUF_MAX_H
#define MUI_CFG_BUF_MAX_H     128
#endif
#ifndef MUI_CFG_STRIP_LINES   /* 仅条带模式：条带行数 */
#define MUI_CFG_STRIP_LINES   16
#endif
#ifndef MUI_CFG_DIRTY         /* 脏矩形优化，缓冲模式默认开 */
#define MUI_CFG_DIRTY         1
#endif
```

派生宏：`MUI_CFG_HAS_BUFFER`（非 DIRECT 为 1）、`MUI_CFG_BUF_PIXELS`。
编译期校验（`#error`）：`STRIP_LINES > BUF_MAX_H`；`FULL_DOUBLE` 时禁用脏区无意义 → 不做限制，
但 `STRIP/FULL` 关闭脏区时退化为"推整块"，仍正确。

各模式静态 RAM：直绘 0 B；条带 `16×128×2 = 4KB`；单缓冲 `32KB`；双缓冲 `64KB`。

## 三、输出层接口（`src/mui_out.h`）

直绘模式（编译期消除，不给 Flash 增负担）：

```c
#define mui_out_init()                     mui_port_init()
#define mui_out_draw_pixel(x,y,c)          mui_port_draw_pixel((x),(y),(c))
#define mui_out_fill_rect(x,y,w,h,c)       mui_port_fill_rect((x),(y),(w),(h),(c))
#define mui_out_draw_bitmap(x,y,w,h,d,s)   mui_port_draw_bitmap((x),(y),(w),(h),(d),(s))
#define mui_out_flush()                    ((void)0)
```

缓冲模式：`mui_out_init/draw_pixel/fill_rect/draw_bitmap/flush` 均为真函数
（签名与 `mui_port_*` 一致，`bitmap` 带 `stride`）。

对外公共接口只在 `mui.h` 增加一个：

```c
#if MUI_CFG_HAS_BUFFER
void mui_screen_flush(void);          /* 把本帧改动推屏；缓冲模式须在帧末调用 */
#else
#define mui_screen_flush()  ((void)0)  /* 直绘下编译期消失，应用代码可无条件调用 */
#endif
```

这样应用层（`app_ui.c`）写死 `mui_screen_flush()` 即可同时兼容两种模式，同步到对端无需再分叉。

## 四、各后端算法要点

统一约定：缓冲**永不 memset**（初值为 0 与上电黑屏一致），脏矩形用单包围盒
`x0,y0,x1,y1` 记录本帧写过的像素，**只推脏区**——这是条带模式正确性的基础
（MUI 从不回读屏幕，不依赖缓冲里的陈旧内容）。

* **DIRECT**：现状。`mui_out_*` 宏直连移植层，`mui_screen_flush()` 为空。

* **STRIP（条带）**：按 `MUI_CFG_STRIP_LINES` 行对齐分带，记当前带 `s_band_y`。
  写入落在其它带 → 先推当前带脏区，再换带；跨带矩形 / 位图按带分段处理。
  撕裂粒度 = 条带行数；带切换频繁时推屏量可能大于直绘。

* **FULL（单缓冲）**：绘制只改内存；`mui_screen_flush()` 用一次
  `mui_port_draw_bitmap(x0,y0,w,h,&buf[y0*W+x0], W)` 推脏包围盒后清包围盒。
  复用现有移植接口，**三种移植层（PC 模拟器 / 单元测试 / 单片机）零改动**。

* **FULL\_DOUBLE（双缓冲）**：`static uint16_t s_buf[2][W*H]` + front/back 指针。
  每帧：`memcpy(back, front)`（增量绘制必须继承上一帧内容）→ 绘制 → 全帧推 back → swap。
  代价：每帧 32KB 拷贝 + 32KB 推屏。

**必须写进文档的结论**：本硬件屏只有一块 GRAM、无基址切换、无 DMA，
MCU 侧双缓冲**不能消除撕裂**（撕裂来自写 GRAM 的过程被扫描看到），
它只把"屏上出现中间状态"的窗口缩短到一次 flush；换方案前应知道这个取舍。

## 五、改动清单

1. `src/mui_gfx.c`：7 处 `mui_port_*` → `mui_out_*`；`#include "mui_out.h"`；
   缓冲模式下在 `mui_init` 里把运行期尺寸钳到 `MUI_CFG_BUF_MAX_W/H`（防缓冲越界）。
2. `src/mui.h`：加 `mui_screen_flush()` 声明 / 空宏（见上），其余 API 与语义不变。
3. `src/mui_config.h`、`src/mui_out.h`、`src/mui_out.c`：新建。
4. `CMakeLists.txt`：`file(GLOB)` 会自动收进 `mui_out.c`；用
   `foreach(mode direct strip full double)` 生成 4 个测试目标与 4 个模拟器目标
   （`-DMUI_CFG_OUTPUT_MODE=n`），其中 `direct` 版保持 `OUTPUT_NAME mui_sim / mui_test`
   不变，保证现有调用方式不破。
5. `test/test_main.c`：`px()` 与 `render_demo()` 在取值 / 导出前调用 `mui_screen_flush()`
   （直绘下是空宏，零影响）；PNG 文件名按模式区分，避免相互覆盖。
6. 对端接入（`E:\WorkSpace\Hk-RLT-001`）：在 Keil C/C++ → Preprocessor Definitions
   里加 `MUI_CFG_OUTPUT_MODE=1` 等覆盖任一配置宏即可，无需额外文件。
   在 `app_ui_init/frame/key` 末尾补 `mui_screen_flush()`（直绘下为空操作）。
   `tools/sync_to_mcu.ps1` **无需改动**。

## 六、非目标

* 不做触摸 / 字体 / 控件模块裁剪（只配显示）。

* 不改变现有公共 API 语义，不改 alpha 混合与抗锯齿"显式 bg"的现状
  （缓冲模式下"回读屏幕"虽然变得可行，但本次不做）。

* 不改单片机 BSP 与移植层实现。

## 七、验证

1. 编译 4 种配置：`cmake -S . -B build\cmake -G "MinGW Makefiles" && cmake --build build\cmake`（`-Wall` 无警告）。
2. 4 个测试目标全跑，要求 `test_failed == 0` 且 `sim_violations() == 0`（越界安全在缓冲模式下同样成立）。
3. 直绘回归：改造前后 `mui_test` 导出的 `test_output.png` 用 SHA256 比对，应**完全相同**。
4. 模拟器逐模式启动，按 `S` 截图，用 System.Drawing 像素采样比对四种模式的画面一致
   （底部键高亮色、内容区非白像素计数、进度条填充高度与边缘色）。
5. 覆盖率检查：确认未选中的后端确实未参与编译（例如 `MUI_OUTPUT_DIRECT` 下 `mui_out.c` 无符号）。

## 八、风险

* **内存现实**：单缓冲 32KB 会吃掉一半 RAM；双缓冲 64KB 正好等于整块 `RW_IRAM2`，
  YC3121 上不可落地（现有固件仅用 2.4KB）。对端实际建议：直绘（默认）或条带 4KB。

* 缓冲模式要求应用在帧末调 `mui_screen_flush()`；漏调会表现为"画面不更新"。

* `MUI_CFG_BUF_MAX_W/H` 必须 ≥ `mui_init` 传入的尺寸，否则会被钳制；
  PC 侧它应与 `sim_config.h` 的 `SIM_W/SIM_H` 一致。

* 新增 `src/mui_out.c` 需在对端 Keil 工程手动加入编译。

* 代码风格须守法：C99 可用（Keil 侧已启用 C99），注释用 `/* */`，不用 `//`。

