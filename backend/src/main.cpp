#define _WIN32_WINNT 0x0A00   // Windows 10+，httplib 编译需要
#include "httplib.h"
#include "db.h"

using namespace httplib;
using namespace std;

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

    // ---------------- 任务接口（空壳） ----------------
    svr.Get("/api/tasks", [](const Request& req, Response& res) {
        res.set_content("{\"ok\":true,\"message\":\"list tasks (stub)\",\"data\":[]}", "application/json");
    });
    svr.Post("/api/tasks", [](const Request& req, Response& res) {
        res.set_content("{\"ok\":true,\"message\":\"create task (stub)\",\"data\":{\"id\":1}}", "application/json");
    });
    svr.Put(R"(/api/tasks/:id)", [](const Request& req, Response& res) {
        res.set_content("{\"ok\":true,\"message\":\"update task (stub)\",\"data\":{}}", "application/json");
    });
    svr.Delete(R"(/api/tasks/:id)", [](const Request& req, Response& res) {
        res.set_content("{\"ok\":true,\"message\":\"delete task (stub)\",\"data\":{}}", "application/json");
    });

    // ---------------- 签到接口（空壳） ----------------
    svr.Get("/api/checkin", [](const Request& req, Response& res) {
        res.set_content("{\"ok\":true,\"message\":\"checkin list (stub)\",\"data\":[]}", "application/json");
    });
    svr.Post("/api/checkin", [](const Request& req, Response& res) {
        res.set_content("{\"ok\":true,\"message\":\"do checkin (stub)\",\"data\":{\"checked\":true}}", "application/json");
    });

    // ---------------- 统计接口（空壳，匹配 /api/stats/*） ----------------
    svr.Get(R"(/api/stats/.*)", [](const Request& req, Response& res) {
        res.set_content("{\"ok\":true,\"message\":\"stats (stub)\",\"data\":{\"summary\":{}}}", "application/json");
    });

    // ---------------- 提醒接口（空壳） ----------------
    svr.Get("/api/reminders", [](const Request& req, Response& res) {
        res.set_content("{\"ok\":true,\"message\":\"reminders (stub)\",\"data\":[]}", "application/json");
    });

    // ---------------- 推荐任务（系统推荐） ----------------
    svr.Get("/api/recommended_tasks", [](const Request& req, Response& res) {
        res.set_content("{\"ok\":true,\"data\":[{\"id\":1,\"title\":\"Learn 50 words\"}]}", "application/json");
    });

    // ---------------- 收藏学习资料（materials） ----------------
    svr.Get("/api/materials", [](const Request& req, Response& res) {
        res.set_content("{\"ok\":true,\"data\":[]}", "application/json");
    });
    svr.Post("/api/materials", [](const Request& req, Response& res) {
        res.set_content("{\"ok\":true,\"message\":\"material added (stub)\",\"data\":{\"id\":1}}", "application/json");
    });
    svr.Delete(R"(/api/materials/:id)", [](const Request& req, Response& res) {
        res.set_content("{\"ok\":true}", "application/json");
    });

    // ---------------- 番茄钟（Pomodoro） ----------------
    svr.Post("/api/pomodoro", [](const Request& req, Response& res) {
        res.set_content("{\"ok\":true,\"message\":\"pomodoro recorded (stub)\"}", "application/json");
    });
    svr.Get("/api/pomodoro/today", [](const Request& req, Response& res) {
        res.set_content("{\"ok\":true,\"data\":[]}", "application/json");
    });

    // ---------------- 账号与用户 ----------------
    svr.Post("/api/auth/register", [](const Request& req, Response& res) {
        res.set_content("{\"ok\":true,\"message\":\"registered (stub)\",\"data\":{\"user_id\":1}}", "application/json");
    });
    svr.Post("/api/auth/login", [](const Request& req, Response& res) {
        res.set_content("{\"ok\":true,\"token\":\"stub-token\"}", "application/json");
    });
    svr.Get(R"(/api/users/:id)", [](const Request& req, Response& res) {
        res.set_content("{\"ok\":true,\"data\":{\"id\":1,\"username\":\"user\"}}", "application/json");
    });

    // ---------------- 好友与社交 ----------------
    svr.Post("/api/friends/request", [](const Request& req, Response& res) {
        res.set_content("{\"ok\":true}", "application/json");
    });
    svr.Get("/api/friends", [](const Request& req, Response& res) {
        res.set_content("{\"ok\":true,\"data\":[]}", "application/json");
    });
    svr.Post(R"(/api/friends/:id/handle)", [](const Request& req, Response& res) {
        res.set_content("{\"ok\":true}", "application/json");
    });
    svr.Delete(R"(/api/friends/:id)", [](const Request& req, Response& res) {
        res.set_content("{\"ok\":true}", "application/json");
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
    svr.Get("/api/signins", [](const Request& req, Response& res) {
        res.set_content("{\"ok\":true,\"data\":[]}", "application/json");
    });
    svr.Post("/api/signins", [](const Request& req, Response& res) {
        res.set_content("{\"ok\":true,\"message\":\"signin (stub)\",\"data\":{\"streak\":1}}", "application/json");
    });

    // ---------------- 打卡记录（更细粒度） ----------------
    svr.Get("/api/checkins", [](const Request& req, Response& res) {
        res.set_content("{\"ok\":true,\"data\":[]}", "application/json");
    });
    svr.Post("/api/checkins", [](const Request& req, Response& res) {
        res.set_content("{\"ok\":true,\"message\":\"create checkin record (stub)\"}", "application/json");
    });

    // ---------------- 名言（Quotes） ----------------
    svr.Get("/api/quotes/random", [](const Request& req, Response& res) {
        res.set_content("{\"ok\":true,\"data\":{\"content\":\"学习使人进步\",\"author\":\"佚名\"}}", "application/json");
    });

    // 5. 关掉 httplib 自带的日志
    svr.set_logger(nullptr);

    // 6. 启动服务器
    cout << "服务器启动: http://localhost:8080" << endl;
    cout << "测试接口: http://localhost:8080/api/hello" << endl;
    svr.listen("0.0.0.0", 8080);

    return 0;
}
