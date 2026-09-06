#!/usr/bin/env python3
"""
图标转换脚本
将 SVG 图标转换为 PNG 格式，并生成 LVGL 可用的 C 数组
"""

import os
import sys
import argparse
from pathlib import Path

try:
    from PIL import Image
    import cairosvg
except ImportError:
    print("请安装依赖: pip install Pillow cairosvg")
    sys.exit(1)

def convert_svg_to_png(svg_file, png_file, size=48):
    """将 SVG 转换为 PNG"""
    cairosvg.svg2png(
        url=str(svg_file),
        write_to=str(png_file),
        output_width=size,
        output_height=size
    )
    print(f"转换: {svg_file} -> {png_file}")

def generate_lvgl_image(png_file, var_name):
    """生成 LVGL 可用的 C 数组"""
    img = Image.open(png_file)
    width, height = img.size

    # 转换为 RGBA
    img = img.convert('RGBA')
    pixels = img.load()

    # 生成 C 数组
    c_array = f"const LV_IMG_DSC_t {var_name} = {{\n"
    c_array += f"    .header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA,\n"
    c_array += f"    .header.w = {width},\n"
    c_array += f"    .header.h = {height},\n"
    c_array += f"    .data_size = {width * height * 4},\n"
    c_array += f"    .data = {var_name}_map,\n"
    c_array += f"}};\n\n"

    c_array += f"const uint8_t {var_name}_map[] = {{\n"

    # 像素数据
    for y in range(height):
        for x in range(width):
            r, g, b, a = pixels[x, y]
            c_array += f"    0x{b:02x}, 0x{g:02x}, 0x{r:02x}, 0x{a:02x},  // ({x},{y})\n"

    c_array += "};\n"

    return c_array

def main():
    parser = argparse.ArgumentParser(description='转换 SVG 图标为 PNG 和 LVGL C 数组')
    parser.add_argument('--size', type=int, default=48, help='图标尺寸 (默认: 48)')
    parser.add_argument('--output', type=str, default='.', help='输出目录')
    args = parser.parse_args()

    # 图标列表
    icons = [
        # 状态图标
        {'name': 'lock', 'size': 48},
        {'name': 'unlock', 'size': 48},
        {'name': 'door_open', 'size': 48},
        {'name': 'door_close', 'size': 48},

        # 功能图标
        {'name': 'settings', 'size': 32},
        {'name': 'history', 'size': 32},
        {'name': 'alarm', 'size': 32},
        {'name': 'battery', 'size': 32},
        {'name': 'wifi', 'size': 32},
        {'name': 'fingerprint', 'size': 32},
        {'name': 'password', 'size': 32},
        {'name': 'nfc', 'size': 32},
    ]

    # 创建输出目录
    output_dir = Path(args.output)
    png_dir = output_dir / 'png'
    c_dir = output_dir / 'generated'

    png_dir.mkdir(exist_ok=True)
    c_dir.mkdir(exist_ok=True)

    # 转换图标
    for icon in icons:
        svg_file = Path(f'svg/{icon["name"]}.svg')
        png_file = png_dir / f'{icon["name"]}.png'
        c_file = c_dir / f'{icon["name"]}.c'

        if not svg_file.exists():
            print(f"警告: {svg_file} 不存在，跳过")
            continue

        # 转换为 PNG
        convert_svg_to_png(svg_file, png_file, icon['size'])

        # 生成 C 数组
        var_name = f'{icon["name"]}_icon'
        c_code = generate_lvgl_image(png_file, var_name)

        with open(c_file, 'w') as f:
            f.write(f'/**\n')
            f.write(f' * @file {icon["name"]}.c\n')
            f.write(f' * @brief {icon["name"]} 图标数据\n')
            f.write(f' */\n\n')
            f.write(f'#include <lvgl/lvgl.h>\n\n')
            f.write(c_code)

        print(f"生成: {c_file}")

    print("\n转换完成！")
    print(f"PNG 文件保存在: {png_dir}")
    print(f"C 文件保存在: {c_dir}")

if __name__ == '__main__':
    main()
