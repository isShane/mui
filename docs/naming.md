# MUI 命名规范与迁移映射表

> 状态：**已执行**（2026-09-22）。方向：**A —— 对象在前**。
> 本文档既是命名规范，也是本次改名的映射记录（"现在"一列保留改名前原名，便于回溯）。
> 执行时的追加决定见文末"执行记录"。

## 1. 命名模式

```
mui_<对象>_<动作>[_<修饰>]
```

- **对象**：被操作的东西（名词，尽量短、具体）
- **动作**：`init` / `draw` / `fill` / `set` / `get` / `clear` / `poll` / `mix` / `contains`
- **修饰**（可选）：`_aa`（抗锯齿）/ `_cell`（整格覆盖）/ `_key`（色键）/ `_mask`（蒙版）

**为什么选 A**：控件层（`mui_button_init` / `mui_button_draw` / `mui_label_set_text`）已经是这套，
图元层（`mui_fill_rect` / `mui_draw_circle`）是另一套；让图元对齐控件，改动面更小、且对象相同的函数在
自动补全里自然聚堆（`mui_image_*` 三兄弟挨在一起）。

### 动作词表（只有这几个，不许扩）

| 动作 | 含义 | 例 |
|---|---|---|
| `init` | 初始化对象 | `mui_button_init` |
| `draw` | 画**轮廓**（1px 边）/ 画整个对象 | `mui_circle_draw`、`mui_button_draw` |
| `fill` | 画**实心**（填充） | `mui_circle_fill` |
| `set` / `get` | 设值 / 取值 | `mui_label_set_text`、`mui_screen_get_width` |
| `clear` | 清空（整屏/整区填充底色） | `mui_screen_clear` |
| `poll` | 取走一个事件 | `mui_touch_poll` |
| `mix` | 两色混合 | `mui_color_mix` |
| `contains` | 命中测试 | `mui_rect_contains` |

> `draw`（轮廓）/ `fill`（实心）这套对立是行业惯例（Adafruit `drawRect`/`fillRect`、LVGL 同理），**保留**。

### 分界规则：单色用 `color`，双色用 `fg` + `bg`

- 只需要一个颜色 → 参数名 `color`（`mui_rect_fill(x, y, w, h, color)`）
- 需要"前景 + 背景"两个色（抗锯齿混合、alpha 蒙版）→ 参数名 `fg` + `bg`

> 这不是不一致，而是"单色/双色"的规则，本次统一在文档里写明即可，参数名不改。

## 2. 全量映射表

### 2.1 框架 / 屏幕（`mui.h`）

| 现在 | 新名 | 备注 |
|---|---|---|
| `mui_init(w, h)` | **不变** | 唯一不带对象的顶层函数 |
| `mui_get_width()` | `mui_screen_get_width()` | |
| `mui_get_height()` | `mui_screen_get_height()` | |
| `mui_clear_screen(c)` | `mui_screen_clear(c)` | |
| `mui_flush()` | `mui_screen_flush()` | |

### 2.2 图元

| 现在 | 新名 |
|---|---|
| `mui_draw_pixel` | `mui_pixel_draw` |
| `mui_draw_line` | `mui_line_draw` |
| `mui_draw_line_aa` | `mui_line_draw_aa` |
| `mui_draw_hline` | `mui_hline_draw` |
| `mui_draw_vline` | `mui_vline_draw` |
| `mui_fill_rect` | `mui_rect_fill` |
| `mui_draw_rect` | `mui_rect_draw` |
| `mui_fill_round_rect` | `mui_round_rect_fill` |
| `mui_draw_round_rect` | `mui_round_rect_draw` |
| `mui_fill_triangle` | `mui_triangle_fill` |
| `mui_draw_triangle` | `mui_triangle_draw` |
| `mui_fill_circle` | `mui_circle_fill` |
| `mui_draw_circle` | `mui_circle_draw` |
| `mui_draw_circle_aa` | `mui_circle_draw_aa` |
| `mui_fill_ellipse` | `mui_ellipse_fill` |
| `mui_draw_ellipse` | `mui_ellipse_draw` |
| `mui_color_mix` | **不变**（已符合） |
| ——（新增能力，2026-09-22 已实现） | `mui_round_rect_fill_aa`（抗锯齿实心圆角矩形） |
| ——（新增能力，2026-09-22 已实现） | `mui_round_rect_draw_aa`（抗锯齿空心圆角矩形，1px 描边） |

> `hline` / `vline` 作为对象名保留（比 `line_draw_h` 更短、更贴近绘图习惯），
> 缩写 `h`/`v` = 水平/垂直，是词表里唯一允许的缩写。

### 2.3 位图 → 图片

| 现在 | 新名 | 备注 |
|---|---|---|
| `mui_draw_bitmap` | `mui_image_draw` | `bitmap` 是**数据格式名**，不是"画什么"，统一改叫 `image` |
| `mui_draw_bitmap_key` | `mui_image_draw_key` | `key` = 色键透明 |
| `mui_draw_bitmap_alpha` | `mui_image_draw_mask` | `mask` 比 `alpha` 直白（8bpp 蒙版，亮度即透明度） |
| `mui_image_t` | **不变** | |
| `mui_image_alpha_t` | `mui_image_mask_t` | 与函数名一致 |

### 2.4 触摸 / 命中

| 现在 | 新名 | 备注 |
|---|---|---|
| `mui_touch_update` / `mui_touch_poll` / `mui_touch_get_xy` | **不变** | 已符合 A 风格 |
| `mui_hit(px, py, x, y, w, h)` | `mui_rect_contains(x, y, w, h, px, py)` | 原名缺宾语；参数顺序同时改成"先矩形后点" |

### 2.5 文字 / 字体

| 现在 | 新名 | 备注 |
|---|---|---|
| `mui_lv_font_t` | `mui_font_t` | 去掉 LVGL（实现细节）泄漏 |
| `mui_lv_font_draw_char` | `mui_text_draw_char` | |
| `mui_lv_font_draw_text` | `mui_text_draw` | |
| `mui_lv_font_text_width` | `mui_text_width` | |
| `mui_lv_font_draw_char_cell` | `mui_text_draw_char_cell` | `_cell` = 整格覆盖（供 label 用） |
| `mui_lv_font_draw_text_cell` | `mui_text_draw_cell` | |
| `mui_font_draw_char` / `mui_font_draw_text` / `mui_font_text_width` | **整体删除** | 5x7 点阵字体退场，只保留 LVGL 转换字体 |
| 形参 `spacing`（5x7 字距） | **删除** | 新字体按 `adv_w` 步进，无需外部字距 |

### 2.6 控件

| 现在 | 新名 | 备注 |
|---|---|---|
| `mui_button_init/draw/touch/set_*` | **不变** | 已是 A 风格，是本次的"参照物" |
| `mui_label_init/draw/set_*` | **不变** | |
| `mui_progressbar_init/draw/set_*` | **不变** | |
| `mui_button(...)` / `mui_button_icon(...)` / `mui_progressbar(...)`（立即模式） | **整体删除** | 控件只保留对象版，库内建模统一 |
| `MUI_BTN_SHAPE_*` | `MUI_BUTTON_SHAPE_*` | 宏与类型 `mui_button_*` 用词统一 |
| `MUI_PB_HORIZONTAL` / `MUI_PB_VERTICAL` | `MUI_PROGRESSBAR_HORIZONTAL` / `MUI_PROGRESSBAR_VERTICAL` | 同上，不用 `PB` 缩写 |

## 3. 明确**不动**的命名空间

| 前缀 | 原因 |
|---|---|
| `mui_port_*` | 移植层是独立契约（用户实现 4 个函数），改名要动对端工程，收益小 |
| `mui_out_*` | 库内私有输出层，app 看不到 |
| `MUI_CFG_*` / `MUI_OUTPUT_*` | 编译期配置，已自洽 |
| `MUI_RGB565`、`MUI_RED` 等颜色宏、`MUI_TOUCH_*` 事件枚举 | 已符合，且被大量示例引用 |
| **所有文件名** | Keil 工程逐文件登记、同步脚本按文件名比对 —— 改路径要两边手工返工 |

## 4. 执行记录（2026-09-22）

改名与清理已按本规范一次性完成，**不做** `MUI_ENABLE_LEGACY_NAMES` 兼容层（旧名直接删除）：

| 决定 | 结果 |
|---|---|
| 立即模式控件 | **删除** `mui_button` / `mui_button_icon` / `mui_progressbar`，控件只保留对象版 |
| 5x7 点阵字体 | **整体删除**（字模表 + 3 个 API），只保留 LVGL 转换字体 |
| 旧名兼容宏 | **不加**，一步到位（PC 与对端同时迁移） |
| 字体指针类型 | `void *font` 收紧为 `const mui_font_t *font`；`mui_button.h` / `mui_label.h` 因此 include `mui_font.h` |
| 命中测试 | `mui_hit(px,py,x,y,w,h)` → `mui_rect_contains(x,y,w,h,px,py)`（参数顺序也调成"先矩形后点"） |
| 缩写宏 | `MUI_BTN_*` → `MUI_BUTTON_*`；`MUI_PB_*` → `MUI_PROGRESSBAR_*` |

执行方式：先脚本化做全量符号替换（420 处 / 32 个文件，覆盖源码、注释、文档、生成脚本），
再逐个删除上面两块废弃 API，最后收紧类型与修正调用点。

本次未处理的遗留：

- **仓库行尾不统一**：`src/mui_progressbar.*`、`test/test_main.c` 等为 CRLF，其余多为 LF。
  建议加 `.gitattributes` 统一，避免后续 diff 噪音（本次未动，以免污染 diff）。
- `mui_conf.h` 的 `MUI_CFG_OUTPUT_MODE` 默认值当前是 `MUI_OUTPUT_FULL`（缓冲后端），
  而 `mui.h` / `mui_conf.h` 注释里仍写"默认直绘"，两者不符，需确认以哪个为准。

## 5. 迁移步骤（本次执行顺序，后续同类改动可复用）

1. 写规范 + old→new 映射表（本文档）
2. 脚本化全量符号替换：`src/` `app/` `test/` `assets/` `tools/` `simulator/` + `README.md`
3. 删除废弃 API（立即模式控件、5x7 字体），修正受影响的调用与注释
4. 语义微调（字体指针类型收紧、参数顺序调整）
5. 三后端（直绘 / 单缓冲 / 双缓冲）+ `mui_test` 回归
6. 跑 `tools\sync_to_mcu.ps1` 推到单片机（**文件名不变 → Keil 工程无需重登记**）

> 改名**不省 Flash**（符号名不进 `.text`，只影响调试信息），收益纯粹是可读性与可维护性；
> 也正因为如此，趁库还小（约 2 700 行、app 单文件、控件 3 个）改代价最低。
