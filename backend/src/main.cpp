#define _WIN32_WINNT 0x0A00   // Windows 10+，httplib 编译需要
#include <cstdlib>
#include "httplib.h"
#include "db.h"
#include "json.hpp"
#include "routes_tasks.h"
#include "routes_auth.h"
#include "routes_material.h"
#include "routes_reminder.h"
#include "routes_checkin.h"
#include "routes_pomodoro.h"
#include "routes_social.h"
#include "routes_admin.h"

using namespace httplib;
using json = nlohmann::json;
using namespace std;

int main() {
    system("chcp 65001 >nul");  // 控制台设为 UTF-8，中文不乱码
    init_tables();

    // 2. 创建 HTTP 服务器
    Server svr;

    // 3. 允许跨域（CORS），不然浏览器会拦截
    //    加了这行，前端不管从哪个地址发请求都能访问
    svr.set_pre_routing_handler([](const Request& req, Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "*");
        // OPTIONS 预检请求直接返回 200
        if (req.method == "OPTIONS") {
            res.status = 200;
            return Server::HandlerResponse::Handled;
        }
        return Server::HandlerResponse::Unhandled;
    });

    // 4. 提供前端静态文件，关闭缓存确保实时生效
    svr.set_mount_point("/", "../frontend");
    svr.set_post_routing_handler([](const Request& req, Response& res) {
        res.set_header("Cache-Control", "no-cache, no-store, must-revalidate");
    });

    // 5. 注册 API 路由
    svr.Get("/api/hello", [](const Request& req, Response& res) {
        res.set_content(R"({"ok":true,"msg":"hello, 后端已启动!"})", "application/json");
    });

    // ---------------- 任务接口 ----------------
    svr.Get("/api/tasks",              handle_get_tasks);
    svr.Get(R"(/api/tasks/(\d+))",     handle_get_one_task);
    svr.Post("/api/tasks",             handle_create_task);
    svr.Put(R"(/api/tasks/(\d+))",     handle_update_task);
    svr.Delete(R"(/api/tasks/(\d+))",  handle_delete_task);

    // ---------------- 收藏任务模板 ----------------
    svr.Post("/api/favorite_tasks",                    handle_favorite_task_add);
    svr.Get("/api/favorite_tasks",                     handle_favorite_task_list);
    svr.Delete(R"(/api/favorite_tasks/(\d+))",         handle_favorite_task_delete);

    // 辅助：从请求中获取 user_id（优先从 Authorization Bearer token 解析）
    auto get_uid = [](const Request& req) -> int {
        if (req.has_header("Authorization")) {
            string auth = req.get_header_value("Authorization");
            if (auth.rfind("Bearer ", 0) == 0) {
                return user_id_by_token(auth.substr(7));
            }
        }
        // 兼容旧方式：query param ?user_id=
        if (req.has_param("user_id")) return stoi(req.get_param_value("user_id"));
        return 1;  // 默认用户 ID（正式上线后改为 -1 强制登录）
    };

    // 辅助宏：检查登录状态，未登录返回 401
    #define REQUIRE_AUTH(uid_var) \
        int uid_var = get_uid(req); \
        if (uid_var <= 0) { \
            res.status = 401; \
            res.set_content(R"({"ok":false,"error":"请先登录"})", "application/json"); \
            return; \
        }

    // ---------------- 签到/打卡接口 ----------------
    svr.Get("/api/checkin",   handle_get_checkin);
    svr.Post("/api/checkin",  handle_do_checkin);

    // ---------------- 统计接口 ----------------
    svr.Get("/api/stats/overview", [&](const Request& req, Response& res) {
        REQUIRE_AUTH(user_id);
        string data_str = stats_overview(user_id);
        json resp = {{"ok", true}, {"data", json::parse(data_str)}};
        res.set_content(resp.dump(), "application/json");
    });

    svr.Get("/api/stats/daily", [&](const Request& req, Response& res) {
        REQUIRE_AUTH(user_id);
        string start = req.has_param("start") ? req.get_param_value("start") : "2026-07-01";
        string end   = req.has_param("end")   ? req.get_param_value("end")   : "2026-07-31";
        string data_str = stats_daily(user_id, start, end);
        json resp = {{"ok", true}, {"data", json::parse(data_str)}};
        res.set_content(resp.dump(), "application/json");
    });

    svr.Get("/api/stats/weekly", [&](const Request& req, Response& res) {
        REQUIRE_AUTH(user_id);
        // weekly 复用 daily，取 7 天
        string week = req.has_param("week") ? req.get_param_value("week") : "2026-07-01";
        // 计算 week 的结束日期（+6 天）
        int y, m, d;
        sscanf(week.c_str(), "%d-%d-%d", &y, &m, &d);
        // 简化：直接用 daily 查 7 天范围
        string end_str(week);
        end_str[8] = (char)('0' + min((d + 6) / 10, 3));
        end_str[9] = (char)('0' + ((d + 6) % 10));  // 简单加 6，不处理跨月
        string data_str = stats_daily(user_id, week, end_str);
        json resp = {{"ok", true}, {"data", json::parse(data_str)}};
        res.set_content(resp.dump(), "application/json");
    });

    svr.Get("/api/stats/monthly", [&](const Request& req, Response& res) {
        REQUIRE_AUTH(user_id);
        string month = req.has_param("month") ? req.get_param_value("month") : "2026-07";
        string start = month + "-01";
        string end   = month + "-31";
        string data_str = stats_daily(user_id, start, end);
        json resp = {{"ok", true}, {"data", json::parse(data_str)}};
        res.set_content(resp.dump(), "application/json");
    });

    // ---------------- 提醒接口 ----------------
    svr.Get("/api/reminders",               handle_get_reminders);
    svr.Post("/api/reminders/mark_read",    handle_mark_read);

    // ---------------- 推荐任务（系统推荐） ----------------
    svr.Get("/api/recommended_tasks", [](const Request& req, Response& res) {
        string data_str = task_recommended();
        json resp = {{"ok", true}, {"data", json::parse(data_str)}};
        res.set_content(resp.dump(), "application/json");
    });

    // ---------------- 收藏学习资料 ----------------
    svr.Get("/api/materials",              handle_get_materials);
    svr.Post("/api/materials",             handle_add_material);
    svr.Delete(R"(/api/materials/(\d+))",  handle_delete_material);

    // ---------------- 番茄钟 ----------------
    svr.Post("/api/pomodoro",         handle_record_pomodoro);
    svr.Get("/api/pomodoro/today",    handle_pomodoro_today);

    // ---------------- 账号与用户 ----------------
    svr.Post("/api/auth/register",  handle_register);
    svr.Post("/api/auth/login",     handle_login);
    svr.Get("/api/me",              handle_get_me);
    svr.Put("/api/me",              handle_update_profile);
    svr.Put("/api/me/password",     handle_change_password);
    svr.Get("/api/users/search",    handle_search_users);

    // ---------------- 好友与社交 ----------------
    svr.Post("/api/friends/request",             handle_friend_request);
    svr.Get("/api/friends",                      handle_friend_list);
    svr.Post(R"(/api/friends/(\d+)/handle)",     handle_friend_action);
    svr.Delete(R"(/api/friends/(\d+))",          handle_friend_delete);

    // ---------------- 聊天 ----------------
    svr.Post("/api/messages",                    handle_send_message);
    svr.Get(R"(/api/messages/(\d+))",            handle_get_messages);
    svr.Get("/api/messages/unread_count",        handle_unread_count);

    // ---------------- 签到 ----------------
    svr.Get("/api/signins",   handle_get_signins);
    svr.Post("/api/signins",  handle_do_signin);

    // ---------------- 打卡记录 ----------------
    svr.Get("/api/checkins",   handle_get_checkins);
    svr.Post("/api/checkins",  handle_create_checkin_record);

    // ---------------- 名言（Quotes） ----------------
    svr.Get("/api/quotes/random", [](const Request& req, Response& res) {
        string data_str = quote_random();
        json resp = {{"ok", true}, {"data", json::parse(data_str)}};
        res.set_content(resp.dump(), "application/json");
    });

    // ---------------- 管理员接口 ----------------
    svr.Get("/api/admin/users",                    handle_admin_users);
    svr.Put(R"(/api/admin/users/(\d+)/role)",      handle_admin_user_role);
    svr.Delete(R"(/api/admin/users/(\d+))",        handle_admin_user_delete);
    svr.Get("/api/admin/recommended",               handle_admin_recommended);
    svr.Post("/api/admin/recommended",              handle_admin_recommended_add);
    svr.Put(R"(/api/admin/recommended/(\d+))",     handle_admin_recommended_update);
    svr.Delete(R"(/api/admin/recommended/(\d+))",  handle_admin_recommended_delete);
    svr.Get("/api/admin/stats",                     handle_admin_stats);

    // 5. 设置前端静态文件目录
    svr.set_mount_point("/", "../frontend");

    // 6. 关掉 httplib 自带的日志
    svr.set_logger(nullptr);

    // 7. 启动服务器
    cout << "Open in browser: http://localhost:8080" << endl;
    svr.listen("0.0.0.0", 8080);

    return 0;
}
