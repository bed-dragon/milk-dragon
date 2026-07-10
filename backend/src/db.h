#pragma once
#include <string>
#include <vector>
#include "sqlite3.h"
using namespace std;

// ============================================================
//  db.h — 数据库层
//  上半段：struct 数据模型（快递盒，传数据用）
//  下半段：函数声明（操作数据库，db.cpp 实现）
// ============================================================

// -------- 数据模型（全部 public，字段名跟建表 SQL 对齐）--------

struct User {
    int id = 0;
    string username;
    string password_hash;
    string nickname;
    string signature;
    string token;
    string created_at;
};

struct Task {
    int id = 0;
    int user_id = 0;
    string title;
    string topic;
    string deadline;
    int priority = 1;
    int status = 0;           // 0待完成 1已完成
    bool need_review = false;
    string created_at;
};

struct Checkin {
    int id = 0;
    int user_id = 0;
    int task_id = 0;
    string date;
    string created_at;
};

struct Signin {
    int id = 0;
    int user_id = 0;
    string date;
    string created_at;
};

struct Reminder {
    int id = 0;
    int user_id = 0;
    int task_id = 0;
    string type;              // "due" 到期提醒 / "review" 复习提醒
    string message;           // 提醒内容
    bool is_read = false;
    string created_at;
};

struct Friendship {
    int id = 0;
    int from_id = 0;          // 发起申请的人
    int to_id = 0;            // 收到申请的人
    int status = 0;           // 0待通过 1已通过 2已拒绝
    string created_at;
};

struct Message {
    int id = 0;
    int from_id = 0;          // 发送者
    int to_id = 0;            // 接收者
    string content;
    bool is_read = false;
    string created_at;
};

struct Material {
    int id = 0;
    int user_id = 0;
    string title;
    string url;
    string created_at;
};

struct Pomodoro {
    int id = 0;
    int user_id = 0;
    int duration = 0;         // 分钟
    string date;
    string created_at;
};

struct Quote {
    int id = 0;
    string content;
    string author;
    string created_at;
};

// -------- 函数声明（db.cpp 实现）--------

// 初始化
sqlite3* open_db();
void init_tables();

// 用户
int    user_create(const string& username, const string& password, const string& nickname = "");
string user_login(const string& username, const string& password);
int    user_id_by_token(const string& token);
string user_get_info(int user_id);
string user_search(const string& keyword);
bool   user_update_profile(int user_id, const string& nickname, const string& signature);
bool   user_change_password(int user_id, const string& old_pwd, const string& new_pwd);

// 任务
int    task_create(int user_id, const Task& t);
string task_get_list(int user_id);
string task_get_one(int task_id);
bool   task_update(int task_id, int user_id, const Task& t);
bool   task_delete(int task_id, int user_id);
string task_recommended();

// 打卡 & 签到
bool   checkin_do(int user_id, int task_id, const string& date);
string checkin_get(int user_id, const string& date);
bool   signin_do(int user_id, const string& date);
int    signin_streak(int user_id);
string signin_history(int user_id);

// 统计
string stats_overview(int user_id);
string stats_daily(int user_id, const string& start, const string& end);

// 提醒
string reminder_list(int user_id, const string& type);
bool   reminder_mark_read(int reminder_id, int user_id);

// 好友
bool   friend_request(int from_id, int to_id);
string friend_list(int user_id);
bool   friend_handle(int friendship_id, int status);
bool   friend_delete(int friendship_id, int user_id);

// 聊天
bool   message_send(int from_id, int to_id, const string& content);
string message_history(int user_id, int friend_id);
int    message_unread_count(int user_id);

// 收藏
string material_list(int user_id);
bool   material_add(int user_id, const string& title, const string& url);
bool   material_delete(int material_id, int user_id);

// 番茄钟
bool   pomodoro_record(int user_id, int duration);
string pomodoro_today(int user_id);

// 名言
string quote_random();
