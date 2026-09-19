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

    # 字符映射段（LVGL 对非连续字符集会拆成多个 range；FORMAT0_FULL 段带
    # glyph_id_ofs_list 偏移表，段内字形非连续递增，须逐字符用 gs+ofs[i] 定位）
    # 1) 收集所有命名偏移表（名字可能是 glyph_id_ofs_list 或带 _0 等后缀）
    ofs_arrays = {}
    for name, body in re.findall(
            r'(glyph_id_ofs_list\w*)\[\s*\]\s*=\s*\{(.*?)\}', src, re.S):
        ofs_arrays[name] = [int(x) for x in re.findall(r'\d+', body)]

    # 2) 解析每段 cmap（rs/rl/gs + 所属偏移表名）
    cmaps = re.findall(
        r'\.range_start\s*=\s*(\d+)\s*,\s*\.range_length\s*=\s*(\d+)\s*,\s*'
        r'\.glyph_id_start\s*=\s*(\d+),'
        r'[\s\S]*?\.glyph_id_ofs_list\s*=\s*(\w+)', src)
    if not cmaps:
        sys.exit('错误：未找到 cmap 或 cmap 不是 FORMAT0')
    segs = []
    for rs, rl, gs, name in cmaps:
        if name == 'NULL':
            segs.append((int(rs), int(rl), int(gs), None))
        else:
            ofs = ofs_arrays.get(name)
            # 自检：声明了偏移表却解析不到 -> 拒绝生成错误文件
            if ofs is None:
                sys.exit('错误：段 U+%04X 声明偏移表 %s，但未能解析，请确认源文件包含该表' % (int(rs), name))
            segs.append((int(rs), int(rl), int(gs), ofs))
    data['cmaps'] = segs
    # 兼容旧单段字段（首段，供参考）
    data['range_start'] = data['cmaps'][0][0]
    data['range_length'] = data['cmaps'][0][1]
    data['glyph_id_start'] = data['cmaps'][0][2]

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
    # 遍历映射段逐字符取字形。
    # FULL 段用 glyph_id_start+ofs[i]、TINY 段用 +i，保证段内字符连续排列，
    # 渲染端按 (first,last)+段内偏移索引即可。
    glyphs = []
    cmaps = []
    for rs, rl, gs, ofs in data['cmaps']:
        seg_start = len(glyphs)
        for i in range(rl):
            gid = gs + (ofs[i] if ofs else i)
            glyphs.append(data['dscs'][gid])
        cmaps.append((rs, rs + rl - 1, seg_start))

    # 头注释：列出所有映射段
    range_note = ','.join('U+%04X-U+%04X' % (rs, rl_end)
                          for rs, rl_end, _ in cmaps)

    lines = []
    lines.append('/**')
    lines.append(' * @file mui_font_%s.c' % name)
    lines.append(' * @brief LVGL 转换字体：%s（%dpx，%dbpp，映射 %d 段：%s）' %
                 (name, data['size'], data['bpp'], len(cmaps), range_note))
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
    lines.append('/* -------- 字形描述（按映射段顺序排列） -------- */')
    lines.append('static const mui_glyph_dsc_t %s_glyphs[] = {' % name)
    for bi, adv, bw, bh, ox, oy in glyphs:
        lines.append('    {%d, %d, %d, %d, %d, %d},' % (bi, adv, bw, bh, ox, oy))
    lines.append('};')
    lines.append('')
    lines.append('/* -------- 字符映射段 -------- */')
    lines.append('static const mui_lv_font_cmap_t %s_cmaps[] = {' % name)
    for first, last, gid in cmaps:
        lines.append('    {%d, %d, %d},' % (first, last, gid))
    lines.append('};')
    lines.append('')
    lines.append('const mui_lv_font_t %s = {' % name)
    lines.append('    .bitmap = %s_bitmap,' % name)
    lines.append('    .glyphs = %s_glyphs,' % name)
    lines.append('    .cmaps = %s_cmaps,' % name)
    lines.append('    .cmap_count = %d,' % len(cmaps))
    lines.append('    .first_char = %d,' % cmaps[0][0])
    lines.append('    .last_char = %d,' % cmaps[-1][1])
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
