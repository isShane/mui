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
 *
 * 缓冲模式的用法：绘制照旧，但在"本帧全部绘制完成后"调用一次 mui_screen_flush()
 * （直绘模式下该宏为空，应用代码可无条件写它）。
 */

#ifndef MUI_CONF_H
#define MUI_CONF_H

/* -------- 输出后端 -------- */

#define MUI_OUTPUT_DIRECT       0   /**< 直绘：无缓冲，逐图元写屏（默认） */
#define MUI_OUTPUT_FULL         1   /**< 全屏单缓冲：帧末推脏区 */
#define MUI_OUTPUT_FULL_DOUBLE  2   /**< 全屏双缓冲：帧末推整帧后交换 */

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

/* -------- 派生宏（不要覆盖） -------- */

/** @brief 是否启用缓冲后端（0 = 直绘） */
#define MUI_CFG_HAS_BUFFER      (MUI_CFG_OUTPUT_MODE != MUI_OUTPUT_DIRECT)

/* -------- 编译期校验 -------- */

#if (MUI_CFG_OUTPUT_MODE != MUI_OUTPUT_DIRECT) \
    && (MUI_CFG_OUTPUT_MODE != MUI_OUTPUT_FULL) \
    && (MUI_CFG_OUTPUT_MODE != MUI_OUTPUT_FULL_DOUBLE)
#error "MUI_CFG_OUTPUT_MODE 必须是 MUI_OUTPUT_DIRECT/FULL/FULL_DOUBLE 之一"
#endif

#if (MUI_CFG_BUF_MAX_W < 1) || (MUI_CFG_BUF_MAX_H < 1)
#error "MUI_CFG_BUF_MAX_W / MUI_CFG_BUF_MAX_H 必须大于 0"
#endif

#endif /* MUI_CONF_H */
