# MUI — 单片机裸机简易图形库

面向无 RTOS、小 RAM（8~20KB 级）彩屏单片机项目的轻量 UI 库。
**直绘 + 立即模式**架构：零动态内存、零帧缓冲、无 RTOS 依赖，
在 PC 模拟器里画好的界面，同一份代码烧进单片机即可得到相同效果。

## 特性

- 图元：矩形/圆角矩形（实心+空心）、直线（含 Bresenham 任意斜率）、圆、椭圆、三角形，全部自动裁剪，越界参数绝对安全
- 抗锯齿：Wu 直线/圆、8bpp alpha 蒙版图标、LVGL 字体渲染（256 级透明混合）
- 位图：RGB565 不透明/色键透明、alpha 蒙版三种绘制模式
- 字体：内置 5×7 ASCII 点阵（可缩放/任意前景色）；支持 LVGL 导出字体转换
- 控件：按钮（形状/配色/图标可配置，立即模式无内部状态）
- 工具链：Python 图片转换器、LVGL 字体转换器、PNG 截图导出（零依赖编码器）
- PC 模拟器：Win32 窗口实时预览，所见即所得

## 目录结构

```
├── src\        库本体（移植必拷）
│   ├── mui.h           对外 API：颜色宏 + 全部图元/控件声明
│   ├── mui_port.h      移植接口（只需实现 4 个函数）
│   ├── mui_gfx.c       图形核心，纯整数运算
│   ├── mui_font.c/h    5×7 点阵字体
│   └── mui_widget.c    按钮控件
├── app\        界面层（移植必拷）
│   └── demo_ui.c/h     示例界面（只调 MUI API，平台无关）
├── assets\     资源（移植必拷）
│   ├── *.png / *.bmp   源图（不参与编译）
│   ├── img_*.c/h       转换生成的图片资源
│   └── mui_font_hm24.* LVGL 转换生成的字体资源
├── simulator\  PC 模拟器（不移植）
├── test\       单元测试（不移植）
├── tools\      资源转换脚本（开发期在 PC 上用）
├── build\      编译产物
├── CMakeLists.txt
└── build.bat   一键构建（可加 run 参数直接弹模拟器）
```

## 快速开始（PC）

依赖：MinGW-w64 gcc、CMake、Python 3 + Pillow（仅资源转换需要）。

```powershell
.\build.bat          # 构建模拟器 + 测试
.\build.bat run      # 构建并弹出模拟器窗口
build\mui_test.exe   # 跑单元测试（输出 test_output.png）
```

模拟器操作：`ESC` 退出，`S` 截图（保存到 build\screenshot_N.png）。
改界面 → 编辑 `app\demo_ui.c` 的 `demo_ui_frame()`；改分辨率 → `simulator\sim_config.h`。

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
#include "demo_ui.h"

volatile uint8_t frame_flag = 0;

void SysTick_Handler(void) { frame_flag = 1; }   /* 每 33ms 置位 */

int main(void)
{
    /* 外设 + 屏幕驱动初始化 */
    mui_init(320, 320);        /* 屏幕分辨率 */

    while (1) {
        if (frame_flag) {
            frame_flag = 0;
            demo_ui_frame();   /* 与模拟器完全相同的界面 */
        }
        /* 其他任务 */
    }
}
```

### 资源占用

| 项目 | 占用 |
|---|---|
| RAM | 全局变量约 10 字节（无帧缓冲、无堆） |
| Flash | 库代码约 8KB + 图片资源（每张 像素数×2 字节） |
| 帧率 | 240×320 全屏重绘，M0 级 MCU 30fps 无压力 |

## API 一览

```c
/* 颜色：RGB565，MUI_BLACK/WHITE/RED... 20 种预定义 + 自定义 */
#define MY_BLUE MUI_RGB565(0x3F, 0x77, 0xB8)

/* 初始化与清屏 */
mui_init(w, h);  mui_clear_screen(color);

/* 图元（x,y = 左上角；w,h = 宽高；cx,cy = 圆心；r = 半径） */
mui_fill_rect / mui_draw_rect                    /* 矩形 */
mui_fill_round_rect / mui_draw_round_rect        /* 圆角矩形，r=0 直角，r=h/2 胶囊 */
mui_draw_line / mui_draw_hline / mui_draw_vline  /* 直线 */
mui_draw_circle / mui_fill_circle                /* 圆 */
mui_draw_ellipse / mui_fill_ellipse              /* 椭圆（rx==ry 退化为圆） */
mui_draw_triangle / mui_fill_triangle            /* 三角形 */

/* 抗锯齿（需传背景色，适合纯色背景） */
mui_color_mix(fg, bg, alpha);                    /* 0~255 混合，AA 及半透明基础 */
mui_draw_line_aa / mui_draw_circle_aa

/* 文字（5×7 点阵，scale_x/scale_y 缩放） */
mui_font_draw_text(x, y, "TEXT", color, scale_x, scale_y);
mui_font_text_width("TEXT", scale_x, scale_y);

/* LVGL 转换字体（8bpp 抗锯齿） */
mui_lv_font_draw_text(x, y, "1433", &hm24, fg, bg, scale);
mui_lv_font_text_width("1433", &hm24, scale);

/* 位图（RGB565） */
mui_draw_bitmap(x, y, w, h, data);               /* 不透明 */
mui_draw_bitmap_key(x, y, w, h, data, key);      /* key 色像素透明 */

/* 位图（8bpp alpha 蒙版，单色图标推荐） */
mui_draw_bitmap_alpha(x, y, w, h, data, fg, bg);

/* 按钮控件（立即模式：无内部状态，每帧调用） */
mui_button(x, y, w, h, "TEXT", pressed, &style);           /* 纯文字 */
mui_button_icon(x, y, w, h, &icon, "TEXT", pressed, &style); /* 图标+文字 */
```

按钮样式（画之前配置好）：

```c
static const mui_button_style_t my_style = {
    .bg       = MUI_RED,               /* 弹起底色 */
    .bg_press = MUI_MAROON,            /* 按下底色 */
    .fg       = MUI_WHITE,             /* 文字/图标色 */
    .border   = MUI_BLACK,             /* 边框色（同 bg = 无边框） */
    .shape    = MUI_BTN_SHAPE_PILL,    /* ROUND 圆角 / RECT 直角 / PILL 胶囊 */
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

- **直绘模式**：每个图元直接写入屏幕，无帧缓冲。RAM 占用≈0，代价是抗锯齿函数需要显式传背景色
- **立即模式控件**：按钮无内部状态，`pressed` 等状态由外部传入，天然适配裸机"主循环轮询按键 + 每帧重画"
- 与 LVGL 的适用分界：RAM 买得起全/半帧缓冲（≥64KB）用 LVGL；小 RAM 裸机用本库

## 已知限制

- AA 函数在复杂（非纯色）背景上会有可见瑕疵——直绘架构固有边界
- 蒙版图标渲染会丢弃原色，统一用前景色染色；彩色图标需走 RGB565+key（边缘硬）
- 图片不压缩，Flash 占用 = 像素数 × 2 字节，大图慎用
- `demo_ui_frame()` 含 static 帧计数，不要在主循环和中断两个上下文同时调用
