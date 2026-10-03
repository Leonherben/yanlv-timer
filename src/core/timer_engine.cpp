#include "src/core/timer_engine.h"
#include "src/db/repository.h"
#include <ctime>
#include <algorithm>

namespace yanlv {

TimerEngine& TimerEngine::Instance() {
    static TimerEngine instance;
    return instance;
}

TimerEngine::TimerEngine() = default;
TimerEngine::~TimerEngine() = default;

void TimerEngine::Initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    // 检查并恢复上次异常退出的未关闭记录
    StudyRecord recovered;
    if (Repository::Instance().CheckAndRecoverInterruptedSession(&recovered)) {
        // 已自动转为中断恢复记录落库
    }
}

void TimerEngine::SetState(TimerState newState) {
    TimerState oldState = m_state.load();
    if (oldState != newState) {
        m_state.store(newState);
        if (m_onStateChange) {
            m_onStateChange(oldState, newState);
        }
    }
}

bool TimerEngine::StartStudy(int64_t categoryId, int64_t durationSeconds) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state.load() != TimerState::Idle || durationSeconds <= 0) {
        return false;
    }

    m_currentCategoryId = categoryId > 0 ? categoryId : 1;
    m_plannedDuration = durationSeconds;
    m_studyStartTime = static_cast<int64_t>(std::time(nullptr));
    m_accumulatedSeconds = 0;
    m_segmentStartTime = std::chrono::steady_clock::now();
    m_lastHeartbeatTime = m_segmentStartTime;
    m_lastReportedRemaining = durationSeconds;

    SetState(TimerState::Studying);
    SaveSessionHeartbeat();

    if (m_onTick) {
        m_onTick(m_plannedDuration, 0);
    }
    return true;
}

bool TimerEngine::Pause() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state.load() != TimerState::Studying) {
        return false;
    }

    auto now = std::chrono::steady_clock::now();
    auto segmentSeconds = std::chrono::duration_cast<std::chrono::seconds>(now - m_segmentStartTime).count();
    m_accumulatedSeconds += segmentSeconds;

    SetState(TimerState::Paused);
    SaveSessionHeartbeat();
    return true;
}

bool TimerEngine::Resume() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state.load() != TimerState::Paused) {
        return false;
    }

    m_segmentStartTime = std::chrono::steady_clock::now();
    m_lastHeartbeatTime = m_segmentStartTime;

    SetState(TimerState::Studying);
    SaveSessionHeartbeat();
    return true;
}

bool TimerEngine::Abort() {
    std::lock_guard<std::mutex> lock(m_mutex);
    TimerState curr = m_state.load();
    if (curr != TimerState::Studying && curr != TimerState::Paused) {
        return false;
    }

    FinalizeStudyRecord(FinishType::Aborted);
    SetState(TimerState::Idle);

    if (m_onTick) {
        m_onTick(0, 0);
    }
    return true;
}

void TimerEngine::FinalizeStudyRecord(FinishType finishType) {
    int64_t actualSeconds = m_accumulatedSeconds;
    if (m_state.load() == TimerState::Studying) {
        auto now = std::chrono::steady_clock::now();
        actualSeconds += std::chrono::duration_cast<std::chrono::seconds>(now - m_segmentStartTime).count();
    }

    if (finishType == FinishType::Normal) {
        actualSeconds = m_plannedDuration; // 正常结束时直接计为设定时长
    } else {
        actualSeconds = (std::min)(actualSeconds, m_plannedDuration);
    }

    if (actualSeconds < 0) actualSeconds = 0;

    StudyRecord record;
    record.categoryId = m_currentCategoryId;
    record.startTime = m_studyStartTime;
    record.endTime = static_cast<int64_t>(std::time(nullptr));
    record.plannedDuration = m_plannedDuration;
    record.actualDuration = actualSeconds;
    record.finishType = finishType;

    Repository::Instance().AddRecord(record);
    Repository::Instance().ClearActiveSession();

    m_accumulatedSeconds = 0;
    m_plannedDuration = 0;
}

void TimerEngine::SaveSessionHeartbeat() {
    int64_t actualSeconds = m_accumulatedSeconds;
    if (m_state.load() == TimerState::Studying) {
        auto now = std::chrono::steady_clock::now();
        actualSeconds += std::chrono::duration_cast<std::chrono::seconds>(now - m_segmentStartTime).count();
    }

    ActiveSession session;
    session.categoryId = m_currentCategoryId;
    session.startTime = m_studyStartTime;
    session.plannedDuration = m_plannedDuration;
    session.elapsedSeconds = actualSeconds;
    session.isPaused = (m_state.load() == TimerState::Paused);

    Repository::Instance().SaveActiveSession(session);
}

void TimerEngine::ConfirmBreak(bool acceptBreak) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state.load() != TimerState::BreakPending) {
        return;
    }

    if (acceptBreak) {
        StartBreak(300);
    } else {
        SetState(TimerState::Idle);
        if (m_onTick) {
            m_onTick(0, 0);
        }
    }
}

bool TimerEngine::StartBreak(int64_t breakDuration) {
    m_breakTotalDuration = breakDuration > 0 ? breakDuration : 300;
    m_breakElapsedSeconds = 0;
    m_breakStartTime = std::chrono::steady_clock::now();

    SetState(TimerState::Breaking);

    if (m_onTick) {
        m_onTick(m_breakTotalDuration, 0);
    }
    return true;
}

bool TimerEngine::SkipBreak() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state.load() != TimerState::Breaking) {
        return false;
    }

    SetState(TimerState::Idle);

    if (m_onBreakFinished) {
        m_onBreakFinished();
    }
    if (m_onTick) {
        m_onTick(0, 0);
    }
    return true;
}

void TimerEngine::OnSystemSleep() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state.load() == TimerState::Studying) {
        auto now = std::chrono::steady_clock::now();
        auto segmentSeconds = std::chrono::duration_cast<std::chrono::seconds>(now - m_segmentStartTime).count();
        m_accumulatedSeconds += segmentSeconds;
        SetState(TimerState::Paused);
        SaveSessionHeartbeat();
    }
}

void TimerEngine::OnSystemWake() {
    // 唤醒后保持 Paused，等待用户明确确认继续，避免后台偷跑学习时间
}

int64_t TimerEngine::GetRemainingSeconds() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    TimerState st = m_state.load();
    if (st == TimerState::Studying) {
        auto now = std::chrono::steady_clock::now();
        int64_t curSeg = std::chrono::duration_cast<std::chrono::seconds>(now - m_segmentStartTime).count();
        int64_t rem = m_plannedDuration - (m_accumulatedSeconds + curSeg);
        return rem > 0 ? rem : 0;
    } else if (st == TimerState::Paused) {
        int64_t rem = m_plannedDuration - m_accumulatedSeconds;
        return rem > 0 ? rem : 0;
    } else if (st == TimerState::Breaking) {
        auto now = std::chrono::steady_clock::now();
        int64_t curSeg = std::chrono::duration_cast<std::chrono::seconds>(now - m_breakStartTime).count();
        int64_t rem = m_breakTotalDuration - curSeg;
        return rem > 0 ? rem : 0;
    }
    return 0;
}

int64_t TimerEngine::GetElapsedSeconds() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    TimerState st = m_state.load();
    if (st == TimerState::Studying) {
        auto now = std::chrono::steady_clock::now();
        int64_t curSeg = std::chrono::duration_cast<std::chrono::seconds>(now - m_segmentStartTime).count();
        return m_accumulatedSeconds + curSeg;
    } else if (st == TimerState::Paused) {
        return m_accumulatedSeconds;
    } else if (st == TimerState::Breaking) {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(now - m_breakStartTime).count();
    }
    return 0;
}

void TimerEngine::Update() {
    std::lock_guard<std::mutex> lock(m_mutex);
    TimerState st = m_state.load();
    auto now = std::chrono::steady_clock::now();

    if (st == TimerState::Studying) {
        int64_t curSeg = std::chrono::duration_cast<std::chrono::seconds>(now - m_segmentStartTime).count();
        int64_t totalElapsed = m_accumulatedSeconds + curSeg;
        int64_t remaining = m_plannedDuration - totalElapsed;

        if (remaining <= 0) {
            // 倒计时自然归零，正常完成
            FinalizeStudyRecord(FinishType::Normal);

            AppConfig config;
            Repository::Instance().LoadConfig(config);

            if (config.breakMode == BreakMode::Auto) {
                StartBreak(300);
            } else {
                SetState(TimerState::BreakPending);
                if (m_onBreakPrompt) {
                    m_onBreakPrompt();
                }
            }
            return;
        }

        // 每 15 秒保存一次心跳
        if (std::chrono::duration_cast<std::chrono::seconds>(now - m_lastHeartbeatTime).count() >= 15) {
            SaveSessionHeartbeat();
            m_lastHeartbeatTime = now;
        }

        if (remaining != m_lastReportedRemaining) {
            m_lastReportedRemaining = remaining;
            if (m_onTick) {
                m_onTick(remaining, totalElapsed);
            }
        }
    } else if (st == TimerState::Breaking) {
        int64_t curSeg = std::chrono::duration_cast<std::chrono::seconds>(now - m_breakStartTime).count();
        int64_t remaining = m_breakTotalDuration - curSeg;

        if (remaining <= 0) {
            SetState(TimerState::Idle);
            if (m_onBreakFinished) {
                m_onBreakFinished();
            }
            if (m_onTick) {
                m_onTick(0, 0);
            }
            return;
        }

        if (remaining != m_lastReportedRemaining) {
            m_lastReportedRemaining = remaining;
            if (m_onTick) {
                m_onTick(remaining, curSeg);
            }
        }
    }
}

} // namespace yanlv
