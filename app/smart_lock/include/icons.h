/**
 * @file icons.h
 * @brief 智能锁图标定义
 * @version 1.0.0
 * @date 2024-01-01
 */

#ifndef __ICONS_H
#define __ICONS_H

#include <nuttx/config.h>
#include <lvgl/lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 图标尺寸定义
 */
#define ICON_SIZE_SMALL     32
#define ICON_SIZE_MEDIUM    48
#define ICON_SIZE_LARGE     64

/**
 * @brief 图标颜色定义
 */
#define ICON_COLOR_LOCKED       0x00ff00  /* 绿色 - 已锁定 */
#define ICON_COLOR_UNLOCKED     0xffd700  /* 黄色 - 已解锁 */
#define ICON_COLOR_DOOR_OPEN    0x00b4d8  /* 蓝色 - 门开 */
#define ICON_COLOR_DOOR_CLOSE   0xffffff  /* 白色 - 门关 */
#define ICON_COLOR_ALARM        0xff0000  /* 红色 - 报警 */
#define ICON_COLOR_SUCCESS      0x00ff00  /* 绿色 - 成功 */
#define ICON_COLOR_WARNING      0xffd700  /* 黄色 - 警告 */

/**
 * @brief 状态图标
 */
extern const lv_img_dsc_t lock_icon;
extern const lv_img_dsc_t unlock_icon;
extern const lv_img_dsc_t door_open_icon;
extern const lv_img_dsc_t door_close_icon;

/**
 * @brief 功能图标
 */
extern const lv_img_dsc_t settings_icon;
extern const lv_img_dsc_t history_icon;
extern const lv_img_dsc_t alarm_icon;
extern const lv_img_dsc_t battery_icon;
extern const lv_img_dsc_t wifi_icon;
extern const lv_img_dsc_t fingerprint_icon;
extern const lv_img_dsc_t password_icon;
extern const lv_img_dsc_t nfc_icon;

/**
 * @brief 动画图标
 */
extern const lv_img_dsc_t loading_icon;
extern const lv_img_dsc_t success_icon;
extern const lv_img_dsc_t warning_icon;
extern const lv_img_dsc_t error_icon;

/**
 * @brief 获取锁定状态图标
 * @param locked true: 锁定, false: 解锁
 * @return 图标指针
 */
static inline const lv_img_dsc_t* get_lock_icon(bool locked)
{
    return locked ? &lock_icon : &unlock_icon;
}

/**
 * @brief 获取门状态图标
 * @param open true: 打开, false: 关闭
 * @return 图标指针
 */
static inline const lv_img_dsc_t* get_door_icon(bool open)
{
    return open ? &door_open_icon : &door_close_icon;
}

/**
 * @brief 获取电池图标
 * @param level 电量等级 (0-100)
 * @return 图标指针
 */
static inline const lv_img_dsc_t* get_battery_icon(int level)
{
    if (level > 80) return &battery_icon;
    if (level > 60) return &battery_icon;
    if (level > 40) return &battery_icon;
    if (level > 20) return &battery_icon;
    return &battery_icon;
}

/**
 * @brief 创建图标对象
 * @param parent 父对象
 * @param icon 图标数据
 * @param x X 坐标
 * @param y Y 坐标
 * @return 图标对象
 */
static inline lv_obj_t* create_icon(lv_obj_t *parent, const lv_img_dsc_t *icon,
                                    lv_coord_t x, lv_coord_t y)
{
    lv_obj_t *img = lv_img_create(parent);
    lv_img_set_src(img, icon);
    lv_obj_align(img, LV_ALIGN_CENTER, x, y);
    return img;
}

/**
 * @brief 创建带颜色的图标
 * @param parent 父对象
 * @param icon 图标数据
 * @param color 图标颜色
 * @param x X 坐标
 * @param y Y 坐标
 * @return 图标对象
 */
static inline lv_obj_t* create_colored_icon(lv_obj_t *parent, const lv_img_dsc_t *icon,
                                            lv_color_t color, lv_coord_t x, lv_coord_t y)
{
    lv_obj_t *img = lv_img_create(parent);
    lv_img_set_src(img, icon);
    lv_obj_set_style_img_recolor(img, color, 0);
    lv_obj_set_style_img_recolor_opa(img, LV_OPA_COVER, 0);
    lv_obj_align(img, LV_ALIGN_CENTER, x, y);
    return img;
}

/**
 * @brief 创建动画图标
 * @param parent 父对象
 * @param icon 图标数据
 * @param duration 动画时长 (ms)
 * @param x X 坐标
 * @param y Y 坐标
 * @return 图标对象
 */
static inline lv_obj_t* create_animated_icon(lv_obj_t *parent, const lv_img_dsc_t *icon,
                                             uint32_t duration, lv_coord_t x, lv_coord_t y)
{
    lv_obj_t *img = lv_img_create(parent);
    lv_img_set_src(img, icon);
    lv_obj_align(img, LV_ALIGN_CENTER, x, y);

    /* 创建旋转动画 */
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, img);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_img_set_angle);
    lv_anim_set_values(&a, 0, 3600);
    lv_anim_set_time(&a, duration);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);

    return img;
}

#ifdef __cplusplus
}
#endif

#endif /* __ICONS_H */
