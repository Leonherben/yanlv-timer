#pragma once

#include "src/core/timer_types.h"
#include <functional>
#include <chrono>
#include <mutex>
#include <atomic>

namespace yanlv {

// 回调函数类型
using StateChangeCallback = std::function<void(TimerState oldState, TimerState newState)>;
using TickCallback = std::function<void(int64_t remainingSeconds, int64_t elapsedSeconds)>;
using BreakPromptCallback = std::function<void()>;
using BreakFinishedCallback = std::function<void()>;

class TimerEngine {
public:
    static TimerEngine& Instance();

    // 初始化（尝试从崩溃中恢复历史未关闭 session）
    void Initialize();

    // 核心操作
    bool StartStudy(int64_t categoryId, int64_t durationSeconds);
    bool Pause();
    bool Resume();
    bool Abort();                                // 提前结束 (计入实际时长)
    bool CancelStudy();                          // 取消学习 (不计入历史记录)
    bool StartBreak(int64_t breakDuration = 300);// 开启全屏/悬浮休息 (默认5分钟)
    bool SkipBreak();                            // 结束/跳过休息

    // 用户在提醒模式下的交互确认
    void ConfirmBreak(bool acceptBreak);

    // 系统电源事件响应
    void OnSystemSleep();
    void OnSystemWake();

    // 周期轮询推进（由主 UI 循环或定时器每 100~250ms 调用）
    void Update();

    // 状态查询
    TimerState GetState() const { return m_state; }
    int64_t GetRemainingSeconds() const;
    int64_t GetElapsedSeconds() const;
    int64_t GetCurrentCategoryId() const { return m_currentCategoryId; }
    int64_t GetPlannedDuration() const { return m_plannedDuration; }
    bool IsPaused() const { return m_state == TimerState::Paused; }

    // 事件注册
    void SetOnStateChange(StateChangeCallback cb) { m_onStateChange = cb; }
    void SetOnTick(TickCallback cb) { m_onTick = cb; }
    void SetOnBreakPrompt(BreakPromptCallback cb) { m_onBreakPrompt = cb; }
    void SetOnBreakFinished(BreakFinishedCallback cb) { m_onBreakFinished = cb; }

private:
    TimerEngine();
    ~TimerEngine();
    TimerEngine(const TimerEngine&) = delete;
    TimerEngine& operator=(const TimerEngine&) = delete;

    void SetState(TimerState newState);
    void SaveSessionHeartbeat();
    void FinalizeStudyRecord(FinishType finishType);

    mutable std::recursive_mutex m_mutex;
    std::atomic<TimerState> m_state{TimerState::Idle};

    int64_t m_currentCategoryId = 1;
    int64_t m_plannedDuration = 0;       // 计划学习秒数
    int64_t m_studyStartTime = 0;        // Unix 秒

    // 基于高精度单调时钟的累计计算
    std::chrono::steady_clock::time_point m_segmentStartTime{};
    int64_t m_accumulatedSeconds = 0;   // 前序已累计秒数（排除暂停）
    int64_t m_breakTotalDuration = 300; // 休息总秒数
    int64_t m_breakElapsedSeconds = 0;  // 休息经过秒数
    std::chrono::steady_clock::time_point m_breakStartTime{};

    std::chrono::steady_clock::time_point m_lastHeartbeatTime{};
    int64_t m_lastReportedRemaining = -1;

    StateChangeCallback m_onStateChange;
    TickCallback m_onTick;
    BreakPromptCallback m_onBreakPrompt;
    BreakFinishedCallback m_onBreakFinished;
};

} // namespace yanlv
