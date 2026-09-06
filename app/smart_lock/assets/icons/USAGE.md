# 图标使用指南

## 图标文件位置

```
app/smart_lock/assets/icons/
├── svg/                    # SVG 源文件
│   ├── lock.svg
│   ├── unlock.svg
│   ├── door_open.svg
│   ├── door_close.svg
│   ├── settings.svg
│   ├── history.svg
│   ├── alarm.svg
│   ├── battery.svg
│   ├── wifi.svg
│   ├── fingerprint.svg
│   ├── password.svg
│   └── nfc.svg
├── png/                    # PNG 文件（转换后）
├── generated/              # 生成的 C 文件
├── convert_icons.py        # 转换脚本
├── README.md               # 设计指南
└── USAGE.md                # 本文件
```

## 图标转换

### 1. 安装依赖

```bash
pip install Pillow cairosvg
```

### 2. 运行转换脚本

```bash
cd app/smart_lock/assets/icons
python convert_icons.py --size 48 --output .
```

### 3. 转换结果

- **PNG 文件**: 保存在 `png/` 目录
- **C 文件**: 保存在 `generated/` 目录

## 在 LVGL 中使用图标

### 方法 1: 使用生成的 C 文件

1. 将生成的 C 文件复制到项目源目录
2. 在头文件中声明图标：

```c
// icon.h
#ifndef __ICON_H
#define __ICON_H

#include <lvgl/lvgl.h>

extern const lv_img_dsc_t lock_icon;
extern const lv_img_dsc_t unlock_icon;
extern const lv_img_dsc_t door_open_icon;
extern const lv_img_dsc_t door_close_icon;
extern const lv_img_dsc_t settings_icon;
extern const lv_img_dsc_t history_icon;
extern const lv_img_dsc_t alarm_icon;
extern const lv_img_dsc_t battery_icon;
extern const lv_img_dsc_t wifi_icon;
extern const lv_img_dsc_t fingerprint_icon;
extern const lv_img_dsc_t password_icon;
extern const lv_img_dsc_t nfc_icon;

#endif
```

3. 在源文件中使用图标：

```c
#include "icon.h"

void create_icon_example(void)
{
    lv_obj_t *img = lv_img_create(lv_scr_act());
    lv_img_set_src(img, &lock_icon);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);
}
```

### 方法 2: 使用文件系统

1. 将 PNG 文件复制到文件系统
2. 使用文件路径加载图标：

```c
void create_icon_from_file(void)
{
    lv_obj_t *img = lv_img_create(lv_scr_act());
    lv_img_set_src(img, "S:/icons/lock.png");
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);
}
```

### 方法 3: 使用 LVGL 内置图标

LVGL 提供了内置图标，可以直接使用：

```c
// 使用内置图标
lv_label_set_text(label, LV_SYMBOL_LOCK);
lv_label_set_text(label, LV_SYMBOL_UNLOCK);
lv_label_set_text(label, LV_SYMBOL_SETTINGS);
lv_label_set_text(label, LV_SYMBOL_WIFI);
lv_label_set_text(label, LV_SYMBOL_BATTERY_FULL);
```

## 图标颜色修改

### 方法 1: 修改 SVG 文件

编辑 SVG 文件中的颜色值：

```xml
<style>
  .lock-body { fill: #00ff00; }  <!-- 修改这里 -->
</style>
```

### 方法 2: 使用 LVGL 颜色变换

```c
// 设置图标颜色
lv_obj_set_style_img_recolor(img, lv_color_hex(0x00ff00), 0);
lv_obj_set_style_img_recolor_opa(img, LV_OPA_COVER, 0);
```

## 图标动画

### 创建加载动画

```c
void create_loading_animation(void)
{
    lv_obj_t *img = lv_img_create(lv_scr_act());
    lv_img_set_src(img, &loading_icon);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);

    // 创建旋转动画
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, img);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_img_set_angle);
    lv_anim_set_values(&a, 0, 3600);
    lv_anim_set_time(&a, 1000);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}
```

## 图标优化建议

### 1. 文件大小优化
- 使用 PNG-8 格式（如果不需要透明度）
- 压缩 PNG 文件
- 使用 SVG 格式（矢量图）

### 2. 性能优化
- 使用图标字体（如 LVGL 内置图标）
- 合并图标为精灵图
- 使用缓存机制

### 3. 内存优化
- 使用合适的图标尺寸
- 避免同时加载过多图标
- 使用延迟加载

## 常见问题

### Q: 图标显示不正确？
A: 检查图标文件路径是否正确，确保文件存在。

### Q: 图标颜色不对？
A: 检查 SVG 文件中的颜色值，或使用 LVGL 颜色变换。

### Q: 图标加载失败？
A: 检查文件系统是否正确挂载，文件路径是否正确。

### Q: 图标动画卡顿？
A: 减少同时运行的动画数量，优化动画参数。

## 参考资源

- [LVGL 图标文档](https://docs.lvgl.io/master/overview/image.html)
- [LVGL 内置符号](https://docs.lvgl.io/master/overview/font.html#symbols)
- [Material Design Icons](https://materialdesignicons.com/)

## 许可证

图标设计遵循项目许可证。
