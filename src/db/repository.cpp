#include "src/db/repository.h"
#include "src/db/sqlite3.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ctime>
#include <sstream>

namespace yanlv {

namespace {

std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), size);
    return result;
}

int64_t GetTodayStartTimestamp() {
    auto now = std::time(nullptr);
    std::tm tmNow{};
#ifdef _MSC_VER
    localtime_s(&tmNow, &now);
#else
    localtime_r(&now, &tmNow);
#endif
    tmNow.tm_hour = 0;
    tmNow.tm_min = 0;
    tmNow.tm_sec = 0;
    return static_cast<int64_t>(std::mktime(&tmNow));
}

} // namespace

Repository& Repository::Instance() {
    static Repository instance;
    return instance;
}

Repository::~Repository() {
    Close();
}

void Repository::Close() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

bool Repository::Initialize(const std::wstring& dbPath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }

    std::string utf8Path = WideToUtf8(dbPath);
    int rc = sqlite3_open_v2(utf8Path.c_str(), &m_db, 
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);
    if (rc != SQLITE_OK) {
        if (m_db) {
            sqlite3_close(m_db);
            m_db = nullptr;
        }
        return false;
    }

    // 启用 WAL 模式以提升并发与崩溃容灾性能
    sqlite3_exec(m_db, "PRAGMA journal_mode = WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(m_db, "PRAGMA synchronous = NORMAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(m_db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);

    return CreateTables() && EnsureDefaultCategory();
}

bool Repository::CreateTables() {
    const char* sql = 
        "CREATE TABLE IF NOT EXISTS categories ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name TEXT NOT NULL UNIQUE,"
        "  created_at INTEGER NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS records ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  category_id INTEGER NOT NULL,"
        "  start_time INTEGER NOT NULL,"
        "  end_time INTEGER NOT NULL,"
        "  planned_duration INTEGER NOT NULL,"
        "  actual_duration INTEGER NOT NULL,"
        "  finish_type INTEGER NOT NULL,"
        "  FOREIGN KEY(category_id) REFERENCES categories(id)"
        ");"
        "CREATE TABLE IF NOT EXISTS active_session ("
        "  id INTEGER PRIMARY KEY CHECK (id = 1),"
        "  category_id INTEGER NOT NULL,"
        "  start_time INTEGER NOT NULL,"
        "  planned_duration INTEGER NOT NULL,"
        "  elapsed_seconds INTEGER NOT NULL,"
        "  is_paused INTEGER NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS settings ("
        "  key TEXT PRIMARY KEY,"
        "  value TEXT NOT NULL"
        ");";

    char* errMsg = nullptr;
    int rc = sqlite3_exec(m_db, sql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        if (errMsg) sqlite3_free(errMsg);
        return false;
    }
    return true;
}

bool Repository::EnsureDefaultCategory() {
    const char* sql = "INSERT OR IGNORE INTO categories (id, name, created_at) VALUES (1, '未分类', strftime('%s', 'now'));";
    return sqlite3_exec(m_db, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
}

std::vector<Category> Repository::GetAllCategories() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<Category> result;
    if (!m_db) return result;

    const char* sql = "SELECT id, name, created_at FROM categories ORDER BY id ASC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Category cat;
            cat.id = sqlite3_column_int64(stmt, 0);
            const char* nameText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            if (nameText) cat.name = Utf8ToWide(nameText);
            cat.createdAt = sqlite3_column_int64(stmt, 2);
            result.push_back(cat);
        }
        sqlite3_finalize(stmt);
    }
    return result;
}

int64_t Repository::AddCategory(const std::wstring& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db || name.empty()) return -1;

    const char* sql = "INSERT INTO categories (name, created_at) VALUES (?, strftime('%s', 'now'));";
    sqlite3_stmt* stmt = nullptr;
    int64_t newId = -1;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        std::string utf8Name = WideToUtf8(name);
        sqlite3_bind_text(stmt, 1, utf8Name.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            newId = sqlite3_last_insert_rowid(m_db);
        }
        sqlite3_finalize(stmt);
    }
    return newId;
}

bool Repository::UpdateCategoryName(int64_t id, const std::wstring& newName) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db || id <= 0 || newName.empty()) return false;

    const char* sql = "UPDATE categories SET name = ? WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    bool success = false;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        std::string utf8Name = WideToUtf8(newName);
        sqlite3_bind_text(stmt, 1, utf8Name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 2, id);
        success = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
    }
    return success;
}

std::wstring Repository::GetCategoryName(int64_t id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db) return L"";

    const char* sql = "SELECT name FROM categories WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    std::wstring name;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (text) name = Utf8ToWide(text);
        }
        sqlite3_finalize(stmt);
    }
    return name;
}

int64_t Repository::AddRecord(const StudyRecord& record) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db) return -1;

    const char* sql = 
        "INSERT INTO records (category_id, start_time, end_time, planned_duration, actual_duration, finish_type) "
        "VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    int64_t newId = -1;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, record.categoryId);
        sqlite3_bind_int64(stmt, 2, record.startTime);
        sqlite3_bind_int64(stmt, 3, record.endTime);
        sqlite3_bind_int64(stmt, 4, record.plannedDuration);
        sqlite3_bind_int64(stmt, 5, record.actualDuration);
        sqlite3_bind_int(stmt, 6, static_cast<int>(record.finishType));
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            newId = sqlite3_last_insert_rowid(m_db);
        }
        sqlite3_finalize(stmt);
    }
    return newId;
}

bool Repository::UpdateRecordCategory(int64_t recordId, int64_t newCategoryId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db || recordId <= 0 || newCategoryId <= 0) return false;

    const char* sql = "UPDATE records SET category_id = ? WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    bool success = false;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, newCategoryId);
        sqlite3_bind_int64(stmt, 2, recordId);
        success = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
    }
    return success;
}

bool Repository::DeleteRecord(int64_t recordId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db || recordId <= 0) return false;

    const char* sql = "DELETE FROM records WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    bool success = false;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, recordId);
        success = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
    }
    return success;
}

std::vector<StudyRecord> Repository::GetRecords(int64_t categoryIdFilter, int limit) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<StudyRecord> result;
    if (!m_db) return result;

    std::string sql = 
        "SELECT r.id, r.category_id, c.name, r.start_time, r.end_time, "
        "       r.planned_duration, r.actual_duration, r.finish_type "
        "FROM records r "
        "LEFT JOIN categories c ON r.category_id = c.id ";
    if (categoryIdFilter > 0) {
        sql += "WHERE r.category_id = ? ";
    }
    sql += "ORDER BY r.start_time DESC LIMIT ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        int paramIdx = 1;
        if (categoryIdFilter > 0) {
            sqlite3_bind_int64(stmt, paramIdx++, categoryIdFilter);
        }
        sqlite3_bind_int(stmt, paramIdx++, limit > 0 ? limit : 200);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            StudyRecord rec;
            rec.id = sqlite3_column_int64(stmt, 0);
            rec.categoryId = sqlite3_column_int64(stmt, 1);
            const char* catName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            rec.categoryName = catName ? Utf8ToWide(catName) : L"未分类";
            rec.startTime = sqlite3_column_int64(stmt, 3);
            rec.endTime = sqlite3_column_int64(stmt, 4);
            rec.plannedDuration = sqlite3_column_int64(stmt, 5);
            rec.actualDuration = sqlite3_column_int64(stmt, 6);
            rec.finishType = static_cast<FinishType>(sqlite3_column_int(stmt, 7));
            result.push_back(rec);
        }
        sqlite3_finalize(stmt);
    }
    return result;
}

StudyStatistics Repository::GetOverallStatistics() {
    std::lock_guard<std::mutex> lock(m_mutex);
    StudyStatistics stats;
    if (!m_db) return stats;

    // 总体累计
    const char* sqlTotal = "SELECT COALESCE(SUM(actual_duration), 0), COUNT(id) FROM records;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sqlTotal, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            stats.totalDurationSeconds = sqlite3_column_int64(stmt, 0);
            stats.totalCount = sqlite3_column_int64(stmt, 1);
        }
        sqlite3_finalize(stmt);
    }

    // 今日统计
    int64_t todayStart = GetTodayStartTimestamp();
    const char* sqlToday = "SELECT COALESCE(SUM(actual_duration), 0), COUNT(id) FROM records WHERE start_time >= ?;";
    if (sqlite3_prepare_v2(m_db, sqlToday, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, todayStart);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            stats.todayDurationSeconds = sqlite3_column_int64(stmt, 0);
            stats.todayCount = sqlite3_column_int64(stmt, 1);
        }
        sqlite3_finalize(stmt);
    }

    return stats;
}

std::vector<CategoryStatistics> Repository::GetCategoryStatistics() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<CategoryStatistics> result;
    if (!m_db) return result;

    const char* sql = 
        "SELECT c.id, c.name, COALESCE(SUM(r.actual_duration), 0) AS total_sec, COUNT(r.id) AS total_cnt "
        "FROM categories c "
        "LEFT JOIN records r ON c.id = r.category_id "
        "GROUP BY c.id, c.name "
        "ORDER BY total_sec DESC, c.id ASC;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            CategoryStatistics item;
            item.categoryId = sqlite3_column_int64(stmt, 0);
            const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            item.categoryName = name ? Utf8ToWide(name) : L"未分类";
            item.totalDurationSeconds = sqlite3_column_int64(stmt, 2);
            item.totalCount = sqlite3_column_int64(stmt, 3);
            result.push_back(item);
        }
        sqlite3_finalize(stmt);
    }
    return result;
}

bool Repository::SaveActiveSession(const ActiveSession& session) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db) return false;

    const char* sql = 
        "INSERT INTO active_session (id, category_id, start_time, planned_duration, elapsed_seconds, is_paused) "
        "VALUES (1, ?, ?, ?, ?, ?) "
        "ON CONFLICT(id) DO UPDATE SET "
        "category_id = excluded.category_id, "
        "start_time = excluded.start_time, "
        "planned_duration = excluded.planned_duration, "
        "elapsed_seconds = excluded.elapsed_seconds, "
        "is_paused = excluded.is_paused;";

    sqlite3_stmt* stmt = nullptr;
    bool success = false;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, session.categoryId);
        sqlite3_bind_int64(stmt, 2, session.startTime);
        sqlite3_bind_int64(stmt, 3, session.plannedDuration);
        sqlite3_bind_int64(stmt, 4, session.elapsedSeconds);
        sqlite3_bind_int(stmt, 5, session.isPaused ? 1 : 0);
        success = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
    }
    return success;
}

bool Repository::ClearActiveSession() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db) return false;
    const char* sql = "DELETE FROM active_session WHERE id = 1;";
    return sqlite3_exec(m_db, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
}

bool Repository::CheckAndRecoverInterruptedSession(StudyRecord* outRecovered) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db) return false;

    const char* sql = "SELECT category_id, start_time, planned_duration, elapsed_seconds FROM active_session WHERE id = 1;";
    sqlite3_stmt* stmt = nullptr;
    bool hasSession = false;
    ActiveSession session;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            hasSession = true;
            session.categoryId = sqlite3_column_int64(stmt, 0);
            session.startTime = sqlite3_column_int64(stmt, 1);
            session.plannedDuration = sqlite3_column_int64(stmt, 2);
            session.elapsedSeconds = sqlite3_column_int64(stmt, 3);
        }
        sqlite3_finalize(stmt);
    }

    if (hasSession && session.elapsedSeconds > 0) {
        // 将未正常闭环的 session 恢复为一条中断记录
        StudyRecord rec;
        rec.categoryId = session.categoryId;
        rec.startTime = session.startTime;
        rec.endTime = session.startTime + session.elapsedSeconds;
        rec.plannedDuration = session.plannedDuration;
        rec.actualDuration = session.elapsedSeconds;
        rec.finishType = FinishType::Interrupted;

        const char* insertSql = 
            "INSERT INTO records (category_id, start_time, end_time, planned_duration, actual_duration, finish_type) "
            "VALUES (?, ?, ?, ?, ?, ?);";
        sqlite3_stmt* insStmt = nullptr;
        if (sqlite3_prepare_v2(m_db, insertSql, -1, &insStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int64(insStmt, 1, rec.categoryId);
            sqlite3_bind_int64(insStmt, 2, rec.startTime);
            sqlite3_bind_int64(insStmt, 3, rec.endTime);
            sqlite3_bind_int64(insStmt, 4, rec.plannedDuration);
            sqlite3_bind_int64(insStmt, 5, rec.actualDuration);
            sqlite3_bind_int(insStmt, 6, static_cast<int>(rec.finishType));
            if (sqlite3_step(insStmt) == SQLITE_DONE) {
                rec.id = sqlite3_last_insert_rowid(m_db);
                if (outRecovered) {
                    *outRecovered = rec;
                }
            }
            sqlite3_finalize(insStmt);
        }
        // 清理暂存表
        sqlite3_exec(m_db, "DELETE FROM active_session WHERE id = 1;", nullptr, nullptr, nullptr);
        return true;
    }

    // 若 elapsedSeconds 为 0，直接清空无效 session
    sqlite3_exec(m_db, "DELETE FROM active_session WHERE id = 1;", nullptr, nullptr, nullptr);
    return false;
}

bool Repository::LoadConfig(AppConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db) return false;

    const char* sql = "SELECT key, value FROM settings;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* key = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            const char* val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            if (!key || !val) continue;

            std::string sKey(key);
            std::string sVal(val);
            if (sKey == "break_mode") config.breakMode = (sVal == "auto" ? BreakMode::Auto : BreakMode::Remind);
            else if (sKey == "last_duration") config.lastDurationSeconds = std::stoll(sVal);
            else if (sKey == "last_category") config.lastCategoryId = std::stoll(sVal);
            else if (sKey == "custom_media") config.customMediaPath = Utf8ToWide(sVal);
            else if (sKey == "video_muted") config.videoMuted = (sVal == "1");
            else if (sKey == "clock_x") config.clockPosX = std::stoi(sVal);
            else if (sKey == "clock_y") config.clockPosY = std::stoi(sVal);
            else if (sKey == "always_on_top") config.alwaysOnTop = (sVal == "1");
        }
        sqlite3_finalize(stmt);
    }
    return true;
}

bool Repository::SaveConfig(const AppConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db) return false;

    auto upsertSetting = [this](const std::string& key, const std::string& val) {
        const char* sql = 
            "INSERT INTO settings (key, value) VALUES (?, ?) "
            "ON CONFLICT(key) DO UPDATE SET value = excluded.value;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, val.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    };

    upsertSetting("break_mode", config.breakMode == BreakMode::Auto ? "auto" : "remind");
    upsertSetting("last_duration", std::to_string(config.lastDurationSeconds));
    upsertSetting("last_category", std::to_string(config.lastCategoryId));
    upsertSetting("custom_media", WideToUtf8(config.customMediaPath));
    upsertSetting("video_muted", config.videoMuted ? "1" : "0");
    upsertSetting("clock_x", std::to_string(config.clockPosX));
    upsertSetting("clock_y", std::to_string(config.clockPosY));
    upsertSetting("always_on_top", config.alwaysOnTop ? "1" : "0");

    return true;
}

} // namespace yanlv
