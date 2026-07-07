#pragma once
#include <string>
#include <vector>
#include "sqlite3.h"
using namespace std;

// ============================================================
//  db.h — 数据库层的声明
//  所有跟 sqlite3 打交道的事都放这里，其他文件只管调用
// ============================================================

// ---------- 初始化 ----------

// 打开数据库连接，返回句柄
sqlite3* open_db();

// 建表（第一次运行时自动创建所有表）
void init_tables();

// 用户模块
int  user_create(const string& username, const string& password);
string user_login(const string& username, const string& password);
int  user_id_by_token(const string& token);
string user_get_info(int user_id);
string user_search(const string& keyword);

// 任务模块
int  task_create(int user_id, const string& title, const string& topic,
                 const string& deadline, int priority, bool need_review);
string task_get_list(int user_id);
string task_get_one(int task_id);
bool task_update(int task_id, int user_id,
                 const string& field, const string& value);
bool task_delete(int task_id, int user_id);
string task_recommended();

// 打卡 & 签到
bool   checkin_do(int user_id, int task_id, const string& date);
string checkin_get(int user_id, const string& date);
bool   signin_do(int user_id, const string& date);
int    signin_streak(int user_id);
string signin_history(int user_id);

// 统计
string stats_overview(int user_id);
string stats_daily(int user_id, const string& start_date, const string& end_date);

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
