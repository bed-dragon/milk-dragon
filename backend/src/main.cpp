#include "httplib.h"
#include "db.h"

using namespace httplib;
using namespace std;

int main() {
    // 1. 初始化数据库（建表）
    init_tables();

    // 2. 创建 HTTP 服务器
    Server svr;

    // 3. 注册路由 —— 每个路径对应一个处理函数
    //    前端 fetch("http://localhost:8080/api/hello") 就会到这里
    svr.Get("/api/hello", [](const Request& req, Response& res) {
        // 返回一个简单的 JSON，前端收到就证明通了
        res.set_content(R"({"ok":true,"msg":"hello, 后端已启动!"})", "application/json");
    });

    // 4. 关掉 httplib 自带的日志（不然每次请求都刷屏）
    svr.set_logger(nullptr);

    // 5. 启动服务器，监听 8080 端口
    cout << "🚀 服务器启动: http://localhost:8080" << endl;
    svr.listen("0.0.0.0", 8080);

    return 0;
}
