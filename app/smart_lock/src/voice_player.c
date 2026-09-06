/**
 * @file voice_player.c
 * @brief 智能锁语音播放接口实现
 * @version 1.0.0
 * @date 2024-01-01
 */

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <syslog.h>
#include <pthread.h>

#include "voice_player.h"

/* 语音文件路径配置 */
#define VOICE_BASE_PATH     "/data/voice"
#define VOICE_DOOR_PATH     VOICE_BASE_PATH "/door_status"
#define VOICE_WARNING_PATH  VOICE_BASE_PATH "/warnings"
#define VOICE_WELCOME_PATH  VOICE_BASE_PATH "/welcome"

/* 语音文件信息结构体 */
typedef struct {
    voice_id_t id;
    voice_type_t type;
    const char *filename;
    const char *text;
    const char *path;
    float duration;
    int priority;
} voice_info_t;

/* 语音文件信息表 */
static const voice_info_t g_voice_info[VOICE_MAX] = {
    /* 门状态语音 */
    {VOICE_DOOR_OPENED,    VOICE_TYPE_DOOR_STATUS, "door_opened.pcm",    "门已打开",                    VOICE_DOOR_PATH "/door_opened.pcm",    1.0, 1},
    {VOICE_DOOR_CLOSED,    VOICE_TYPE_DOOR_STATUS, "door_closed.pcm",    "门已关闭",                    VOICE_DOOR_PATH "/door_closed.pcm",    1.0, 1},
    {VOICE_DOOR_LOCKED,    VOICE_TYPE_DOOR_STATUS, "door_locked.pcm",    "门已锁定",                    VOICE_DOOR_PATH "/door_locked.pcm",    1.0, 1},
    {VOICE_DOOR_UNLOCKED,  VOICE_TYPE_DOOR_STATUS, "door_unlocked.pcm",  "门已解锁",                    VOICE_DOOR_PATH "/door_unlocked.pcm",  1.0, 1},

    /* 警告语音 */
    {VOICE_ANTI_PINCH,     VOICE_TYPE_WARNING,     "anti_pinch.pcm",     "检测到障碍物，请注意安全",         VOICE_WARNING_PATH "/anti_pinch.pcm",  2.0, 3},
    {VOICE_TIMEOUT_WARNING,VOICE_TYPE_WARNING,     "timeout_warning.pcm","门将在%d秒后自动关闭",           VOICE_WARNING_PATH "/timeout_warning.pcm", 2.0, 2},
    {VOICE_ABNORMAL_OPEN,  VOICE_TYPE_WARNING,     "abnormal_open.pcm",  "检测到异常开门",                VOICE_WARNING_PATH "/abnormal_open.pcm", 1.5, 3},
    {VOICE_LOW_BATTERY,    VOICE_TYPE_WARNING,     "low_battery.pcm",    "电池电量低，请及时更换",          VOICE_WARNING_PATH "/low_battery.pcm", 2.0, 2},
    {VOICE_TAMPER_ALARM,   VOICE_TYPE_WARNING,     "tamper_alarm.pcm",   "检测到异常，请注意安全",          VOICE_WARNING_PATH "/tamper_alarm.pcm", 2.0, 3},

    /* 欢迎语音 */
    {VOICE_WELCOME_HOME,   VOICE_TYPE_WELCOME,     "welcome_home.pcm",   "欢迎回家",                    VOICE_WELCOME_PATH "/welcome_home.pcm", 1.0, 1},
    {VOICE_GOODBYE,        VOICE_TYPE_WELCOME,     "goodbye.pcm",        "再见，请注意安全",              VOICE_WELCOME_PATH "/goodbye.pcm",      1.5, 1},
};

/* 播放器状态 */
static struct {
    player_status_t status;
    voice_config_t config;
    voice_play_complete_cb callback;
    void *callback_data;
    pthread_mutex_t mutex;
    pthread_t play_thread;
    bool thread_running;
    voice_id_t current_voice;
} g_player = {
    .status = PLAYER_STATUS_IDLE,
    .config = {
        .volume = 80,
        .speed = 5,
        .pitch = 5,
        .repeat = false,
        .repeat_count = 1,
    },
    .callback = NULL,
    .callback_data = NULL,
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .thread_running = false,
    .current_voice = VOICE_MAX,
};

/* 函数声明 */
static void* play_thread_func(void *arg);
static int play_pcm_file(const char *filepath, int volume);
static int generate_timeout_voice(int seconds, const char *output_path);

/**
 * @brief 初始化语音播放器
 */
int voice_player_init(void)
{
    pthread_mutex_lock(&g_player.mutex);

    g_player.status = PLAYER_STATUS_IDLE;
    g_player.thread_running = false;
    g_player.current_voice = VOICE_MAX;

    pthread_mutex_unlock(&g_player.mutex);

    syslog(LOG_INFO, "Voice player initialized\n");
    return 0;
}

/**
 * @brief 播放语音
 */
int voice_player_play(voice_id_t voice_id)
{
    if (voice_id >= VOICE_MAX) {
        syslog(LOG_ERR, "Invalid voice ID: %d\n", voice_id);
        return -1;
    }

    pthread_mutex_lock(&g_player.mutex);

    /* 如果正在播放，先停止 */
    if (g_player.status == PLAYER_STATUS_PLAYING) {
        g_player.thread_running = false;
        pthread_mutex_unlock(&g_player.mutex);
        usleep(100000);  /* 等待播放线程结束 */
        pthread_mutex_lock(&g_player.mutex);
    }

    g_player.current_voice = voice_id;
    g_player.status = PLAYER_STATUS_PLAYING;
    g_player.thread_running = true;

    /* 创建播放线程 */
    int ret = pthread_create(&g_player.play_thread, NULL, play_thread_func, NULL);
    if (ret != 0) {
        syslog(LOG_ERR, "Failed to create play thread: %d\n", ret);
        g_player.status = PLAYER_STATUS_ERROR;
        pthread_mutex_unlock(&g_player.mutex);
        return -1;
    }

    pthread_mutex_unlock(&g_player.mutex);

    syslog(LOG_INFO, "Playing voice: %s\n", g_voice_info[voice_id].text);
    return 0;
}

/**
 * @brief 播放带参数的语音
 */
int voice_player_play_with_params(voice_id_t voice_id, int *params, int param_count)
{
    if (voice_id >= VOICE_MAX) {
        syslog(LOG_ERR, "Invalid voice ID: %d\n", voice_id);
        return -1;
    }

    /* 对于超时警告语音，需要生成带参数的语音文件 */
    if (voice_id == VOICE_TIMEOUT_WARNING && param_count > 0) {
        char output_path[256];
        snprintf(output_path, sizeof(output_path), "%s/timeout_%d.pcm",
                 VOICE_WARNING_PATH, params[0]);

        /* 生成带参数的语音文件 */
        int ret = generate_timeout_voice(params[0], output_path);
        if (ret < 0) {
            syslog(LOG_ERR, "Failed to generate timeout voice\n");
            return -1;
        }

        /* 临时修改语音文件路径 */
        voice_info_t *info = (voice_info_t *)&g_voice_info[voice_id];
        const char *original_path = info->path;
        info->path = output_path;

        ret = voice_player_play(voice_id);

        /* 恢复原始路径 */
        info->path = original_path;
        return ret;
    }

    return voice_player_play(voice_id);
}

/**
 * @brief 停止播放
 */
int voice_player_stop(void)
{
    pthread_mutex_lock(&g_player.mutex);

    if (g_player.status == PLAYER_STATUS_PLAYING) {
        g_player.thread_running = false;
        g_player.status = PLAYER_STATUS_IDLE;
    }

    pthread_mutex_unlock(&g_player.mutex);

    return 0;
}

/**
 * @brief 暂停播放
 */
int voice_player_pause(void)
{
    pthread_mutex_lock(&g_player.mutex);

    if (g_player.status == PLAYER_STATUS_PLAYING) {
        g_player.status = PLAYER_STATUS_PAUSED;
    }

    pthread_mutex_unlock(&g_player.mutex);

    return 0;
}

/**
 * @brief 恢复播放
 */
int voice_player_resume(void)
{
    pthread_mutex_lock(&g_player.mutex);

    if (g_player.status == PLAYER_STATUS_PAUSED) {
        g_player.status = PLAYER_STATUS_PLAYING;
    }

    pthread_mutex_unlock(&g_player.mutex);

    return 0;
}

/**
 * @brief 设置音量
 */
int voice_player_set_volume(int volume)
{
    if (volume < 0 || volume > 100) {
        syslog(LOG_ERR, "Invalid volume: %d\n", volume);
        return -1;
    }

    pthread_mutex_lock(&g_player.mutex);
    g_player.config.volume = volume;
    pthread_mutex_unlock(&g_player.mutex);

    syslog(LOG_INFO, "Volume set to %d\n", volume);
    return 0;
}

/**
 * @brief 获取音量
 */
int voice_player_get_volume(void)
{
    int volume;

    pthread_mutex_lock(&g_player.mutex);
    volume = g_player.config.volume;
    pthread_mutex_unlock(&g_player.mutex);

    return volume;
}

/**
 * @brief 设置播放配置
 */
int voice_player_set_config(const voice_config_t *config)
{
    if (config == NULL) {
        return -1;
    }

    pthread_mutex_lock(&g_player.mutex);
    memcpy(&g_player.config, config, sizeof(voice_config_t));
    pthread_mutex_unlock(&g_player.mutex);

    return 0;
}

/**
 * @brief 获取播放配置
 */
int voice_player_get_config(voice_config_t *config)
{
    if (config == NULL) {
        return -1;
    }

    pthread_mutex_lock(&g_player.mutex);
    memcpy(config, &g_player.config, sizeof(voice_config_t));
    pthread_mutex_unlock(&g_player.mutex);

    return 0;
}

/**
 * @brief 获取播放状态
 */
player_status_t voice_player_get_status(void)
{
    player_status_t status;

    pthread_mutex_lock(&g_player.mutex);
    status = g_player.status;
    pthread_mutex_unlock(&g_player.mutex);

    return status;
}

/**
 * @brief 检查是否正在播放
 */
bool voice_player_is_playing(void)
{
    return voice_player_get_status() == PLAYER_STATUS_PLAYING;
}

/**
 * @brief 等待播放完成
 */
int voice_player_wait_completion(uint32_t timeout_ms)
{
    uint32_t elapsed = 0;

    while (elapsed < timeout_ms) {
        if (!voice_player_is_playing()) {
            return 0;
        }
        usleep(10000);  /* 10ms */
        elapsed += 10;
    }

    return -1;  /* 超时 */
}

/**
 * @brief 注册播放完成回调
 */
void voice_player_register_callback(voice_play_complete_cb callback, void *user_data)
{
    pthread_mutex_lock(&g_player.mutex);
    g_player.callback = callback;
    g_player.callback_data = user_data;
    pthread_mutex_unlock(&g_player.mutex);
}

/**
 * @brief 获取语音文件路径
 */
const char* voice_player_get_file_path(voice_id_t voice_id)
{
    if (voice_id >= VOICE_MAX) {
        return NULL;
    }

    return g_voice_info[voice_id].path;
}

/**
 * @brief 获取语音文本
 */
const char* voice_player_get_text(voice_id_t voice_id)
{
    if (voice_id >= VOICE_MAX) {
        return NULL;
    }

    return g_voice_info[voice_id].text;
}

/**
 * @brief 反初始化语音播放器
 */
int voice_player_deinit(void)
{
    voice_player_stop();

    pthread_mutex_lock(&g_player.mutex);
    g_player.status = PLAYER_STATUS_IDLE;
    g_player.thread_running = false;
    pthread_mutex_unlock(&g_player.mutex);

    syslog(LOG_INFO, "Voice player deinitialized\n");
    return 0;
}

/**
 * @brief 播放线程函数
 */
static void* play_thread_func(void *arg)
{
    voice_id_t voice_id;
    const char *filepath;
    int volume;

    pthread_mutex_lock(&g_player.mutex);
    voice_id = g_player.current_voice;
    filepath = g_voice_info[voice_id].path;
    volume = g_player.config.volume;
    pthread_mutex_unlock(&g_player.mutex);

    /* 播放 PCM 文件 */
    int ret = play_pcm_file(filepath, volume);

    pthread_mutex_lock(&g_player.mutex);

    /* 检查是否被停止 */
    if (g_player.thread_running) {
        g_player.status = PLAYER_STATUS_IDLE;
        g_player.thread_running = false;

        /* 调用完成回调 */
        if (g_player.callback) {
            g_player.callback(voice_id, g_player.callback_data);
        }
    }

    pthread_mutex_unlock(&g_player.mutex);

    return NULL;
}

/**
 * @brief 播放 PCM 文件
 */
static int play_pcm_file(const char *filepath, int volume)
{
    if (filepath == NULL) {
        syslog(LOG_ERR, "Invalid filepath\n");
        return -1;
    }

    /* 检查文件是否存在 */
    struct stat st;
    if (stat(filepath, &st) != 0) {
        syslog(LOG_ERR, "File not found: %s\n", filepath);
        return -1;
    }

    /* 打开音频设备 */
    int fd = open("/dev/audio/pcm0", O_WRONLY);
    if (fd < 0) {
        syslog(LOG_ERR, "Failed to open audio device\n");
        return -1;
    }

    /* 打开 PCM 文件 */
    FILE *pcm_file = fopen(filepath, "rb");
    if (pcm_file == NULL) {
        syslog(LOG_ERR, "Failed to open PCM file: %s\n", filepath);
        close(fd);
        return -1;
    }

    /* 设置音量 */
    /* TODO: 调用音频设备接口设置音量 */

    /* 读取并播放 PCM 数据 */
    uint8_t buffer[4096];
    size_t bytes_read;

    while (g_player.thread_running) {
        bytes_read = fread(buffer, 1, sizeof(buffer), pcm_file);
        if (bytes_read == 0) {
            break;  /* 播放完成 */
        }

        /* 写入音频设备 */
        ssize_t bytes_written = write(fd, buffer, bytes_read);
        if (bytes_written < 0) {
            syslog(LOG_ERR, "Failed to write audio data\n");
            break;
        }

        /* 检查是否暂停 */
        while (g_player.status == PLAYER_STATUS_PAUSED && g_player.thread_running) {
            usleep(10000);
        }
    }

    /* 清理资源 */
    fclose(pcm_file);
    close(fd);

    return 0;
}

/**
 * @brief 生成带参数的超时警告语音
 */
static int generate_timeout_voice(int seconds, const char *output_path)
{
    /* TODO: 使用 TTS 生成带参数的语音文件 */
    /* 这里简化处理，直接复制原始语音文件 */
    const char *source_path = g_voice_info[VOICE_TIMEOUT_WARNING].path;

    FILE *source = fopen(source_path, "rb");
    if (source == NULL) {
        syslog(LOG_ERR, "Failed to open source file: %s\n", source_path);
        return -1;
    }

    FILE *output = fopen(output_path, "wb");
    if (output == NULL) {
        syslog(LOG_ERR, "Failed to open output file: %s\n", output_path);
        fclose(source);
        return -1;
    }

    /* 复制文件 */
    uint8_t buffer[4096];
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), source)) > 0) {
        fwrite(buffer, 1, bytes_read, output);
    }

    fclose(source);
    fclose(output);

    syslog(LOG_INFO, "Generated timeout voice: %s\n", output_path);
    return 0;
}
