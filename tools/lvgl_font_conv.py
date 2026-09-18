#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
LVGL 字体转 MUI 格式工具

用法：
    python tools/lvgl_font_conv.py <lvgl_font.c> <输出名> [输出目录]

示例（把 hm24.c 转换到 app 目录，字体名 hm24）：
    python tools/lvgl_font_conv.py hm24.c hm24 app

限制：仅支持 lv_font_conv 常规导出（--no-compress --stride 1，
     8bpp/4bpp/1bpp，FORMAT0_TINY 连续字符段），bpp 值会写入注释，
     当前渲染器仅实现 8bpp。
"""

import re
import sys
import os


def parse_lvgl_font(path):
    """解析 LVGL 字体 C 文件，返回数据字典"""
    with open(path, encoding='utf-8') as f:
        src = f.read()

    data = {}

    # 位图数据
    m = re.search(r'glyph_bitmap\[\]\s*=\s*\{(.*?)\};', src, re.S)
    if not m:
        sys.exit('错误：未找到 glyph_bitmap[]')
    data['bitmap'] = [int(b, 16) for b in re.findall(r'0x[0-9a-fA-F]+', m.group(1))]

    # 字形描述（跳过 id=0 保留项：通过 adv_w=0 识别）
    dscs = re.findall(
        r'\{\.bitmap_index\s*=\s*(\d+),\s*\.adv_w\s*=\s*(\d+),\s*'
        r'\.box_w\s*=\s*(\d+),\s*\.box_h\s*=\s*(\d+),\s*'
        r'\.ofs_x\s*=\s*(-?\d+),\s*\.ofs_y\s*=\s*(-?\d+)\}', src)
    data['dscs'] = [tuple(map(int, d)) for d in dscs]

    # 字符映射（仅支持单个连续段）
    m = re.search(
        r'\.range_start\s*=\s*(\d+),\s*\.range_length\s*=\s*(\d+),\s*'
        r'\.glyph_id_start\s*=\s*(\d+)', src)
    if not m:
        sys.exit('错误：未找到 cmap 或 cmap 不是 FORMAT0_TINY 连续段')
    data['range_start'], data['range_length'], data['glyph_id_start'] = map(int, m.groups())

    # 行高与基线
    data['line_height'] = int(re.search(r'\.line_height\s*=\s*(\d+)', src).group(1))
    m = re.search(r'\.base_line\s*=\s*(-?\d+)', src)
    data['base_line'] = int(m.group(1)) if m else 0

    # bpp
    data['bpp'] = int(re.search(r'\.bpp\s*=\s*(\d+)', src).group(1))

    # 字号（从文件头注释提取）
    m = re.search(r'Size:\s*(\d+)\s*px', src)
    data['size'] = int(m.group(1)) if m else 0
    return data


def emit_c(data, name):
    """生成 MUI 格式 C 源码"""
    gs = data['glyph_id_start']
    glyphs = []
    for i in range(data['range_length']):
        g = data['dscs'][gs + i]
        glyphs.append(g)

    lines = []
    lines.append('/**')
    lines.append(' * @file mui_font_%s.c' % name)
    lines.append(' * @brief LVGL 转换字体：%s（%dpx，%dbpp，U+%04X-U+%04X）' %
                 (name, data['size'], data['bpp'],
                  data['range_start'], data['range_start'] + data['range_length'] - 1))
    lines.append(' * 由 tools/lvgl_font_conv.py 生成，勿手工修改')
    lines.append(' */')
    lines.append('')
    lines.append('#include "mui_font.h"')
    lines.append('#include "mui_font_%s.h"' % name)
    lines.append('')
    lines.append('/* -------- 字形位图（%d 字节） -------- */' % len(data['bitmap']))
    lines.append('static const uint8_t %s_bitmap[] = {' % name)
    row = []
    for b in data['bitmap']:
        row.append('0x%02x,' % b)
        if len(row) == 16:
            lines.append('    ' + ''.join(row))
            row = []
    if row:
        lines.append('    ' + ''.join(row))
    lines.append('};')
    lines.append('')
    lines.append('/* -------- 字形描述 -------- */')
    lines.append('static const mui_glyph_dsc_t %s_glyphs[] = {' % name)
    for bi, adv, bw, bh, ox, oy in glyphs:
        lines.append('    {%d, %d, %d, %d, %d, %d},' % (bi, adv, bw, bh, ox, oy))
    lines.append('};')
    lines.append('')
    lines.append('const mui_lv_font_t %s = {' % name)
    lines.append('    .bitmap = %s_bitmap,' % name)
    lines.append('    .glyphs = %s_glyphs,' % name)
    lines.append('    .first_char = %d,' % data['range_start'])
    lines.append('    .last_char = %d,' % (data['range_start'] + data['range_length'] - 1))
    lines.append('    .line_height = %d,' % data['line_height'])
    lines.append('    .base_line = %d,' % data['base_line'])
    lines.append('};')
    return '\n'.join(lines) + '\n'


def emit_h(name):
    """生成对应头文件"""
    return """/**
 * @file mui_font_%s.h
 * @brief LVGL 转换字体 %s 声明（由 tools/lvgl_font_conv.py 生成）
 */

#ifndef MUI_FONT_%s_H
#define MUI_FONT_%s_H

#include "mui_font.h"

extern const mui_lv_font_t %s;

#endif /* MUI_FONT_%s_H */
""" % (name, name, name.upper(), name.upper(), name, name.upper())


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)
    src_path, name = sys.argv[1], sys.argv[2]
    out_dir = sys.argv[3] if len(sys.argv) > 3 else '.'

    data = parse_lvgl_font(src_path)
    os.makedirs(out_dir, exist_ok=True)
    c_path = os.path.join(out_dir, 'mui_font_%s.c' % name)
    h_path = os.path.join(out_dir, 'mui_font_%s.h' % name)
    with open(c_path, 'w', encoding='utf-8') as f:
        f.write(emit_c(data, name))
    with open(h_path, 'w', encoding='utf-8') as f:
        f.write(emit_h(name))
    print('已生成 %s / %s（%d 个字形，U+%04X 起）' %
          (c_path, h_path, data['range_length'], data['range_start']))


if __name__ == '__main__':
    main()
