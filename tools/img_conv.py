#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
图片转 MUI 位图 C 数组工具（RGB565）

用法：
    python tools/img_conv.py <图片路径> <变量名> [输出目录] [--resize WxH] [--key] [--alpha] [--inv]
    python tools/img_conv.py <图片目录> [输出目录] [--resize WxH] [--key] [--alpha] [--inv]

选项：
    --resize WxH   缩放到指定尺寸（如 --resize 64x64）
    --key          透明色键模式：PNG 透明像素（alpha<128）转为 MUI_MAGENTA，
                   绘制时用 mui_image_draw_key(..., MUI_MAGENTA) 即可透出背景
    --alpha        生成 8bpp alpha 蒙版资源（mui_image_mask_t）
    --inv          alpha 模式下亮度反相（适合白底深色图案）

批量模式：
    第一个参数为目录时，转换目录下所有图片（png/jpg/jpeg/bmp/webp），
    变量名自动取文件名（非法字符转为下划线），其余选项对所有图片生效

示例（转换单张 logo.png 为 app/img_logo.c，48x48，带透明）：
    python tools/img_conv.py logo.png img_logo app --resize 48x48 --key

示例（批量转换 icons 目录下所有图片到 app/icons）：
    python tools/img_conv.py icons app/icons --resize 24x24 --key

依赖：Pillow（pip install Pillow）
"""

import sys
import os

try:
    from PIL import Image
except ImportError:
    sys.exit('错误：需要 Pillow 库，先执行 pip install Pillow')

# -------- 批量模式支持的图片扩展名 --------
IMG_EXTS = ('.png', '.jpg', '.jpeg', '.bmp', '.webp')


def rgb565(r, g, b):
    """888 转 565"""
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def sanitize_name(stem):
    """文件名转合法 C 变量名：非法字符转下划线，数字开头加前缀"""
    name = ''.join(c if c.isalnum() else '_' for c in stem)
    if name[0].isdigit():
        name = '_' + name
    return name


def write_alpha_asset(name, var, w, h, data_lines, out_dir):
    """写出 8bpp alpha 蒙版资源（mui_image_mask_t）"""
    c_path = os.path.join(out_dir, 'img_%s.c' % var)
    h_path = os.path.join(out_dir, 'img_%s.h' % var)

    lines = []
    lines.append('/**')
    lines.append(' * @file img_%s.c' % var)
    lines.append(' * @brief 蒙版资源：%s（%dx%d，8bpp alpha，亮度即透明度）' % (var, w, h))
    lines.append(' * 由 tools/img_conv.py 生成，勿手工修改')
    lines.append(' */')
    lines.append('')
    lines.append('#include "mui.h"')
    lines.append('#include "img_%s.h"' % var)
    lines.append('')
    lines.append('/* -------- 蒙版数据（%d 字节） -------- */' % (w * h))
    lines.append('static const uint8_t %s_data[] = {' % var)
    lines.extend(data_lines)
    lines.append('};')
    lines.append('')
    lines.append('const mui_image_mask_t %s = {' % var)
    lines.append('    .w = %d,' % w)
    lines.append('    .h = %d,' % h)
    lines.append('    .data = %s_data,' % var)
    lines.append('};')

    with open(c_path, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(lines) + '\n')

    hdr = """/**
 * @file img_%s.h
 * @brief 蒙版资源 %s 声明（由 tools/img_conv.py 生成）
 */

#ifndef IMG_%s_H
#define IMG_%s_H

#include "mui.h"

extern const mui_image_mask_t %s;

#endif /* IMG_%s_H */
""" % (var, var, var.upper(), var.upper(), var, var.upper())
    with open(h_path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(hdr)

    print('已生成 %s / %s（%dx%d alpha 蒙版，%d 字节 Flash）' %
          (c_path, h_path, w, h, w * h))


def convert_image(src_path, name, out_dir, use_key, use_alpha, inv, size):
    """转换单张图片为 C 数组资源"""
    img = Image.open(src_path)
    if size:
        img = img.resize(size, Image.LANCZOS)
    w, h = img.size

    if use_alpha:
        # 8bpp alpha 蒙版
        # 有透明通道（RGBA 且存在半透明像素）：直接用 alpha 通道做蒙版（最优，保留全部边缘过渡）
        # 无透明通道：用亮度推导（黑底白线），--inv 反相适合白底深色图案
        var = name.removeprefix('img_')
        rgba = img.convert('RGBA')
        alpha_ch = rgba.split()[3]
        amin, _ = alpha_ch.getextrema()
        use_channel = amin < 255
        mp = alpha_ch.load() if use_channel else img.convert('L').load()
        data_lines = []
        for y in range(h):
            row = []
            for x in range(w):
                if use_channel:
                    a = mp[x, y]
                else:
                    a = 255 - mp[x, y] if inv else mp[x, y]
                row.append('0x%02X,' % a)
            data_lines.append('        ' + ''.join(row))
        write_alpha_asset(name, var, w, h, data_lines, out_dir)
        return

    img = img.convert('RGBA')
    px = img.load()
    values = []
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            if use_key and a < 128:
                values.append(0xF81F)  # MUI_MAGENTA 色键
            else:
                values.append(rgb565(r, g, b))

    os.makedirs(out_dir, exist_ok=True)
    c_path = os.path.join(out_dir, 'img_%s.c' % name.removeprefix('img_'))
    h_path = os.path.join(out_dir, 'img_%s.h' % name.removeprefix('img_'))
    var = name.removeprefix('img_')

    lines = []
    lines.append('/**')
    lines.append(' * @file img_%s.c' % var)
    lines.append(' * @brief 位图资源：%s（%dx%d，RGB565%s）' %
                 (var, w, h, '，透明键=MAGENTA' if use_key else ''))
    lines.append(' * 由 tools/img_conv.py 生成，勿手工修改')
    lines.append(' */')
    lines.append('')
    lines.append('#include "mui.h"')
    lines.append('#include "img_%s.h"' % var)
    lines.append('')
    lines.append('/* -------- 像素数据（%d 字节） -------- */' % (w * h * 2))
    lines.append('static const uint16_t %s_data[] = {' % var)
    row = []
    for v in values:
        row.append('0x%04X,' % v)
        if len(row) == 12:
            lines.append('        ' + ' '.join(row))
            row = []
    if row:
        lines.append('        ' + ' '.join(row))
    lines.append('};')
    lines.append('')
    lines.append('const mui_image_t %s = {' % var)
    lines.append('    .w = %d,' % w)
    lines.append('    .h = %d,' % h)
    lines.append('    .data = %s_data,' % var)
    lines.append('};')

    with open(c_path, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(lines) + '\n')

    hdr = """/**
 * @file img_%s.h
 * @brief 位图资源 %s 声明（由 tools/img_conv.py 生成）
 */

#ifndef IMG_%s_H
#define IMG_%s_H

#include "mui.h"

extern const mui_image_t %s;

#endif /* IMG_%s_H */
""" % (var, var, var.upper(), var.upper(), var, var.upper())
    with open(h_path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(hdr)

    print('已生成 %s / %s（%dx%d，%d 字节 Flash）' %
          (c_path, h_path, w, h, w * h * 2))


def main():
    # -------- 解析参数：位置参数 + 选项 --------
    argv = sys.argv[1:]
    args = []
    opts = []
    i = 0
    while i < len(argv):
        a = argv[i]
        if a.startswith('--'):
            if a == '--resize' and i + 1 < len(argv):
                opts.append(a + '=' + argv[i + 1])  # 空格形式合并为 =
                i += 2
                continue
            opts.append(a)
        else:
            args.append(a)
        i += 1

    if not args:
        print(__doc__)
        sys.exit(1)
    src_path = args[0]
    use_key = '--key' in opts
    use_alpha = '--alpha' in opts
    inv = '--inv' in opts

    size = None
    for o in opts:
        if o.startswith('--resize='):
            size = tuple(int(v) for v in o.split('=', 1)[1].lower().split('x'))

    # -------- 批量模式：第一个参数为目录 --------
    if os.path.isdir(src_path):
        out_dir = args[1] if len(args) > 1 else '.'
        files = sorted(f for f in os.listdir(src_path)
                       if os.path.splitext(f)[1].lower() in IMG_EXTS)
        if not files:
            sys.exit('错误：目录 %s 中没有找到图片（%s）' %
                     (src_path, '、'.join(IMG_EXTS)))
        print('批量转换 %d 张图片 -> %s' % (len(files), out_dir))
        os.makedirs(out_dir, exist_ok=True)
        for f in files:
            name = sanitize_name(os.path.splitext(f)[0])
            print('[%s] 变量名 img_%s' % (f, name))
            convert_image(os.path.join(src_path, f), name, out_dir,
                          use_key, use_alpha, inv, size)
        return

    # -------- 单张模式：<图片路径> <变量名> [输出目录] --------
    if len(args) < 2:
        print(__doc__)
        sys.exit(1)
    out_dir = args[2] if len(args) > 2 else '.'
    convert_image(src_path, args[1], out_dir, use_key, use_alpha, inv, size)


if __name__ == '__main__':
    main()
