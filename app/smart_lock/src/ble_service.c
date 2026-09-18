/**
 * @file ble_service.c
 * @brief BLE GATT服务实现 - 智锁卫士
 *
 * 实现BLE GATT服务用于手机通信，包括：
 * - 服务UUID: 0x1820
 * - 特征UUID: 0x2B20 (门状态)
 * - 特征UUID: 0x2B21 (控制命令)
 *
 * Copyright (C) 2026 weihedui Team
 * Licensed under the Apache License, Version 2.0
 *
 * ============================================================================
 * 集成修复说明 (2026-09-18)
 * ============================================================================
 * 原文件直接使用 Zephyr 蓝牙 API（bt_conn / BT_GATT_PRIMARY_SERVICE /
 * bt_le_adv_start / bt_gatt_service_register 等），并包含 nuttx 蓝牙头文件
 * <nuttx/bluetooth/bluetooth.h>、<nuttx/bluetooth/gatt.h>、
 * <nuttx/bluetooth/hci.h>。经核查实际情况为：
 *
 *   1. 本仓库中不存在上述三个头文件，任何路径下都没有 nuttx/bluetooth/
 *      目录（只有 nuttx/wireless/bluetooth/ 和 nuttx/net/bluetooth.h，
 *      且 API 完全不同）；
 *   2. Zephyr 蓝牙 API 由 apps/external/zblue 提供，头文件路径是
 *      <zephyr/bluetooth/...>。该头文件目录不在应用的全局搜索路径中，
 *      必须在 CMakeLists.txt 里显式加入（见该文件中的说明）。
 *
 * 关于开关，需要区分两个层次的配置：
 *
 *   - CONFIG_BT —— zblue 协议栈本身。黄山派板级 defconfig 中已经开启
 *     （CONFIG_BT=y，含 BT_PERIPHERAL / BT_GATT_DYNAMIC_DB / BT_H4 等），
 *     libzblue.a 也已正常参与构建。
 *   - CONFIG_UART_BTH4 —— 芯片侧 HCI 传输层。它同时是
 *     vendor/sifli/chips/sf32lb52/CMakeLists.txt 中 LCPU_BT_SRCS
 *     （sf32lb52_bth4.c / sf32lb52_bt_adapter.c / lcpu_boot.c）的编译条件，
 *     也是 nuttx/drivers/serial/uart_bth4.c 的开关。该选项当前未开启，
 *     因此 /dev/ttyHCI0 不存在，协议栈没有可用的控制器。
 *
 *   也就是说：协议栈在，但传输层没接。sf32lb52_bth4.c 中已提供
 *   sf32lb52_bt_initialize()，它会注册 /dev/ttyHCI0 并调用 z_sys_init()
 *   拉起协议栈；zblue 侧的 CONFIG_BT_H4=y 默认就从 /dev/ttyHCI0 取数，
 *   两端是对得上的。
 *
 * 因此本文件以“协议栈 + 传输层”同时具备为条件编译真实实现；否则编译为
 * 接口等价的占位实现，使整个应用能够完成集成构建与链接、并跑通其余全部
 * 业务逻辑。启用办法见仓库 README 的「蓝牙」一节。
 *
 * 注意：get_door_state() 与 process_ble_command() 不依赖蓝牙协议栈，
 * 两者在两个分支中共用同一份真实实现。
 * ============================================================================
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <nuttx/config.h>

#include "smart_lock.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* GATT服务UUID */
#define BLE_SERVICE_UUID        0x1820
#define DOOR_STATE_CHAR_UUID    0x2B20
#define CONTROL_CMD_CHAR_UUID   0x2B21

/* 控制命令定义 */
#define CMD_OPEN_DOOR           0x01
#define CMD_CLOSE_DOOR          0x02
#define CMD_LOCK_DOOR           0x03
#define CMD_UNLOCK_DOOR         0x04

/****************************************************************************
 * Public Functions (与蓝牙协议栈无关，两个分支共用)
 ****************************************************************************/

/**
 * @brief 获取当前门状态
 *
 * @return uint8_t 0: 关闭, 1: 开启
 */

uint8_t get_door_state(void)
{
    /* 从门磁传感器读取实际状态 */
    door_status_t status;

    if (door_sensor_get_status(&status) < 0)
    {
        return 0;
    }

    return (status.door_state == DOOR_STATE_OPEN) ? 1 : 0;
}

/**
 * @brief 处理BLE控制命令
 *
 * @param cmd 命令字节
 * @return int 0: 成功, 负值: 失败
 */

int process_ble_command(uint8_t cmd)
{
    printf("BLE: Processing command 0x%02X\n", cmd);

    switch (cmd)
    {
    case CMD_OPEN_DOOR:
        return motor_control(MOTOR_CMD_OPEN);
    case CMD_CLOSE_DOOR:
        return motor_control(MOTOR_CMD_CLOSE);
    case CMD_LOCK_DOOR:
        return motor_control(MOTOR_CMD_LOCK);
    case CMD_UNLOCK_DOOR:
        return motor_control(MOTOR_CMD_UNLOCK);
    default:
        printf("BLE: Unknown command 0x%02X\n", cmd);
        return -EINVAL;
    }
}

/****************************************************************************
 * Zephyr 蓝牙协议栈分支
 *
 * 条件为「协议栈已开启」且「芯片侧 HCI 传输层已接入」。以下为成员原始实现，
 * 仅作两处必要修正：
 *   a) 头文件改为 zblue 实际提供的 <zephyr/bluetooth/...>；
 *   b) ble_service_init() 中补上控制器初始化与 bt_enable() —— 原实现直接
 *      注册 GATT 服务，但从未启动协议栈，即使传输层就绪也无法工作。
 ****************************************************************************/

#if defined(CONFIG_BT) && defined(CONFIG_UART_BTH4)

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>

/* 芯片侧 HCI 传输层入口：注册 /dev/ttyHCI0 并拉起 zblue */
extern int sf32lb52_bt_initialize(void);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static uint8_t door_state = 0;  /* 0: 关闭, 1: 开启 */
static struct bt_conn *current_conn = NULL;
static bool notify_enabled = false;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static ssize_t read_door_state(struct bt_conn *conn,
                               const struct bt_gatt_attr *attr,
                               void *buf, uint16_t len, uint16_t offset);

static ssize_t write_control_cmd(struct bt_conn *conn,
                                 const struct bt_gatt_attr *attr,
                                 const void *buf, uint16_t len,
                                 uint16_t offset, uint8_t flags);

static void ble_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value);

static void connected(struct bt_conn *conn, uint8_t err);
static void disconnected(struct bt_conn *conn, uint8_t reason);

/****************************************************************************
 * GATT Service Definition
 ****************************************************************************/

static struct bt_gatt_attr attrs[] = {
    /* Primary Service */
    BT_GATT_PRIMARY_SERVICE(BT_UUID_DECLARE_16(BLE_SERVICE_UUID)),

    /* 门状态特征 - 可读、可通知 */
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_16(DOOR_STATE_CHAR_UUID),
                          BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                          BT_GATT_PERM_READ,
                          read_door_state, NULL, &door_state),
    BT_GATT_CCC(ble_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

    /* 控制命令特征 - 可写 */
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_16(CONTROL_CMD_CHAR_UUID),
                          BT_GATT_CHRC_WRITE,
                          BT_GATT_PERM_WRITE,
                          NULL, write_control_cmd, NULL),
};

static struct bt_gatt_service ble_service = BT_GATT_SERVICE(attrs);

/****************************************************************************
 * Connection Callbacks
 ****************************************************************************/

static struct bt_conn_cb conn_callbacks = {
    .connected = connected,
    .disconnected = disconnected,
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/**
 * @brief 读取门状态回调
 */

static ssize_t read_door_state(struct bt_conn *conn,
                               const struct bt_gatt_attr *attr,
                               void *buf, uint16_t len, uint16_t offset)
{
    uint8_t state = get_door_state();
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &state,
                             sizeof(state));
}

/**
 * @brief 写入控制命令回调
 */

static ssize_t write_control_cmd(struct bt_conn *conn,
                                 const struct bt_gatt_attr *attr,
                                 const void *buf, uint16_t len,
                                 uint16_t offset, uint8_t flags)
{
    if (len < 1)
    {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }

    uint8_t cmd = ((const uint8_t *)buf)[0];
    int ret = process_ble_command(cmd);

    if (ret < 0)
    {
        return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
    }

    return len;
}

/**
 * @brief CCC描述符变化回调
 */

static void ble_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    notify_enabled = (value == BT_GATT_CCC_NOTIFY);
    printf("BLE: Notifications %s\n",
           notify_enabled ? "enabled" : "disabled");
}

/**
 * @brief BLE连接回调
 */

static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err)
    {
        printf("BLE: Connection failed (err %u)\n", err);
        return;
    }

    printf("BLE: Connected\n");
    current_conn = bt_conn_ref(conn);
}

/**
 * @brief BLE断开回调
 */

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    printf("BLE: Disconnected (reason %u)\n", reason);

    if (current_conn)
    {
        bt_conn_unref(current_conn);
        current_conn = NULL;
    }

    notify_enabled = false;

    /* 重新开始广播 */
    ble_start_advertising();
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/**
 * @brief 初始化BLE服务
 */

int ble_service_init(void)
{
    int ret;

    /* 先接入芯片侧 HCI 传输层（注册 /dev/ttyHCI0） */
    ret = sf32lb52_bt_initialize();
    if (ret < 0)
    {
        printf("BLE: Failed to initialize controller (%d)\n", ret);
        return ret;
    }

    /* 启动蓝牙协议栈；调用后 zblue 才会打开 /dev/ttyHCI0 并与控制器握手 */
    ret = bt_enable(NULL);
    if (ret < 0)
    {
        printf("BLE: Failed to enable Bluetooth stack (%d)\n", ret);
        return ret;
    }

    /* 注册GATT服务 */
    ret = bt_gatt_service_register(&ble_service);
    if (ret < 0)
    {
        printf("BLE: Failed to register GATT service\n");
        return ret;
    }

    /* 注册连接回调 */
    bt_conn_cb_register(&conn_callbacks);

    printf("BLE: GATT service initialized\n");
    return 0;
}

/**
 * @brief 开始BLE广播
 */

int ble_start_advertising(void)
{
    struct bt_le_adv_param adv_param = {
        .options = BT_LE_ADV_OPT_CONNECTABLE,
        .interval_min = BT_GAP_ADV_FAST_INT_MIN_2,
        .interval_max = BT_GAP_ADV_FAST_INT_MAX_2,
    };

    const char *device_name = "SmartLock";
    struct bt_data ad[] = {
        BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
        BT_DATA(BT_DATA_NAME_COMPLETE, device_name, strlen(device_name)),
    };

    int ret = bt_le_adv_start(&adv_param, ad, ARRAY_SIZE(ad), NULL, 0);
    if (ret < 0)
    {
        printf("BLE: Failed to start advertising\n");
        return ret;
    }

    printf("BLE: Advertising started\n");
    return 0;
}

/**
 * @brief 停止BLE广播
 */

int ble_stop_advertising(void)
{
    int ret = bt_le_adv_stop();
    if (ret < 0)
    {
        printf("BLE: Failed to stop advertising\n");
        return ret;
    }

    printf("BLE: Advertising stopped\n");
    return 0;
}

/**
 * @brief 发送门状态通知
 */

int ble_notify_door_state(uint8_t state)
{
    if (!notify_enabled || !current_conn)
    {
        return -ENOTCONN;
    }

    door_state = state;
    int ret = bt_gatt_notify(current_conn, &attrs[1], &state, sizeof(state));
    if (ret < 0)
    {
        printf("BLE: Failed to send notification\n");
        return ret;
    }

    return 0;
}

/**
 * @brief 查询 BLE 当前是否有活动连接
 *
 * @return true 已连接, false 未连接
 */

bool ble_is_connected(void)
{
    return (current_conn != NULL);
}

/****************************************************************************
 * 未启用蓝牙协议栈时的占位实现
 *
 * 接口与语义和上面完全一致，只是没有真实的 GATT/广播行为：所有函数立即
 * 返回成功，ble_is_connected() 恒为 false。目的是让整机能构建、能链接、
 * 能跑通其余全部业务逻辑（门磁、雷达、电机、防夹、AI 工具）。
 ****************************************************************************/

#else /* !(CONFIG_BT && CONFIG_UART_BTH4) */

static uint8_t door_state_stub = 0;
static bool notify_enabled_stub = false;

int ble_service_init(void)
{
    printf("BLE: [stub] controller transport not enabled "
           "(CONFIG_UART_BTH4); GATT service not registered\n");
    return 0;
}

int ble_start_advertising(void)
{
    printf("BLE: [stub] advertising not started\n");
    return 0;
}

int ble_stop_advertising(void)
{
    printf("BLE: [stub] advertising already stopped\n");
    return 0;
}

int ble_notify_door_state(uint8_t state)
{
    door_state_stub = state;

    if (!notify_enabled_stub)
    {
        return -ENOTCONN;
    }

    return 0;
}

bool ble_is_connected(void)
{
    return false;
}

#endif /* CONFIG_BT && CONFIG_UART_BTH4 */
