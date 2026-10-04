/**
 * @file mui_conf.h
 * @brief MUI 编译期配置（库内唯一配置文件，随整套库文件一起拷贝）
 *
 * 所有宏在本文件中给出默认值，且每项都有 #ifndef 保护：
 *   - 直接改本文件 → 生效（配置跟着库走，库拷到哪、配置就到哪）；
 *   - 或用编译选项 -DMUI_CFG_XXX=值 覆盖（命令行优先；PC 模拟器的
 *     多种后端对照构建就靠这种方式，见根目录 CMakeLists.txt）。
 * 注意：受同步脚本管理的工程里，本文件由库源覆盖 —— 改配置请改库源。
 *
 * 静态 RAM 对照（128x128 RGB565）：
 *   MUI_OUTPUT_DIRECT       0 字节（默认），绘制直接写屏
 *   MUI_OUTPUT_FULL         1 * W * H * 2 = 32KB
 *   MUI_OUTPUT_FULL_DOUBLE  2 * W * H * 2 = 64KB
 *   MUI_OUTPUT_STRIP        W * STRIP_H * 2（只留一条横带，见 MUI_CFG_STRIP_H）
 *
 * 缓冲模式的用法：绘制照旧，但在"本帧全部绘制完成后"调用一次 mui_screen_flush()
 * （直绘模式下该宏为空，应用代码可无条件写它）。
 *
 * 条带模式（MUI_OUTPUT_STRIP）是另一套用法：RAM 放不下整帧时，只留一条横带的
 * 缓冲，**同一份绘制代码按带重跑 N 遍**（每遍只保留该带的像素）。因此必须用
 * mui_screen_frame(绘制函数, ctx) 代替"直接绘制 + mui_screen_flush()"，
 * 且绘制函数要能整屏全量重画（见 mui.h 里 mui_screen_frame 的说明）。
 */

#ifndef MUI_CONF_H
#define MUI_CONF_H

/* -------- 输出后端 -------- */

#define MUI_OUTPUT_DIRECT       0   /**< 直绘：无缓冲，逐图元写屏（默认） */
#define MUI_OUTPUT_FULL         1   /**< 全屏单缓冲：帧末推脏区 */
#define MUI_OUTPUT_FULL_DOUBLE  2   /**< 全屏双缓冲：帧末推整帧后交换 */
#define MUI_OUTPUT_STRIP        3   /**< 条带缓冲：只留一条横带，按带重跑绘制 */

/** @brief 输出后端选择（取上面 MUI_OUTPUT_xxx 之一）
 *  @note  库默认直绘（零 RAM，拷到任何工程都能直接跑）。某个工程要用缓冲后端时，
 *         **在那个工程里覆盖**，不要改这里的默认值：
 *           Keil     ：Options → C/C++ → Define 里加 `MUI_CFG_OUTPUT_MODE=1`
 *           命令行   ：`-DMUI_CFG_OUTPUT_MODE=1`
 *         工程级配置不会被同步脚本覆盖；改库默认值则会连带影响所有用这套库的工程。 */
#ifndef MUI_CFG_OUTPUT_MODE
#define MUI_CFG_OUTPUT_MODE     MUI_OUTPUT_DIRECT
#endif

/* -------- 缓冲几何 -------- */

/** @brief 缓冲支持的最大宽度（缓冲按它静态分配，必须 >= 实际屏幕宽度）
 *  @note  单屏项目里直接填屏幕宽度即可；改屏后要与 bsp 的屏幕尺寸宏保持一致
 *         （工程侧有编译期校验兜底，忘记同步会在编译时报错，而不是运行期
 *         被 mui_init() 静默钳制） */
#ifndef MUI_CFG_BUF_MAX_W
#define MUI_CFG_BUF_MAX_W       128
#endif

/** @brief 缓冲支持的最大高度（同上，填屏幕高度） */
#ifndef MUI_CFG_BUF_MAX_H
#define MUI_CFG_BUF_MAX_H       128
#endif

/** @brief 全屏单缓冲的脏矩形优化：1=只推改动过的包围盒，0=整块推屏
 *  @note  双缓冲后端不使用该开关（固定推整帧） */
#ifndef MUI_CFG_DIRTY
#define MUI_CFG_DIRTY           1
#endif

/* -------- 抗锯齿总开关 -------- */

/** @brief 全局抗锯齿开关：1=开启（默认），0=关闭
 *  @note  置 0 时所有 *_aa 图元（圆弧/圆环/圆/圆角矩形/直线）与进度条的
 *         style.aa 一律退化为对应的硬边实现，不再做逐像素覆盖率与颜色混合，
 *         用于低配 MCU 或量产固件省 CPU。改这一处即可全局生效。 */
#ifndef MUI_CFG_AA
#define MUI_CFG_AA              1
#endif

/* -------- 脏矩形跟踪 -------- */

/** @brief 脏区槽位数量：>0 开启脏矩形跟踪，0 = 关闭
 *  @note  条带后端下本项纯属额外开销（帧循环暂未消费脏区），追求极致可置 0。
 *  @note  开启后每个像素/矩形/位图绘制都会尝试把写入区域并入脏区列表
 *         （只放大、不漏报），供"按需重绘 / 局部刷新"查询；关闭时列表数组
 *         不占 RAM，`mui_dirty_*` API 仍然存在但恒返回 0。 */
#ifndef MUI_CFG_DIRTY_N
#define MUI_CFG_DIRTY_N         4
#endif

/* -------- 条带缓冲 -------- */

/** @brief 条带缓冲的带高（行数）：仅 MUI_OUTPUT_STRIP 使用
 *  @note  静态分配 = MUI_CFG_BUF_MAX_W * MUI_CFG_STRIP_H * 2 字节。
 *         带越高 → 重放遍数越少 → 越快，但 RAM 越大：
 *           240 宽屏：16 行 = 7.5KB / 8 行 = 3.75KB / 32 行 = 15KB
 *         运行期可用 mui_strip_set_height() 临时调小（<= 本值），方便按帧率/内存权衡。
 *  @note  带高必须 >= 1；建议取 8 的倍数，且让屏高能被它整除以减少末带浪费。 */
#ifndef MUI_CFG_STRIP_H
#define MUI_CFG_STRIP_H         16
#endif

/* -------- 派生宏（不要覆盖） -------- */

/** @brief 是否启用缓冲后端（0 = 直绘） */
#define MUI_CFG_HAS_BUFFER      (MUI_CFG_OUTPUT_MODE != MUI_OUTPUT_DIRECT)

/** @brief 是否为条带后端（1 = 每帧按带重放绘制，见 mui_screen_frame） */
#define MUI_CFG_IS_STRIP        (MUI_CFG_OUTPUT_MODE == MUI_OUTPUT_STRIP)

/* -------- 编译期校验 -------- */

#if (MUI_CFG_OUTPUT_MODE != MUI_OUTPUT_DIRECT) \
    && (MUI_CFG_OUTPUT_MODE != MUI_OUTPUT_FULL) \
    && (MUI_CFG_OUTPUT_MODE != MUI_OUTPUT_FULL_DOUBLE) \
    && (MUI_CFG_OUTPUT_MODE != MUI_OUTPUT_STRIP)
#error "MUI_CFG_OUTPUT_MODE 必须是 MUI_OUTPUT_DIRECT/FULL/FULL_DOUBLE/STRIP 之一"
#endif

#if MUI_CFG_IS_STRIP && (MUI_CFG_STRIP_H < 1)
#error "MUI_CFG_STRIP_H 必须大于 0"
#endif

#if (MUI_CFG_BUF_MAX_W < 1) || (MUI_CFG_BUF_MAX_H < 1)
#error "MUI_CFG_BUF_MAX_W / MUI_CFG_BUF_MAX_H 必须大于 0"
#endif

#endif /* MUI_CONF_H */
