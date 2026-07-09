#include "routes_auth.h"
#include "db.h"
#include "json.hpp"
using namespace std;
using json = nlohmann::json;

#define CATCH_ERR catch(exception& e) { \
    cerr << "[ERROR] " << e.what() << endl; \
    res.set_content(string(R"({"ok":false,"error":")") + e.what() + "\"}", "application/json"); \
}

// POST /api/auth/register
void handle_register(const Request& req, Response& res) { try {
    json body = json::parse(req.body);
    string username = body["username"];
    string password = body["password"];

    int uid = user_create(username, password);

    json resp;
    if (uid > 0) {
        resp["ok"] = true;
        resp["data"]["user_id"] = uid;
    } else {
        resp["ok"] = false;
        resp["msg"] = "用户名已存在";
    }
    res.set_content(resp.dump(), "application/json");
} CATCH_ERR }

// POST /api/auth/login
void handle_login(const Request& req, Response& res) { try {
    json body = json::parse(req.body);
    string username = body["username"];
    string password = body["password"];

    string data = user_login(username, password);
    json result = json::parse(data);

    json resp;
    if (result.contains("token")) {
        resp["ok"] = true;
        resp["token"] = result["token"];
        resp["user_id"] = result["user_id"];
    } else {
        resp["ok"] = false;
        resp["msg"] = "用户名或密码错误";
    }
    res.set_content(resp.dump(), "application/json");
} CATCH_ERR }

// GET /api/me
void handle_get_me(const Request& req, Response& res) { try {
    // 从请求头取 token，去掉 "Bearer " 前缀
    string auth = req.get_header_value("Authorization");
    string token = (auth.size() > 7) ? auth.substr(7) : "";

    int uid = user_id_by_token(token);
    json resp;
    if (uid > 0) {
        resp["ok"] = true;
        resp["data"] = json::parse(user_get_info(uid));
    } else {
        resp["ok"] = false;
        resp["msg"] = "未登录或 token 已过期";
    }
    res.set_content(resp.dump(), "application/json");
} CATCH_ERR }

// GET /api/users/search?q=关键词
void handle_search_users(const Request& req, Response& res) { try {
    string q = req.get_param_value("q");
    string data = user_search(q);

    json resp;
    resp["ok"] = true;
    resp["data"] = json::parse(data);
    res.set_content(resp.dump(), "application/json");
} CATCH_ERR }
