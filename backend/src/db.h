#pragma once
#include <string>
#include <vector>
#include "sqlite3.h"
using namespace std;

// ============================================================
//  db.h — 数据库层的声明
//  所有跟 sqlite3 打交道的事都放这里，其他文件只管调用
// ============================================================

class user // 用户及其账号
{
    private:
        int id; // 用户ID，唯一
        string username; // 用户昵称，可不唯一
        string password_hash; // 密码的 SHA256 哈希值
        string phone_number; // 电话号码, 与唯一账号绑定
        string signature; // 个性签名 
        string token; // 登录令牌
        string created_at; // 创建时间
        enum class Role
        {
            Guest,
            Admin,
            SuperAdmin
        };
        Role role; // 用户角色
    public:
        user()
        {

        }
        user(int id_, const string& phone_, const string& password_hash_, const string& username_ = "NewUser"):id(id_), phone_number(phone_), password_hash(password_hash_), username(username_)
        {
            signature = "Hello World!";
        }
        void user_print();
};

class task //任务
{
    private:
        int id; // 任务ID，唯一
        int user_id; // 用户ID，外键
        string title; // 任务标题
        string topic; // 学习主题
        string deadline; // 截止日期
        int priority; // 优先级
        enum class Status 
        {
            Pending, // 待定
            Completed, // 已完成
            InProgress, // 进行中
            Cancelled // 已取消
        };
        Status status; // 任务状态
        bool need_review; // 是否需要复习提醒
        string created_at; // 创建时间
    public:
        task()
        {

        }
        task(int id_, int user_id_, const string& title_, const string& topic_, const string& deadline_, int priority_, bool need_review_):
            id(id_), user_id(user_id_), title(title_), topic(topic_), deadline(deadline_), priority(priority_), need_review(need_review_)
        {
            status = Status::Pending;
        }
        void task_print();
};

class checkin // 打卡
{
    private:
        int id; // 打卡ID，唯一
        int user_id; // 用户ID，外键
        int task_id; // 任务ID，外键
        string date; // 打卡日期
        string created_at; // 记录生成时间
    public:
        checkin()
        {

        }
        checkin(int id_, int user_id_, int task_id_, const string& date_):id(id_), user_id(user_id_), task_id(task_id_), date(date_)
        {

        }
        void checkin_print();
};

class signin // 签到
{
    private:
        int id; // 签到ID，唯一
        int user_id; // 用户ID，外键
        string date; // 签到日期
        string created_at; // 记录生成时间
    public:
        signin()
        {

        }
        signin(int id_, int user_id_, const string& date_):id(id_), user_id(user_id_), date(date_)
        {

        }
};

class reminder // 提醒
{
    private:
        int id; // 提醒ID，唯一
        int user_id; // 用户ID，外键
        enum class Type 
        {
            Due,    // 任务到期提醒
            Review  // 复习提醒
        };
        Type type; // 提醒类型
        string content; // 提醒内容
        bool is_read; // 是否已读
        string created_at; // 记录生成时间
    public:
        reminder()
        {

        }
        reminder(int id_, int user_id_, const string& content_, bool is_read_):id(id_), user_id(user_id_), content(content_), is_read(is_read_)
        {

        }
        void reminder_print();
};

class friendship // 好友关系
{
    private:
        int id; // 关系ID，唯一
        int user_id_1; // 用户1的ID，外键
        int user_id_2; // 用户2的ID，外键
        enum class Status 
        {
            Pending,    // 待处理
            Accepted,   // 已接受
            Rejected    // 已拒绝
        };
        Status status; // 好友关系状态
        string created_at; // 记录生成时间
    public:
        friendship()
        {
        }
        friendship(int id_, int user_id_1_, int user_id_2_):
            id(id_), user_id_1(user_id_1_), user_id_2(user_id_2_)
        {

        }
        void friendship_print();
};

class message // 聊天消息
{
    private:
        int id; // 消息ID，唯一
        int sender_id; // 发送者ID，外键
        int receiver_id; // 接收者ID，外键
        string content; // 消息内容
        bool is_read; // 是否已读
        string created_at; // 记录生成时间
    public:
        message()
        {

        }
        message(int id_, int sender_id_, int receiver_id_, const string& content_, bool is_read_):
            id(id_), sender_id(sender_id_), receiver_id(receiver_id_), content(content_), is_read(is_read_)
        {

        }
        void message_print();
};

class material // 收藏的学习资料
{
    private:
        int id; // 资料ID，唯一
        int user_id; // 用户ID，外键
        string keyword; // 关键字
        string title; // 资料标题
        string url; // 资料链接
        string created_at; // 记录生成时间
    public:
        material()
        {

        }
        material(int id_, int user_id_, const string& keyword_, const string& title_, const string& url_):
            id(id_), user_id(user_id_), keyword(keyword_), title(title_), url(url_)
        {

        }
        void material_print();
};

class pomodoro // 番茄钟记录
{
    private:
        int id; // 记录ID，唯一
        int user_id; // 用户ID，外键
        long long start_time; // 番茄钟开始时间（时间戳）
        int duration; // 番茄钟时长（分钟）
        enum class Status
        {
            InProgress, // 进行中
            Completed,  // 已完成
            Interrupted, // 被打断
            Paused // 暂停
        };
        Status status; // 番茄钟状态
        string date; // 使用日期
        string created_at; // 记录生成时间
    public:
        pomodoro()
        {

        }
        pomodoro(int id_, int user_id_, long long start_time_, int duration_, const string& date_):
            id(id_), user_id(user_id_), start_time(start_time_), duration(duration_), date(date_)
        {

        }
        void pomodoro_print();
};

class quote // 名言
{
    private:
        int id; // 名言ID，唯一
        string content; // 名言内容
        string author; // 作者
        string created_at; // 记录生成时间
    public:
        quote()
        {

        }
        quote(int id_, const string& content_, const string& author_):
            id(id_), content(content_), author(author_)
        {

        }
        void quote_print();
};

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
