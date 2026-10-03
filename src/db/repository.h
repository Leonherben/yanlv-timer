#pragma once

#include "src/core/timer_types.h"
#include <vector>
#include <string>
#include <memory>
#include <mutex>

struct sqlite3;

namespace yanlv {

class Repository {
public:
    static Repository& Instance();

    bool Initialize(const std::wstring& dbPath);
    void Close();

    // 类别管理
    std::vector<Category> GetAllCategories();
    int64_t AddCategory(const std::wstring& name);
    bool UpdateCategoryName(int64_t id, const std::wstring& newName);
    bool DeleteCategory(int64_t id);
    std::wstring GetCategoryName(int64_t id);

    // 学习记录管理
    int64_t AddRecord(const StudyRecord& record);
    bool UpdateRecordCategory(int64_t recordId, int64_t newCategoryId);
    bool DeleteRecord(int64_t recordId);
    std::vector<StudyRecord> GetRecords(int64_t categoryIdFilter = 0, int limit = 200);

    // 统计分析
    StudyStatistics GetOverallStatistics();
    std::vector<CategoryStatistics> GetCategoryStatistics();

    // 崩溃容灾心跳表
    bool SaveActiveSession(const ActiveSession& session);
    bool ClearActiveSession();
    bool CheckAndRecoverInterruptedSession(StudyRecord* outRecovered = nullptr);

    // 应用配置
    bool LoadConfig(AppConfig& config);
    bool SaveConfig(const AppConfig& config);
    bool SaveClockPosition(int x, int y, bool alwaysOnTop);

private:
    Repository() = default;
    ~Repository();
    Repository(const Repository&) = delete;
    Repository& operator=(const Repository&) = delete;

    sqlite3* m_db = nullptr;
    std::mutex m_mutex;

    bool CreateTables();
    bool EnsureDefaultCategory();
};

} // namespace yanlv
