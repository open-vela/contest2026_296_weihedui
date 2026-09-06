# 智能锁语音文件生成指南

## 语音规格

- **格式**: PCM
- **采样率**: 16000Hz
- **位深**: 16bit
- **声道**: 单声道
- **文件大小**: 约 32KB/秒

## 语音文件列表

### 1. 门状态语音

| 文件名 | 文本内容 | 时长 | 优先级 |
|--------|----------|------|--------|
| door_opened.pcm | "门已打开" | 1.0s | 1 |
| door_closed.pcm | "门已关闭" | 1.0s | 1 |
| door_locked.pcm | "门已锁定" | 1.0s | 1 |
| door_unlocked.pcm | "门已解锁" | 1.0s | 1 |

### 2. 警告语音

| 文件名 | 文本内容 | 时长 | 优先级 |
|--------|----------|------|--------|
| anti_pinch.pcm | "检测到障碍物，请注意安全" | 2.0s | 3 |
| timeout_warning.pcm | "门将在{N}秒后自动关闭" | 2.0s | 2 |
| abnormal_open.pcm | "检测到异常开门" | 1.5s | 3 |
| low_battery.pcm | "电池电量低，请及时更换" | 2.0s | 2 |
| tamper_alarm.pcm | "检测到异常，请注意安全" | 2.0s | 3 |

### 3. 欢迎语音

| 文件名 | 文本内容 | 时长 | 优先级 |
|--------|----------|------|--------|
| welcome_home.pcm | "欢迎回家" | 1.0s | 1 |
| goodbye.pcm | "再见，请注意安全" | 1.5s | 1 |

## TTS 工具使用

### 1. 百度 TTS (推荐)

#### 注册步骤
1. 访问 https://ai.baidu.com/
2. 注册账号并登录
3. 创建应用，获取 API Key 和 Secret Key
4. 开通语音合成服务

#### 使用方法
```bash
# 安装依赖
pip install requests

# 设置环境变量
export BAIDU_TTS_API_KEY="your_api_key"
export BAIDU_TTS_SECRET_KEY="your_secret_key"

# 运行生成脚本
python generate_voice.py --tts baidu --output .
```

#### API 参考
```python
import requests

def generate_baidu_tts(text, output_file, api_key, secret_key):
    # 获取访问令牌
    token_url = "https://aip.baidubce.com/oauth/2.0/token"
    params = {
        "grant_type": "client_credentials",
        "client_id": api_key,
        "client_secret": secret_key
    }
    response = requests.post(token_url, params=params)
    access_token = response.json()["access_token"]

    # 生成语音
    tts_url = "https://tsn.baidu.com/text2audio"
    params = {
        "tex": text,
        "tok": access_token,
        "cuid": "smart_lock",
        "ctp": 1,
        "lan": "zh",
        "spd": 5,  # 语速 0-15
        "pit": 5,  # 音调 0-15
        "vol": 5,  # 音量 0-15
        "per": 0,  # 发音人 0:女声 1:男声
        "aue": 4,  # 音频格式 4:pcm
    }
    response = requests.post(tts_url, params=params)

    # 保存文件
    with open(output_file, "wb") as f:
        f.write(response.content)
```

### 2. 腾讯 TTS

#### 注册步骤
1. 访问 https://cloud.tencent.com/
2. 注册账号并登录
3. 开通语音合成服务
4. 获取 SecretId 和 SecretKey

#### 使用方法
```bash
# 安装依赖
pip install tencentcloud-sdk-python

# 设置环境变量
export TENCENT_SECRET_ID="your_secret_id"
export TENCENT_SECRET_KEY="your_secret_key"

# 运行生成脚本
python generate_voice.py --tts tencent --output .
```

#### API 参考
```python
from tencentcloud.common import credential
from tencentcloud.tts.v20190823 import tts_client, models

def generate_tencent_tts(text, output_file, secret_id, secret_key):
    # 认证
    cred = credential.Credential(secret_id, secret_key)
    client = tts_client.TtsClient(cred, "ap-guangzhou")

    # 创建请求
    req = models.TextToVoiceRequest()
    req.Text = text
    req.VoiceType = 0  # 0: 女声
    req.Codec = "pcm"
    req.SampleRate = 16000

    # 发送请求
    response = client.TextToVoice(req)

    # 保存文件
    import base64
    audio_data = base64.b64decode(response.Audio)
    with open(output_file, "wb") as f:
        f.write(audio_data)
```

### 3. 阿里云 TTS

#### 注册步骤
1. 访问 https://www.aliyun.com/
2. 注册账号并登录
3. 开通智能语音交互服务
4. 获取 AccessKey ID 和 AccessKey Secret

#### 使用方法
```bash
# 安装依赖
pip install alibabacloud-nls

# 设置环境变量
export ALIYUN_ACCESS_KEY_ID="your_access_key_id"
export ALIYUN_ACCESS_KEY_SECRET="your_access_key_secret"

# 运行生成脚本
python generate_voice.py --tts aliyun --output .
```

### 4. 本地 TTS (pyttsx3)

#### 安装
```bash
pip install pyttsx3
```

#### 使用方法
```bash
# 运行生成脚本
python generate_voice.py --tts local --output .
```

#### 注意事项
- 本地 TTS 生成的语音质量较低
- 适合测试和调试使用
- 不建议用于生产环境

## 在线 TTS 工具

### 1. 百度在线 TTS
- 网址: https://tts.baidu.com/
- 特点: 免费、简单易用
- 限制: 需要手动下载

### 2. 腾讯在线 TTS
- 网址: https://ai.tencent.com/ai/nlp/tts
- 特点: 语音质量高
- 限制: 需要注册账号

### 3. 阿里云在线 TTS
- 网址: https://nls-portal.console.aliyun.com/
- 特点: 语音选择多
- 限制: 需要开通服务

## 手动生成 PCM 文件

### 1. 使用 Audacity

1. 下载并安装 Audacity: https://www.audacityteam.org/
2. 录制语音
3. 设置音频参数:
   - 采样率: 16000Hz
   - 声道: 单声道
   - 格式: 16-bit PCM
4. 导出为 PCM 格式

### 2. 使用 FFmpeg

```bash
# 转换 WAV 为 PCM
ffmpeg -i input.wav -f s16le -acodec pcm_s16le -ac 1 -ar 16000 output.pcm

# 转换 MP3 为 PCM
ffmpeg -i input.mp3 -f s16le -acodec pcm_s16le -ac 1 -ar 16000 output.pcm
```

## 语音文件验证

### 1. 使用 Python 验证

```python
import wave
import struct

def verify_pcm_file(filepath):
    with open(filepath, 'rb') as f:
        data = f.read()

    # 检查文件大小
    expected_samples = 16000 * 1.0  # 1秒的样本数
    expected_size = expected_samples * 2  # 16bit = 2字节/样本

    print(f"文件大小: {len(data)} 字节")
    print(f"预期大小: {expected_size} 字节")
    print(f"时长: {len(data) / (16000 * 2):.2f} 秒")

    # 检查数据格式
    samples = struct.unpack(f'<{len(data)//2}h', data)
    print(f"样本数: {len(samples)}")
    print(f"最大值: {max(samples)}")
    print(f"最小值: {min(samples)}")
```

### 2. 使用 Audacity 验证

1. 打开 Audacity
2. 导入 PCM 文件
3. 设置导入参数:
   - 编码: 有符号 16-bit PCM
   - 声道: 单声道
   - 采样率: 16000Hz
4. 检查波形和时长

## 语音文件集成

### 1. 复制到文件系统

```bash
# 复制到 NuttX 文件系统
cp -r voice/* /data/voice/
```

### 2. 编译到固件

```makefile
# 在 Makefile 中添加
VOICE_FILES = \
    door_status/door_opened.pcm \
    door_status/door_closed.pcm \
    door_status/door_locked.pcm \
    door_status/door_unlocked.pcm \
    warnings/anti_pinch.pcm \
    warnings/timeout_warning.pcm \
    warnings/abnormal_open.pcm \
    warnings/low_battery.pcm \
    warnings/tamper_alarm.pcm \
    welcome/welcome_home.pcm \
    welcome/goodbye.pcm

# 编译到 ROMFS
ROMFS_FILES += $(foreach f,$(VOICE_FILES),voice/$(f):$(f))
```

## 常见问题

### Q: 生成的语音文件没有声音？
A: 检查 PCM 文件格式是否正确，确保采样率、位深、声道设置正确。

### Q: 语音播放时有噪音？
A: 检查音频设备驱动，调整音量设置，避免音量过大导致失真。

### Q: 语音文件太大？
A: 降低采样率（如 8000Hz）或缩短语音时长。

### Q: 如何支持多语言？
A: 为每种语言生成单独的语音文件，根据用户设置选择播放。

## 参考资源

- [百度 TTS 文档](https://ai.baidu.com/ai-doc/SPEECH/vk3hlyx81)
- [腾讯 TTS 文档](https://cloud.tencent.com/document/product/1073)
- [阿里云 TTS 文档](https://help.aliyun.com/document_detail/324261.html)
- [PCM 音频格式](https://en.wikipedia.org/wiki/Pulse-code_modulation)

## 许可证

语音文件遵循项目许可证。
