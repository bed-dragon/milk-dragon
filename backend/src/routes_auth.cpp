#include "routes_auth.h"
#include "db.h"
#include "json.hpp"
#include "../libs/sha256.h"
#include <ctime>
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
    string nickname = body.value("nickname", username);

    if (username.length() > 90) {
        res.status = 400; res.set_content(R"({"ok":false,"error":"用户名最多90个字符"})", "application/json"); return;
    }
    if (nickname.length() > 90) {
        res.status = 400; res.set_content(R"({"ok":false,"error":"昵称最多90个字符"})", "application/json"); return;
    }  // 没传昵称就用用户名

    int uid = user_create(username, password, nickname);

    json resp;
    if (uid > 0) {
        // 注册成功后自动生成 token（即自动登录）
        string token = sha256(username + to_string(time(nullptr)));
        // 更新 token 到数据库
        sqlite3* db = open_db();
        if (db) {
            const char* upd = "UPDATE users SET token=? WHERE id=?";
            sqlite3_stmt* stmt = nullptr;
            sqlite3_prepare_v2(db, upd, -1, &stmt, 0);
            sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 2, uid);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            sqlite3_close(db);
        }
        resp["ok"] = true;
        resp["data"]["user_id"] = uid;
        resp["data"]["token"] = token;
        resp["data"]["username"] = username;
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
    string token = req.get_header_value("Authorization");
    if (token.rfind("Bearer ", 0) != 0) {
        res.status = 401; res.set_content(R"({"ok":false,"error":"请先登录"})", "application/json"); return;
    }
    string q = req.get_param_value("q");
    string data = user_search(q);

    json resp;
    resp["ok"] = true;
    resp["data"] = json::parse(data);
    res.set_content(resp.dump(), "application/json");
} CATCH_ERR }

// PUT /api/me — 更新个人资料
void handle_update_profile(const Request& req, Response& res) { try {
    string auth = req.get_header_value("Authorization");
    string token = (auth.size() > 7) ? auth.substr(7) : "";
    int uid = user_id_by_token(token);
    if (uid <= 0) { res.status = 401; res.set_content(R"({"ok":false,"error":"请先登录"})", "application/json"); return; }
    json body = json::parse(req.body);
    string nickname  = body.value("nickname", "");
    string signature = body.value("signature", "");

    if (nickname.length() > 90) {
        res.status = 400; res.set_content(R"({"ok":false,"error":"昵称最多90个字符"})", "application/json"); return;
    }
    if (signature.length() > 100) {
        res.status = 400; res.set_content(R"({"ok":false,"error":"签名最多100个字符"})", "application/json"); return;
    }

    bool ok = user_update_profile(uid, nickname, signature);
    json resp;
    resp["ok"] = ok;
    res.set_content(resp.dump(), "application/json");
} CATCH_ERR }

// PUT /api/me/password — 修改密码
void handle_change_password(const Request& req, Response& res) { try {
    string auth = req.get_header_value("Authorization");
    string token = (auth.size() > 7) ? auth.substr(7) : "";
    int uid = user_id_by_token(token);
    if (uid <= 0) { res.status = 401; res.set_content(R"({"ok":false,"error":"请先登录"})", "application/json"); return; }
    json body = json::parse(req.body);
    string old_pwd = body["old_password"];
    string new_pwd = body["new_password"];
    bool ok = user_change_password(uid, old_pwd, new_pwd);
    json resp;
    resp["ok"] = ok;
    if (!ok) resp["msg"] = "旧密码错误";
    res.set_content(resp.dump(), "application/json");
} CATCH_ERR }
