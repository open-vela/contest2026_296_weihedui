/**
 * @file voice_player.h
 * @brief 智能锁语音播放接口
 * @version 1.0.0
 * @date 2024-01-01
 */

#ifndef __VOICE_PLAYER_H
#define __VOICE_PLAYER_H

#include <nuttx/config.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 语音文件类型
 */
typedef enum {
    VOICE_TYPE_DOOR_STATUS = 0,  /* 门状态语音 */
    VOICE_TYPE_WARNING,          /* 警告语音 */
    VOICE_TYPE_WELCOME           /* 欢迎语音 */
} voice_type_t;

/**
 * @brief 语音文件 ID
 */
typedef enum {
    /* 门状态语音 */
    VOICE_DOOR_OPENED = 0,
    VOICE_DOOR_CLOSED,
    VOICE_DOOR_LOCKED,
    VOICE_DOOR_UNLOCKED,

    /* 警告语音 */
    VOICE_ANTI_PINCH,
    VOICE_TIMEOUT_WARNING,
    VOICE_ABNORMAL_OPEN,
    VOICE_LOW_BATTERY,
    VOICE_TAMPER_ALARM,

    /* 欢迎语音 */
    VOICE_WELCOME_HOME,
    VOICE_GOODBYE,

    VOICE_MAX
} voice_id_t;

/**
 * @brief 语音播放状态
 */
typedef enum {
    PLAYER_STATUS_IDLE = 0,      /* 空闲 */
    PLAYER_STATUS_PLAYING,       /* 播放中 */
    PLAYER_STATUS_PAUSED,        /* 暂停 */
    PLAYER_STATUS_ERROR          /* 错误 */
} player_status_t;

/**
 * @brief 语音播放配置
 */
typedef struct {
    int volume;                  /* 音量 (0-100) */
    int speed;                   /* 语速 (0-15) */
    int pitch;                   /* 音调 (0-15) */
    bool repeat;                 /* 是否重复播放 */
    int repeat_count;            /* 重复次数 */
} voice_config_t;

/**
 * @brief 初始化语音播放器
 * @return 0: 成功, 其他: 失败
 */
int voice_player_init(void);

/**
 * @brief 播放语音
 * @param voice_id 语音 ID
 * @return 0: 成功, 其他: 失败
 */
int voice_player_play(voice_id_t voice_id);

/**
 * @brief 播放带参数的语音
 * @param voice_id 语音 ID
 * @param params 参数数组
 * @param param_count 参数数量
 * @return 0: 成功, 其他: 失败
 */
int voice_player_play_with_params(voice_id_t voice_id, int *params, int param_count);

/**
 * @brief 停止播放
 * @return 0: 成功, 其他: 失败
 */
int voice_player_stop(void);

/**
 * @brief 暂停播放
 * @return 0: 成功, 其他: 失败
 */
int voice_player_pause(void);

/**
 * @brief 恢复播放
 * @return 0: 成功, 其他: 失败
 */
int voice_player_resume(void);

/**
 * @brief 设置音量
 * @param volume 音量 (0-100)
 * @return 0: 成功, 其他: 失败
 */
int voice_player_set_volume(int volume);

/**
 * @brief 获取音量
 * @return 当前音量 (0-100)
 */
int voice_player_get_volume(void);

/**
 * @brief 设置播放配置
 * @param config 播放配置
 * @return 0: 成功, 其他: 失败
 */
int voice_player_set_config(const voice_config_t *config);

/**
 * @brief 获取播放配置
 * @param config 播放配置
 * @return 0: 成功, 其他: 失败
 */
int voice_player_get_config(voice_config_t *config);

/**
 * @brief 获取播放状态
 * @return 播放状态
 */
player_status_t voice_player_get_status(void);

/**
 * @brief 检查是否正在播放
 * @return true: 正在播放, false: 未播放
 */
bool voice_player_is_playing(void);

/**
 * @brief 等待播放完成
 * @param timeout_ms 超时时间 (ms)
 * @return 0: 成功, 其他: 失败
 */
int voice_player_wait_completion(uint32_t timeout_ms);

/**
 * @brief 注册播放完成回调
 * @param callback 回调函数
 * @param user_data 用户数据
 */
typedef void (*voice_play_complete_cb)(voice_id_t voice_id, void *user_data);
void voice_player_register_callback(voice_play_complete_cb callback, void *user_data);

/**
 * @brief 获取语音文件路径
 * @param voice_id 语音 ID
 * @return 语音文件路径
 */
const char* voice_player_get_file_path(voice_id_t voice_id);

/**
 * @brief 获取语音文本
 * @param voice_id 语音 ID
 * @return 语音文本
 */
const char* voice_player_get_text(voice_id_t voice_id);

/**
 * @brief 反初始化语音播放器
 * @return 0: 成功, 其他: 失败
 */
int voice_player_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* __VOICE_PLAYER_H */
