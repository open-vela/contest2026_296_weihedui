# Smart Lock Application

智能锁 LVGL 界面应用，基于 openvela (NuttX) 操作系统。

## 功能特性

### 1. 主界面
- 门状态显示（开/关/关闭中/打开中）
- 锁状态显示（锁定/解锁）
- 系统状态图标（电池、WiFi）
- 时间显示
- 底部操作按钮（设置、开锁、锁定）

### 2. 倒计时界面
- 自动关门倒计时
- 进度条动画效果
- 取消按钮
- 倒计时结束后自动关门

### 3. 报警界面
- 防夹报警
- 异常开门报警
- 低电量报警
- 防撬报警
- 声光提示（待实现）

### 4. 设置界面
- 自动关门时间调节（5-30秒）
- 语音音量调节（0-100%）
- 屏幕亮度调节（10-100%）
- 防夹功能开关

## 目录结构

```
app/smart_lock/
├── src/
│   └── ui_manager.c        # UI 管理器实现
├── include/
│   └── ui_manager.h        # UI 管理器头文件
├── assets/                 # 资源文件（图片、字体等）
├── Makefile               # 编译配置
├── Kconfig                # 配置选项
└── README.md              # 本文件
```

## 编译配置

在 NuttX 配置中启用以下选项：

```
CONFIG_SMART_LOCK=y
CONFIG_LVGL=y
CONFIG_SMART_LOCK_PROGNAME="smart_lock"
CONFIG_SMART_LOCK_PRIORITY=100
CONFIG_SMART_LOCK_STACKSIZE=8192
CONFIG_SMART_LOCK_DEFAULT_CLOSE_TIME=10
CONFIG_SMART_LOCK_DEFAULT_VOLUME=80
CONFIG_SMART_LOCK_DEFAULT_BRIGHTNESS=100
CONFIG_SMART_LOCK_ANTIPINCH=y
```

## 编译方法

```bash
# 配置项目
./tools/configure.sh <board_name>

# 启用 Smart Lock 应用
kconfig-tweak --enable CONFIG_SMART_LOCK

# 编译
make -j$(nproc)
```

## 运行方法

```bash
# 在 NuttX shell 中运行
smart_lock
```

## API 接口

### 初始化
```c
int ui_manager_init(void);
```

### 界面切换
```c
void ui_manager_show_main(void);
void ui_manager_show_countdown(int seconds);
void ui_manager_show_alarm(alarm_type_t type);
void ui_manager_show_settings(void);
```

### 状态更新
```c
void ui_manager_update_door_status(door_status_t status);
void ui_manager_update_lock_status(lock_status_t status);
void ui_manager_update_system_status(system_status_t status);
```

### 参数获取
```c
int ui_manager_get_auto_close_time(void);
int ui_manager_get_voice_volume(void);
int ui_manager_get_screen_brightness(void);
bool ui_manager_get_antipinch_enabled(void);
```

## 待实现功能

1. **硬件接口**
   - 电机控制接口（开关门）
   - 语音播放接口
   - 屏幕亮度控制接口
   - 声光报警接口

2. **网络功能**
   - WiFi 连接管理
   - 远程控制接口
   - 状态上报

3. **安全功能**
   - 指纹识别
   - 密码验证
   - NFC/RFID

4. **数据存储**
   - 配置持久化
   - 开锁记录
   - 报警日志

## 开发计划

- [x] 基础 UI 框架
- [x] 主界面实现
- [x] 倒计时界面实现
- [x] 报警界面实现
- [x] 设置界面实现
- [ ] 硬件接口集成
- [ ] 网络功能实现
- [ ] 安全功能实现
- [ ] 数据存储实现

## 许可证

本项目采用 Apache License 2.0 许可证。

## 联系方式

如有问题或建议，请提交 Issue 或 Pull Request。
