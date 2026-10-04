# MUI — 单片机裸机简易图形库

面向无 RTOS、小 RAM（8~20KB 级）彩屏单片机项目的轻量 UI 库。
**直绘为主 + 可选全缓冲 / 双缓冲 / 条带缓冲**后端：零动态内存、不依赖 RTOS，
在 PC 模拟器里画好的界面，同一份代码烧进单片机即可得到相同效果。

## 特性

- 图元：矩形/圆角矩形（实心+空心）、直线（含 Bresenham 任意斜率）、圆、椭圆、三角形、单像素，全部自动裁剪，越界参数绝对安全
- 抗锯齿：Wu 直线/圆、**实心/空心（描边）圆角矩形**、**控件圆角**（进度条的槽/边框/填充，`style.aa` 默认开启）、8bpp alpha 蒙版图标、LVGL 转换字体渲染（256 级透明混合）；缓冲后端可自动回读屏幕底色，形状能压在图片/渐变上
- 图片：RGB565 不透明 / 色键透明 / 8bpp 蒙版三种绘制模式
- 字体：LVGL 导出的 8bpp 抗锯齿字体（`tools/lvgl_font_conv.py` 转换生成）；支持**软件加粗 / 下划线 / 删除线**（`MUI_TEXT_*` 标志，零额外字库）
- 控件：按钮 / 文本标签 / 进度条 —— 统一为**保留模式对象**（`init` 一次 → `set_*` 改状态 → 每帧 `draw`），只重画变化部分；按钮内置状态缓存，**状态未变时 `draw()` 直接跳过不写屏**（`mui_button_invalidate` 可强制重绘）
- 滑块：`mui_slider`（水平/垂直 · 拖动 · 可选 AA · 可选数值文字）。**实际占位 = 外接矩形向外扩圆头半径**（圆头比轨道粗，绘制前会先擦掉这块），可用 `mui_slider_get_bounds` 查询；布局时必须按这个扩过的范围避让相邻控件，否则会把邻居擦掉一条
- 裁剪区：`mui_set_clip` / `mui_reset_clip` / `mui_clip_save` / `mui_clip_restore`，所有图元（含位图、抗锯齿、文字）统一受裁剪、支持任意层嵌套，是滚动/容器类控件的地基
- **条带缓冲**（`MUI_OUTPUT_STRIP`）：RAM 里只留 `屏宽 × MUI_CFG_STRIP_H` 的一条横带（240 宽 16 行 = **7.5KB**），
  `mui_screen_frame()` 把同一份绘制代码**按带重放 N 遍** —— 8~20KB 的 MCU 也能用上"可回读底色"的缓冲后端；带高可运行期用 `mui_strip_set_height()` 调（越大越省 CPU、越费 RAM）。实测与全屏缓冲**逐像素完全一致**（原理、取舍与验证见 [docs/strip_buffer.md](docs/strip_buffer.md)）
- 脏矩形跟踪：`mui_dirty_*` 记录本帧被写入的区域（并集覆盖所有写入，只放大、不漏报），供按需重绘/局部刷新；槽位数由 `MUI_CFG_DIRTY_N` 控制（0 = 关闭且不占 RAM）
- 动画：`mui_anim.h` 提供毫秒时间基（`mui_tick_update` / `mui_tick_delta`，抗 32 位回绕）+ 8 种整数缓动（`mui_ease`）+ 补间对象（`mui_anim_*`），可把进度/位移做成平滑过渡
- 文本对齐：`mui_text_draw_rect[_ex]` 在矩形内水平对齐 + 垂直居中；`mui_label_set_align` 让保留模式标签也支持左/中/右对齐
- 容器与布局：`mui_container_*` 把子内容裁剪在矩形内（可滚动、可嵌套）；`mui_layout_*` 提供行/列/网格游标与等分、内缩、对齐工具，不用再手算坐标
- 更多控件：`mui_gauge`（圆环仪表盘）/ `mui_list`（可滚动列表）/ `mui_dropdown`（下拉框）/ `mui_popup`（模态弹窗），风格与既有控件一致（保留模式 · 按需重绘）
- 命名：统一 `mui_<对象>_<动作>[_<修饰>]`，规则见 [docs/naming.md](docs/naming.md)
- 工具链：Python 图片转换器、LVGL 字体转换器、PNG 截图导出（零依赖编码器）
- PC 模拟器：Win32 窗口实时预览，所见即所得

## 目录结构

```
├── src\        库本体（移植必拷）
│   ├── mui.h           对外 API：颜色宏 + 全部图元声明
│   ├── mui_port.h      移植接口（只需实现 4 个函数）
│   ├── mui_conf.h      编译期配置（输出后端 / 缓冲尺寸 / 脏矩形）
│   ├── mui_gfx.c       图形核心（图元 / 图片 / AA），纯整数运算
│   ├── mui_math.h      库内整数数学（isqrt / 8.8 定点开方；定义为 mui_gfx.c，供控件复用）
│   ├── mui_anim.h      动画支撑（时间基 / 缓动 / 补间；定义为 mui_gfx.c，供各控件复用）
│   ├── mui_out.c/h     输出层（直绘下是宏；全/双/条带缓冲后端负责推屏与带平移）
│   ├── mui_font.c/h    字体渲染（LVGL 转换字体，8bpp 抗锯齿 + 文本对齐）
│   ├── mui_button.c/h  按钮控件（保留模式对象 · 状态缓存按需重绘）
│   ├── mui_label.c/h   文本标签控件（增量擦旧画新 · 可选对齐）
│   ├── mui_progressbar.c/h  进度条控件（只重画变化区间）
│   ├── mui_slider.c/h  滑块控件（水平/垂直 · 拖动）
│   ├── mui_toggle.c/h  开关 / 复选 / 单选控件
│   ├── mui_gauge.c/h   圆环仪表盘控件（轨道 + 进度弧 + 中心数值）
│   ├── mui_list.c/h    可滚动列表（视口 + 触摸滚动/选中）
│   ├── mui_dropdown.c/h 下拉框（按钮 + 覆盖式列表）
│   ├── mui_popup.c/h   模态弹窗（标题 + 正文 + 按钮）
│   ├── mui_layout.c/h  容器（裁剪 + 滚动）与行/列/网格布局
│   └── mui_touch.c     触摸状态机 + 命中测试（API 声明在 mui.h）
├── app\        界面层（移植必拷）
│   └── app_ui.c/h      应用界面（与单片机工程同名同源，模拟器即实机效果）
├── demo\       控件总览 demo（不移植）：跑一次即可看到全部控件效果
│   ├── demo_ui.c/h     界面（1024x600，5 列 x 2 行面板）
│   ├── demo_bind.c     接入 Win32 模拟器宿主
│   └── demo_shot_main.c 无头截图入口（配 mui_demo_shot 目标，导出 demo_shot.png）
├── assets\     资源（移植必拷）
│   ├── *.png / *.bmp   源图（不参与编译）
│   ├── img_*.c/h       转换生成的图片资源
│   └── mui_font_harmony_os_10/32.*  LVGL 转换生成的字体资源
├── docs\       设计文档（命名规范、改造计划等）
├── simulator\  PC 模拟器（不移植）
│   └── pc_bsp\  单片机 BSP 的 PC 替身：让 app_ui.c 原样复用（只有头文件）
├── test\       单元测试（不移植；test\strip\ 为条带后端回归）
├── tools\      资源转换脚本（开发期在 PC 上用）
├── build\      编译产物
├── CMakeLists.txt
└── build.bat   一键构建（可加 run 参数直接弹模拟器）
```

## 快速开始（PC）

依赖：MinGW-w64 gcc、CMake、Python 3 + Pillow（仅资源转换需要）。

```powershell
.\build.bat                # 构建模拟器 + 测试
.\build.bat run            # 构建并弹出模拟器窗口
build\mui_test.exe         # 跑单元测试（输出 test_output.png）
build\mui_test_strip.exe   # 跑条带后端回归（带高 1~32 结果须逐像素一致）
```

模拟器操作：`ESC` 退出，`S` 截图（保存到 build\screenshot_N.png）。
改界面 → 编辑 `app\app_ui.c`；改分辨率 → `simulator\sim_config.h`。
与单片机工程同步：[tools/sync_to_mcu.ps1](tools/sync_to_mcu.ps1)（推库过去），加 `-Pull` 则反向把对端界面拉过来。

**控件总览 demo**（`build\mui_demo.exe`，1024x600）：把库支持的全部控件摆在一块屏上
（5 列 x 2 行共 10 个面板），**改完库跑一次即可目视总览**。无 GUI 环境可用
`build\mui_demo_shot.exe` 直接导出 `demo_shot.png`（与窗口版同一份界面代码）。

**同一份界面 + 四种后端**（每套各一个可执行文件，便于逐一对照行为）：

| 后端 | 窗口版 | 截图 | 测试 |
|---|---|---|---|
| 直绘（默认） | `mui_demo.exe` / `mui_sim.exe` | `demo_shot.png` | `mui_test.exe` |
| 全屏单缓冲 | `mui_sim_full.exe` | — | `mui_test_full.exe` |
| 全屏双缓冲 | `mui_sim_double.exe` | — | `mui_test_double.exe` |
| **条带缓冲** | `mui_demo_strip.exe` | `demo_shot_strip.png` | `mui_test_strip.exe` |

另有两个低配对照测试目标：`mui_test_noaa.exe`（关全局 AA）、`mui_test_nodirty.exe`（关脏矩形跟踪）。

## 移植到单片机

### 第 1 步：拷文件

`src\` + `app\` + `assets\` 三个目录全部加入工程（Keil/CubeIDE 均可）。

### 第 2 步：实现移植层

新建 `mui_port_lcd.c`，实现 [src/mui_port.h](src/mui_port.h) 的 4 个函数：

```c
#include "mui.h"

/* 屏幕初始化（你的驱动已有 init 就调它） */
void mui_port_init(void) { lcd_init(); }

/* 画单像素 */
void mui_port_draw_pixel(int16_t x, int16_t y, uint16_t c)
{
    lcd_set_window(x, y, 1, 1);
    lcd_write_data(c);
}

/* 填矩形：性能关键！务必用"开窗口+批量写"实现 */
void mui_port_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c)
{
    lcd_set_window(x, y, w, h);
    lcd_fill_color(c, (uint32_t)w * h);
}

/* 位图块：注意 stride 是源图行宽，与块宽 w 不同，必须逐行取 */
void mui_port_draw_bitmap(int16_t x, int16_t y, int16_t w, int16_t h,
                          const uint16_t *data, int16_t stride)
{
    uint8_t d[2];
    lcd_set_window(x, y, w, h);
    for (int16_t row = 0; row < h; row++) {
        const uint16_t *line = data + (uint32_t)row * stride;
        for (int16_t col = 0; col < w; col++) {
            d[0] = (uint8_t)(line[col] >> 8);   /* 高字节在前 */
            d[1] = (uint8_t)line[col];
            lcd_write_data_bytes(d, 2);
        }
    }
}
```

### 第 3 步：主循环里定时刷新

```c
#include "mui.h"
#include "app_ui.h"

volatile uint8_t frame_flag = 0;

void SysTick_Handler(void) { frame_flag = 1; }   /* 每 33ms 置位 */

int main(void)
{
    /* 外设 + 屏幕驱动初始化 */
    app_ui_init();             /* 内部完成 mui_init(屏宽, 屏高) 与首屏绘制 */

    while (1) {
        if (frame_flag) {
            frame_flag = 0;
            app_ui_frame();    /* 与模拟器完全相同的界面 */
        }
        /* 其他任务 */
    }
}
```

### 资源占用

| 项目 | 占用 |
|---|---|
| RAM | 直绘后端约 10 字节（无帧缓冲、无堆）；全屏单缓冲 +W×H×2；双缓冲 +2×；**条带缓冲只 +W×STRIP_H×2**（240×16 = 7.5KB） |
| Flash | 库代码约 8KB + 图片资源（每张 像素数×2 字节） |
| 帧率 | 240×320 全屏重绘，M0 级 MCU 30fps 无压力；条带后端按带重放，遍数 = 屏高/带高（16 行 = 20 遍），带越高越快 |

## API 一览

```c
/* 颜色：RGB565，MUI_BLACK/WHITE/RED... 20 种预定义 + 自定义 */
#define MY_BLUE MUI_RGB565(0x3F, 0x77, 0xB8)

/* 初始化与屏操作（命名规则：mui_<对象>_<动作>） */
mui_init(w, h);
mui_screen_clear(color);                         /* 清屏（受裁剪区限制） */
mui_screen_flush();                              /* 缓冲后端：帧末推屏（直绘/条带下为空操作） */
mui_screen_get_width() / mui_screen_get_height();

/* 整帧绘制通道：条带后端必须走它（其它后端等价于"画一次 + 推屏"） */
static void my_draw(void *ctx) {                 /* 必须整屏全量重画：先铺底色再画内容 */
    (void)ctx;
    mui_reset_clip();                            /* 条带模式下"全屏"== 当前条带 */
    mui_screen_clear(UI_BG);
    /* ... 画全部界面 ... */
}
mui_screen_frame(my_draw, NULL);                 /* 直绘/整帧缓冲：回调 1 次；条带：回调 N 次 */
mui_strip_set_height(16);                        /* 仅条带后端：运行期调带高（夹到编译期上限） */

/* 裁剪区：所有图元（含位图/AA/文字）的写入都被限制在区内，可嵌套 */
mui_set_clip(x, y, w, h);                        /* 设置裁剪区（自动与屏幕求交，越界安全） */
mui_reset_clip();                                /* 恢复为全屏 */
mui_rect_t c = mui_clip_save();                  /* 保存当前裁剪区（返回值在栈上） */
mui_clip_restore(c);                             /* 恢复（与 save 配对，支持任意层嵌套） */

/* 脏矩形跟踪（MUI_CFG_DIRTY_N>0 开启；=0 时下列 API 恒返回 0） */
mui_dirty_clear();                               /* 每帧绘制前清空 */
mui_dirty_mark(x, y, w, h);                      /* 主动标记一块为脏（受裁剪区限制） */
mui_dirty_count() / mui_dirty_get(i, &r);        /* 逐块查询脏区 */
if (mui_dirty_bounds(&r)) { /* 本帧有变化，只刷 r 这块 */ }

/* 图元（x,y = 左上角；w,h = 宽高；cx,cy = 圆心；r = 半径） */
mui_rect_fill / mui_rect_draw                    /* 矩形 */
mui_round_rect_fill / mui_round_rect_draw        /* 圆角矩形，r=0 直角，r=h/2 胶囊 */
mui_line_draw / mui_hline_draw / mui_vline_draw  /* 直线 */
mui_circle_draw / mui_circle_fill                /* 圆 */
mui_ellipse_draw / mui_ellipse_fill              /* 椭圆（rx==ry 退化为圆） */
mui_triangle_draw / mui_triangle_fill            /* 三角形 */
mui_pixel_draw(x, y, color);                     /* 单像素 */

/* 抗锯齿（缓冲后端自动回读屏幕底色，可压在图片/渐变上；直绘后端用传入的 bg） */
mui_color_mix(fg, bg, alpha);                    /* 0~255 混合，AA 及半透明基础 */
mui_line_draw_aa / mui_circle_draw_aa
mui_round_rect_fill_aa(x, y, w, h, r, fg, bg);   /* 抗锯齿实心圆角矩形 */
mui_round_rect_draw_aa(x, y, w, h, r, fg, bg);   /* 抗锯齿空心圆角矩形（1px 描边） */

/* 文字（LVGL 转换字体，8bpp 抗锯齿） */
mui_text_draw(x, y, "1433", &font, fg, bg, scale);
mui_text_width("1433", &font, scale);
mui_text_draw_rect(x, y, w, h, "OK", &font, fg, bg, scale, MUI_ALIGN_CENTER);
                                      /* 矩形内水平对齐（左/中/右）+ 垂直居中绘制 */

/* 文字装饰（加粗 / 下划线 / 删除线，flags 位或；不带 _ex 的旧函数 = flags 0）
 * 加粗：字形膨胀 1px，步进与包围盒不变（≥16px 字号效果较好；要更好就用粗体字重字库）
 * 下划线：基线下方 1px，贯穿整串；可能落在行框外，覆盖行数用 mui_text_height 查 */
mui_text_draw_ex(x, y, "1433", &font, fg, bg, scale,
                 MUI_TEXT_BOLD | MUI_TEXT_UNDERLINE);
mui_text_height(&font, scale, MUI_TEXT_UNDERLINE);   /* 含装饰的纵向行数 */

/* 图片 */
mui_image_draw(x, y, w, h, data);                /* RGB565 不透明 */
mui_image_draw_key(x, y, w, h, data, key);       /* key 色像素透明 */
mui_image_draw_mask(x, y, w, h, data, fg, bg);   /* 8bpp 蒙版，单色图标推荐 */

/* 控件：保留模式对象 —— init 一次 → set_* 改状态 → 每帧 draw */
mui_button_init(&btn, x, y, w, h, &style, "TEXT", &icon);
mui_button_set_font(&btn, &font);  mui_button_set_pressed(&btn, 1);
mui_button_set_style(&btn, &other_style);   /* 换配色（含文字色 fg，颜色统一在样式里，无逐色 setter） */
mui_button_draw(&btn);            mui_button_touch(&btn, ev);   /* 返回 1 = 被点击 */
mui_button_invalidate(&btn);      /* 作废绘制缓存，强制下次 draw 重绘 */

mui_label_init(&lbl, x, y, &font, fg, bg, scale);
mui_label_set_text(&lbl, "12:30");               /* 只重画差异部分 */
mui_label_set_decor(&lbl, MUI_TEXT_UNDERLINE);   /* 可选：加粗/下划线/删除线 */
mui_label_set_fg(&lbl, MUI_RED);                 /* 换字色（装饰线一起变，无需重设文本） */
mui_label_set_colors(&lbl, MUI_RED, MUI_BLACK);  /* 底色也变：用新底色擦旧再画 */
mui_label_set_align(&lbl, MUI_ALIGN_RIGHT, 120); /* 在 [x, x+120) 内右对齐（也可 CENTER） */

mui_progressbar_init(&pb, x, y, w, h, &style);   /* style.aa 默认 1：圆角边缘抗锯齿 */
mui_progressbar_set_value(&pb, 70);              /* 只重画变化区间（AA 也只混被重画的那几列） */

/* 动画（mui_anim.h）：毫秒时间基 + 缓动 + 补间 */
mui_tick_update(bsp_system_tick_ms());           /* 主循环每帧喂一次毫秒时基 */
mui_anim_update(&a, mui_tick_delta());           /* 推进补间（内部抗 32 位回绕） */
int32_t v = mui_anim_value(&a);                  /* 取当前动画值用于绘制 */
int16_t e = mui_ease(MUI_EASE_OUT_CUBIC, t256);  /* 也可只用缓动函数（t256: 0~256 = 0~1） */

/* 容器与布局（mui_layout.h）：把子内容裁在矩形内 + 顺序摆控件 */
mui_container_t box;
mui_container_init(&box, x, y, w, h);
mui_container_set_content(&box, 0, 600);         /* 内容比视口高 → 可滚动 */
mui_container_begin(&box);                       /* 子内容超出容器的部分自动裁掉 */
mui_layout_t lay;
mui_layout_init(&lay, mui_container_ox(&box), mui_container_oy(&box),
                box.w, box.h, MUI_LAYOUT_COL, 6);
mui_rect_t cell = mui_layout_next(&lay, 0, 24);  /* 取下一格（宽 0 = 占满整行） */
mui_container_end(&box);

/* 更多控件 */
mui_gauge_init(&g, cx, cy, r, 0, 100, 62, NULL); /* mui_gauge.h：圆环仪表盘 */
mui_gauge_draw(&g);                              /* 值与几何都没变时不写屏 */

mui_list_init(&lst, x, y, w, h, 24, 4);          /* mui_list.h：可滚动列表 */
mui_list_set_rows(&lst, 20);
mui_list_begin(&lst); /* 逐行画内容 */ mui_list_end(&lst);
/* 触摸：uint8_t chg = mui_list_touch(&lst, ev); 位或 SEL/SCROLL 变化 */

mui_dropdown_init(&dd, x, y, w, h, items, n, 24, font, NULL);  /* mui_dropdown.h */
mui_dropdown_draw(&dd);   mui_dropdown_touch(&dd, ev);   /* 展开时画在最后 */

mui_popup_init(&pp, x, y, w, h, "标题", "正文", font);   /* mui_popup.h：模态弹窗 */
mui_popup_add_button(&pp, "确定", NULL);
mui_popup_open(&pp);
mui_popup_draw(&pp);      mui_popup_touch(&pp, ev);      /* 画在最上层 */
```

按钮样式（画之前配置好）：

```c
static const mui_button_style_t my_style = {
    .bg       = MUI_RED,               /* 弹起底色 */
    .bg_press = MUI_MAROON,            /* 按下底色 */
    .fg       = MUI_WHITE,             /* 文字/图标色 */
    .border   = MUI_BLACK,             /* 边框色（同 bg = 无边框） */
    .shape    = MUI_BUTTON_SHAPE_PILL,    /* ROUND 圆角 / RECT 直角 / PILL 胶囊 */
    .radius   = 4,                     /* 圆角半径，<=0 自动 */
};
```

## 资源工具链

### 图片转换（tools/img_conv.py）

```powershell
python tools\img_conv.py assets\图标.png 图标名 assets [--resize 宽x高] [--key | --alpha] [--inv]
```

| 素材类型 | 选项 | 说明 |
|---|---|---|
| 透明底 PNG（推荐） | `--alpha` | alpha 通道直取蒙版，全 256 级过渡保留 |
| 黑底白线图标 | `--alpha` | 亮度即透明度 |
| 白底深色图案 | `--alpha --inv` | 亮度反相 |
| 彩色图片 | （无） | RGB565，不透明 |
| 彩色图 + 透明 | `--key` | 透明像素烙成品红键，边缘为硬边 |

变量名 = 命令第 2 个参数，文件自动加 `img_` 前缀，生成后 `#include "img_名字.h"`。

**图标规格建议**：按实际显示尺寸 1:1 制作（不要靠 --resize 缩）、带透明通道或灰阶渐变（素材质量决定显示上限）。

### 字体转换（tools/lvgl_font_conv.py）

LVGL 字体工具（lvgl.fluentui.co）导出 .c 后：

```powershell
python tools\lvgl_font_conv.py 字体.c 名字 assets
```

## 架构说明

```
app\（界面代码）──┐
                 ├──> src\（库核心，纯算法）──> mui_port.h（4 个函数）──> 你的屏幕驱动
simulator\（PC）─┘                                                    └─ 单片机 LCD / Win32 窗口
```

- **输出后端**（`mui_conf.h` 选）：直绘 = 图元直接写屏、零 RAM 代价，但抗锯齿函数必须显式传背景色；
  单/双缓冲 = 先画进静态帧缓冲、帧末 `mui_screen_flush()` 推屏（能回读背景，抗锯齿可自动取底）；
  **条带缓冲** = 只留一条横带，`mui_screen_frame()` 把绘制回调按带重放，每带画完立刻推屏。
  注意：全屏缓冲后端须把 `MUI_CFG_BUF_MAX_W/H` 设为屏幕尺寸（模拟器侧见 CMakeLists 的 `MUI_SIM_BUF_W/H`，
  `simulator/sim_config.h` 有编译期校验，忘记同步会直接报错而不是被 `mui_init()` 静默钳制）；
  条带后端只受 `MUI_CFG_BUF_MAX_W` 约束（屏高可以远大于缓冲）
- **条带模式的写法**（与其它后端不同，务必注意）：
  1. 必须用 `mui_screen_frame(绘制回调, ctx)` 驱动，不能"直接画 + `mui_screen_flush()`"；
  2. 回调必须能**整屏全量重画**（先铺满底色再画全部内容），因为它会被反复调用；
     1、2 两条合起来意味着"只画变化部分"的增量式界面（如 `app\app_ui.c`）需要先改成可重放的全量绘制
     （`demo\demo_ui.c` 是两种写法并存的样板：`ui_draw_all()` 可重放，非条带后端仍走增量）；
  3. 控件的 `set_*` 在条带模式下**只改状态不写屏**，`xxx_draw()` 每次全量重绘（缓存跳过失效）——
     这对上层透明，无需改调用代码；
  4. 回调内的 `mui_reset_clip()` 恢复到的是**当前条带**而不是整屏，所以既有的"清屏 + 全屏重绘"代码可直接复用
- **保留模式控件**：按钮/标签/进度条都是 `init` 一次 + `set_*` 改状态 + 每帧 `draw`，
  只重画变化部分；按键或触摸只负责改状态，天然适配裸机"主循环轮询 + 每帧重画"
- 与 LVGL 的适用分界：RAM 买得起全/半帧缓冲（≥64KB）用 LVGL；小 RAM 裸机用本库

## 已知限制

- **文字只支持单字节字符（ASCII / Latin-1）**：字形查找按**字节**进行（`mui_font_find_glyph`
  把 `char` 当码点用），且现有四套字库的字符段都只有 32/33~126。中文等多字节文本要能用，
  需要（1）渲染层解码 UTF-8 + 按码点查表，（2）用 LVGL 工具导出**含中文段**的字库；
  两者目前都没有，所以中文会被整段跳过（不显示、也不占宽度）
- **条带后端要求"绘制可重放"**：界面必须能整屏全量重画，且绘制只能发生在 `mui_screen_frame()` 的回调里
  （帧外写屏会落进条带缓冲、被下一次推带带出去）。`app\app_ui.c` 目前是逐处局部重画的增量写法，
  要用条带后端需先加一个"按当前页全量重画"的 `app_ui_draw()`；`demo\demo_ui.c` 已经是可重放的写法
- **同一界面在不同后端上，抗锯齿边缘可能差几个像素**：缓冲/条带后端能回读屏上真实底色，
  直绘后端只能用手传的 `bg`。实测 demo 截图差异为 60 个 AA 边缘像素（条带 vs 全屏缓冲 = 0 差异）
- AA 函数在复杂（非纯色）背景上会有可见瑕疵——直绘架构固有边界
- 蒙版图标渲染会丢弃原色，统一用前景色染色；彩色图标需走 RGB565+key（边缘硬）
- 图片不压缩，Flash 占用 = 像素数 × 2 字节，大图慎用
- `app_ui_frame()` 含 static 帧计数，不要在主循环和中断两个上下文同时调用
