#include <iostream>
#include <cassert>
#include <chrono>
#include <thread>
#include <filesystem>
#include "src/core/timer_types.h"
#include "src/db/repository.h"
#include "src/core/timer_engine.h"

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "[FAIL] " << msg << " (" << #cond << ") at line " << __LINE__ << std::endl; \
            return false; \
        } \
    } while(0)

namespace fs = std::filesystem;

bool TestDatabaseAndCategories() {
    std::cout << "[RUN] Testing Database and Category Management..." << std::endl;
    std::wstring testDb = L"test_yanlv.db";
    if (fs::exists("test_yanlv.db")) {
        fs::remove("test_yanlv.db");
    }

    auto& repo = yanlv::Repository::Instance();
    TEST_ASSERT(repo.Initialize(testDb), "Failed to initialize test DB");

    // 1. 验证默认分类
    auto cats = repo.GetAllCategories();
    TEST_ASSERT(!cats.empty(), "Categories should not be empty");
    TEST_ASSERT(cats[0].id == 1, "Default category ID should be 1");
    TEST_ASSERT(cats[0].name == L"未分类", "Default category name should be 未分类");

    // 2. 添加新分类
    int64_t catId = repo.AddCategory(L"算法刷题");
    TEST_ASSERT(catId > 1, "New category ID should be greater than 1");

    // 3. 重命名分类
    bool renamed = repo.UpdateCategoryName(catId, L"数据结构与算法");
    TEST_ASSERT(renamed, "Failed to rename category");
    TEST_ASSERT(repo.GetCategoryName(catId) == L"数据结构与算法", "Renamed category name mismatch");

    repo.Close();
    if (fs::exists("test_yanlv.db")) fs::remove("test_yanlv.db");
    std::cout << "[PASS] Database and Category Management passed!" << std::endl;
    return true;
}

bool TestRecordsAndStatistics() {
    std::cout << "[RUN] Testing Records CRUD and Statistics Sync..." << std::endl;
    std::wstring testDb = L"test_stats.db";
    if (fs::exists("test_stats.db")) fs::remove("test_stats.db");

    auto& repo = yanlv::Repository::Instance();
    repo.Initialize(testDb);

    int64_t catAlgo = repo.AddCategory(L"数据结构与算法");

    // 添加记录 1: 算法课 25 分钟 (正常完成)
    yanlv::StudyRecord rec1;
    rec1.categoryId = catAlgo;
    rec1.startTime = 1700000000;
    rec1.endTime = 1700001500;
    rec1.plannedDuration = 1500;
    rec1.actualDuration = 1500;
    rec1.finishType = yanlv::FinishType::Normal;
    int64_t r1 = repo.AddRecord(rec1);
    TEST_ASSERT(r1 > 0, "Record 1 insert failed");

    // 添加记录 2: 未分类 10 分钟 (提前结束)
    yanlv::StudyRecord rec2;
    rec2.categoryId = 1;
    rec2.startTime = 1700002000;
    rec2.endTime = 1700002600;
    rec2.plannedDuration = 1500;
    rec2.actualDuration = 600;
    rec2.finishType = yanlv::FinishType::Aborted;
    int64_t r2 = repo.AddRecord(rec2);
    TEST_ASSERT(r2 > 0, "Record 2 insert failed");

    // 校验总体统计
    auto overall = repo.GetOverallStatistics();
    TEST_ASSERT(overall.totalCount == 2, "Total count should be 2");
    TEST_ASSERT(overall.totalDurationSeconds == 2100, "Total duration should be 2100s");

    // 校验类别统计
    auto catStats = repo.GetCategoryStatistics();
    TEST_ASSERT(catStats.size() >= 2, "Category stats should have at least 2 items");

    // 测试修改记录分类: 把记录2从“未分类”转移到“数据结构与算法”
    bool catUpdated = repo.UpdateRecordCategory(r2, catAlgo);
    TEST_ASSERT(catUpdated, "Failed to update record category");

    // 转移后总体统计不变，分类统计转移
    overall = repo.GetOverallStatistics();
    TEST_ASSERT(overall.totalCount == 2, "Overall total count should remain 2");
    TEST_ASSERT(overall.totalDurationSeconds == 2100, "Overall duration should remain 2100s");

    catStats = repo.GetCategoryStatistics();
    for (const auto& item : catStats) {
        if (item.categoryId == catAlgo) {
            TEST_ASSERT(item.totalCount == 2, "CatAlgo count should now be 2");
            TEST_ASSERT(item.totalDurationSeconds == 2100, "CatAlgo duration should now be 2100s");
        } else if (item.categoryId == 1) {
            TEST_ASSERT(item.totalCount == 0, "Default cat count should now be 0");
            TEST_ASSERT(item.totalDurationSeconds == 0, "Default cat duration should now be 0");
        }
    }

    // 测试删除无效记录: 删除记录2
    bool deleted = repo.DeleteRecord(r2);
    TEST_ASSERT(deleted, "Failed to delete record");

    overall = repo.GetOverallStatistics();
    TEST_ASSERT(overall.totalCount == 1, "Overall count should decrease to 1 after deletion");
    TEST_ASSERT(overall.totalDurationSeconds == 1500, "Overall duration should decrease to 1500s");

    repo.Close();
    if (fs::exists("test_stats.db")) fs::remove("test_stats.db");
    std::cout << "[PASS] Records CRUD and Statistics Sync passed!" << std::endl;
    return true;
}

bool TestHeartbeatAndCrashRecovery() {
    std::cout << "[RUN] Testing 15s Heartbeat and Crash Recovery..." << std::endl;
    std::wstring testDb = L"test_recovery.db";
    if (fs::exists("test_recovery.db")) fs::remove("test_recovery.db");

    auto& repo = yanlv::Repository::Instance();
    repo.Initialize(testDb);

    // 模拟一次进行中的学习因断电或崩溃未正常结束
    yanlv::ActiveSession session;
    session.categoryId = 1;
    session.startTime = 1700000000;
    session.plannedDuration = 1500;
    session.elapsedSeconds = 480; // 已学 8 分钟
    session.isPaused = false;
    repo.SaveActiveSession(session);

    // 模拟软件重启，触发容灾恢复
    yanlv::StudyRecord recovered;
    bool hadInterrupted = repo.CheckAndRecoverInterruptedSession(&recovered);
    TEST_ASSERT(hadInterrupted, "Should detect and recover interrupted session");
    TEST_ASSERT(recovered.actualDuration == 480, "Recovered duration mismatch");
    TEST_ASSERT(recovered.finishType == yanlv::FinishType::Interrupted, "Should mark as Interrupted");

    // 再次检查应已被消费，不能重复恢复
    bool doubleCheck = repo.CheckAndRecoverInterruptedSession(nullptr);
    TEST_ASSERT(!doubleCheck, "Session should not be recovered twice");

    repo.Close();
    if (fs::exists("test_recovery.db")) fs::remove("test_recovery.db");
    std::cout << "[PASS] 15s Heartbeat and Crash Recovery passed!" << std::endl;
    return true;
}

bool TestTimerEngineLifecycle() {
    std::cout << "[RUN] Testing TimerEngine Lifecycle & State Transitions..." << std::endl;
    std::wstring testDb = L"test_engine.db";
    if (fs::exists("test_engine.db")) fs::remove("test_engine.db");

    auto& repo = yanlv::Repository::Instance();
    repo.Initialize(testDb);

    auto& engine = yanlv::TimerEngine::Instance();
    engine.Initialize();

    // 注册状态回调，验证回调重入 GetRemainingSeconds() 绝不死锁
    engine.SetOnStateChange([&](yanlv::TimerState oldSt, yanlv::TimerState newSt) {
        int64_t rem = engine.GetRemainingSeconds();
        (void)rem;
    });

    TEST_ASSERT(engine.GetState() == yanlv::TimerState::Idle, "Initial state should be Idle");

    // 开启 10 秒倒计时
    bool started = engine.StartStudy(1, 10);
    TEST_ASSERT(started, "Failed to start study");
    TEST_ASSERT(engine.GetState() == yanlv::TimerState::Studying, "State should be Studying");

    // 测试暂停
    bool paused = engine.Pause();
    TEST_ASSERT(paused, "Failed to pause");
    TEST_ASSERT(engine.GetState() == yanlv::TimerState::Paused, "State should be Paused");

    // 测试继续
    bool resumed = engine.Resume();
    TEST_ASSERT(resumed, "Failed to resume");
    TEST_ASSERT(engine.GetState() == yanlv::TimerState::Studying, "State should be Studying");

    // 测试提前结束
    bool aborted = engine.Abort();
    TEST_ASSERT(aborted, "Failed to abort");
    TEST_ASSERT(engine.GetState() == yanlv::TimerState::Idle, "State should return to Idle");

    // 验证提前结束有如实生成记录
    auto records = repo.GetRecords();
    TEST_ASSERT(!records.empty(), "Aborted study should produce a record");
    TEST_ASSERT(records[0].finishType == yanlv::FinishType::Aborted, "Record finish type should be Aborted");

    repo.Close();
    if (fs::exists("test_engine.db")) fs::remove("test_engine.db");
    std::cout << "[PASS] TimerEngine Lifecycle & State Transitions passed!" << std::endl;
    return true;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "   言律时钟 (Yanlv-timer) 本地核心测试   " << std::endl;
    std::cout << "========================================" << std::endl;

    if (!TestDatabaseAndCategories()) return 1;
    if (!TestRecordsAndStatistics()) return 1;
    if (!TestHeartbeatAndCrashRecovery()) return 1;
    if (!TestTimerEngineLifecycle()) return 1;

    std::cout << "========================================" << std::endl;
    std::cout << "   ALL TESTS PASSED SUCCESSFULLY! (4/4) " << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
}
