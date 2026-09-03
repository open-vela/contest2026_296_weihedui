# BLE通信使用指南

## 1. BLE服务概述

智锁卫士项目使用BLE（低功耗蓝牙）进行设备与手机之间的通信。

### 服务UUID
- **主服务UUID**: `0x1820`
- **门状态特征UUID**: `0x2B20`
- **控制命令特征UUID**: `0x2B21`

## 2. 数据格式

### 门状态 (0x2B20)

**读取操作**: 返回1字节数据

| 值 | 含义 |
|---|---|
| `0x00` | 门关闭 |
| `0x01` | 门开启 |

**通知功能**: 当门状态发生变化时，设备会主动发送通知。

### 控制命令 (0x2B21)

**写入操作**: 发送1字节命令

| 命令值 | 功能 |
|---|---|
| `0x01` | 开门 |
| `0x02` | 关门 |
| `0x03` | 锁门 |
| `0x04` | 解锁 |

## 3. 连接流程

### 3.1 扫描设备
```
1. 启动BLE扫描
2. 过滤服务UUID为0x1820的设备
3. 显示设备列表
```

### 3.2 连接设备
```
1. 选择目标设备
2. 建立BLE连接
3. 发现服务 (0x1820)
4. 获取特征 (0x2B20, 0x2B21)
```

### 3.3 数据交互
```
1. 读取门状态: 读取特征0x2B20
2. 发送控制命令: 写入特征0x2B21
3. 订阅状态通知: 启用特征0x2B20的通知
```

## 4. 代码示例

### 4.1 扫描设备
```javascript
const devices = await ble.scan({
    services: ['0x1820']
});
```

### 4.2 连接设备
```javascript
const device = await ble.connect(deviceId);
const service = await device.getService('0x1820');
```

### 4.3 读取状态
```javascript
const characteristic = await service.getCharacteristic('0x2B20');
const value = await characteristic.read();
// value[0] = 0x00 表示门关闭, 0x01 表示门开启
```

### 4.4 发送命令
```javascript
const characteristic = await service.getCharacteristic('0x2B21');
await characteristic.write([0x01]); // 开门命令
```

## 5. 常见问题

### 5.1 无法扫描到设备
- 检查手机BLE是否开启
- 确认设备是否在广播状态
- 检查设备距离是否过远

### 5.2 连接失败
- 检查设备距离（建议10米以内）
- 确认设备未被其他手机连接
- 尝试重启手机BLE

### 5.3 数据读写错误
- 检查UUID是否正确
- 确认特征权限（读/写/通知）
- 检查数据格式是否正确

### 5.4 通知不工作
- 确认已启用CCC描述符
- 检查设备是否支持通知
- 确认连接状态正常

## 6. 调试技巧

### 6.1 使用nRF Connect
1. 下载nRF Connect应用
2. 扫描并连接设备
3. 查看服务和特征
4. 手动读写数据测试

### 6.2 日志查看
```bash
# 查看BLE相关日志
grep "BLE:" logs/smart_lock.log
```

## 7. 参考资料

- [BLE GATT规范](https://www.bluetooth.com)
- [NuttX BLE驱动文档](https://nuttx.apache.org)
- [快应用BLE API](https://doc.quickapp.cn)
