#include "db.h"
#include <iostream>
#include <set>
#include <map>
#include <ctime>
#include <direct.h>
#include "sqlite3.h"
#include "json.hpp"
#include "sha256.h"

using json = nlohmann::json;
using namespace std;

// ============================================================
//  db.cpp — 数据库层实现
//  所有跟 SQLite 交互的代码都在这一个文件里
// ============================================================

// ---------- 1. open_db ----------

sqlite3* open_db() {
    // 确保 data 目录存在（合并后可能丢失）
    _mkdir("data");
    sqlite3* db = nullptr;
    int rc = sqlite3_open("data/study.db", &db);
    if (rc != SQLITE_OK) {
        cerr << "[ERROR] 无法打开数据库: " << sqlite3_errmsg(db) << endl;
        return nullptr;
    }
    sqlite3_busy_timeout(db, 5000);               // 锁等待最多 5s
    sqlite3_exec(db, "PRAGMA journal_mode=WAL;", 0, 0, 0);  // WAL 读写不互斥
    return db;
}

// ---------- 2. exec_sql ----------

static bool exec_sql(sqlite3* db, const string& sql) {
    char* err_msg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        cerr << "[ERROR] SQL 执行出错: " << err_msg << endl;
        sqlite3_free(err_msg);
        return false;
    }
    return true;
}

// ---------- 3. init_tables ----------

void init_tables() {
    sqlite3* db = open_db();
    if (!db) return;

    // 1. 用户表
    exec_sql(db, R"(
        CREATE TABLE IF NOT EXISTS users (
            id            INTEGER PRIMARY KEY AUTOINCREMENT,
            username      TEXT    NOT NULL UNIQUE,
            password_hash TEXT    NOT NULL,
            nickname      TEXT    DEFAULT '',
            signature     TEXT    DEFAULT '',
            token         TEXT    DEFAULT '',
            role          TEXT    DEFAULT 'user',
            created_at    TEXT    DEFAULT (datetime('now','localtime'))
        );
    )");

    // 兼容旧数据库：如果 role 列不存在则添加
    exec_sql(db, "ALTER TABLE users ADD COLUMN role TEXT DEFAULT 'user';");

    // 2. 任务表
    exec_sql(db, R"(
        CREATE TABLE IF NOT EXISTS tasks (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id     INTEGER NOT NULL,
            title       TEXT    NOT NULL,
            topic       TEXT    DEFAULT '',
            deadline    TEXT    DEFAULT '',
            priority    INTEGER DEFAULT 1,
            status      INTEGER DEFAULT 0,
            need_review INTEGER DEFAULT 0,
            created_at  TEXT    DEFAULT (datetime('now','localtime')),
            FOREIGN KEY (user_id) REFERENCES users(id)
        );
    )");

    // 3. 打卡表
    exec_sql(db, R"(
        CREATE TABLE IF NOT EXISTS checkins (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id    INTEGER NOT NULL,
            task_id    INTEGER NOT NULL,
            date       TEXT    NOT NULL,
            created_at TEXT    DEFAULT (datetime('now','localtime')),
            FOREIGN KEY (user_id) REFERENCES users(id),
            FOREIGN KEY (task_id) REFERENCES tasks(id),
            UNIQUE(task_id, date)
        );
    )");

    // 4. 签到表
    exec_sql(db, R"(
        CREATE TABLE IF NOT EXISTS signins (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id    INTEGER NOT NULL,
            date       TEXT    NOT NULL,
            created_at TEXT    DEFAULT (datetime('now','localtime')),
            FOREIGN KEY (user_id) REFERENCES users(id),
            UNIQUE(user_id, date)
        );
    )");

    // 5. 提醒表
    exec_sql(db, R"(
        CREATE TABLE IF NOT EXISTS reminders (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id    INTEGER NOT NULL,
            task_id    INTEGER NOT NULL,
            type       TEXT    NOT NULL DEFAULT 'due',
            message    TEXT    NOT NULL,
            is_read    INTEGER NOT NULL DEFAULT 0,
            created_at TEXT    DEFAULT (datetime('now','localtime')),
            FOREIGN KEY (user_id) REFERENCES users(id),
            FOREIGN KEY (task_id) REFERENCES tasks(id)
        );
    )");

    // 6. 好友关系表
    exec_sql(db, R"(
        CREATE TABLE IF NOT EXISTS friendships (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            from_id    INTEGER NOT NULL,
            to_id      INTEGER NOT NULL,
            status     INTEGER NOT NULL DEFAULT 0,
            created_at TEXT    DEFAULT (datetime('now','localtime')),
            FOREIGN KEY (from_id) REFERENCES users(id),
            FOREIGN KEY (to_id) REFERENCES users(id)
        );
    )");

    // 7. 聊天消息表
    exec_sql(db, R"(
        CREATE TABLE IF NOT EXISTS messages (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            from_id    INTEGER NOT NULL,
            to_id      INTEGER NOT NULL,
            content    TEXT    NOT NULL,
            is_read    INTEGER NOT NULL DEFAULT 0,
            created_at TEXT    DEFAULT (datetime('now','localtime')),
            FOREIGN KEY (from_id) REFERENCES users(id),
            FOREIGN KEY (to_id) REFERENCES users(id)
        );
    )");

    // 8. 收藏表
    exec_sql(db, R"(
        CREATE TABLE IF NOT EXISTS materials (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id    INTEGER NOT NULL,
            title      TEXT    NOT NULL,
            url        TEXT    NOT NULL,
            created_at TEXT    DEFAULT (datetime('now','localtime')),
            FOREIGN KEY (user_id) REFERENCES users(id)
        );
    )");

    // 9. 番茄钟表
    exec_sql(db, R"(
        CREATE TABLE IF NOT EXISTS pomodoros (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id    INTEGER NOT NULL,
            duration   INTEGER NOT NULL,
            date       TEXT    NOT NULL,
            created_at TEXT    DEFAULT (datetime('now','localtime')),
            FOREIGN KEY (user_id) REFERENCES users(id)
        );
    )");

    // 11. 名言表（含种子数据）
    exec_sql(db, R"(
        CREATE TABLE IF NOT EXISTS quotes (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            content    TEXT    NOT NULL,
            author     TEXT    DEFAULT '佚名',
            created_at TEXT    DEFAULT (datetime('now','localtime'))
        );
    )");

    // 插入种子名言（IGNORE 保证重复运行不会重复插入）
    exec_sql(db, "INSERT OR IGNORE INTO quotes(id,content,author) VALUES(1,'学习使人进步','佚名');");
    exec_sql(db, "INSERT OR IGNORE INTO quotes(id,content,author) VALUES(2,'千里之行，始于足下','老子');");
    exec_sql(db, "INSERT OR IGNORE INTO quotes(id,content,author) VALUES(3,'学而不思则罔，思而不学则殆','孔子');");
    exec_sql(db, "INSERT OR IGNORE INTO quotes(id,content,author) VALUES(4,'天道酬勤','《周易》');");
    exec_sql(db, "INSERT OR IGNORE INTO quotes(id,content,author) VALUES(5,'书山有路勤为径，学海无涯苦作舟','韩愈');");
    exec_sql(db, "INSERT OR IGNORE INTO quotes(id,content,author) VALUES(6,'不积跬步，无以至千里','荀子');");
    exec_sql(db, "INSERT OR IGNORE INTO quotes(id,content,author) VALUES(7,'业精于勤，荒于嬉','韩愈');");
    exec_sql(db, "INSERT OR IGNORE INTO quotes(id,content,author) VALUES(8,'少壮不努力，老大徒伤悲','《乐府诗集》');");
    exec_sql(db, "INSERT OR IGNORE INTO quotes(id,content,author) VALUES(9,'有志者，事竟成','《后汉书》');");
    exec_sql(db, "INSERT OR IGNORE INTO quotes(id,content,author) VALUES(10,'知识就是力量','培根');");
    exec_sql(db, "INSERT OR IGNORE INTO quotes(id,content,author) VALUES(11,'温故而知新，可以为师矣','孔子');");
    exec_sql(db, "INSERT OR IGNORE INTO quotes(id,content,author) VALUES(12,'锲而不舍，金石可镂','荀子');");

    // 12. 推荐任务表（含种子数据）
    exec_sql(db, R"(
        CREATE TABLE IF NOT EXISTS recommended_tasks (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            title      TEXT    NOT NULL,
            topic      TEXT    DEFAULT '',
            priority   INTEGER DEFAULT 1,
            created_at TEXT    DEFAULT (datetime('now','localtime'))
        );
    )");

    exec_sql(db, "INSERT OR IGNORE INTO recommended_tasks(id,title,topic,priority) VALUES(1,'背30个英语单词','英语',2);");
    exec_sql(db, "INSERT OR IGNORE INTO recommended_tasks(id,title,topic,priority) VALUES(2,'复习当天课程笔记','学习方法',1);");
    exec_sql(db, "INSERT OR IGNORE INTO recommended_tasks(id,title,topic,priority) VALUES(3,'阅读课外书30分钟','阅读',3);");
    exec_sql(db, "INSERT OR IGNORE INTO recommended_tasks(id,title,topic,priority) VALUES(4,'完成LeetCode每日一题','编程',2);");
    exec_sql(db, "INSERT OR IGNORE INTO recommended_tasks(id,title,topic,priority) VALUES(5,'整理课程思维导图','学习方法',2);");
    exec_sql(db, "INSERT OR IGNORE INTO recommended_tasks(id,title,topic,priority) VALUES(6,'练习听力20分钟','英语',2);");
    exec_sql(db, "INSERT OR IGNORE INTO recommended_tasks(id,title,topic,priority) VALUES(7,'写学习日记','学习方法',3);");
    exec_sql(db, "INSERT OR IGNORE INTO recommended_tasks(id,title,topic,priority) VALUES(8,'运动30分钟','健康',3);");

    // 13. 收藏任务表（任务模板快速复用）
    exec_sql(db, R"(
        CREATE TABLE IF NOT EXISTS favorite_tasks (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id    INTEGER NOT NULL,
            title      TEXT    NOT NULL,
            topic      TEXT    DEFAULT '',
            deadline   TEXT    DEFAULT '',
            priority   INTEGER DEFAULT 1,
            need_review INTEGER DEFAULT 0,
            created_at TEXT    DEFAULT (datetime('now','localtime')),
            FOREIGN KEY (user_id) REFERENCES users(id)
        );
    )");

    sqlite3_close(db);
    cout << "[OK] Database initialized, all tables ready" << endl;

    // 播种测试用户
    sqlite3* db2 = open_db();
    if (db2) {
        const char* sql = "INSERT OR IGNORE INTO users (username, password_hash, nickname) VALUES (?, ?, ?)";
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db2, sql, -1, &stmt, 0);

        auto seed_user = [&](const char* user, const char* pass, const char* nick) {
            string hash = sha256(pass);
            sqlite3_reset(stmt);
            sqlite3_bind_text(stmt, 1, user, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, hash.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, nick, -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt);
        };

        seed_user("test",  "123456",   "测试用户");
        seed_user("demo",  "123456",   "演示账号");
        seed_user("admin", "admin123", "管理员");

        // 确保管理员拥有 admin 角色
        exec_sql(db2, "UPDATE users SET role='admin' WHERE username='admin';");

        sqlite3_finalize(stmt);
        sqlite3_close(db2);
        cout << "[OK] Test users seeded" << endl;
    }
}

// ============================================================
// 用户模块
// ============================================================

// 注册：密码 SHA256 哈希后存入，返回新用户 id，重名返回 -1
int user_create(const string& username, const string& password, const string& nickname) {
    sqlite3* db = open_db();
    if (!db) return -1;

    string hash = sha256(password);

    const char* sql = "INSERT INTO users (username, password_hash, nickname) VALUES (?, ?, ?)";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, hash.c_str(),     -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, nickname.c_str(), -1, SQLITE_TRANSIENT);

    int ok = sqlite3_step(stmt);
    int new_id = (ok == SQLITE_DONE) ? (int)sqlite3_last_insert_rowid(db) : -1;

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return new_id;
}

// ============================================================
// 辅助函数
// ============================================================

// YYYY-MM-DD → 自公元以来的天数（用于连续签到计算）
static int date_to_days(const string& date_str) {
    int y, m, d;
    sscanf(date_str.c_str(), "%d-%d-%d", &y, &m, &d);
    auto leap = [](int yr) { return yr % 4 == 0 && (yr % 100 != 0 || yr % 400 == 0); };
    int days = 0;
    for (int i = 1; i < y; i++) days += leap(i) ? 366 : 365;
    static const int md[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
    for (int i = 1; i < m; i++) {
        days += md[i - 1];
        if (i == 2 && leap(y)) days++;
    }
    return days + d;
}

// 今天的日期字符串 "YYYY-MM-DD"
static string today_str() {
    time_t t = time(nullptr);
    struct tm* tm = localtime(&t);
    char buf[11];
    strftime(buf, sizeof(buf), "%Y-%m-%d", tm);
    return string(buf);
}

// 当前日期时间 "YYYY-MM-DD HH:MM:SS"（北京时间）
static string now_local() {
    time_t t = time(nullptr);
    struct tm* tm = localtime(&t);
    char buf[20];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm);
    return string(buf);
}

// n 天后（负=过去）的日期字符串
static string date_offset_str(int days_offset) {
    time_t t = time(nullptr) + days_offset * 86400;
    struct tm* tm = localtime(&t);
    char buf[11];
    strftime(buf, sizeof(buf), "%Y-%m-%d", tm);
    return string(buf);
}

// ============================================================
// ============================================================
// 4. 用户登录
// ============================================================
// 验证用户名密码，成功返回 token JSON，失败返回 "{}"
string user_login(const string& username, const string& password) {
    sqlite3* db = open_db();
    if (!db) return "{}";

    string hash = sha256(password);

    // 先查密码哈希是否匹配
    const char* sql = "SELECT id, password_hash FROM users WHERE username=?";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);

    string result = "{}";
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        int uid = sqlite3_column_int(stmt, 0);
        const char* db_hash = (const char*)sqlite3_column_text(stmt, 1);

        if (hash == string(db_hash)) {  // 哈希匹配，登录成功
            // 生成 token：用户名+时间戳 的 SHA256
            string token = sha256(username + to_string(time(nullptr)));

            sqlite3_finalize(stmt);  // 先释放 SELECT 的 stmt
            stmt = nullptr;

            // 把 token 存进数据库
            const char* upd = "UPDATE users SET token=? WHERE id=?";
            sqlite3_prepare_v2(db, upd, -1, &stmt, 0);
            sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 2, uid);
            sqlite3_step(stmt);

            result = "{\"token\":\"" + token + "\",\"user_id\":" + to_string(uid) + "}";
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return result;
}
// 通过 token 查用户 id，用于中间件验证，失败返回 -1
int user_id_by_token(const string& token) {
    sqlite3* db = open_db();
    if (!db || token.empty()) return -1;

    const char* sql = "SELECT id FROM users WHERE token=?";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_TRANSIENT);

    int uid = (sqlite3_step(stmt) == SQLITE_ROW) ? sqlite3_column_int(stmt, 0) : -1;

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return uid;
}

// 查用户信息，返回 JSON（不含密码和 token）
string user_get_info(int user_id) {
    sqlite3* db = open_db();
    if (!db) return "{}";

    const char* sql = "SELECT id, username, nickname, signature, role, created_at FROM users WHERE id=?";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, user_id);

    string result = "{}";
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = "{";
        result += "\"id\":"        + to_string(sqlite3_column_int(stmt, 0)) + ",";
        result += "\"username\":\"" + string((const char*)sqlite3_column_text(stmt, 1)) + "\",";
        result += "\"nickname\":\"" + string((const char*)sqlite3_column_text(stmt, 2)) + "\",";
        result += "\"signature\":\"" + string((const char*)sqlite3_column_text(stmt, 3)) + "\",";
        result += "\"role\":\""      + string((const char*)sqlite3_column_text(stmt, 4)) + "\",";
        result += "\"created_at\":\"" + string((const char*)sqlite3_column_text(stmt, 5)) + "\"";
        result += "}";
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return result;
}

// 模糊搜索用户
string user_search(const string& keyword) {
    sqlite3* db = open_db();
    if (!db) return "[]";

    string like = "%" + keyword + "%";
    const char* sql = "SELECT id, username FROM users WHERE username LIKE ? LIMIT 20";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, like.c_str(), -1, SQLITE_TRANSIENT);

    string result = "[";
    bool first = true;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (!first) result += ",";
        first = false;
        result += "{\"id\":"     + to_string(sqlite3_column_int(stmt, 0)) + ",";
        result += "\"username\":\"" + string((const char*)sqlite3_column_text(stmt, 1)) + "\"}";
    }
    result += "]";

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return result;
}

// 更新个人资料（昵称 + 签名）
bool user_update_profile(int user_id, const string& nickname, const string& signature) {
    sqlite3* db = open_db();
    if (!db) return false;
    const char* sql = "UPDATE users SET nickname=?, signature=? WHERE id=?";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, nickname.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, signature.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt,  3, user_id);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return ok;
}

// 修改密码：先验证旧密码，再更新
bool user_change_password(int user_id, const string& old_pwd, const string& new_pwd) {
    sqlite3* db = open_db();
    if (!db) return false;

    // 验证旧密码
    string old_hash = sha256(old_pwd);
    const char* check = "SELECT id FROM users WHERE id=? AND password_hash=?";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db, check, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_text(stmt, 2, old_hash.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return false;  // 旧密码不对
    }
    sqlite3_finalize(stmt);

    // 更新为新密码
    string new_hash = sha256(new_pwd);
    const char* upd = "UPDATE users SET password_hash=?, token='' WHERE id=?";
    sqlite3_prepare_v2(db, upd, -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, new_hash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, user_id);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return ok;
}

// ============================================================
// 任务模块（使用 Task struct 传参）
// ============================================================

int task_create(int user_id, const Task& t) {
    sqlite3* db = open_db();
    if (!db) return -1;

    sqlite3_stmt* stmt = nullptr;
    const char* sql = R"(
        INSERT INTO tasks (user_id, title, topic, deadline, priority, need_review, created_at)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    )";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
        cerr << "[ERROR] task_create prepare: " << sqlite3_errmsg(db) << endl;
        sqlite3_close(db);
        return -1;
    }

    string now = now_local();
    sqlite3_bind_int(stmt,  1, user_id);
    sqlite3_bind_text(stmt, 2, t.title.c_str(),    -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, t.topic.c_str(),    -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, t.deadline.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt,  5, t.priority);
    sqlite3_bind_int(stmt,  6, t.need_review ? 1 : 0);
    sqlite3_bind_text(stmt, 7, now.c_str(), -1, SQLITE_TRANSIENT);

    int ok = sqlite3_step(stmt);
    if (ok != SQLITE_DONE) {
        cerr << "[ERROR] task_create step: " << sqlite3_errmsg(db) << endl;
    }
    int new_id = (ok == SQLITE_DONE) ? (int)sqlite3_last_insert_rowid(db) : -1;

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return new_id;
}

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

bool task_update(int task_id, int user_id, const Task& t) {
    sqlite3* db = open_db();
    if (!db) return false;

    // 如果 status=-1，表示前端没传，保留数据库原值
    int st = t.status;
    if (st == -1) {
        const char* q = "SELECT status FROM tasks WHERE id=?";
        sqlite3_stmt* s = nullptr;
        if (sqlite3_prepare_v2(db, q, -1, &s, 0) == SQLITE_OK) {
            sqlite3_bind_int(s, 1, task_id);
            if (sqlite3_step(s) == SQLITE_ROW) st = sqlite3_column_int(s, 0);
            else st = 0;
            sqlite3_finalize(s);
        }
    }

    const char* sql = R"(
        UPDATE tasks SET title=?, topic=?, deadline=?, priority=?, status=?, need_review=?
        WHERE id=? AND user_id=?
    )";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);

    sqlite3_bind_text(stmt, 1, t.title.c_str(),    -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, t.topic.c_str(),    -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, t.deadline.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt,  4, t.priority);
    sqlite3_bind_int(stmt,  5, st);  // 用保留后的状态
    sqlite3_bind_int(stmt,  6, t.need_review ? 1 : 0);
    sqlite3_bind_int(stmt,  7, task_id);
    sqlite3_bind_int(stmt,  8, user_id);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(db) > 0);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return ok;
}

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

// 额外函数：按 task_id 查打卡记录（供 main.cpp 调用）
string checkin_get_by_task(int user_id, int task_id) {
    sqlite3* db = open_db();
    if (!db) return "[]";

    json arr = json::array();
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT c.id, c.user_id, c.task_id, c.date, c.created_at, t.title "
        "FROM checkins c JOIN tasks t ON c.task_id = t.id "
        "WHERE c.user_id = ? AND c.task_id = ? ORDER BY c.created_at DESC;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, user_id);
        sqlite3_bind_int(stmt, 2, task_id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* ca = (const char*)sqlite3_column_text(stmt, 4);
            string ct = "-";
            if (ca) { string s(ca); if (s.length() >= 16) ct = s.substr(11, 5); }
            json c;
            c["id"] = sqlite3_column_int(stmt, 0);
            c["user_id"] = sqlite3_column_int(stmt, 1);
            c["task_id"] = sqlite3_column_int(stmt, 2);
            c["date"] = (const char*)sqlite3_column_text(stmt, 3);
            c["created_at"] = ca ? ca : "";
            c["task_title"] = (const char*)sqlite3_column_text(stmt, 5);
            c["checkin_date"] = (const char*)sqlite3_column_text(stmt, 3);
            c["checkin_time"] = ct;
            arr.push_back(c);
        }
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return arr.dump();
}

bool checkin_do(int user_id, int task_id, const string& date) {
    sqlite3* db = open_db();
    if (!db) return false;

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT INTO checkins (user_id, task_id, date, created_at) VALUES (?, ?, ?, ?);";
    bool ok = false;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        string now = now_local();
        sqlite3_bind_int(stmt, 1, user_id);
        sqlite3_bind_int(stmt, 2, task_id);
        sqlite3_bind_text(stmt, 3, date.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, now.c_str(), -1, SQLITE_TRANSIENT);
        ok = (sqlite3_step(stmt) == SQLITE_DONE);
        if (!ok) cerr << "[WARN] checkin_do: 可能重复打卡 " << sqlite3_errmsg(db) << endl;
        sqlite3_finalize(stmt);
    }

    // 打卡成功 → 标记任务为已完成
    if (ok) {
        const char* upd = "UPDATE tasks SET status=1 WHERE id=? AND user_id=?";
        sqlite3_stmt* stmt2 = nullptr;
        if (sqlite3_prepare_v2(db, upd, -1, &stmt2, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt2, 1, task_id);
            sqlite3_bind_int(stmt2, 2, user_id);
            sqlite3_step(stmt2);
            sqlite3_finalize(stmt2);
        }
    }

    sqlite3_close(db);
    return ok;
}

string checkin_get(int user_id, const string& date) {
    sqlite3* db = open_db();
    if (!db) return "[]";

    json arr = json::array();
    sqlite3_stmt* stmt = nullptr;
    string sql;
    if (date.empty())
        sql = "SELECT c.id, c.user_id, c.task_id, c.date, c.created_at, t.title "
              "FROM checkins c JOIN tasks t ON c.task_id = t.id "
              "WHERE c.user_id = ? ORDER BY c.created_at DESC;";
    else
        sql = "SELECT c.id, c.user_id, c.task_id, c.date, c.created_at, t.title "
              "FROM checkins c JOIN tasks t ON c.task_id = t.id "
              "WHERE c.user_id = ? AND c.date = ? ORDER BY c.created_at DESC;";

    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, user_id);
        if (!date.empty())
            sqlite3_bind_text(stmt, 2, date.c_str(), -1, SQLITE_TRANSIENT);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* ca = (const char*)sqlite3_column_text(stmt, 4);
            string ct = "-";
            if (ca) { string s(ca); if (s.length() >= 16) ct = s.substr(11, 5); }
            json c;
            c["id"] = sqlite3_column_int(stmt, 0);
            c["user_id"] = sqlite3_column_int(stmt, 1);
            c["task_id"] = sqlite3_column_int(stmt, 2);
            c["date"] = (const char*)sqlite3_column_text(stmt, 3);
            c["created_at"] = ca ? ca : "";
            c["task_title"] = (const char*)sqlite3_column_text(stmt, 5);
            c["checkin_date"] = (const char*)sqlite3_column_text(stmt, 3);
            c["checkin_time"] = ct;
            arr.push_back(c);
        }
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return arr.dump();
}

bool signin_do(int user_id, const string& date) {
    sqlite3* db = open_db();
    if (!db) return false;

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT INTO signins (user_id, date, created_at) VALUES (?, ?, ?);";
    bool ok = false;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        string now = now_local();
        sqlite3_bind_int(stmt, 1, user_id);
        sqlite3_bind_text(stmt, 2, date.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, now.c_str(), -1, SQLITE_TRANSIENT);
        ok = (sqlite3_step(stmt) == SQLITE_DONE);
        if (!ok) cerr << "[WARN] signin_do: 可能重复签到 " << sqlite3_errmsg(db) << endl;
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return ok;
}

int signin_streak(int user_id) {
    sqlite3* db = open_db();
    if (!db) return 0;

    int streak = 0;
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT DISTINCT date FROM signins WHERE user_id = ? ORDER BY date DESC;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, user_id);
        int prev_days = -1;
        bool first = true;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int cur = date_to_days((const char*)sqlite3_column_text(stmt, 0));
            if (first) { streak = 1; prev_days = cur; first = false; }
            else if (prev_days - cur == 1) { streak++; prev_days = cur; }
            else break;
        }
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return streak;
}

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
// 统计
// ============================================================

string stats_overview(int user_id) {
    sqlite3* db = open_db();
    if (!db) return R"({"total_tasks":0,"completed":0,"completion_rate":0,"streak":0})";

    json result;
    result["total_tasks"] = 0;
    result["completed"] = 0;
    result["completion_rate"] = 0.0;
    result["streak"] = 0;

    sqlite3_stmt* stmt = nullptr;

    const char* sql_total = "SELECT COUNT(*) FROM tasks WHERE user_id = ?;";
    if (sqlite3_prepare_v2(db, sql_total, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, user_id);
        if (sqlite3_step(stmt) == SQLITE_ROW)
            result["total_tasks"] = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }

    const char* sql_done =
        "SELECT COUNT(*) FROM tasks WHERE user_id = ? AND status = 1;";
    if (sqlite3_prepare_v2(db, sql_done, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, user_id);
        if (sqlite3_step(stmt) == SQLITE_ROW)
            result["completed"] = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }

    int total = result["total_tasks"].get<int>();
    int comp  = result["completed"].get<int>();
    result["completion_rate"] = (total > 0) ? (double)comp / total : 0.0;
    result["streak"] = signin_streak(user_id);

    sqlite3_close(db);
    return result.dump();
}

string stats_daily(int user_id, const string& start_date, const string& end_date) {
    sqlite3* db = open_db();
    if (!db) return "[]";

    map<string, pair<int,int>> daily;  // date → <added, completed>
    sqlite3_stmt* stmt = nullptr;

    const char* sql_added =
        "SELECT DATE(created_at) AS d, COUNT(*) FROM tasks "
        "WHERE user_id = ? AND DATE(created_at) BETWEEN ? AND ? GROUP BY d;";
    if (sqlite3_prepare_v2(db, sql_added, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, user_id);
        sqlite3_bind_text(stmt, 2, start_date.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, end_date.c_str(),   -1, SQLITE_TRANSIENT);
        while (sqlite3_step(stmt) == SQLITE_ROW)
            daily[(const char*)sqlite3_column_text(stmt, 0)].first = sqlite3_column_int(stmt, 1);
        sqlite3_finalize(stmt);
    }

    const char* sql_done =
        "SELECT c.date, COUNT(DISTINCT c.task_id) FROM checkins c "
        "JOIN tasks t ON c.task_id = t.id "
        "WHERE t.user_id = ? AND c.date BETWEEN ? AND ? GROUP BY c.date;";
    if (sqlite3_prepare_v2(db, sql_done, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, user_id);
        sqlite3_bind_text(stmt, 2, start_date.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, end_date.c_str(),   -1, SQLITE_TRANSIENT);
        while (sqlite3_step(stmt) == SQLITE_ROW)
            daily[(const char*)sqlite3_column_text(stmt, 0)].second = sqlite3_column_int(stmt, 1);
        sqlite3_finalize(stmt);
    }

    json arr = json::array();
    for (auto& kv : daily) {
        string label = kv.first.length() >= 10 ? kv.first.substr(5, 5) : kv.first;
        int a = kv.second.first, c = kv.second.second;
        arr.push_back({
            {"date", label},
            {"added", a},
            {"completed", c},
            {"rate", (a > 0) ? (double)c / a : 0.0}
        });
    }
    sqlite3_close(db);
    return arr.dump();
}

// ============================================================
// 提醒
// ============================================================

string reminder_list(int user_id, const string& type) {
    sqlite3* db = open_db();
    if (!db) return "[]";

    json arr = json::array();
    sqlite3_stmt* stmt = nullptr;
    string today = today_str();

    // 1. 到期提醒 — 3 天内未完成任务
    if (type.empty() || type == "due") {
        string cutoff = date_offset_str(3);
        const char* sql_due =
            "SELECT t.id, t.title, t.deadline, t.priority FROM tasks t "
            "WHERE t.user_id = ? AND t.status = 0 AND t.deadline != '' AND t.deadline <= ? "
            "ORDER BY t.deadline ASC;";
        if (sqlite3_prepare_v2(db, sql_due, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, user_id);
            sqlite3_bind_text(stmt, 2, cutoff.c_str(), -1, SQLITE_TRANSIENT);
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                int tid = sqlite3_column_int(stmt, 0);
                string dl = (const char*)sqlite3_column_text(stmt, 2);
                // 截断 datetime-local 格式中的时间部分，只保留日期
                if (dl.length() > 10) dl = dl.substr(0, 10);
                string urg = "low";
                if (dl <= today) urg = "high";
                else if (dl == date_offset_str(1)) urg = "medium";

                // 查是否已读
                int is_read = 0;
                sqlite3_stmt* s2 = nullptr;
                if (sqlite3_prepare_v2(db,
                        "SELECT COUNT(*) FROM reminders "
                        "WHERE user_id=? AND task_id=? AND type='due' AND is_read=1;",
                        -1, &s2, nullptr) == SQLITE_OK) {
                    sqlite3_bind_int(s2, 1, user_id);
                    sqlite3_bind_int(s2, 2, tid);
                    if (sqlite3_step(s2) == SQLITE_ROW)
                        is_read = sqlite3_column_int(s2, 0) > 0 ? 1 : 0;
                    sqlite3_finalize(s2);
                }
                arr.push_back({
                    {"id", tid}, {"task_id", tid},
                    {"task_title", (const char*)sqlite3_column_text(stmt, 1)},
                    {"type", "due"}, {"urgency", urg},
                    {"remind_date", dl}, {"is_read", is_read}
                });
            }
            sqlite3_finalize(stmt);
        }
    }

    // 2. 复习提醒 — 打卡后 1/2/4/7 天 + need_review=1
    if (type.empty() || type == "review") {
        vector<int> review_days = {1, 2, 4, 7};
        for (int days_ago : review_days) {
            string cd = date_offset_str(-days_ago);
            const char* sql_rev =
                "SELECT c.task_id, t.title, t.priority FROM checkins c "
                "JOIN tasks t ON c.task_id = t.id "
                "WHERE t.user_id = ? AND t.need_review = 1 AND c.date = ? "
                "  AND NOT EXISTS (SELECT 1 FROM checkins c2 "
                "    WHERE c2.task_id = c.task_id AND c2.date = ?) "
                "GROUP BY c.task_id;";
            if (sqlite3_prepare_v2(db, sql_rev, -1, &stmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_int(stmt, 1, user_id);
                sqlite3_bind_text(stmt, 2, cd.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 3, today.c_str(), -1, SQLITE_TRANSIENT);
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    int tid = sqlite3_column_int(stmt, 0);
                    int pri = sqlite3_column_int(stmt, 2);
                    string urg = (pri >= 3) ? "high" : (pri == 2) ? "medium" : "low";
                    int review_id = -(tid * 1000 + days_ago);  // 负ID=review
                    int is_read = 0;
                    sqlite3_stmt* s2 = nullptr;
                    string ds = to_string(days_ago);
                    if (sqlite3_prepare_v2(db,
                            "SELECT COUNT(*) FROM reminders "
                            "WHERE user_id=? AND task_id=? AND type='review' "
                            "  AND message=? AND is_read=1;",
                            -1, &s2, nullptr) == SQLITE_OK) {
                        sqlite3_bind_int(s2, 1, user_id);
                        sqlite3_bind_int(s2, 2, tid);
                        sqlite3_bind_text(s2, 3, ds.c_str(), -1, SQLITE_TRANSIENT);
                        if (sqlite3_step(s2) == SQLITE_ROW)
                            is_read = sqlite3_column_int(s2, 0) > 0 ? 1 : 0;
                        sqlite3_finalize(s2);
                    }
                    arr.push_back({
                        {"id", review_id}, {"task_id", tid},
                        {"task_title", (const char*)sqlite3_column_text(stmt, 1)},
                        {"type", "review"}, {"urgency", urg},
                        {"remind_date", today}, {"is_read", is_read},
                        {"days_ago", days_ago}
                    });
                }
                sqlite3_finalize(stmt);
            }
        }
    }

    sqlite3_close(db);
    return arr.dump();
}

bool reminder_mark_read(int reminder_id, int user_id) {
    sqlite3* db = open_db();
    if (!db) return false;

    string rem_type;
    int task_id = 0;
    string message;

    if (reminder_id < 0) {
        rem_type = "review";
        int abs_id = -reminder_id;
        task_id = abs_id / 1000;
        message = to_string(abs_id % 1000);
    } else {
        rem_type = "due";
        task_id = reminder_id;
    }

    // UPDATE first
    sqlite3_stmt* stmt = nullptr;
    bool ok = false;
    const char* sql_upd =
        "UPDATE reminders SET is_read = 1 "
        "WHERE user_id = ? AND task_id = ? AND type = ? AND message = ?;";

    if (sqlite3_prepare_v2(db, sql_upd, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, user_id);
        sqlite3_bind_int(stmt, 2, task_id);
        sqlite3_bind_text(stmt, 3, rem_type.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, message.c_str(),  -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(db) > 0)
            ok = true;
        sqlite3_finalize(stmt);
    }

    // INSERT if UPDATE didn't hit
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

// ============================================================
// 以下模块 — 占位实现
// ============================================================

// 好友
bool friend_request(int from_id, int to_id) {
    if (from_id == to_id) return false;

    sqlite3* db = open_db();
    if (!db) return false;

    // 检查是否已经存在好友关系
    sqlite3_stmt* ck = nullptr;
    const char* check_sql =
        "SELECT id, status FROM friendships "
        "WHERE (from_id=? AND to_id=?) OR (from_id=? AND to_id=?);";
    bool exists = false;
    if (sqlite3_prepare_v2(db, check_sql, -1, &ck, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(ck, 1, from_id);
        sqlite3_bind_int(ck, 2, to_id);
        sqlite3_bind_int(ck, 3, to_id);
        sqlite3_bind_int(ck, 4, from_id);
        if (sqlite3_step(ck) == SQLITE_ROW)
            exists = true;  // 已存在，不管什么状态都不重复申请
        sqlite3_finalize(ck);
    }
    if (exists) { sqlite3_close(db); return false; }

    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT INTO friendships (from_id, to_id, status, created_at) VALUES (?, ?, 0, ?);";
    bool ok = false;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        string now = now_local();
        sqlite3_bind_int(stmt, 1, from_id);
        sqlite3_bind_int(stmt, 2, to_id);
        sqlite3_bind_text(stmt, 3, now.c_str(), -1, SQLITE_TRANSIENT);
        ok = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return ok;
}

string friend_list(int user_id) {
    sqlite3* db = open_db();
    if (!db) return "[]";

    json arr = json::array();
    sqlite3_stmt* stmt = nullptr;

    // 查所有跟 user_id 相关的好友关系，JOIN users 拿对方信息
    const char* sql = R"(
        SELECT f.id, f.from_id, f.to_id, f.status, f.created_at,
               u.id AS other_id, u.username, u.nickname, u.signature
        FROM friendships f
        JOIN users u ON (
            (f.from_id = ? AND u.id = f.to_id) OR
            (f.to_id = ? AND u.id = f.from_id)
        )
        WHERE f.from_id = ? OR f.to_id = ?
        ORDER BY f.created_at DESC
    )";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, user_id);
        sqlite3_bind_int(stmt, 2, user_id);
        sqlite3_bind_int(stmt, 3, user_id);
        sqlite3_bind_int(stmt, 4, user_id);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int from_id     = sqlite3_column_int(stmt, 1);
            int status_int  = sqlite3_column_int(stmt, 3);
            int other_id    = sqlite3_column_int(stmt, 5);

            json f;
            f["id"]          = sqlite3_column_int(stmt, 0);
            f["from_id"]     = from_id;
            f["to_id"]       = sqlite3_column_int(stmt, 2);
            f["status"]      = status_int;
            f["created_at"]  = (const char*)sqlite3_column_text(stmt, 4);

            // 对方信息
            f["other_id"]    = other_id;
            f["other_name"]  = (const char*)sqlite3_column_text(stmt, 6);
            f["other_nick"]  = (const char*)sqlite3_column_text(stmt, 7);
            f["other_sign"]  = (const char*)sqlite3_column_text(stmt, 8);

            // 方便前端判断：我是哪一方
            f["i_am_sender"] = (from_id == user_id);

            // 状态文本
            if (status_int == 0)
                f["status_text"] = (from_id == user_id) ? "等待对方通过" : "待处理";
            else if (status_int == 1)
                f["status_text"] = "已添加";
            else
                f["status_text"] = "已拒绝";

            arr.push_back(f);
        }
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return arr.dump();
}

bool friend_handle(int friendship_id, int status) {
    sqlite3* db = open_db();
    if (!db) return false;

    // 只能把"待通过"改成"已通过"或"已拒绝"
    const char* sql =
        "UPDATE friendships SET status=? WHERE id=? AND status=0;";

    sqlite3_stmt* stmt = nullptr;
    bool ok = false;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, status);
        sqlite3_bind_int(stmt, 2, friendship_id);
        ok = (sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(db) > 0);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return ok;
}

bool friend_delete(int friendship_id, int user_id) {
    sqlite3* db = open_db();
    if (!db) return false;

    // 只能删除跟自己有关的好友关系
    const char* sql =
        "DELETE FROM friendships WHERE id=? AND (from_id=? OR to_id=?);";

    sqlite3_stmt* stmt = nullptr;
    bool ok = false;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, friendship_id);
        sqlite3_bind_int(stmt, 2, user_id);
        sqlite3_bind_int(stmt, 3, user_id);
        ok = (sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(db) > 0);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return ok;
}
// 聊天
bool message_send(int from_id, int to_id, const string& content) {
    if (from_id == to_id || content.empty()) return false;
    sqlite3* db = open_db();
    if (!db) return false;

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT INTO messages (from_id, to_id, content, is_read, created_at) VALUES (?, ?, ?, 0, ?);";
    bool ok = false;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        string now = now_local();
        sqlite3_bind_int(stmt, 1, from_id);
        sqlite3_bind_int(stmt, 2, to_id);
        sqlite3_bind_text(stmt, 3, content.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, now.c_str(), -1, SQLITE_TRANSIENT);
        ok = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return ok;
}

string message_history(int user_id, int friend_id) {
    sqlite3* db = open_db();
    if (!db) return "[]";

    json arr = json::array();
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT id, from_id, to_id, content, is_read, created_at "
        "FROM messages "
        "WHERE (from_id=? AND to_id=?) OR (from_id=? AND to_id=?) "
        "ORDER BY created_at ASC;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, user_id);
        sqlite3_bind_int(stmt, 2, friend_id);
        sqlite3_bind_int(stmt, 3, friend_id);
        sqlite3_bind_int(stmt, 4, user_id);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            json m;
            m["id"]         = sqlite3_column_int(stmt, 0);
            m["from_id"]    = sqlite3_column_int(stmt, 1);
            m["to_id"]      = sqlite3_column_int(stmt, 2);
            m["content"]    = (const char*)sqlite3_column_text(stmt, 3);
            m["is_read"]    = sqlite3_column_int(stmt, 4);
            m["created_at"] = (const char*)sqlite3_column_text(stmt, 5);
            m["mine"]       = (sqlite3_column_int(stmt, 1) == user_id);
            arr.push_back(m);
        }
        sqlite3_finalize(stmt);

        // 对方发给我的标记为已读
        const char* upd =
            "UPDATE messages SET is_read=1 WHERE from_id=? AND to_id=? AND is_read=0;";
        if (sqlite3_prepare_v2(db, upd, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, friend_id);
            sqlite3_bind_int(stmt, 2, user_id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }
    sqlite3_close(db);
    return arr.dump();
}

int message_unread_count(int user_id) {
    sqlite3* db = open_db();
    if (!db) return 0;

    int count = 0;
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT COUNT(*) FROM messages WHERE to_id=? AND is_read=0;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, user_id);
        if (sqlite3_step(stmt) == SQLITE_ROW)
            count = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return count;
}
// 收藏 — 获取列表
string material_list(int user_id) {
    sqlite3* db = open_db();
    if (!db) return "[]";
    const char* sql = "SELECT id, title, url, created_at FROM materials WHERE user_id=? ORDER BY id DESC";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, user_id);
    string result = "[";
    bool first = true;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (!first) result += ",";
        first = false;
        result += "{\"id\":"     + to_string(sqlite3_column_int(stmt, 0)) + ",";
        result += "\"title\":\""  + string((const char*)sqlite3_column_text(stmt, 1)) + "\",";
        result += "\"url\":\""    + string((const char*)sqlite3_column_text(stmt, 2)) + "\",";
        result += "\"created_at\":\"" + string((const char*)sqlite3_column_text(stmt, 3)) + "\"}";
    }
    result += "]";
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return result;
}
// 收藏 — 添加
bool material_add(int user_id, const string& title, const string& url) {
    sqlite3* db = open_db();
    if (!db) return false;
    const char* sql = "INSERT INTO materials (user_id, title, url, created_at) VALUES (?, ?, ?, ?)";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    string now = now_local();
    sqlite3_bind_int(stmt,  1, user_id);
    sqlite3_bind_text(stmt, 2, title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, url.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, now.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return ok;
}
// 收藏 — 删除
bool material_delete(int material_id, int user_id) {
    sqlite3* db = open_db();
    if (!db) return false;
    const char* sql = "DELETE FROM materials WHERE id=? AND user_id=?";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, material_id);
    sqlite3_bind_int(stmt, 2, user_id);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return ok;
}

// ============================================================
// 收藏任务（任务模板快速复用）
// ============================================================

bool favorite_task_add(int user_id, const Task& t) {
    sqlite3* db = open_db();
    if (!db) return false;

    const char* sql = R"(
        INSERT INTO favorite_tasks (user_id, title, topic, deadline, priority, need_review)
        VALUES (?, ?, ?, ?, ?, ?)
    )";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_int(stmt,  1, user_id);
    sqlite3_bind_text(stmt, 2, t.title.c_str(),    -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, t.topic.c_str(),    -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, t.deadline.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt,  5, t.priority);
    sqlite3_bind_int(stmt,  6, t.need_review ? 1 : 0);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return ok;
}

string favorite_task_list(int user_id) {
    sqlite3* db = open_db();
    if (!db) return "[]";

    const char* sql = R"(
        SELECT id, title, topic, priority, need_review
        FROM favorite_tasks WHERE user_id=? ORDER BY created_at DESC
    )";
    sqlite3_stmt* stmt = nullptr;
    json arr = json::array();
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, user_id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            json f;
            f["id"]          = sqlite3_column_int(stmt, 0);
            f["title"]       = (const char*)sqlite3_column_text(stmt, 1);
            f["topic"]       = (const char*)sqlite3_column_text(stmt, 2);
            f["priority"]    = sqlite3_column_int(stmt, 3);
            f["need_review"] = sqlite3_column_int(stmt, 4);
            arr.push_back(f);
        }
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return arr.dump();
}

bool favorite_task_delete(int favorite_id, int user_id) {
    sqlite3* db = open_db();
    if (!db) return false;

    const char* sql = "DELETE FROM favorite_tasks WHERE id=? AND user_id=?";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, favorite_id);
    sqlite3_bind_int(stmt, 2, user_id);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(db) > 0;
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return ok;
}

// 番茄钟
bool pomodoro_record(int user_id, int duration) {
    sqlite3* db = open_db();
    if (!db) return false;

    string today = today_str();
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT INTO pomodoros (user_id, duration, date, created_at) VALUES (?, ?, ?, ?);";
    bool ok = false;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        string now = now_local();
        sqlite3_bind_int(stmt, 1, user_id);
        sqlite3_bind_int(stmt, 2, duration);
        sqlite3_bind_text(stmt, 3, today.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, now.c_str(), -1, SQLITE_TRANSIENT);
        ok = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return ok;
}

string pomodoro_today(int user_id) {
    sqlite3* db = open_db();
    if (!db) return "[]";

    string today = today_str();
    json arr = json::array();
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT id, duration, created_at, date FROM pomodoros "
        "WHERE user_id = ? AND date = ? ORDER BY created_at ASC;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, user_id);
        sqlite3_bind_text(stmt, 2, today.c_str(), -1, SQLITE_TRANSIENT);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* ca = (const char*)sqlite3_column_text(stmt, 2);
            string start_time = "-";
            if (ca) {
                string s(ca);
                if (s.length() >= 16) start_time = s.substr(11, 5);  // "HH:MM"
            }
            json r;
            r["id"]         = sqlite3_column_int(stmt, 0);
            r["duration"]   = sqlite3_column_int(stmt, 1);
            r["start_time"] = start_time;
            r["date"]       = (const char*)sqlite3_column_text(stmt, 3);
            arr.push_back(r);
        }
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return arr.dump();
}
// 名言
string quote_random() {
    sqlite3* db = open_db();
    if (!db) return "{}";

    json obj = {{"content", "学无止境"}, {"author", "佚名"}};

    sqlite3_stmt* stmt = nullptr;
    // ORDER BY RANDOM() 随机取一条
    const char* sql = "SELECT content, author FROM quotes ORDER BY RANDOM() LIMIT 1;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            obj["content"] = (const char*)sqlite3_column_text(stmt, 0);
            obj["author"]  = (const char*)sqlite3_column_text(stmt, 1);
        }
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return obj.dump();
}

// ============================================================
// 管理员功能
// ============================================================

// 获取全部用户列表
string user_list_all() {
    sqlite3* db = open_db();
    if (!db) return "[]";

    json arr = json::array();
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT id, username, nickname, signature, role, created_at "
        "FROM users ORDER BY id ASC;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            json u;
            u["id"]         = sqlite3_column_int(stmt, 0);
            u["username"]   = (const char*)sqlite3_column_text(stmt, 1);
            u["nickname"]   = (const char*)sqlite3_column_text(stmt, 2);
            u["signature"]  = (const char*)sqlite3_column_text(stmt, 3);
            u["role"]       = (const char*)sqlite3_column_text(stmt, 4);
            u["created_at"] = (const char*)sqlite3_column_text(stmt, 5);
            arr.push_back(u);
        }
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return arr.dump();
}

// 修改用户角色
bool user_update_role(int user_id, const string& role) {
    sqlite3* db = open_db();
    if (!db) return false;

    const char* sql = "UPDATE users SET role=? WHERE id=?;";
    sqlite3_stmt* stmt = nullptr;
    bool ok = false;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, role.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, user_id);
        ok = (sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(db) > 0);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return ok;
}

// 删除用户
bool user_delete(int user_id) {
    sqlite3* db = open_db();
    if (!db) return false;

    // 删除用户及相关数据
    exec_sql(db, "DELETE FROM tasks WHERE user_id=" + to_string(user_id) + ";");
    exec_sql(db, "DELETE FROM checkins WHERE user_id=" + to_string(user_id) + ";");
    exec_sql(db, "DELETE FROM signins WHERE user_id=" + to_string(user_id) + ";");
    exec_sql(db, "DELETE FROM reminders WHERE user_id=" + to_string(user_id) + ";");
    exec_sql(db, "DELETE FROM friendships WHERE from_id=" + to_string(user_id) + " OR to_id=" + to_string(user_id) + ";");
    exec_sql(db, "DELETE FROM messages WHERE from_id=" + to_string(user_id) + " OR to_id=" + to_string(user_id) + ";");
    exec_sql(db, "DELETE FROM materials WHERE user_id=" + to_string(user_id) + ";");
    exec_sql(db, "DELETE FROM pomodoros WHERE user_id=" + to_string(user_id) + ";");

    const char* sql = "DELETE FROM users WHERE id=?;";
    sqlite3_stmt* stmt = nullptr;
    bool ok = false;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, user_id);
        ok = (sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(db) > 0);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return ok;
}

// 检查是否是管理员
bool user_is_admin(int user_id) {
    sqlite3* db = open_db();
    if (!db) return false;

    const char* sql = "SELECT role FROM users WHERE id=?;";
    sqlite3_stmt* stmt = nullptr;
    bool is_admin = false;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, user_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            string role = (const char*)sqlite3_column_text(stmt, 0);
            is_admin = (role == "admin");
        }
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return is_admin;
}

// 获取推荐任务库全部数据（管理员视角，含时间）
string task_recommended_all() {
    sqlite3* db = open_db();
    if (!db) return "[]";

    json arr = json::array();
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT id, title, topic, priority, created_at FROM recommended_tasks ORDER BY id ASC;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            json t;
            t["id"]         = sqlite3_column_int(stmt, 0);
            t["title"]      = (const char*)sqlite3_column_text(stmt, 1);
            t["topic"]      = (const char*)sqlite3_column_text(stmt, 2);
            t["priority"]   = sqlite3_column_int(stmt, 3);
            t["created_at"] = (const char*)sqlite3_column_text(stmt, 4);
            arr.push_back(t);
        }
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return arr.dump();
}

// 添加推荐任务
bool task_recommended_add(const string& title, const string& topic, int priority) {
    sqlite3* db = open_db();
    if (!db) return false;

    const char* sql = "INSERT INTO recommended_tasks (title, topic, priority) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    bool ok = false;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, title.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, topic.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, priority);
        ok = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return ok;
}

// 更新推荐任务
bool task_recommended_update(int id, const string& title, const string& topic, int priority) {
    sqlite3* db = open_db();
    if (!db) return false;

    const char* sql = "UPDATE recommended_tasks SET title=?, topic=?, priority=? WHERE id=?;";
    sqlite3_stmt* stmt = nullptr;
    bool ok = false;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, title.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, topic.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, priority);
        sqlite3_bind_int(stmt, 4, id);
        ok = (sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(db) > 0);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return ok;
}

// 删除推荐任务
bool task_recommended_delete(int id) {
    sqlite3* db = open_db();
    if (!db) return false;

    const char* sql = "DELETE FROM recommended_tasks WHERE id=?;";
    sqlite3_stmt* stmt = nullptr;
    bool ok = false;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        ok = (sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(db) > 0);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return ok;
}

// 系统全局统计
string stats_system() {
    sqlite3* db = open_db();
    if (!db) return "{}";

    json result;
    sqlite3_stmt* stmt = nullptr;

    // 总用户数
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM users;", -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW)
            result["total_users"] = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }

    // 总任务数
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM tasks;", -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW)
            result["total_tasks"] = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }

    // 已完成任务数
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM tasks WHERE status=1;", -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW)
            result["completed_tasks"] = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }

    // 总打卡次数
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM checkins;", -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW)
            result["total_checkins"] = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }

    // 总番茄钟次数
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM pomodoros;", -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW)
            result["total_pomodoros"] = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }

    // 好友关系数
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM friendships WHERE status=1;", -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW)
            result["total_friendships"] = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }

    // 今日活跃用户数（今天打过卡或记录过番茄钟）
    string today = today_str();
    if (sqlite3_prepare_v2(db,
            "SELECT COUNT(DISTINCT user_id) FROM ("
            "  SELECT user_id FROM checkins WHERE date=? "
            "  UNION "
            "  SELECT user_id FROM pomodoros WHERE date=?"
            ");",
            -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, today.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, today.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW)
            result["active_today"] = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }

    sqlite3_close(db);
    return result.dump();
}
