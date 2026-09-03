/**
 * @file ble_service.c
 * @brief BLE GATT服务实现 - 智锁卫士
 *
 * 实现BLE GATT服务用于手机通信，包括：
 * - 服务UUID: 0x1820
 * - 特征UUID: 0x2B20 (门状态)
 * - 特征UUID: 0x2B21 (控制命令)
 */

#include <stdio.h>
#include <string.h>
#include <nuttx/config.h>
#include <nuttx/bluetooth/bluetooth.h>
#include <nuttx/bluetooth/gatt.h>
#include <nuttx/bluetooth/hci.h>

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
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &state, sizeof(state));
}

/**
 * @brief 写入控制命令回调
 */
static ssize_t write_control_cmd(struct bt_conn *conn,
                                 const struct bt_gatt_attr *attr,
                                 const void *buf, uint16_t len,
                                 uint16_t offset, uint8_t flags)
{
    if (len < 1) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }

    uint8_t cmd = ((const uint8_t *)buf)[0];
    int ret = process_ble_command(cmd);

    if (ret < 0) {
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
    printf("BLE: Notifications %s\n", notify_enabled ? "enabled" : "disabled");
}

/**
 * @brief BLE连接回调
 */
static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
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

    if (current_conn) {
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

    /* 注册GATT服务 */
    ret = bt_gatt_service_register(&ble_service);
    if (ret < 0) {
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
    if (ret < 0) {
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
    if (ret < 0) {
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
    if (!notify_enabled || !current_conn) {
        return -ENOTCONN;
    }

    door_state = state;
    int ret = bt_gatt_notify(current_conn, &attrs[1], &state, sizeof(state));
    if (ret < 0) {
        printf("BLE: Failed to send notification\n");
        return ret;
    }

    return 0;
}

/**
 * @brief 获取当前门状态
 */
uint8_t get_door_state(void)
{
    /* 从门磁传感器读取实际状态 */
    door_status_t status;
    door_sensor_get_status(&status);
    return (status.door_state == DOOR_STATE_OPEN) ? 1 : 0;
}

/**
 * @brief 处理BLE控制命令
 */
int process_ble_command(uint8_t cmd)
{
    printf("BLE: Processing command 0x%02X\n", cmd);

    switch (cmd) {
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
