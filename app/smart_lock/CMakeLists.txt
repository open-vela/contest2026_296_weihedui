# ##############################################################################
# app/smart_lock/CMakeLists.txt
#
# 智锁卫士 - Team 296 (weihedui)
#
# 此前该文件缺失，导致 --cmake 构建完全无法识别本应用。
# 参照 packages/demos/contest2026_296_hello_app/CMakeLists.txt 的写法。
#
# Copyright (C) 2026 weihedui (Team 296)
# SPDX-License-Identifier: Apache-2.0
# ##############################################################################

if(CONFIG_SMART_LOCK)

  # 本应用的头文件放在 include/ 子目录下，需要显式加入搜索路径。
  # 用 include_directories()（目录级作用域）而非 target_include_directories()，
  # 因为 nuttx_add_application 生成的 target 名称是由宏内部决定的。

  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

  nuttx_add_application(
    NAME smart_lock
    SRCS
      src/radar_driver.c
      src/door_sensor.c
      src/door_control.c
      src/safety.c
      src/ble_service.c
      src/smart_lock_main.c
    STACKSIZE 8192
    PRIORITY 100)

  # AI Agent 工具命令
  #
  # docs/communication/agent_guide.md 约定的 `agent <tool> [args]` 界面：
  # agent_tools.c 提供五个 tool_*() 实现，agent_main.c 负责解析与分发。
  # 两者同属 apps_agent —— agent_tools.c 只被 agent 命令使用，放在这里可以
  # 保证 tool_*() 全仓库只有一个定义。
  #
  # agent_tools.c 依赖的应用侧符号（radar_get_status / door_sensor_get_status /
  # motor_control / motor_get_status / ble_is_connected / set_auto_close_timeout）
  # 由 libapps_smart_lock.a 提供。两个库在链接命令里同处一个
  # -Wl,--start-group ... -Wl,--end-group，因此先后顺序无关。

  nuttx_add_application(
    NAME agent
    SRCS
      src/agent_main.c
      src/agent_tools.c
    STACKSIZE 4096
    PRIORITY 100)

  # 蓝牙协议栈由 apps/external/zblue 提供，头文件是 <zephyr/bluetooth/...>。
  # 这些头文件不在应用的全局搜索路径中 —— 不显式接入的话，ble_service.c 里
  # 的 #include <zephyr/bluetooth/bluetooth.h> 会报 "No such file or directory"。
  #
  # zblue 通过 INTERFACE 目标 zblue_headers 导出全部 18 个头文件目录
  # （含构建期生成的那份），这也是 zblue 自带示例应用的用法，见
  # apps/external/zblue/CMakeLists.default.txt 中 hfp_hf 示例的写法：
  #     target_link_libraries(apps_hfp_hf PRIVATE zblue_headers)
  #
  # 仅在实际接入 HCI 传输层（CONFIG_UART_BTH4）时才需要，未接入时
  # ble_service.c 走占位实现，不引用任何 zephyr 头文件。

  if(CONFIG_UART_BTH4)
    if(TARGET apps_smart_lock)
      target_link_libraries(apps_smart_lock PRIVATE zblue_headers)
    endif()
  endif()

endif()
