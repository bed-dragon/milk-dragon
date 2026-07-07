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

    // 5. 关掉 httplib 自带的日志
    svr.set_logger(nullptr);

    // 6. 启动服务器
    cout << "服务器启动: http://localhost:8080" << endl;
    cout << "测试接口: http://localhost:8080/api/hello" << endl;
    svr.listen("0.0.0.0", 8080);

    return 0;
}
