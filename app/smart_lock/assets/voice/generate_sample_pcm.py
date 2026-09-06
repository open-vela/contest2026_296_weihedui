#!/usr/bin/env python3
"""
示例 PCM 文件生成脚本
用于测试和调试
"""

import struct
import math
import os
from pathlib import Path

# 语音规格
SAMPLE_RATE = 16000  # 采样率 16000Hz
BIT_DEPTH = 16       # 位深 16bit
CHANNELS = 1         # 单声道

def generate_sine_wave(frequency, duration, sample_rate):
    """生成正弦波"""
    num_samples = int(sample_rate * duration)
    samples = []

    for i in range(num_samples):
        t = i / sample_rate
        value = int(32767 * math.sin(2 * math.pi * frequency * t))
        samples.append(value)

    return samples

def generate_silence(duration, sample_rate):
    """生成静音"""
    num_samples = int(sample_rate * duration)
    return [0] * num_samples

def generate_beep(frequency, duration, sample_rate):
    """生成蜂鸣声"""
    num_samples = int(sample_rate * duration)
    samples = []

    for i in range(num_samples):
        t = i / sample_rate
        # 生成方波
        if math.sin(2 * math.pi * frequency * t) >= 0:
            value = 16000
        else:
            value = -16000
        samples.append(value)

    return samples

def save_pcm_file(filepath, samples):
    """保存 PCM 文件"""
    with open(filepath, 'wb') as f:
        for sample in samples:
            # 写入 16 位有符号整数（小端序）
            f.write(struct.pack('<h', sample))

def generate_door_status_voice(text, duration, output_dir):
    """生成门状态语音（简化版本）"""
    print(f"生成: {text}")

    # 生成简单的音调序列
    samples = []

    # 开始提示音
    samples.extend(generate_beep(800, 0.1, SAMPLE_RATE))
    samples.extend(generate_silence(0.05, SAMPLE_RATE))

    # 模拟语音（使用不同频率的正弦波）
    if "打开" in text or "开" in text:
        samples.extend(generate_sine_wave(400, duration * 0.8, SAMPLE_RATE))
    elif "关闭" in text or "关" in text:
        samples.extend(generate_sine_wave(300, duration * 0.8, SAMPLE_RATE))
    elif "锁定" in text or "锁" in text:
        samples.extend(generate_sine_wave(500, duration * 0.8, SAMPLE_RATE))
    elif "解锁" in text:
        samples.extend(generate_sine_wave(350, duration * 0.8, SAMPLE_RATE))
    else:
        samples.extend(generate_sine_wave(400, duration * 0.8, SAMPLE_RATE))

    # 结束提示音
    samples.extend(generate_silence(0.05, SAMPLE_RATE))
    samples.extend(generate_beep(1000, 0.1, SAMPLE_RATE))

    # 保存文件
    filename = text.replace(" ", "_").replace("，", "_").replace("。", "_") + ".pcm"
    filepath = os.path.join(output_dir, filename)
    save_pcm_file(filepath, samples)

    print(f"  文件: {filepath}")
    print(f"  时长: {len(samples) / SAMPLE_RATE:.2f} 秒")
    print(f"  大小: {len(samples) * 2} 字节")

    return filepath

def generate_warning_voice(text, duration, output_dir):
    """生成警告语音（简化版本）"""
    print(f"生成: {text}")

    # 生成警告音调
    samples = []

    # 警告开始音
    samples.extend(generate_beep(1000, 0.2, SAMPLE_RATE))
    samples.extend(generate_silence(0.1, SAMPLE_RATE))
    samples.extend(generate_beep(1000, 0.2, SAMPLE_RATE))
    samples.extend(generate_silence(0.1, SAMPLE_RATE))

    # 模拟语音
    if "障碍物" in text:
        samples.extend(generate_sine_wave(500, duration * 0.6, SAMPLE_RATE))
    elif "自动关闭" in text:
        samples.extend(generate_sine_wave(400, duration * 0.6, SAMPLE_RATE))
    elif "异常" in text:
        samples.extend(generate_sine_wave(600, duration * 0.6, SAMPLE_RATE))
    elif "电量" in text:
        samples.extend(generate_sine_wave(350, duration * 0.6, SAMPLE_RATE))
    else:
        samples.extend(generate_sine_wave(450, duration * 0.6, SAMPLE_RATE))

    # 结束音
    samples.extend(generate_silence(0.1, SAMPLE_RATE))
    samples.extend(generate_beep(1200, 0.15, SAMPLE_RATE))

    # 保存文件
    filename = text.replace(" ", "_").replace("，", "_").replace("。", "_") + ".pcm"
    filepath = os.path.join(output_dir, filename)
    save_pcm_file(filepath, samples)

    print(f"  文件: {filepath}")
    print(f"  时长: {len(samples) / SAMPLE_RATE:.2f} 秒")
    print(f"  大小: {len(samples) * 2} 字节")

    return filepath

def generate_welcome_voice(text, duration, output_dir):
    """生成欢迎语音（简化版本）"""
    print(f"生成: {text}")

    # 生成友好音调
    samples = []

    # 开始音
    samples.extend(generate_sine_wave(600, 0.15, SAMPLE_RATE))
    samples.extend(generate_silence(0.05, SAMPLE_RATE))

    # 模拟语音
    if "欢迎" in text:
        samples.extend(generate_sine_wave(500, duration * 0.7, SAMPLE_RATE))
    elif "再见" in text:
        samples.extend(generate_sine_wave(400, duration * 0.7, SAMPLE_RATE))
    else:
        samples.extend(generate_sine_wave(450, duration * 0.7, SAMPLE_RATE))

    # 结束音
    samples.extend(generate_silence(0.05, SAMPLE_RATE))
    samples.extend(generate_sine_wave(700, 0.1, SAMPLE_RATE))

    # 保存文件
    filename = text.replace(" ", "_").replace("，", "_").replace("。", "_") + ".pcm"
    filepath = os.path.join(output_dir, filename)
    save_pcm_file(filepath, samples)

    print(f"  文件: {filepath}")
    print(f"  时长: {len(samples) / SAMPLE_RATE:.2f} 秒")
    print(f"  大小: {len(samples) * 2} 字节")

    return filepath

def main():
    """主函数"""
    # 创建输出目录
    output_dir = Path("sample_pcm")
    output_dir.mkdir(exist_ok=True)

    print("=" * 50)
    print("智能锁示例 PCM 文件生成器")
    print("=" * 50)
    print(f"采样率: {SAMPLE_RATE}Hz")
    print(f"位深: {BIT_DEPTH}bit")
    print(f"声道: {CHANNELS}")
    print("=" * 50)

    # 门状态语音
    print("\n【门状态语音】")
    door_status_dir = output_dir / "door_status"
    door_status_dir.mkdir(exist_ok=True)

    generate_door_status_voice("门已打开", 1.0, door_status_dir)
    generate_door_status_voice("门已关闭", 1.0, door_status_dir)
    generate_door_status_voice("门已锁定", 1.0, door_status_dir)
    generate_door_status_voice("门已解锁", 1.0, door_status_dir)

    # 警告语音
    print("\n【警告语音】")
    warnings_dir = output_dir / "warnings"
    warnings_dir.mkdir(exist_ok=True)

    generate_warning_voice("检测到障碍物，请注意安全", 2.0, warnings_dir)
    generate_warning_voice("门将在5秒后自动关闭", 2.0, warnings_dir)
    generate_warning_voice("检测到异常开门", 1.5, warnings_dir)
    generate_warning_voice("电池电量低，请及时更换", 2.0, warnings_dir)
    generate_warning_voice("检测到异常，请注意安全", 2.0, warnings_dir)

    # 欢迎语音
    print("\n【欢迎语音】")
    welcome_dir = output_dir / "welcome"
    welcome_dir.mkdir(exist_ok=True)

    generate_welcome_voice("欢迎回家", 1.0, welcome_dir)
    generate_welcome_voice("再见，请注意安全", 1.5, welcome_dir)

    print("\n" + "=" * 50)
    print("生成完成！")
    print(f"文件保存在: {output_dir.absolute()}")
    print("=" * 50)

    # 显示文件列表
    print("\n生成的文件：")
    for root, dirs, files in os.walk(output_dir):
        for file in files:
            if file.endswith('.pcm'):
                filepath = os.path.join(root, file)
                size = os.path.getsize(filepath)
                print(f"  {filepath} ({size} 字节)")

if __name__ == '__main__':
    main()
