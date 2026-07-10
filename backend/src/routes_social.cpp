#include "routes_social.h"
#include "db.h"
#include "json.hpp"
using namespace std;
using json = nlohmann::json;

extern int user_id_by_token(const string& token);
static int get_uid(const Request& req) {
    if (req.has_header("Authorization")) {
        string auth = req.get_header_value("Authorization");
        if (auth.rfind("Bearer ", 0) == 0)
            return user_id_by_token(auth.substr(7));
    }
    return -1;  // 未认证
}

#define REQUIRE_AUTH(uid_var) \
    int uid_var = get_uid(req); \
    if (uid_var <= 0) { \
        res.status = 401; \
        res.set_content(R"({"ok":false,"error":"请先登录"})", "application/json"); \
        return; \
    }

#define CATCH_ERR catch(exception& e) { \
    cerr << "[ERROR] " << e.what() << endl; \
    res.set_content(string(R"({"ok":false,"error":")") + e.what() + "\"}", "application/json"); \
}

// ========== 好友 ==========

// POST /api/friends/request
void handle_friend_request(const Request& req, Response& res) { try {
    json body = json::parse(req.body);
    REQUIRE_AUTH(user_id);
    int to_id = body.value("to_id", 0);
    if (to_id <= 0) {
        res.status = 400;
        res.set_content(R"({"ok":false,"error":"缺少 to_id"})", "application/json");
        return;
    }
    if (friend_request(user_id, to_id))
        res.set_content(R"({"ok":true,"message":"好友申请已发送"})", "application/json");
    else {
        res.status = 409;
        res.set_content(R"({"ok":false,"error":"好友关系已存在或申请失败"})", "application/json");
    }
} CATCH_ERR }

// GET /api/friends
void handle_friend_list(const Request& req, Response& res) {
    REQUIRE_AUTH(user_id);
    string data = friend_list(user_id);
    json resp = {{"ok", true}, {"data", json::parse(data)}};
    res.set_content(resp.dump(), "application/json");
}

// POST /api/friends/:id/handle
void handle_friend_action(const Request& req, Response& res) { try {
    REQUIRE_AUTH(user_id);
    int friendship_id = stoi(req.matches[1]);
    json body = json::parse(req.body);
    int action = body.value("status", 0);
    if (action != 1 && action != 2) {
        res.status = 400;
        json err = {{"ok", false}, {"error", "status must be 1(accept) or 2(reject)"}};
        res.set_content(err.dump(), "application/json");
        return;
    }
    if (friend_handle(friendship_id, action))
        res.set_content(R"({"ok":true,"message":"已处理"})", "application/json");
    else {
        res.status = 404;
        res.set_content(R"({"ok":false,"error":"申请不存在或已处理"})", "application/json");
    }
} CATCH_ERR }

// DELETE /api/friends/:id
void handle_friend_delete(const Request& req, Response& res) {
    REQUIRE_AUTH(user_id);
    int friendship_id = stoi(req.matches[1]);
    if (friend_delete(friendship_id, user_id))
        res.set_content(R"({"ok":true,"message":"已删除"})", "application/json");
    else {
        res.status = 404;
        res.set_content(R"({"ok":false,"error":"删除失败"})", "application/json");
    }
}

// ========== 聊天 ==========

// POST /api/messages
void handle_send_message(const Request& req, Response& res) { try {
    json body = json::parse(req.body);
    REQUIRE_AUTH(user_id);
    int to_id = body.value("to_id", 0);
    string content = body.value("content", "");
    if (to_id <= 0 || content.empty()) {
        res.status = 400;
        res.set_content(R"({"ok":false,"error":"缺少 to_id 或 content"})", "application/json");
        return;
    }
    if (content.length() > 3000) {
        res.status = 400;
        res.set_content(R"({"ok":false,"error":"消息内容过长（最多3000字）"})", "application/json");
        return;
    }
    if (message_send(user_id, to_id, content))
        res.set_content(R"({"ok":true})", "application/json");
    else {
        res.status = 500;
        res.set_content(R"({"ok":false,"error":"发送失败"})", "application/json");
    }
} CATCH_ERR }

// GET /api/messages/:friend_id
void handle_get_messages(const Request& req, Response& res) {
    REQUIRE_AUTH(user_id);
    int friend_id = stoi(req.matches[1]);
    string data = message_history(user_id, friend_id);
    json resp = {{"ok", true}, {"data", json::parse(data)}};
    res.set_content(resp.dump(), "application/json");
}

// GET /api/messages/unread_count
void handle_unread_count(const Request& req, Response& res) {
    REQUIRE_AUTH(user_id);
    int count = message_unread_count(user_id);
    json resp = {{"ok", true}, {"count", count}};
    res.set_content(resp.dump(), "application/json");
}
