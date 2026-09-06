#!/usr/bin/env python3
"""
智能锁语音生成脚本
使用 TTS 工具生成语音提示文件
"""

import os
import sys
import argparse
import subprocess
from pathlib import Path
from typing import Dict, List

# 语音配置
VOICE_CONFIG = {
    "door_status": {
        "door_opened.pcm": "门已打开",
        "door_closed.pcm": "门已关闭",
        "door_locked.pcm": "门已锁定",
        "door_unlocked.pcm": "门已解锁",
    },
    "warnings": {
        "anti_pinch.pcm": "检测到障碍物，请注意安全",
        "timeout_warning.pcm": "门将在{N}秒后自动关闭",
        "abnormal_open.pcm": "检测到异常开门",
        "low_battery.pcm": "电池电量低，请及时更换",
        "tamper_alarm.pcm": "检测到异常，请注意安全",
    },
    "welcome": {
        "welcome_home.pcm": "欢迎回家",
        "goodbye.pcm": "再见，请注意安全",
    }
}

# 语音规格
VOICE_SPECS = {
    "sample_rate": 16000,  # 采样率 16000Hz
    "bit_depth": 16,       # 位深 16bit
    "channels": 1,         # 单声道
    "format": "pcm"        # PCM 格式
}

class VoiceGenerator:
    """语音生成器基类"""

    def __init__(self, output_dir: str):
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(parents=True, exist_ok=True)

    def generate(self, text: str, output_file: str) -> bool:
        """生成语音文件"""
        raise NotImplementedError

    def generate_all(self, config: Dict) -> bool:
        """批量生成语音文件"""
        success_count = 0
        total_count = 0

        for category, files in config.items():
            category_dir = self.output_dir / category
            category_dir.mkdir(exist_ok=True)

            for filename, text in files.items():
                output_file = category_dir / filename
                total_count += 1

                if self.generate(text, str(output_file)):
                    success_count += 1
                    print(f"✅ 生成: {filename}")
                else:
                    print(f"❌ 失败: {filename}")

        print(f"\n生成完成: {success_count}/{total_count}")
        return success_count == total_count

class BaiduTTSGenerator(VoiceGenerator):
    """百度 TTS 语音生成器"""

    def __init__(self, output_dir: str, api_key: str = None, secret_key: str = None):
        super().__init__(output_dir)
        self.api_key = api_key or os.getenv("BAIDU_TTS_API_KEY")
        self.secret_key = secret_key or os.getenv("BAIDU_TTS_SECRET_KEY")
        self.access_token = None

    def get_access_token(self) -> str:
        """获取百度 TTS 访问令牌"""
        if self.access_token:
            return self.access_token

        import requests

        url = "https://aip.baidubce.com/oauth/2.0/token"
        params = {
            "grant_type": "client_credentials",
            "client_id": self.api_key,
            "client_secret": self.secret_key
        }

        try:
            response = requests.post(url, params=params)
            response.raise_for_status()
            data = response.json()
            self.access_token = data["access_token"]
            return self.access_token
        except Exception as e:
            print(f"获取访问令牌失败: {e}")
            return None

    def generate(self, text: str, output_file: str) -> bool:
        """使用百度 TTS 生成语音"""
        import requests

        if not self.get_access_token():
            return False

        url = "https://tsn.baidu.com/text2audio"
        params = {
            "tex": text,
            "tok": self.access_token,
            "cuid": "smart_lock",
            "ctp": 1,
            "lan": "zh",
            "spd": 5,  # 语速 0-15
            "pit": 5,  # 音调 0-15
            "vol": 5,  # 音量 0-15
            "per": 0,  # 发音人 0:女声 1:男声
            "aue": 4,  # 音频格式 4:pcm
        }

        try:
            response = requests.post(url, params=params)
            response.raise_for_status()

            # 保存 PCM 文件
            with open(output_file, "wb") as f:
                f.write(response.content)

            return True
        except Exception as e:
            print(f"生成语音失败: {e}")
            return False

class TencentTTSGenerator(VoiceGenerator):
    """腾讯 TTS 语音生成器"""

    def __init__(self, output_dir: str, secret_id: str = None, secret_key: str = None):
        super().__init__(output_dir)
        self.secret_id = secret_id or os.getenv("TENCENT_SECRET_ID")
        self.secret_key = secret_key or os.getenv("TENCENT_SECRET_KEY")

    def generate(self, text: str, output_file: str) -> bool:
        """使用腾讯 TTS 生成语音"""
        try:
            from tencentcloud.common import credential
            from tencentcloud.tts.v20190823 import tts_client, models

            # 认证
            cred = credential.Credential(self.secret_id, self.secret_key)
            client = tts_client.TtsClient(cred, "ap-guangzhou")

            # 创建请求
            req = models.TextToVoiceRequest()
            req.Text = text
            req.VoiceType = 0  # 0: 女声
            req.Codec = "pcm"
            req.SampleRate = 16000

            # 发送请求
            response = client.TextToVoice(req)

            # 保存 PCM 文件
            import base64
            audio_data = base64.b64decode(response.Audio)
            with open(output_file, "wb") as f:
                f.write(audio_data)

            return True
        except Exception as e:
            print(f"生成语音失败: {e}")
            return False

class AliyunTTSGenerator(VoiceGenerator):
    """阿里云 TTS 语音生成器"""

    def __init__(self, output_dir: str, access_key_id: str = None, access_key_secret: str = None):
        super().__init__(output_dir)
        self.access_key_id = access_key_id or os.getenv("ALIYUN_ACCESS_KEY_ID")
        self.access_key_secret = access_key_secret or os.getenv("ALIYUN_ACCESS_KEY_SECRET")

    def generate(self, text: str, output_file: str) -> bool:
        """使用阿里云 TTS 生成语音"""
        try:
            import nls
            import json

            # 配置
            url = "wss://nls-gateway.cn-shanghai.aliyuncs.com/ws/v1"
            token = self.get_token()

            # 创建 TTS 实例
            tts = nls.NlsSpeechSynthesizer(
                url=url,
                token=token,
                appkey="your_appkey",
                on_data=lambda data: self.save_pcm(data, output_file),
                on_completed=lambda: print(f"完成: {output_file}")
            )

            # 合成语音
            tts.start(
                text=text,
                voice="xiaoyun",
                sample_rate=16000,
                format="pcm"
            )

            return True
        except Exception as e:
            print(f"生成语音失败: {e}")
            return False

    def get_token(self) -> str:
        """获取阿里云访问令牌"""
        # TODO: 实现阿里云 token 获取
        return "your_token"

    def save_pcm(self, data: bytes, output_file: str):
        """保存 PCM 数据"""
        with open(output_file, "ab") as f:
            f.write(data)

class LocalTTSGenerator(VoiceGenerator):
    """本地 TTS 语音生成器（使用 pyttsx3）"""

    def __init__(self, output_dir: str):
        super().__init__(output_dir)
        try:
            import pyttsx3
            self.engine = pyttsx3.init()
            self.engine.setProperty('rate', 150)  # 语速
            self.engine.setProperty('volume', 0.9)  # 音量
        except ImportError:
            print("警告: pyttsx3 未安装，使用模拟生成")
            self.engine = None

    def generate(self, text: str, output_file: str) -> bool:
        """使用本地 TTS 生成语音"""
        try:
            if self.engine:
                # 使用 pyttsx3 生成
                temp_wav = output_file.replace('.pcm', '.wav')
                self.engine.save_to_file(text, temp_wav)
                self.engine.runAndWait()

                # 转换为 PCM
                self.convert_wav_to_pcm(temp_wav, output_file)
                os.remove(temp_wav)
            else:
                # 模拟生成（创建静音 PCM 文件）
                self.generate_silence_pcm(output_file)

            return True
        except Exception as e:
            print(f"生成语音失败: {e}")
            return False

    def convert_wav_to_pcm(self, wav_file: str, pcm_file: str):
        """将 WAV 转换为 PCM"""
        import wave

        with wave.open(wav_file, 'rb') as wav:
            params = wav.getparams()
            frames = wav.readframes(params.nframes)

        with open(pcm_file, 'wb') as pcm:
            pcm.write(frames)

    def generate_silence_pcm(self, output_file: str, duration: float = 1.0):
        """生成静音 PCM 文件"""
        import struct
        import math

        sample_rate = VOICE_SPECS["sample_rate"]
        num_samples = int(sample_rate * duration)

        # 生成静音数据
        with open(output_file, 'wb') as f:
            for i in range(num_samples):
                # 写入 16 位有符号整数
                f.write(struct.pack('<h', 0))

def main():
    parser = argparse.ArgumentParser(description='生成智能锁语音提示文件')
    parser.add_argument('--tts', type=str, default='local',
                       choices=['baidu', 'tencent', 'aliyun', 'local'],
                       help='TTS 工具选择')
    parser.add_argument('--output', type=str, default='.',
                       help='输出目录')
    parser.add_argument('--api-key', type=str, default=None,
                       help='API Key')
    parser.add_argument('--secret-key', type=str, default=None,
                       help='Secret Key')
    args = parser.parse_args()

    # 选择 TTS 生成器
    if args.tts == 'baidu':
        generator = BaiduTTSGenerator(args.output, args.api_key, args.secret_key)
    elif args.tts == 'tencent':
        generator = TencentTTSGenerator(args.output, args.api_key, args.secret_key)
    elif args.tts == 'aliyun':
        generator = AliyunTTSGenerator(args.output, args.api_key, args.secret_key)
    else:
        generator = LocalTTSGenerator(args.output)

    # 生成语音文件
    print(f"使用 {args.tts} TTS 生成语音文件...")
    generator.generate_all(VOICE_CONFIG)

if __name__ == '__main__':
    main()
