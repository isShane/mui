# MUI 裁剪区与脏矩形刷新方案

> 状态：已评审，待实施
> 范围：`src/`（库本体）、`app/demo_ui.c`（示例）、`test/`（回归用例）
> 原则：不引入帧缓冲，保持"零动态内存 + 无缓冲直绘"定位

## 1. 背景与目标

当前库的所有绘制都是无条件写入屏幕，存在两个能力缺口：

| 缺口 | 现状表现 |
|---|---|
| 无绘制裁剪区 | 控件无法限制在父容器矩形内绘制；滚动列表、进度条填充、圆角内裁剪都实现不了 |
| 无变化感知 | `demo_ui_frame()` 每帧无条件重画全部按钮，SPI 写入量恒定，与"实际变化量"无关 |

目标：

1. 提供可嵌套的裁剪区，作为容器类控件的地基
2. 提供脏矩形跟踪，让"本帧哪些区域发生了变化"可被查询
3. 让控件在状态无变化时跳过绘制，把每帧写入量从"全屏"降到"实际变化量"

明确不做：整屏/分块帧缓冲（理由见第 7 节）。

## 2. 现状分析：改造挂载点

所有绘制最终只经过三个移植层出口：

- `mui_port_draw_pixel`
- `mui_port_fill_rect`
- `mui_port_draw_bitmap`

而"裁剪到屏幕"的逻辑目前在 [mui_gfx.c](../src/mui_gfx.c) 中重复了四份：
`mui_fill_rect`、`mui_draw_bitmap`、`mui_draw_bitmap_key`、`mui_draw_bitmap_alpha`。

因此裁剪区与脏矩形可以统一挂载在这一个位置，且改造后重复代码是净减少的。
`mui_port.h` 的契约（收到的坐标必为合法屏幕内坐标）保持不变。

## 3. P1 裁剪区

### 3.1 对外 API

```c
/** @brief 矩形区域（开区间：x <= px < x2，y <= py < y2） */
typedef struct {
    int16_t x;    /**< 左边界（含） */
    int16_t y;    /**< 上边界（含） */
    int16_t x2;   /**< 右边界（不含） */
    int16_t y2;   /**< 下边界（不含） */
} mui_rect_t;

/**
 * @brief 设置裁剪区（自动与屏幕求交，越界参数安全）
 * @param x 左边界
 * @param y 上边界
 * @param w 宽度（<1 时裁剪区为空，后续绘制全部丢弃）
 * @param h 高度
 */
void mui_set_clip(int16_t x, int16_t y, int16_t w, int16_t h);

/** @brief 恢复裁剪区为全屏 */
void mui_reset_clip(void);

/**
 * @brief 保存当前裁剪区（返回值在栈上，不占静态 RAM）
 * @return 当前裁剪区
 */
mui_rect_t mui_clip_save(void);

/**
 * @brief 恢复裁剪区（配合 mui_clip_save，支持任意层嵌套）
 * @param c 之前保存的裁剪区
 */
void mui_clip_restore(mui_rect_t c);
```

选择 `save/restore` 结构体传值，而非 push/pop 栈：不占静态 RAM、嵌套层数不受限。
典型用法：

```c
mui_rect_t old = mui_clip_save();

mui_set_clip(container_x, container_y, container_w, container_h);
/* 在此绘制子控件，超出容器的部分自动被裁掉 */
mui_clip_restore(old);
```

### 3.2 语义约定

- 初始值 = 全屏，`mui_init()` 内复位
- 裁剪区本身永远保证落在屏幕内（`mui_set_clip` 内完成求交）
- 内部求交顺序固定为「裁剪区 ∩ 矩形」，因裁剪区已在屏幕内，结果必然在屏幕内
- 空裁剪区（`x2 == x` 或 `y2 == y`）表示"全部丢弃"，可用于临时隐藏
- `mui_clear_screen()` 也受裁剪区限制（清屏 = 填充裁剪区），需要真清全屏时先 `mui_reset_clip()`

### 3.3 内部实现

```c
static mui_rect_t s_clip;   /* 当前裁剪区，8 字节 */

/**
 * @brief 内部：把矩形裁剪到当前裁剪区
 * @param x/y/w/h 输入输出：矩形，裁剪后就地更新
 * @return 1=有可见部分，0=完全不可见
 */
static uint8_t mui__clip_rect(int16_t *x, int16_t *y, int16_t *w, int16_t *h)
{
    int32_t x2;
    int32_t y2;

    if (*w < 1 || *h < 1) {
        return 0;
    }
    x2 = (int32_t)(*x) + *w;
    y2 = (int32_t)(*y) + *h;
    if (*x < s_clip.x)  { *x = s_clip.x; }
    if (*y < s_clip.y)  { *y = s_clip.y; }
    if (x2 > s_clip.x2) { x2 = s_clip.x2; }
    if (y2 > s_clip.y2) { y2 = s_clip.y2; }
    if (x2 - *x < 1 || y2 - *y < 1) {
        return 0;
    }
    *w = (int16_t)(x2 - *x);
    *h = (int16_t)(y2 - *y);
    return 1;
}
```

`mui_draw_pixel` 直接做四次比较，不调用上述函数（逐像素路径，避免函数调用开销）：

```c
void mui_draw_pixel(int16_t x, int16_t y, uint16_t color)
{
    if (x < s_clip.x || y < s_clip.y || x >= s_clip.x2 || y >= s_clip.y2) {
        return;
    }
    mui__dirty_add(x, y, 1, 1);
    mui_port_draw_pixel(x, y, color);
}
```

### 3.4 各图元改造要点

| 函数 | 改造内容 |
|---|---|
| `mui_fill_rect` | 原裁剪逻辑替换为 `mui__clip_rect`，注意同步改 `hline/vline`（它们走 fill_rect，无需单独改） |
| `mui_draw_bitmap` | 裁剪前先保存原始行宽 `stride = w`，源偏移用 `stride` 计算（**不能**用裁剪后的 `w`） |
| `mui_draw_bitmap_key` | 同上保存 `stride`；逐行 `line = data + (row - oy) * stride + (x - ox)` |
| `mui_draw_bitmap_alpha` | 保存 `stride` 修正现有缺陷（见第 6 节）；裁剪后整块记脏，内部循环改用 `mui_port_draw_pixel` |
| `mui_draw_line` / 圆 / 椭圆 / 三角 | 无需改动，最终都落到 `draw_pixel` 或 `hline`，自动生效 |
| `mui_draw_line_aa` / `mui_draw_circle_aa` | 无需改动，同上 |
| `mui_fill_triangle` | 内部有 `y_start/y_end` 的屏幕裁剪，可简化为交给 `mui_draw_hline` 处理 |

## 4. P2 脏矩形跟踪

### 4.1 对外 API

```c
/** @brief 清空脏区列表（通常在每帧绘制前调用） */
void mui_dirty_clear(void);

/**
 * @brief 主动标记一块区域为脏（同样受裁剪区限制）
 * @param x 左边界
 * @param y 上边界
 * @param w 宽度
 * @param h 高度
 */
void mui_dirty_mark(int16_t x, int16_t y, int16_t w, int16_t h);

/** @brief 当前脏区数量 */
uint8_t mui_dirty_count(void);

/**
 * @brief 取出第 idx 个脏区
 * @param idx 下标（0 ~ mui_dirty_count()-1）
 * @param r   输出：脏区矩形
 * @return 1=成功，0=下标越界
 */
uint8_t mui_dirty_get(uint8_t idx, mui_rect_t *r);

/**
 * @brief 把所有脏区合并为单个包围盒
 * @param r 输出：包围盒
 * @return 1=本帧有变化，0=无变化
 */
uint8_t mui_dirty_bounds(mui_rect_t *r);
```

### 4.2 数据结构与合并策略

```c
static mui_rect_t s_dirty[MUI_CFG_DIRTY_N];   /* 默认 N=4，32 字节 */
static uint8_t    s_dirty_count;
```

`mui__dirty_add(x, y, w, h)` 的处理顺序，按开销从低到高：

1. 被某个已有脏区完全包含 → 立即返回（`line_aa` / `circle_aa` 逐像素路径的主要快路径）
2. 与某个已有脏区合并后"增长可控" → 就地 union，返回
3. 有空槽 → 占用新槽
4. 槽已满 → 合并"并集面积增量最小"的两个脏区，腾出槽后再放新矩形

合并只能让区域变大，绝不允许丢弃。第 4 步的代价上限很低（N=4 时最多 6 次比较），
且只在绘制区域零散时才触发。

### 4.3 核心不变式

> 脏区列表的并集必须始终覆盖所有被实际写入的像素。

可以多报（区域偏大，浪费一点刷新），绝不允许漏报（会导致残影）。
这一条要作为单测的核心断言。

### 4.4 配置开关

在 `src/mui.h` 顶部提供默认值，允许用编译选项覆盖：

```c
#ifndef MUI_CFG_DIRTY_N
#define MUI_CFG_DIRTY_N 4   /**< 脏区槽位数量，0 = 关闭脏区跟踪 */
#endif
```

`MUI_CFG_DIRTY_N = 0` 时：静态数组不占 RAM，`mui__dirty_add` 编译为空，
对外 API 仍然存在但恒返回 0（保证源码兼容）。

## 5. P3 按需重绘

脏矩形的价值在直绘模式下无法靠"局部搬运"兑现——像素早就写出去了。
真正的收益是**驱动控件按需重绘**：状态没变的控件直接跳过绘制。

### 5.1 button 增加状态缓存

`mui_button_t` 新增字段（每个按钮约 +14 字节）：

```c
uint8_t  cache_valid;   /**< 绘制缓存是否有效 */
uint8_t  c_pressed;     /**< 上次绘制时的按下状态 */
uint8_t  c_enabled;     /**< 上次绘制时的使能状态 */
uint8_t  c_visible;     /**< 上次绘制时的可见状态 */
int16_t  c_x;           /**< 上次绘制时的横坐标 */
int16_t  c_y;           /**< 上次绘制时的纵坐标 */
const char *c_text;     /**< 上次绘制时的文字指针 */
const mui_image_alpha_t *c_icon;  /**< 上次绘制时的图标指针 */
const void *c_font;     /**< 上次绘制时的字体指针 */
```

`mui_button_draw()` 改为：先比对上列字段，完全一致则直接返回，否则重绘并更新缓存。

### 5.2 新增接口与兼容性

```c
/** @brief 作废绘制缓存，强制下次 draw 重绘（整屏被清后恢复用） */
void mui_button_invalidate(mui_button_t *btn);
```

需要注意的两点：

- `mui_button_draw` 的参数由 `const mui_button_t *` 改为 `mui_button_t *`
  （要写缓存）。现有调用传的都是非 const 指针，不受影响；若外部有 const 指针调用会编译报错，属可接受的破坏性变更
- 缓存按**指针**比对文字，无法感知"同一个 buffer 内容被改写"。
  这种场景要求调用者显式调 `mui_button_invalidate()`，限制写进头文件注释

`mui_button_touch` 在状态发生变化时（按下高亮、锁存切换）内部自动作废缓存，无需应用介入。

### 5.3 demo 改造

`demo_ui_frame()` 里每帧无条件的 `mui_button_draw()` 调用保持不变——
开销已经由 `draw()` 内部的缓存比对拦掉，界面代码不需要写脏区逻辑。

需要变化的只有一处：整屏 `mui_clear_screen()` 之后，所有按钮要 `invalidate`，
因为屏幕内容已被外部破坏。

## 6. 顺带修复：alpha 位图裁剪缺陷

[mui_draw_bitmap_alpha](../src/mui_gfx.c) 当前用裁剪后的宽度反推行宽：

```c
const uint8_t *line = data + (int32_t)(sy + row) * (sx + w) + sx;
```

`sx + w` 只在"仅左侧越界"时才等于原始行宽。当图宽大于屏宽（左右都越界）时行宽算错，
例如原宽 100、`x = -10`、屏宽 50：`sx + w = 10 + 50 = 60`，正确行宽是 100 → 整行数据错位。

P1 改造时改为裁剪前保存 `stride = w`，与 `mui_draw_bitmap` / `mui_draw_bitmap_key` 保持一致。

## 7. 明确不做的事

**不引入整屏或分块帧缓冲。**

- 320×240 的半缓冲就需要 75 KB，超出目标设备（8~20 KB RAM）一个量级
- 会破坏"无缓冲直绘"这一核心定位，以及 AA 函数"无需回读"的现有 API 形态
- P1~P3 已经拿走了该架构在目标 RAM 预算内能取得的全部收益

## 8. 验证计划

复用 `test/mui_port_sim.c` 已有的越界断言机制（`sim_record_violation`），
并直接读回 `sim_get_fb()` 校验像素。

新增用例：

**裁剪区**
- 裁剪区内填充生效、裁剪区外像素保持原值
- 矩形完全在裁剪区外 → 无任何写入且不触发 violation
- 矩形跨裁剪区边界 → 只写入交集部分
- 空裁剪区 → 全部丢弃
- 嵌套 save/restore：内层绘制不影响外层边界，恢复后行为与保存前一致
- 各图元（圆/椭圆/三角/AA 线/AA 圆）在裁剪区下的越界安全性

**脏矩形**
- 同一区域连续标记 N 次，`count` 仍为 1
- 标记超过槽位数量的零散区域后，`count <= MUI_CFG_DIRTY_N`
- 关键不变式：随机标记一批区域后，用矩形集合像素级回放，比对实际被写入像素集合，必须全部包含于脏区并集
- `MUI_CFG_DIRTY_N = 0` 时 API 可编译且恒返回 0

**alpha 位图**
- 用比屏幕宽的蒙版做左右双裁剪，逐像素比对期望值（覆盖第 6 节的缺陷）

**按需重绘**
- 状态不变时连续 `draw()`，帧缓冲内容不变
- 状态改变后 `draw()` 生效；`invalidate()` 后强制重绘

## 9. 实施顺序与改动文件

| 阶段 | 内容 | 改动文件 |
|---|---|---|
| P1 | 裁剪区 + alpha 缺陷修复 + 单测 | `src/mui.h`、`src/mui_gfx.c`、`test/test_main.c` |
| P2 | 脏矩形跟踪 + 单测 | `src/mui.h`、`src/mui_gfx.c`、`test/test_main.c` |
| P3 | button 状态缓存 + invalidate + demo 适配 | `src/mui_button.h`、`src/mui_button.c`、`app/demo_ui.c` |
| 收尾 | README 同步 | `README.md` |

每阶段独立提交，`.\build.bat` 构建 + `build\mui_test.exe` 全绿后再进入下一阶段。

README 需同步的既有偏差：目录结构里写的 `src\mui_widget.c` 实际已拆成
`src\mui_button.c/h` 与 `src\mui_label.c/h`；控件章节的"按钮（立即模式无内部状态）"
在 P3 后也不再准确。

## 10. 成本估算

| 项目 | RAM | Flash |
|---|---|---|
| 裁剪区 | 8 字节 | 约 150 B（含求交函数被复用后节省的重复代码） |
| 脏矩形（N=4） | 33 字节 | 约 350 B |
| button 状态缓存 | 约 14 字节/按钮 | 约 200 B |
| **合计** | **约 55 字节 + 14×按钮数** | **约 700 B** |

相对现有"全局变量约 10 字节"的基线，属于同量级；关闭 `MUI_CFG_DIRTY_N` 可省掉脏矩形部分。