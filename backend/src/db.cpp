#include "db.h"
#include <iostream>
#include <set>
#include <map>
#include <ctime>
#include "sqlite3.h"   // 通过 -Ilibs 编译参数找到
#include "json.hpp"     // nlohmann/json

using json = nlohmann::json;
using namespace std;

// ============================================================
//  db.cpp — 数据库层实现
//  所有跟 SQLite 交互的代码都在这一个文件里
//  其他文件（routes_*.cpp）只需要调这里的函数，不用碰 SQL
// ============================================================

// -----------------------------------------------------------
// 1. open_db() — 打开数据库连接
// -----------------------------------------------------------
// 相当于「双击打开 study.db 文件」。
// 如果文件不存在，SQLite 会自动创建一个空文件。
// 返回的是 sqlite3* 指针，后面所有操作都要用到它。
// 注意：用完记得 sqlite3_close(db) 关掉！
// -----------------------------------------------------------

sqlite3* open_db() {
    sqlite3* db = nullptr;
    // sqlite3_open 第一个参数是文件路径，第二个是输出指针
    // 成功返回 SQLITE_OK(0)，失败返回错误码
    int rc = sqlite3_open("./data/study.db", &db);
    if (rc != SQLITE_OK) {
        // cerr 是标准错误输出，类似 cout 但专门打错误日志
        cerr << "[ERROR] 无法打开数据库: " << sqlite3_errmsg(db) << endl;
        return nullptr;
    }
    return db;
}

// -----------------------------------------------------------
// 2. exec_sql() — 执行一句 SQL（私有辅助函数）
// -----------------------------------------------------------
// 因为建表要执行很多条 SQL，每次都写一遍「打开库→执行→关库」太啰嗦，
// 所以抽一个内部函数，传入 sql 字符串就能执行。
// static 表示这个函数只在 db.cpp 内部用，外部不可见。
// -----------------------------------------------------------

static bool exec_sql(sqlite3* db, const string& sql) {
    char* err_msg = nullptr;
    // sqlite3_exec 是 SQLite 最原始的执行方式：
    //   参数1: 数据库句柄
    //   参数2: SQL 字符串
    //   参数3: 回调函数（不需要就传 0）
    //   参数4: 回调参数（不需要就传 0）
    //   参数5: 错误信息（如果出错会把错误文字写到这里）
    int rc = sqlite3_exec(db, sql.c_str(), 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        cerr << "[ERROR] SQL 执行出错: " << err_msg << endl;
        // sqlite3_free 释放错误信息占用的内存，否则会内存泄漏
        sqlite3_free(err_msg);
        return false;
    }
    return true;
}

// -----------------------------------------------------------
// 3. init_tables() — 建表（程序启动时调用一次）
// -----------------------------------------------------------
// IF NOT EXISTS 的意思是：如果表已经存在就跳过，不会重复建。
// 所以每次启动都可以安全地调用这个函数，不用担心覆盖数据。
//
// 表结构设计说明：
//   - 每张表的第一列都是 id，主键 + 自增 = 自动编号 1,2,3...
//   - 带 user_id 的表都是「属于某个用户的」，用于多用户数据隔离
//   - created_at 自动填当前时间，方便统计
// -----------------------------------------------------------

void init_tables() {
    sqlite3* db = open_db();
    if (!db) return;  // 打不开数据库就没必要继续了

    // ============ 1. 用户表 users ============
    // 干啥的：存储所有注册用户的信息
    // token 存的是登录凭证，每次登录都会更新
    // password_hash 存 SHA256 哈希值，不存明文（安全性）
    exec_sql(db, R"(
        CREATE TABLE IF NOT EXISTS users (
            id            INTEGER PRIMARY KEY AUTOINCREMENT,  -- 用户编号，自增
            username      TEXT    NOT NULL UNIQUE,             -- 用户名，UNIQUE 表示不能重复注册
            password_hash TEXT    NOT NULL,                    -- 密码的 SHA256 哈希（不存明文！）
            nickname      TEXT    DEFAULT '',                  -- 昵称，可修改
            signature     TEXT    DEFAULT '',                  -- 个性签名，可修改
            token         TEXT    DEFAULT '',                  -- 当前登录凭证（登录时生成，退出时清空）
            created_at    TEXT    DEFAULT (datetime('now'))    -- 注册时间，datetime('now') 是 SQLite 内置函数
        );
    )");

    // ============ 2. 任务表 tasks ============
    // 干啥的：每个用户创建的学习任务
    // priority: 1=低 2=中 3=高（数字越小越不重要）
    // status: 0=待完成 1=已完成（完成状态由打卡自动更新）
    // need_review: 1=需要复习提醒 0=不需要
    exec_sql(db, R"(
        CREATE TABLE IF NOT EXISTS tasks (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id     INTEGER NOT NULL,                      -- 这个任务属于哪个用户
            title       TEXT    NOT NULL,                       -- 任务标题，比如"背英语单词"
            topic       TEXT    DEFAULT '',                     -- 学习主题，比如"英语"
            deadline    TEXT    DEFAULT '',                     -- 截止日期，格式 YYYY-MM-DD
            priority    INTEGER DEFAULT 1,                     -- 优先级 1低 2中 3高
            status      INTEGER DEFAULT 0,                     -- 状态 0待完成 1已完成
            need_review INTEGER DEFAULT 0,                     -- 是否开启复习提醒 0否 1是
            created_at  TEXT    DEFAULT (datetime('now')),
            FOREIGN KEY (user_id) REFERENCES users(id)         -- 外键：保证 user_id 一定对应一个真实用户
        );
    )");

    // ============ 3. 打卡表 checkins ============
    // 干啥的：记录用户每次完成任务打卡
    // 与 tasks 表关联，知道自己打的是哪个任务的卡
    // UNIQUE(task_id, date) 保证同一天同一个任务不会重复打卡
    exec_sql(db, R"(
        CREATE TABLE IF NOT EXISTS checkins (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id    INTEGER NOT NULL,
            task_id    INTEGER NOT NULL,                        -- 给哪个任务打卡
            date       TEXT    NOT NULL,                         -- 打卡日期 YYYY-MM-DD
            created_at TEXT    DEFAULT (datetime('now')),
            FOREIGN KEY (user_id) REFERENCES users(id),
            FOREIGN KEY (task_id) REFERENCES tasks(id),
            UNIQUE(task_id, date)                               -- 同一任务同一天只能打一次卡
        );
    )");

    // ============ 4. 签到表 signins ============
    // 干啥的：每日签到，跟任务无关，就是"我今天学习了"的声明
    // 跟 checkins 的区别：checkins 绑定具体任务，signins 只是签到
    // UNIQUE(user_id, date) 保证同一个人同一天只能签到一次
    exec_sql(db, R"(
        CREATE TABLE IF NOT EXISTS signins (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id    INTEGER NOT NULL,
            date       TEXT    NOT NULL,                         -- 签到日期
            created_at TEXT    DEFAULT (datetime('now')),
            FOREIGN KEY (user_id) REFERENCES users(id),
            UNIQUE(user_id, date)
        );
    )");

    // ============ 5. 提醒表 reminders ============
    // 干啥的：系统自动生成的任务提醒
    // type: "due" = 到期提醒（任务快截止了），"review" = 复习提醒（学过该复习了）
    // is_read: 0=未读 1=已读，用户点过就变成 1
    exec_sql(db, R"(
        CREATE TABLE IF NOT EXISTS reminders (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id    INTEGER NOT NULL,
            task_id    INTEGER NOT NULL,
            type       TEXT    NOT NULL DEFAULT 'due',          -- 提醒类型：due 或 review
            message    TEXT    NOT NULL,                         -- 提醒内容文字
            is_read    INTEGER NOT NULL DEFAULT 0,              -- 是否已读 0未读 1已读
            created_at TEXT    DEFAULT (datetime('now')),
            FOREIGN KEY (user_id) REFERENCES users(id),
            FOREIGN KEY (task_id) REFERENCES tasks(id)
        );
    )");

    // ============ 6. 好友关系表 friendships ============
    // 干啥的：记录用户之间的好友关系
    // status: 0=待通过（发了申请但对方还没理）
    //         1=已通过（双方互为好友）
    //         2=已拒绝（对方拒绝了申请）
    // 加好友是单向申请→对方同意的流程
    exec_sql(db, R"(
        CREATE TABLE IF NOT EXISTS friendships (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            from_id    INTEGER NOT NULL,                        -- 发起好友申请的人
            to_id      INTEGER NOT NULL,                         -- 收到申请的人
            status     INTEGER NOT NULL DEFAULT 0,             -- 0待通过 1已通过 2已拒绝
            created_at TEXT    DEFAULT (datetime('now')),
            FOREIGN KEY (from_id) REFERENCES users(id),
            FOREIGN KEY (to_id) REFERENCES users(id)
        );
    )");

    // ============ 7. 聊天消息表 messages ============
    // 干啥的：存储好友之间的聊天记录
    // from_id = 谁发的，to_id = 发给谁
    // 查询时用 WHERE (from_id=A AND to_id=B) OR (from_id=B AND to_id=A)
    // 就能拿到 A 和 B 两个人的完整对话
    exec_sql(db, R"(
        CREATE TABLE IF NOT EXISTS messages (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            from_id    INTEGER NOT NULL,                        -- 发送者
            to_id      INTEGER NOT NULL,                         -- 接收者
            content    TEXT    NOT NULL,                         -- 消息内容
            is_read    INTEGER NOT NULL DEFAULT 0,             -- 0未读 1已读（对方看了就变1）
            created_at TEXT    DEFAULT (datetime('now')),
            FOREIGN KEY (from_id) REFERENCES users(id),
            FOREIGN KEY (to_id) REFERENCES users(id)
        );
    )");

    // ============ 8. 收藏表 materials ============
    // 干啥的：用户收藏的学习资料链接
    exec_sql(db, R"(
        CREATE TABLE IF NOT EXISTS materials (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id    INTEGER NOT NULL,
            title      TEXT    NOT NULL,                         -- 资料标题
            url        TEXT    NOT NULL,                          -- 资料链接
            created_at TEXT    DEFAULT (datetime('now')),
            FOREIGN KEY (user_id) REFERENCES users(id)
        );
    )");

    // ============ 9. 番茄钟表 pomodoros ============
    // 干啥的：记录每次番茄钟的使用
    // duration 单位是分钟（通常 25 分钟一个番茄钟）
    // date 记录在哪一天使用的
    exec_sql(db, R"(
        CREATE TABLE IF NOT EXISTS pomodoros (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id    INTEGER NOT NULL,
            duration   INTEGER NOT NULL,                        -- 时长（分钟）
            date       TEXT    NOT NULL,                          -- 日期 YYYY-MM-DD
            created_at TEXT    DEFAULT (datetime('now')),
            FOREIGN KEY (user_id) REFERENCES users(id)
        );
    )");

    // ============ 10. 名言表 quotes ============
    // 干啥的：存储励志名言，随机展示给用户
    // 这张表由管理员维护，普通用户只读
    exec_sql(db, R"(
        CREATE TABLE IF NOT EXISTS quotes (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            content    TEXT    NOT NULL,                         -- 名言内容
            author     TEXT    DEFAULT '佚名',                  -- 作者，不知道就写佚名
            created_at TEXT    DEFAULT (datetime('now'))
        );
    )");

    // ============ 11. 推荐任务表 recommended_tasks ============
    // 干啥的：系统预设的任务模板，管理员维护
    // 普通用户看到后可以一键添加到自己的任务里
    exec_sql(db, R"(
        CREATE TABLE IF NOT EXISTS recommended_tasks (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            title      TEXT    NOT NULL,                         -- 任务标题
            topic      TEXT    DEFAULT '',                       -- 所属主题
            priority   INTEGER DEFAULT 1,                       -- 推荐优先级
            created_at TEXT    DEFAULT (datetime('now'))
        );
    )");

    sqlite3_close(db);
    cout << "[OK] 数据库初始化完成，所有表已就绪" << endl;
}

// ============================================================
// 辅助函数
// ============================================================

// 把 "YYYY-MM-DD" 转成自公元元年以来的天数（用于计算连续打卡）
// 算法：累加整年天数 + 当年已过月天数 + 当月日数
static int date_to_days(const string& date_str) {
    int y, m, d;
    sscanf(date_str.c_str(), "%d-%d-%d", &y, &m, &d);

    // 整年天数（含闰年）
    auto leap = [](int yr) { return yr % 4 == 0 && (yr % 100 != 0 || yr % 400 == 0); };
    int days = 0;
    for (int i = 1; i < y; i++) days += leap(i) ? 366 : 365;

    // 每月天数
    static const int md[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
    for (int i = 1; i < m; i++) {
        days += md[i - 1];
        if (i == 2 && leap(y)) days++;  // 闰二月 +1
    }
    days += d;
    return days;
}

// ============================================================
// 用户模块（占位 — 待后续实现）
// ============================================================

int user_create(const string& username, const string& password) {
    // TODO: 后续实现
    return -1;
}

string user_login(const string& username, const string& password) {
    // TODO: 后续实现
    return "{}";
}

int user_id_by_token(const string& token) {
    // TODO: 后续实现
    return -1;
}

string user_get_info(int user_id) {
    // TODO: 后续实现
    return "{}";
}

string user_search(const string& keyword) {
    // TODO: 后续实现
    return "[]";
}

// ============================================================
// 任务模块
// ============================================================

// -----------------------------------------------------------
// task_create — 创建新任务，返回新任务 ID（失败返回 -1）
// -----------------------------------------------------------
int task_create(int user_id, const string& title, const string& topic,
                const string& deadline, int priority, bool need_review) {
    sqlite3* db = open_db();
    if (!db) return -1;

    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT INTO tasks (user_id, title, topic, deadline, priority, need_review) "
        "VALUES (?, ?, ?, ?, ?, ?);";

    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        cerr << "[ERROR] task_create prepare: " << sqlite3_errmsg(db) << endl;
        sqlite3_close(db);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_text(stmt, 2, title.c_str(),    -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, topic.c_str(),    -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, deadline.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, priority);
    sqlite3_bind_int(stmt, 6, need_review ? 1 : 0);

    rc = sqlite3_step(stmt);
    int new_id = -1;
    if (rc == SQLITE_DONE) {
        new_id = (int)sqlite3_last_insert_rowid(db);
    } else {
        cerr << "[ERROR] task_create step: " << sqlite3_errmsg(db) << endl;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return new_id;
}

// -----------------------------------------------------------
// task_get_list — 获取用户的所有任务，返回 JSON 数组字符串
// -----------------------------------------------------------
string task_get_list(int user_id) {
    sqlite3* db = open_db();
    if (!db) return "[]";

    json arr = json::array();
    sqlite3_stmt* stmt = nullptr;

    const char* sql =
        "SELECT id, user_id, title, topic, deadline, priority, status, need_review, created_at "
        "FROM tasks WHERE user_id = ? ORDER BY created_at DESC;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, user_id);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            json t;
            t["id"]          = sqlite3_column_int(stmt, 0);
            t["user_id"]     = sqlite3_column_int(stmt, 1);
            t["title"]       = (const char*)sqlite3_column_text(stmt, 2);
            t["topic"]       = (const char*)sqlite3_column_text(stmt, 3);
            t["deadline"]    = (const char*)sqlite3_column_text(stmt, 4);
            t["priority"]    = sqlite3_column_int(stmt, 5);
            t["status"]      = sqlite3_column_int(stmt, 6);
            t["need_review"] = sqlite3_column_int(stmt, 7);
            t["created_at"]  = (const char*)sqlite3_column_text(stmt, 8);
            arr.push_back(t);
        }
        sqlite3_finalize(stmt);
    }

    sqlite3_close(db);
    return arr.dump();
}

// -----------------------------------------------------------
// task_get_one — 获取单个任务详情，返回 JSON 对象字符串
// -----------------------------------------------------------
string task_get_one(int task_id) {
    sqlite3* db = open_db();
    if (!db) return "{}";

    json obj;
    sqlite3_stmt* stmt = nullptr;

    const char* sql =
        "SELECT id, user_id, title, topic, deadline, priority, status, need_review, created_at "
        "FROM tasks WHERE id = ?;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, task_id);

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            obj["id"]          = sqlite3_column_int(stmt, 0);
            obj["user_id"]     = sqlite3_column_int(stmt, 1);
            obj["title"]       = (const char*)sqlite3_column_text(stmt, 2);
            obj["topic"]       = (const char*)sqlite3_column_text(stmt, 3);
            obj["deadline"]    = (const char*)sqlite3_column_text(stmt, 4);
            obj["priority"]    = sqlite3_column_int(stmt, 5);
            obj["status"]      = sqlite3_column_int(stmt, 6);
            obj["need_review"] = sqlite3_column_int(stmt, 7);
            obj["created_at"]  = (const char*)sqlite3_column_text(stmt, 8);
        }
        sqlite3_finalize(stmt);
    }

    sqlite3_close(db);
    return obj.dump();
}

// -----------------------------------------------------------
// task_update — 更新任务的单个字段（白名单防注入）
// -----------------------------------------------------------
bool task_update(int task_id, int user_id,
                 const string& field, const string& value) {
    // 白名单：只允许更新这些字段
    static const set<string> allowed = {
        "title", "topic", "deadline", "priority", "status", "need_review"
    };
    if (allowed.find(field) == allowed.end()) {
        cerr << "[ERROR] task_update: 不允许的字段 '" << field << "'" << endl;
        return false;
    }

    sqlite3* db = open_db();
    if (!db) return false;

    string sql = "UPDATE tasks SET " + field + " = ? WHERE id = ? AND user_id = ?;";

    sqlite3_stmt* stmt = nullptr;
    bool ok = false;

    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        // 整数字段 bind 数值，文本字段 bind 字符串
        if (field == "priority" || field == "status" || field == "need_review") {
            sqlite3_bind_int(stmt, 1, stoi(value));
        } else {
            sqlite3_bind_text(stmt, 1, value.c_str(), -1, SQLITE_TRANSIENT);
        }
        sqlite3_bind_int(stmt, 2, task_id);
        sqlite3_bind_int(stmt, 3, user_id);

        ok = (sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(db) > 0);
        sqlite3_finalize(stmt);
    }

    sqlite3_close(db);
    return ok;
}

// -----------------------------------------------------------
// task_delete — 删除任务（需校验 user_id 防止越权）
// -----------------------------------------------------------
bool task_delete(int task_id, int user_id) {
    sqlite3* db = open_db();
    if (!db) return false;

    sqlite3_stmt* stmt = nullptr;
    bool ok = false;

    const char* sql = "DELETE FROM tasks WHERE id = ? AND user_id = ?;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, task_id);
        sqlite3_bind_int(stmt, 2, user_id);

        ok = (sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(db) > 0);
        sqlite3_finalize(stmt);
    }

    sqlite3_close(db);
    return ok;
}

// -----------------------------------------------------------
// task_recommended — 获取系统推荐任务列表，返回 JSON 数组字符串
// -----------------------------------------------------------
string task_recommended() {
    sqlite3* db = open_db();
    if (!db) return "[]";

    json arr = json::array();
    sqlite3_stmt* stmt = nullptr;

    const char* sql =
        "SELECT id, title, topic, priority FROM recommended_tasks ORDER BY priority DESC;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            json t;
            t["id"]       = sqlite3_column_int(stmt, 0);
            t["title"]    = (const char*)sqlite3_column_text(stmt, 1);
            t["topic"]    = (const char*)sqlite3_column_text(stmt, 2);
            t["priority"] = sqlite3_column_int(stmt, 3);
            arr.push_back(t);
        }
        sqlite3_finalize(stmt);
    }

    sqlite3_close(db);
    return arr.dump();
}

// ============================================================
// 打卡 & 签到
// ============================================================

// -----------------------------------------------------------
// checkin_do — 执行打卡（同一任务同一天 UNIQUE 约束防重复）
// -----------------------------------------------------------
bool checkin_do(int user_id, int task_id, const string& date) {
    sqlite3* db = open_db();
    if (!db) return false;

    sqlite3_stmt* stmt = nullptr;
    bool ok = false;

    const char* sql =
        "INSERT INTO checkins (user_id, task_id, date) VALUES (?, ?, ?);";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, user_id);
        sqlite3_bind_int(stmt, 2, task_id);
        sqlite3_bind_text(stmt, 3, date.c_str(), -1, SQLITE_TRANSIENT);

        ok = (sqlite3_step(stmt) == SQLITE_DONE);
        if (!ok) {
            cerr << "[WARN] checkin_do: 可能重复打卡 " << sqlite3_errmsg(db) << endl;
        }
        sqlite3_finalize(stmt);
    }

    sqlite3_close(db);
    return ok;
}

// -----------------------------------------------------------
// checkin_get — 按日期查询打卡记录（JOIN tasks 拿标题）
//   返回 JSON 数组，每条包含 DB 字段 + 前端友好的字段名
// -----------------------------------------------------------
string checkin_get(int user_id, const string& date) {
    sqlite3* db = open_db();
    if (!db) return "[]";

    json arr = json::array();
    sqlite3_stmt* stmt = nullptr;

    // 如果 date 为空则查该用户全部打卡记录
    string sql;
    if (date.empty()) {
        sql =
            "SELECT c.id, c.user_id, c.task_id, c.date, c.created_at, t.title "
            "FROM checkins c "
            "JOIN tasks t ON c.task_id = t.id "
            "WHERE c.user_id = ? "
            "ORDER BY c.created_at DESC;";
    } else {
        sql =
            "SELECT c.id, c.user_id, c.task_id, c.date, c.created_at, t.title "
            "FROM checkins c "
            "JOIN tasks t ON c.task_id = t.id "
            "WHERE c.user_id = ? AND c.date = ? "
            "ORDER BY c.created_at DESC;";
    }

    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, user_id);
        if (!date.empty()) {
            sqlite3_bind_text(stmt, 2, date.c_str(), -1, SQLITE_TRANSIENT);
        }

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* created_at = (const char*)sqlite3_column_text(stmt, 4);
            string checkin_time = "-";
            if (created_at) {
                // created_at 格式: "YYYY-MM-DD HH:MM:SS" → 取后 5 位 "HH:MM"
                string cat(created_at);
                if (cat.length() >= 16) {
                    checkin_time = cat.substr(11, 5);  // "HH:MM"
                }
            }

            json c;
            // 数据库原始字段
            c["id"]         = sqlite3_column_int(stmt, 0);
            c["user_id"]    = sqlite3_column_int(stmt, 1);
            c["task_id"]    = sqlite3_column_int(stmt, 2);
            c["date"]       = (const char*)sqlite3_column_text(stmt, 3);
            c["created_at"] = created_at ? created_at : "";
            c["task_title"] = (const char*)sqlite3_column_text(stmt, 5);
            // 前端兼容字段
            c["checkin_date"] = (const char*)sqlite3_column_text(stmt, 3);
            c["checkin_time"] = checkin_time;
            arr.push_back(c);
        }
        sqlite3_finalize(stmt);
    }

    sqlite3_close(db);
    return arr.dump();
}

// -----------------------------------------------------------
// checkin_get_by_task — 按任务 ID 查询打卡记录（供路由层调用）
//   这个函数补充 checkin_get 按日期查的场景，提供按 task_id 查的能力
// -----------------------------------------------------------
string checkin_get_by_task(int user_id, int task_id) {
    sqlite3* db = open_db();
    if (!db) return "[]";

    json arr = json::array();
    sqlite3_stmt* stmt = nullptr;

    const char* sql =
        "SELECT c.id, c.user_id, c.task_id, c.date, c.created_at, t.title "
        "FROM checkins c "
        "JOIN tasks t ON c.task_id = t.id "
        "WHERE c.user_id = ? AND c.task_id = ? "
        "ORDER BY c.created_at DESC;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, user_id);
        sqlite3_bind_int(stmt, 2, task_id);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* created_at = (const char*)sqlite3_column_text(stmt, 4);
            string checkin_time = "-";
            if (created_at) {
                string cat(created_at);
                if (cat.length() >= 16) checkin_time = cat.substr(11, 5);
            }

            json c;
            c["id"]            = sqlite3_column_int(stmt, 0);
            c["user_id"]       = sqlite3_column_int(stmt, 1);
            c["task_id"]       = sqlite3_column_int(stmt, 2);
            c["date"]          = (const char*)sqlite3_column_text(stmt, 3);
            c["created_at"]    = created_at ? created_at : "";
            c["task_title"]    = (const char*)sqlite3_column_text(stmt, 5);
            c["checkin_date"]  = (const char*)sqlite3_column_text(stmt, 3);
            c["checkin_time"]  = checkin_time;
            arr.push_back(c);
        }
        sqlite3_finalize(stmt);
    }

    sqlite3_close(db);
    return arr.dump();
}

// -----------------------------------------------------------
// signin_do — 每日签到（UNIQUE 约束保证一天一次）
// -----------------------------------------------------------
bool signin_do(int user_id, const string& date) {
    sqlite3* db = open_db();
    if (!db) return false;

    sqlite3_stmt* stmt = nullptr;
    bool ok = false;

    const char* sql =
        "INSERT INTO signins (user_id, date) VALUES (?, ?);";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, user_id);
        sqlite3_bind_text(stmt, 2, date.c_str(), -1, SQLITE_TRANSIENT);

        ok = (sqlite3_step(stmt) == SQLITE_DONE);
        if (!ok) {
            cerr << "[WARN] signin_do: 可能重复签到 " << sqlite3_errmsg(db) << endl;
        }
        sqlite3_finalize(stmt);
    }

    sqlite3_close(db);
    return ok;
}

// -----------------------------------------------------------
// signin_streak — 计算连续签到天数
//   从最近一次签到往前数，直到出现间断
// -----------------------------------------------------------
int signin_streak(int user_id) {
    sqlite3* db = open_db();
    if (!db) return 0;

    int streak = 0;
    sqlite3_stmt* stmt = nullptr;

    // 取所有签到日期，从最新到最旧排列
    const char* sql =
        "SELECT DISTINCT date FROM signins WHERE user_id = ? ORDER BY date DESC;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, user_id);

        int prev_days = -1;  // 上一个日期的"天数"
        bool first_row = true;

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            string date_str = (const char*)sqlite3_column_text(stmt, 0);
            int cur_days = date_to_days(date_str);

            if (first_row) {
                streak = 1;
                prev_days = cur_days;
                first_row = false;
            } else {
                // 如果当前日期正好是上一个日期的前一天，连续
                if (prev_days - cur_days == 1) {
                    streak++;
                    prev_days = cur_days;
                } else {
                    break;  // 连续中断
                }
            }
        }
        sqlite3_finalize(stmt);
    }

    sqlite3_close(db);
    return streak;
}

// -----------------------------------------------------------
// signin_history — 获取用户签到历史，返回 JSON 数组
// -----------------------------------------------------------
string signin_history(int user_id) {
    sqlite3* db = open_db();
    if (!db) return "[]";

    json arr = json::array();
    sqlite3_stmt* stmt = nullptr;

    const char* sql =
        "SELECT id, user_id, date, created_at FROM signins "
        "WHERE user_id = ? ORDER BY date DESC LIMIT 365;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, user_id);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            json s;
            s["id"]         = sqlite3_column_int(stmt, 0);
            s["user_id"]    = sqlite3_column_int(stmt, 1);
            s["date"]       = (const char*)sqlite3_column_text(stmt, 2);
            s["created_at"] = (const char*)sqlite3_column_text(stmt, 3);
            arr.push_back(s);
        }
        sqlite3_finalize(stmt);
    }

    sqlite3_close(db);
    return arr.dump();
}

// ============================================================
// 以下模块的占位实现（返回空 JSON，后续逐个实现）
// ============================================================

// ---------- 统计 ----------

// -----------------------------------------------------------
// stats_overview — 获取用户统计总览（仪表盘/分析页共用）
// -----------------------------------------------------------
string stats_overview(int user_id) {
    sqlite3* db = open_db();
    if (!db) return R"({"total_tasks":0,"completed":0,"completion_rate":0,"streak":0})";

    json result;
    result["total_tasks"] = 0;
    result["completed"] = 0;
    result["completion_rate"] = 0.0;
    result["streak"] = 0;

    sqlite3_stmt* stmt = nullptr;

    // 总任务数
    const char* sql_total = "SELECT COUNT(*) FROM tasks WHERE user_id = ?;";
    if (sqlite3_prepare_v2(db, sql_total, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, user_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            result["total_tasks"] = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    // 已完成任务数 = 至少打过一次卡的任务数
    const char* sql_done =
        "SELECT COUNT(DISTINCT task_id) FROM checkins WHERE user_id = ?;";
    if (sqlite3_prepare_v2(db, sql_done, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, user_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            result["completed"] = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    int total = result["total_tasks"].get<int>();
    int completed = result["completed"].get<int>();
    result["completion_rate"] = (total > 0) ? (double)completed / total : 0.0;

    // 连续签到天数
    result["streak"] = signin_streak(user_id);

    sqlite3_close(db);
    return result.dump();
}

// -----------------------------------------------------------
// stats_daily — 获取每日任务添加数和完成率（用于折线图/柱状图）
// -----------------------------------------------------------
string stats_daily(int user_id, const string& start_date, const string& end_date) {
    sqlite3* db = open_db();
    if (!db) return "[]";

    // 用 map 按日期合并 added 和 completed
    struct Daily { int added = 0; int completed = 0; };
    map<string, Daily> daily_map;
    sqlite3_stmt* stmt = nullptr;

    // 查询每天添加的任务数
    const char* sql_added =
        "SELECT DATE(created_at) AS d, COUNT(*) "
        "FROM tasks WHERE user_id = ? AND DATE(created_at) BETWEEN ? AND ? "
        "GROUP BY d;";

    if (sqlite3_prepare_v2(db, sql_added, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, user_id);
        sqlite3_bind_text(stmt, 2, start_date.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, end_date.c_str(),   -1, SQLITE_TRANSIENT);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            string date = (const char*)sqlite3_column_text(stmt, 0);
            daily_map[date].added = sqlite3_column_int(stmt, 1);
        }
        sqlite3_finalize(stmt);
    }

    // 查询每天完成打卡的任务数
    const char* sql_done =
        "SELECT c.date, COUNT(DISTINCT c.task_id) "
        "FROM checkins c "
        "JOIN tasks t ON c.task_id = t.id "
        "WHERE t.user_id = ? AND c.date BETWEEN ? AND ? "
        "GROUP BY c.date;";

    if (sqlite3_prepare_v2(db, sql_done, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, user_id);
        sqlite3_bind_text(stmt, 2, start_date.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, end_date.c_str(),   -1, SQLITE_TRANSIENT);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            string date = (const char*)sqlite3_column_text(stmt, 0);
            daily_map[date].completed = sqlite3_column_int(stmt, 1);
        }
        sqlite3_finalize(stmt);
    }

    // 生成 JSON 数组，按日期排序
    json arr = json::array();
    for (auto& [date, d] : daily_map) {
        // 显示格式 "MM-DD"
        string label;
        if (date.length() >= 10) {
            label = date.substr(5, 5);  // "07-08"
        } else {
            label = date;
        }

        json item;
        item["date"]      = label;
        item["added"]     = d.added;
        item["completed"] = d.completed;
        item["rate"]      = (d.added > 0) ? (double)d.completed / d.added : 0.0;
        arr.push_back(item);
    }

    sqlite3_close(db);
    return arr.dump();
}

// ---------- 提醒 ----------

// 辅助：获取今天的日期字符串 "YYYY-MM-DD"
static string today_str() {
    time_t t = time(nullptr);
    struct tm* tm = localtime(&t);
    char buf[11];
    strftime(buf, sizeof(buf), "%Y-%m-%d", tm);
    return string(buf);
}

// 辅助：获取 n 天前/后的日期字符串（负 = 过去，正 = 未来）
static string date_offset_str(int days_offset) {
    time_t t = time(nullptr) + days_offset * 86400;
    struct tm* tm = localtime(&t);
    char buf[11];
    strftime(buf, sizeof(buf), "%Y-%m-%d", tm);
    return string(buf);
}

// -----------------------------------------------------------
// reminder_list — 动态生成到期提醒 + 复习提醒
//   ID 编码规则：
//     due  : id = task_id（小数字，< 10000）
//     review: id = task_id * 1000 + days_ago（大数字，> 10000）
//   这样 mark_read 时可以根据 ID 反向解析出类型和 task_id
// -----------------------------------------------------------
string reminder_list(int user_id, const string& type) {
    sqlite3* db = open_db();
    if (!db) return "[]";

    json arr = json::array();
    sqlite3_stmt* stmt = nullptr;
    string today = today_str();

    // ═══════════════════════════════════
    // 1. 到期提醒 — 截止日期在 3 天内且未完成的任务
    // ═══════════════════════════════════
    if (type.empty() || type == "due") {
        string cutoff = date_offset_str(3);  // today + 3 days

        const char* sql_due =
            "SELECT t.id, t.title, t.deadline, t.priority "
            "FROM tasks t "
            "WHERE t.user_id = ? AND t.status = 0 "
            "  AND t.deadline != '' "
            "  AND t.deadline <= ? "
            "ORDER BY t.deadline ASC;";

        if (sqlite3_prepare_v2(db, sql_due, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, user_id);
            sqlite3_bind_text(stmt, 2, cutoff.c_str(), -1, SQLITE_TRANSIENT);

            while (sqlite3_step(stmt) == SQLITE_ROW) {
                int task_id    = sqlite3_column_int(stmt, 0);
                string title   = (const char*)sqlite3_column_text(stmt, 1);
                string deadline = (const char*)sqlite3_column_text(stmt, 2);

                // 紧急程度：今天及以前=high，明天=medium，后天及以后=low
                string urgency = "low";
                if (deadline <= today)          urgency = "high";
                else if (deadline == date_offset_str(1)) urgency = "medium";

                // 检查是否已标记为已读
                int is_read = 0;
                sqlite3_stmt* s2 = nullptr;
                const char* sql_read =
                    "SELECT COUNT(*) FROM reminders "
                    "WHERE user_id = ? AND task_id = ? AND type = 'due' AND is_read = 1;";
                if (sqlite3_prepare_v2(db, sql_read, -1, &s2, nullptr) == SQLITE_OK) {
                    sqlite3_bind_int(s2, 1, user_id);
                    sqlite3_bind_int(s2, 2, task_id);
                    if (sqlite3_step(s2) == SQLITE_ROW) {
                        is_read = sqlite3_column_int(s2, 0) > 0 ? 1 : 0;
                    }
                    sqlite3_finalize(s2);
                }

                json r;
                r["id"]          = task_id;
                r["task_id"]     = task_id;
                r["task_title"]  = title;
                r["type"]        = "due";
                r["urgency"]     = urgency;
                r["remind_date"] = deadline;
                r["is_read"]     = is_read;
                arr.push_back(r);
            }
            sqlite3_finalize(stmt);
        }
    }

    // ═══════════════════════════════════
    // 2. 复习提醒 — 打卡后第 1/2/4/7 天，且今天还没打卡的任务
    //    这是间隔重复（spaced repetition）的复习策略
    // ═══════════════════════════════════
    if (type.empty() || type == "review") {
        // 复习周期：打卡后 1 天、2 天、4 天、7 天
        vector<int> review_days = {1, 2, 4, 7};
        string today = today_str();

        for (int days_ago : review_days) {
            string checkin_date = date_offset_str(-days_ago);  // N days ago

            // 查找在 checkin_date 打过卡、且 need_review=1、且今天还没打卡的任务
            const char* sql_review =
                "SELECT c.task_id, t.title, t.priority "
                "FROM checkins c "
                "JOIN tasks t ON c.task_id = t.id "
                "WHERE t.user_id = ? "
                "  AND t.need_review = 1 "
                "  AND c.date = ? "
                "  AND NOT EXISTS ("
                "    SELECT 1 FROM checkins c2 "
                "    WHERE c2.task_id = c.task_id AND c2.date = ?"
                "  ) "
                "GROUP BY c.task_id;";

            if (sqlite3_prepare_v2(db, sql_review, -1, &stmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_int(stmt, 1, user_id);
                sqlite3_bind_text(stmt, 2, checkin_date.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 3, today.c_str(),   -1, SQLITE_TRANSIENT);

                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    int task_id  = sqlite3_column_int(stmt, 0);
                    string title = (const char*)sqlite3_column_text(stmt, 1);
                    int priority = sqlite3_column_int(stmt, 2);

                    // 紧急程度：基于原始任务优先级
                    string urgency = "low";
                    if (priority >= 3)      urgency = "high";
                    else if (priority == 2) urgency = "medium";

                    // ID: 负数编码 task_id*1000+days_ago（负数=review，正数=due）
                    int review_id = -(task_id * 1000 + days_ago);

                    // 检查是否已读（用 message 字段存 days_ago）
                    int is_read = 0;
                    sqlite3_stmt* s2 = nullptr;
                    const char* sql_read =
                        "SELECT COUNT(*) FROM reminders "
                        "WHERE user_id = ? AND task_id = ? AND type = 'review' "
                        "  AND message = ? AND is_read = 1;";
                    string ds = to_string(days_ago);
                    if (sqlite3_prepare_v2(db, sql_read, -1, &s2, nullptr) == SQLITE_OK) {
                        sqlite3_bind_int(s2, 1, user_id);
                        sqlite3_bind_int(s2, 2, task_id);
                        sqlite3_bind_text(s2, 3, ds.c_str(), -1, SQLITE_TRANSIENT);
                        if (sqlite3_step(s2) == SQLITE_ROW) {
                            is_read = sqlite3_column_int(s2, 0) > 0 ? 1 : 0;
                        }
                        sqlite3_finalize(s2);
                    }

                    json r;
                    r["id"]          = review_id;
                    r["task_id"]     = task_id;
                    r["task_title"]  = title;
                    r["type"]        = "review";
                    r["urgency"]     = urgency;
                    r["remind_date"] = today;
                    r["is_read"]     = is_read;
                    r["days_ago"]    = days_ago;
                    arr.push_back(r);
                }
                sqlite3_finalize(stmt);
            }
        }
    }

    sqlite3_close(db);
    return arr.dump();
}

// -----------------------------------------------------------
// reminder_mark_read — 标记提醒为已读
//   ID 编码：>0 = due（id=task_id），<0 = review（id = -(task_id*1000+days_ago)）
// -----------------------------------------------------------
bool reminder_mark_read(int reminder_id, int user_id) {
    sqlite3* db = open_db();
    if (!db) return false;

    string rem_type;
    int task_id = 0;
    string message;

    if (reminder_id < 0) {
        // 复习提醒：负数解码
        rem_type = "review";
        int abs_id = -reminder_id;
        task_id = abs_id / 1000;
        int days_ago = abs_id % 1000;
        message = to_string(days_ago);
    } else {
        // 到期提醒
        rem_type = "due";
        task_id = reminder_id;
    }

    // UPDATE or INSERT
    const char* sql_upd =
        "UPDATE reminders SET is_read = 1 "
        "WHERE user_id = ? AND task_id = ? AND type = ? AND message = ?;";

    sqlite3_stmt* stmt = nullptr;
    bool ok = false;

    if (sqlite3_prepare_v2(db, sql_upd, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, user_id);
        sqlite3_bind_int(stmt, 2, task_id);
        sqlite3_bind_text(stmt, 3, rem_type.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, message.c_str(),  -1, SQLITE_TRANSIENT);

        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (rc == SQLITE_DONE && sqlite3_changes(db) > 0) {
            ok = true;
        }
    }

    if (!ok) {
        const char* sql_ins =
            "INSERT INTO reminders (user_id, task_id, type, message, is_read) "
            "VALUES (?, ?, ?, ?, 1);";

        if (sqlite3_prepare_v2(db, sql_ins, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, user_id);
            sqlite3_bind_int(stmt, 2, task_id);
            sqlite3_bind_text(stmt, 3, rem_type.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 4, message.c_str(),  -1, SQLITE_TRANSIENT);

            ok = (sqlite3_step(stmt) == SQLITE_DONE);
            sqlite3_finalize(stmt);
        }
    }

    sqlite3_close(db);
    return ok;
}

// ---------- 好友 ----------
bool friend_request(int from_id, int to_id) {
    return false;
}

string friend_list(int user_id) {
    return "[]";
}

bool friend_handle(int friendship_id, int status) {
    return false;
}

bool friend_delete(int friendship_id, int user_id) {
    return false;
}

// ---------- 聊天 ----------
bool message_send(int from_id, int to_id, const string& content) {
    return false;
}

string message_history(int user_id, int friend_id) {
    return "[]";
}

int message_unread_count(int user_id) {
    return 0;
}

// ---------- 收藏 ----------
string material_list(int user_id) {
    return "[]";
}

bool material_add(int user_id, const string& title, const string& url) {
    return false;
}

bool material_delete(int material_id, int user_id) {
    return false;
}

// ---------- 番茄钟 ----------
bool pomodoro_record(int user_id, int duration) {
    return false;
}

string pomodoro_today(int user_id) {
    return "[]";
}

// ---------- 名言 ----------
string quote_random() {
    return "{}";
}
