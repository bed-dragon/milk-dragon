#define _WIN32_WINNT 0x0A00   // Windows 10+，httplib 编译需要
#include "httplib.h"
#include "db.h"
#include "json.hpp"
#include "routes_tasks.h"
#include "routes_auth.h"
#include "routes_material.h"

using namespace httplib;
using json = nlohmann::json;
using namespace std;

// db.cpp 中额外提供的按 task_id 查打卡的函数（补充 db.h 中的 checkin_get）
extern string checkin_get_by_task(int user_id, int task_id);

int main() {
    // 1. 初始化数据库（建表）
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

    // 4. 注册路由
    svr.Get("/api/hello", [](const Request& req, Response& res) {
        res.set_content(R"({"ok":true,"msg":"hello, 后端已启动!"})", "application/json");
    });

    // ---------------- 任务接口 ----------------
    svr.Get("/api/tasks",              handle_get_tasks);
    svr.Get(R"(/api/tasks/(\d+))",     handle_get_one_task);
    svr.Post("/api/tasks",             handle_create_task);
    svr.Put(R"(/api/tasks/(\d+))",     handle_update_task);
    svr.Delete(R"(/api/tasks/(\d+))",  handle_delete_task);

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

    // ---------------- 签到/打卡接口（真实数据库操作） ----------------

    // GET /api/checkin — 查询打卡记录（支持 ?date= 和 ?task_id=）
    svr.Get("/api/checkin", [&](const Request& req, Response& res) {
        REQUIRE_AUTH(user_id);
        string data_str;

        if (req.has_param("task_id")) {
            int task_id = stoi(req.get_param_value("task_id"));
            data_str = checkin_get_by_task(user_id, task_id);
        } else {
            string date = req.has_param("date") ? req.get_param_value("date") : "";
            data_str = checkin_get(user_id, date);
        }

        json resp = {{"ok", true}, {"data", json::parse(data_str)}};
        res.set_content(resp.dump(), "application/json");
    });

    // POST /api/checkin — 执行打卡
    svr.Post("/api/checkin", [&](const Request& req, Response& res) {
        try {
            json body = json::parse(req.body);
            REQUIRE_AUTH(user_id);
            int task_id = body.value("task_id", 0);
            string date = body.value("date", "");

            if (task_id == 0 || date.empty()) {
                res.status = 400;
                res.set_content(R"({"ok":false,"error":"task_id and date are required"})", "application/json");
                return;
            }

            if (checkin_do(user_id, task_id, date)) {
                json resp = {{"ok", true}, {"message", "checked"}, {"data", {{"checked", true}}}};
                res.set_content(resp.dump(), "application/json");
            } else {
                res.status = 409;  // Conflict — 可能已重复打卡
                json resp = {{"ok", false}, {"error", "打卡失败，可能已重复打卡"}};
                res.set_content(resp.dump(), "application/json");
            }
        } catch (const exception& e) {
            res.status = 400;
            json resp = {{"ok", false}, {"error", string("请求解析失败: ") + e.what()}};
            res.set_content(resp.dump(), "application/json");
        }
    });

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

    // ---------------- 提醒接口（真实数据库操作） ----------------
    svr.Get("/api/reminders", [&](const Request& req, Response& res) {
        REQUIRE_AUTH(user_id);
        string filter_type = req.has_param("type") ? req.get_param_value("type") : "";
        string data_str = reminder_list(user_id, filter_type);
        json resp = {{"ok", true}, {"data", json::parse(data_str)}};
        res.set_content(resp.dump(), "application/json");
    });

    svr.Post("/api/reminders/mark_read", [&](const Request& req, Response& res) {
        try {
            json body = json::parse(req.body);
            REQUIRE_AUTH(user_id);
            int reminder_id = body.value("reminder_id", 0);

            if (reminder_id == 0) {
                res.status = 400;
                res.set_content(R"({"ok":false,"error":"reminder_id is required"})", "application/json");
                return;
            }

            reminder_mark_read(reminder_id, user_id);
            json resp = {{"ok", true}, {"message", "marked read"}};
            res.set_content(resp.dump(), "application/json");
        } catch (const exception& e) {
            res.status = 400;
            json resp = {{"ok", false}, {"error", string("请求解析失败: ") + e.what()}};
            res.set_content(resp.dump(), "application/json");
        }
    });

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

    // ---------------- 番茄钟（Pomodoro） ----------------
    svr.Post("/api/pomodoro", [&](const Request& req, Response& res) {
        try {
            json body = json::parse(req.body);
            int user_id = body.value("user_id", 1);
            int duration = body.value("duration", 25);

            if (pomodoro_record(user_id, duration)) {
                res.set_content(R"({"ok":true,"message":"pomodoro recorded"})", "application/json");
            } else {
                res.status = 500;
                res.set_content(R"({"ok":false,"error":"record failed"})", "application/json");
            }
        } catch (const exception& e) {
            res.status = 400;
            json resp = {{"ok", false}, {"error", string("parse error: ") + e.what()}};
            res.set_content(resp.dump(), "application/json");
        }
    });

    svr.Get("/api/pomodoro/today", [&](const Request& req, Response& res) {
        int user_id = get_uid(req);
        string data_str = pomodoro_today(user_id);
        json resp = {{"ok", true}, {"data", json::parse(data_str)}};
        res.set_content(resp.dump(), "application/json");
    });

    // ---------------- 账号与用户 ----------------
    svr.Post("/api/auth/register",  handle_register);
    svr.Post("/api/auth/login",     handle_login);
    svr.Get("/api/me",              handle_get_me);
    svr.Get("/api/users/search",    handle_search_users);

    // ---------------- 好友与社交 ----------------
    // POST /api/friends/request → 发送好友申请
    svr.Post("/api/friends/request", [&](const Request& req, Response& res) {
        try {
            json body = json::parse(req.body);
            REQUIRE_AUTH(user_id);
            int to_id = body.value("to_id", 0);

            if (to_id <= 0) {
                res.status = 400;
                res.set_content(R"({"ok":false,"error":"缺少 to_id"})", "application/json");
                return;
            }

            if (friend_request(user_id, to_id)) {
                res.set_content(R"({"ok":true,"message":"好友申请已发送"})", "application/json");
            } else {
                res.status = 409;
                res.set_content(R"({"ok":false,"error":"好友关系已存在或申请失败"})", "application/json");
            }
        } catch (const exception& e) {
            res.status = 400;
            json resp = {{"ok", false}, {"error", string("请求解析失败: ") + e.what()}};
            res.set_content(resp.dump(), "application/json");
        }
    });

    // GET /api/friends → 好友列表
    svr.Get("/api/friends", [&](const Request& req, Response& res) {
        REQUIRE_AUTH(user_id);
        string data_str = friend_list(user_id);
        json resp = {{"ok", true}, {"data", json::parse(data_str)}};
        res.set_content(resp.dump(), "application/json");
    });

    // POST /api/friends/:id/handle → 通过/拒绝好友申请
    svr.Post(R"(/api/friends/(\d+)/handle)", [&](const Request& req, Response& res) {
        try {
            REQUIRE_AUTH(user_id);
            int friendship_id = stoi(req.matches[1]);
            json body = json::parse(req.body);
            int action = body.value("status", 0);  // 1=接受, 2=拒绝

            if (action != 1 && action != 2) {
                res.status = 400;
                json err = {{"ok", false}, {"error", "status must be 1(accept) or 2(reject)"}};
                res.set_content(err.dump(), "application/json");
                return;
            }

            if (friend_handle(friendship_id, action)) {
                res.set_content(R"({"ok":true,"message":"已处理"})", "application/json");
            } else {
                res.status = 404;
                res.set_content(R"({"ok":false,"error":"申请不存在或已处理"})", "application/json");
            }
        } catch (const exception& e) {
            res.status = 400;
            json resp = {{"ok", false}, {"error", string("请求解析失败: ") + e.what()}};
            res.set_content(resp.dump(), "application/json");
        }
    });

    // DELETE /api/friends/:id → 删除好友
    svr.Delete(R"(/api/friends/(\d+))", [&](const Request& req, Response& res) {
        REQUIRE_AUTH(user_id);
        int friendship_id = stoi(req.matches[1]);

        if (friend_delete(friendship_id, user_id)) {
            res.set_content(R"({"ok":true,"message":"已删除"})", "application/json");
        } else {
            res.status = 404;
            res.set_content(R"({"ok":false,"error":"好友关系不存在"})", "application/json");
        }
    });

    // ---------------- 聊天（Messages） ----------------
    svr.Post("/api/messages", [](const Request& req, Response& res) {
        res.set_content("{\"ok\":true}", "application/json");
    });
    svr.Get(R"(/api/messages/:friend_id)", [](const Request& req, Response& res) {
        res.set_content("{\"ok\":true,\"data\":[]}", "application/json");
    });
    svr.Get("/api/messages/unread_count", [](const Request& req, Response& res) {
        res.set_content("{\"ok\":true,\"count\":0}", "application/json");
    });

    // ---------------- 签到历史与连续天数 ----------------
    svr.Get("/api/signins", [&](const Request& req, Response& res) {
        REQUIRE_AUTH(user_id);
        string data_str = signin_history(user_id);
        json resp = {{"ok", true}, {"data", json::parse(data_str)}};
        res.set_content(resp.dump(), "application/json");
    });
    svr.Post("/api/signins", [&](const Request& req, Response& res) {
        try {
            json body = json::parse(req.body);
            REQUIRE_AUTH(user_id);
            string date = body.value("date", "");

            if (date.empty()) {
                res.status = 400;
                res.set_content(R"({"ok":false,"error":"date is required"})", "application/json");
                return;
            }

            signin_do(user_id, date);  // 如果重复签到会被 UNIQUE 约束忽略
            int streak = signin_streak(user_id);
            json resp = {{"ok", true}, {"data", {{"streak", streak}}}};
            res.set_content(resp.dump(), "application/json");
        } catch (const exception& e) {
            res.status = 400;
            json resp = {{"ok", false}, {"error", string("请求解析失败: ") + e.what()}};
            res.set_content(resp.dump(), "application/json");
        }
    });

    // ---------------- 打卡记录（更细粒度） ----------------
    svr.Get("/api/checkins", [&](const Request& req, Response& res) {
        REQUIRE_AUTH(user_id);
        string date = req.has_param("date") ? req.get_param_value("date") : "";
        string data_str = checkin_get(user_id, date);
        json resp = {{"ok", true}, {"data", json::parse(data_str)}};
        res.set_content(resp.dump(), "application/json");
    });
    svr.Post("/api/checkins", [&](const Request& req, Response& res) {
        try {
            json body = json::parse(req.body);
            REQUIRE_AUTH(user_id);
            int task_id = body.value("task_id", 0);
            string date = body.value("date", "");

            if (task_id == 0 || date.empty()) {
                res.status = 400;
                res.set_content(R"({"ok":false,"error":"task_id and date are required"})", "application/json");
                return;
            }

            if (checkin_do(user_id, task_id, date)) {
                json resp = {{"ok", true}, {"message", "checkin record created"}};
                res.set_content(resp.dump(), "application/json");
            } else {
                res.status = 409;
                json resp = {{"ok", false}, {"error", "打卡记录创建失败"}};
                res.set_content(resp.dump(), "application/json");
            }
        } catch (const exception& e) {
            res.status = 400;
            json resp = {{"ok", false}, {"error", string("请求解析失败: ") + e.what()}};
            res.set_content(resp.dump(), "application/json");
        }
    });

    // ---------------- 名言（Quotes） ----------------
    svr.Get("/api/quotes/random", [](const Request& req, Response& res) {
        string data_str = quote_random();
        json resp = {{"ok", true}, {"data", json::parse(data_str)}};
        res.set_content(resp.dump(), "application/json");
    });

    // 5. 设置前端静态文件目录
    svr.set_mount_point("/", "../frontend");

    // 6. 关掉 httplib 自带的日志
    svr.set_logger(nullptr);

    // 7. 启动服务器
    cout << "Open in browser: http://localhost:8080" << endl;
    svr.listen("0.0.0.0", 8080);

    return 0;
}
