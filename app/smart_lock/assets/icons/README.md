# 智能锁图标设计指南

## 图标规格

### 基本规格
- **尺寸**: 32x32 或 48x48 像素
- **格式**: PNG (透明背景)
- **颜色模式**: RGBA
- **文件命名**: 小写字母 + 下划线

### 颜色规范
- **主色**: #00b4d8 (蓝色)
- **成功色**: #00ff00 (绿色)
- **警告色**: #ffd700 (黄色)
- **错误色**: #ff0000 (红色)
- **中性色**: #ffffff (白色)

## 图标列表

### 1. 状态图标 (status/)

| 文件名 | 说明 | 尺寸 |
|--------|------|------|
| lock.png | 锁定状态 | 48x48 |
| unlock.png | 解锁状态 | 48x48 |
| door_open.png | 门开状态 | 48x48 |
| door_close.png | 门关状态 | 48x48 |

### 2. 功能图标 (functions/)

| 文件名 | 说明 | 尺寸 |
|--------|------|------|
| settings.png | 设置功能 | 32x32 |
| history.png | 历史记录 | 32x32 |
| alarm.png | 报警功能 | 32x32 |
| battery.png | 电池状态 | 32x32 |
| wifi.png | WiFi 状态 | 32x32 |
| fingerprint.png | 指纹识别 | 32x32 |
| password.png | 密码输入 | 32x32 |
| nfc.png | NFC 功能 | 32x32 |

### 3. 动画图标 (animations/)

| 文件名 | 说明 | 尺寸 |
|--------|------|------|
| loading.gif | 加载动画 | 48x48 |
| success.gif | 成功动画 | 48x48 |
| warning.gif | 警告动画 | 48x48 |
| error.gif | 错误动画 | 48x48 |

## 设计工具推荐

### 1. Figma (推荐)
- **网址**: https://www.figma.com
- **优点**: 免费、协作方便、支持导出多种格式
- **使用方法**:
  1. 创建 48x48 画板
  2. 使用矢量工具绘制图标
  3. 导出为 PNG 格式

### 2. Canva
- **网址**: https://www.canva.com
- **优点**: 模板丰富、操作简单
- **使用方法**:
  1. 搜索图标模板
  2. 自定义颜色和样式
  3. 下载 PNG 格式

### 3. IconFinder
- **网址**: https://www.iconfinder.com
- **优点**: 图标资源丰富
- **使用方法**:
  1. 搜索相关图标
  2. 下载 SVG 格式
  3. 转换为 PNG 格式

### 4. Adobe Illustrator
- **网址**: https://www.adobe.com/products/illustrator.html
- **优点**: 专业设计工具
- **使用方法**:
  1. 创建 48x48 画板
  2. 使用钢笔工具绘制
  3. 导出为 PNG 格式

## 设计规范

### 1. 线条规范
- **线条粗细**: 2px (32x32) 或 3px (48x48)
- **线条端点**: 圆角
- **线条连接**: 圆角

### 2. 颜色规范
- **锁定状态**: #00ff00 (绿色)
- **解锁状态**: #ffd700 (黄色)
- **门开状态**: #00b4d8 (蓝色)
- **门关状态**: #ffffff (白色)

### 3. 间距规范
- **内边距**: 4px
- **元素间距**: 2px
- **对齐方式**: 居中对齐

## 图标设计示例

### 锁定图标 (lock.png)
```
设计元素：
- 锁身：矩形，圆角
- 锁孔：圆形
- 锁梁：弧形

颜色：
- 锁身：#00ff00
- 锁孔：#000000
- 锁梁：#00ff00
```

### 解锁图标 (unlock.png)
```
设计元素：
- 锁身：矩形，圆角
- 锁孔：圆形
- 锁梁：弧形（打开状态）

颜色：
- 锁身：#ffd700
- 锁孔：#000000
- 锁梁：#ffd700
```

### 门开图标 (door_open.png)
```
设计元素：
- 门框：矩形
- 门板：平行四边形（透视效果）

颜色：
- 门框：#00b4d8
- 门板：#00b4d8
```

### 门关图标 (door_close.png)
```
设计元素：
- 门框：矩形
- 门板：矩形

颜色：
- 门框：#ffffff
- 门板：#ffffff
```

## 图标使用方法

### 1. 在 LVGL 中使用
```c
/* 加载图标 */
LV_IMG_DECLARE(lock_icon);
lv_obj_t *img = lv_img_create(screen);
lv_img_set_src(img, &lock_icon);
lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);
```

### 2. 图标数组定义
```c
/* 图标数据 */
const lv_img_dsc_t lock_icon = {
    .header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA,
    .header.w = 48,
    .header.h = 48,
    .data_size = 48 * 48 * LV_IMG_PX_SIZE_ALPHA_BYTE,
    .data = lock_icon_map,
};
```

## 图标转换工具

### 1. 在线转换工具
- **Convertio**: https://convertio.co/png-svg/
- **CloudConvert**: https://cloudconvert.com/svg-to-png

### 2. 命令行工具
```bash
# 使用 ImageMagick 转换
convert icon.svg -resize 48x48 icon.png

# 使用 Inkscape 转换
inkscape icon.svg --export-png=icon.png --export-width=48 --export-height=48
```

### 3. Python 脚本
```python
from PIL import Image
import cairosvg

# SVG 转 PNG
cairosvg.svg2png(url="icon.svg", write_to="icon.png", 
                 output_width=48, output_height=48)
```

## 图标优化

### 1. 文件大小优化
- 使用 PNG-8 格式（如果不需要透明度）
- 压缩 PNG 文件
- 使用 SVG 格式（矢量图）

### 2. 性能优化
- 使用图标字体（如 LVGL 内置图标）
- 合并图标为精灵图
- 使用缓存机制

## 参考资源

- [Material Design Icons](https://materialdesignicons.com/)
- [Font Awesome](https://fontawesome.com/)
- [Feather Icons](https://feathericons.com/)
- [Heroicons](https://heroicons.com/)

## 许可证

图标设计遵循项目许可证。
