#include "db.h"
#include <iostream>
#include "sqlite3.h"   // 通过 -Ilibs 编译参数找到

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
    int rc = sqlite3_open("data/study.db", &db);
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
// 4. task_create() — 创建任务
// ============================================================
// 来个完整的 CRUD 示例，后面 task_get_list、task_update、
// task_delete 都是照这个模板改几行 SQL。
//
// 为什么用 sqlite3_prepare + sqlite3_step，而不是 sqlite3_exec？
//   1. 防 SQL 注入 —— 用户输入内容通过 bind 传进去，不会被当成 SQL 执行
//   2. 能拿到新插入的 id —— sqlite3_last_insert_rowid
//   3. 效率更高 —— SQL 只编译一次，可以重复 bind 不同参数执行
// ============================================================

int task_create(int user_id, const Task& t) {
    sqlite3* db = open_db();
    if (!db) return -1;

    const char* sql = R"(
        INSERT INTO tasks (user_id, title, topic, deadline, priority, need_review)
        VALUES (?, ?, ?, ?, ?, ?)
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
        cerr << "[ERROR] prepare: " << sqlite3_errmsg(db) << endl;
        sqlite3_close(db);
        return -1;
    }

    // 从 struct Task t 里取字段，按 ? 顺序绑定
    sqlite3_bind_int(stmt,  1, user_id);
    sqlite3_bind_text(stmt, 2, t.title.c_str(),    -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, t.topic.c_str(),    -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, t.deadline.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt,  5, t.priority);
    sqlite3_bind_int(stmt,  6, t.need_review ? 1 : 0);

    int ok = sqlite3_step(stmt);
    int new_id = (ok == SQLITE_DONE) ? (int)sqlite3_last_insert_rowid(db) : -1;

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return new_id;
}





// 5. task_get_list() — 查询任务列表
// ============================================================
// SELECT 跟 INSERT 的区别：
//   - INSERT 用 sqlite3_step 执行一次就完
//   - SELECT 用 while(sqlite3_step == SQLITE_ROW) 循环取每一行
//   每行用 sqlite3_column_xxx 读字段值，拼成 JSON 数组返回
// ============================================================

string task_get_list(int user_id) {
    sqlite3* db = open_db();
    if (!db) return "[]";

    const char* sql = "SELECT id, title, topic, deadline, priority, status, need_review FROM tasks WHERE user_id=? ORDER BY id DESC";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, user_id);

    string result = "[";                          // JSON 数组开始
    bool first = true;
    while (sqlite3_step(stmt) == SQLITE_ROW) {    // 逐行遍历结果
        if (!first) result += ",";                // 不是第一个就加逗号
        first = false;

        result += "{";
        result += "\"id\":"          + to_string(sqlite3_column_int(stmt, 0)) + ",";
        result += "\"title\":\""     + string((const char*)sqlite3_column_text(stmt, 1)) + "\",";
        result += "\"topic\":\""     + string((const char*)sqlite3_column_text(stmt, 2)) + "\",";
        result += "\"deadline\":\""  + string((const char*)sqlite3_column_text(stmt, 3)) + "\",";
        result += "\"priority\":"    + to_string(sqlite3_column_int(stmt, 4)) + ",";
        result += "\"status\":"      + to_string(sqlite3_column_int(stmt, 5)) + ",";
        result += "\"need_review\":" + to_string(sqlite3_column_int(stmt, 6));
        result += "}";
    }
    result += "]";

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return result;                                // 返回 JSON 数组字符串
}




// 6. task_get_one() — 查询单个任务
// ============================================================
// 跟 task_get_list 一样是 SELECT，但只查一行（WHERE id=?）
// 返回单个 JSON 对象，不是数组

string task_get_one(int task_id) {
    sqlite3* db = open_db();
    if (!db) return "{}";

    const char* sql = "SELECT id, user_id, title, topic, deadline, priority, status, need_review FROM tasks WHERE id=?";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, task_id);

    string result = "{}";
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = "{";
        result += "\"id\":"          + to_string(sqlite3_column_int(stmt, 0)) + ",";
        result += "\"user_id\":"     + to_string(sqlite3_column_int(stmt, 1)) + ",";
        result += "\"title\":\""     + string((const char*)sqlite3_column_text(stmt, 2)) + "\",";
        result += "\"topic\":\""     + string((const char*)sqlite3_column_text(stmt, 3)) + "\",";
        result += "\"deadline\":\""  + string((const char*)sqlite3_column_text(stmt, 4)) + "\",";
        result += "\"priority\":"    + to_string(sqlite3_column_int(stmt, 5)) + ",";
        result += "\"status\":"      + to_string(sqlite3_column_int(stmt, 6)) + ",";
        result += "\"need_review\":" + to_string(sqlite3_column_int(stmt, 7));
        result += "}";
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return result;
}

// ============================================================
// 7. task_update() — 编辑任务
// ============================================================
bool task_update(int task_id, int user_id, const Task& t) {
    sqlite3* db = open_db();
    if (!db) return false;

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
    sqlite3_bind_int(stmt,  5, t.status);
    sqlite3_bind_int(stmt,  6, t.need_review ? 1 : 0);
    sqlite3_bind_int(stmt,  7, task_id);
    sqlite3_bind_int(stmt,  8, user_id);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return ok;
}

// ============================================================
// 8. task_delete() — 删除任务
// ============================================================
// 跟 UPDATE 一个结构，就 SQL 不同
// WHERE id=? AND user_id=? 双重验证，防止删别人的任务
bool task_delete(int task_id, int user_id) {
    sqlite3* db = open_db();
    if (!db) return false;

    const char* sql = "DELETE FROM tasks WHERE id=? AND user_id=?";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, task_id);
    sqlite3_bind_int(stmt, 2, user_id);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return ok;
}
