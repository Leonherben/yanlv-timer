#pragma once

#include <string>
#include <cstdint>
#include <chrono>

namespace yanlv {

// 状态机状态
enum class TimerState {
    Idle,           // 待机状态 (显示 00:00 或上次设定)
    Studying,       // 学习倒计时进行中
    Paused,         // 学习已暂停
    BreakPending,   // 正常学习结束，提醒模式下等待用户选择是否休息
    Breaking,       // 5分钟全屏/悬浮休息倒计时中
};

// 学习结束方式
enum class FinishType {
    Normal = 0,     // 正常倒计时结束
    Aborted = 1,    // 用户提前结束
    Interrupted = 2 // 异常崩溃/关机恢复
};

// 休息触发模式
enum class BreakMode {
    Auto = 0,       // 倒计时结束后自动进入5分钟全屏休息
    Remind = 1      // 倒计时结束后提醒用户选择
};

// 媒体类型
enum class MediaType {
    Image,
    Video
};

// 学习类别
struct Category {
    int64_t id = 0;
    std::wstring name;
    int64_t createdAt = 0;
};

// 学习记录
struct StudyRecord {
    int64_t id = 0;
    int64_t categoryId = 1;
    std::wstring categoryName;
    int64_t startTime = 0;         // Unix 时间戳 (秒)
    int64_t endTime = 0;           // Unix 时间戳 (秒)
    int64_t plannedDuration = 0;   // 计划时长 (秒)
    int64_t actualDuration = 0;    // 实际有效学习时长 (秒，排除暂停)
    FinishType finishType = FinishType::Normal;
};

// 崩溃容灾心跳表结构
struct ActiveSession {
    int64_t categoryId = 1;
    int64_t startTime = 0;
    int64_t plannedDuration = 0;
    int64_t elapsedSeconds = 0;
    bool isPaused = false;
};

// 统计汇总
struct StudyStatistics {
    int64_t totalDurationSeconds = 0;
    int64_t totalCount = 0;
    int64_t todayDurationSeconds = 0;
    int64_t todayCount = 0;
};

// 类别统计项
struct CategoryStatistics {
    int64_t categoryId = 0;
    std::wstring categoryName;
    int64_t totalDurationSeconds = 0;
    int64_t totalCount = 0;
};

// 应用配置
struct AppConfig {
    BreakMode breakMode = BreakMode::Remind;
    int64_t lastDurationSeconds = 25 * 60; // 默认 25 分钟
    int64_t lastCategoryId = 1;            // 默认“未分类”
    std::wstring customMediaPath;          // 用户选择的休息媒体路径
    bool videoMuted = true;                // 视频默认静音
    int clockPosX = 100;
    int clockPosY = 100;
    bool alwaysOnTop = true;
};

} // namespace yanlv
