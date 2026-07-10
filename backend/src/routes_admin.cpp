#include "routes_admin.h"
#include "db.h"
#include "json.hpp"
#include <iostream>
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

#define REQUIRE_ADMIN(uid_var) \
    REQUIRE_AUTH(uid_var) \
    if (!user_is_admin(uid_var)) { \
        res.status = 403; \
        res.set_content(R"({"ok":false,"error":"需要管理员权限"})", "application/json"); \
        return; \
    }

#define CATCH_ERR catch(exception& e) { \
    cerr << "[ERROR] " << e.what() << endl; \
    res.set_content(string(R"({"ok":false,"error":")") + e.what() + "\"}", "application/json"); \
}

// ============================================================
// 用户管理
// ============================================================

// GET /api/admin/users
void handle_admin_users(const Request& req, Response& res) {
    REQUIRE_ADMIN(uid);
    string data = user_list_all();
    json resp = {{"ok", true}, {"data", json::parse(data)}};
    res.set_content(resp.dump(), "application/json");
}

// PUT /api/admin/users/:id/role
void handle_admin_user_role(const Request& req, Response& res) { try {
    REQUIRE_ADMIN(uid);
    int target_id = stoi(req.matches[1]);
    json body = json::parse(req.body);
    string role = body.value("role", "user");

    if (role != "admin" && role != "user") {
        res.status = 400;
        res.set_content(R"({"ok":false,"error":"role 必须是 admin 或 user"})", "application/json");
        return;
    }

    if (user_update_role(target_id, role))
        res.set_content(R"({"ok":true,"message":"角色已更新"})", "application/json");
    else {
        res.status = 404;
        res.set_content(R"({"ok":false,"error":"用户不存在"})", "application/json");
    }
} CATCH_ERR }

// DELETE /api/admin/users/:id
void handle_admin_user_delete(const Request& req, Response& res) {
    REQUIRE_ADMIN(uid);
    int target_id = stoi(req.matches[1]);

    // 不能删除自己
    if (target_id == uid) {
        res.status = 400;
        res.set_content(R"({"ok":false,"error":"不能删除自己"})", "application/json");
        return;
    }

    if (user_delete(target_id))
        res.set_content(R"({"ok":true,"message":"用户已删除"})", "application/json");
    else {
        res.status = 404;
        res.set_content(R"({"ok":false,"error":"用户不存在"})", "application/json");
    }
}

// ============================================================
// 推荐任务库管理
// ============================================================

// GET /api/admin/recommended
void handle_admin_recommended(const Request& req, Response& res) {
    REQUIRE_ADMIN(uid);
    string data = task_recommended_all();
    json resp = {{"ok", true}, {"data", json::parse(data)}};
    res.set_content(resp.dump(), "application/json");
}

// POST /api/admin/recommended
void handle_admin_recommended_add(const Request& req, Response& res) { try {
    REQUIRE_ADMIN(uid);
    json body = json::parse(req.body);
    string title = body.value("title", "");
    string topic = body.value("topic", "");
    int priority = body.value("priority", 1);

    if (title.empty()) {
        res.status = 400;
        res.set_content(R"({"ok":false,"error":"标题不能为空"})", "application/json");
        return;
    }

    if (task_recommended_add(title, topic, priority))
        res.set_content(R"({"ok":true,"message":"推荐任务已添加"})", "application/json");
    else {
        res.status = 500;
        res.set_content(R"({"ok":false,"error":"添加失败"})", "application/json");
    }
} CATCH_ERR }

// PUT /api/admin/recommended/:id
void handle_admin_recommended_update(const Request& req, Response& res) { try {
    REQUIRE_ADMIN(uid);
    int id = stoi(req.matches[1]);
    json body = json::parse(req.body);
    string title = body.value("title", "");
    string topic = body.value("topic", "");
    int priority = body.value("priority", 1);

    if (task_recommended_update(id, title, topic, priority))
        res.set_content(R"({"ok":true,"message":"推荐任务已更新"})", "application/json");
    else {
        res.status = 404;
        res.set_content(R"({"ok":false,"error":"推荐任务不存在"})", "application/json");
    }
} CATCH_ERR }

// DELETE /api/admin/recommended/:id
void handle_admin_recommended_delete(const Request& req, Response& res) {
    REQUIRE_ADMIN(uid);
    int id = stoi(req.matches[1]);

    if (task_recommended_delete(id))
        res.set_content(R"({"ok":true,"message":"推荐任务已删除"})", "application/json");
    else {
        res.status = 404;
        res.set_content(R"({"ok":false,"error":"推荐任务不存在"})", "application/json");
    }
}

// ============================================================
// 系统统计
// ============================================================

// GET /api/admin/stats
void handle_admin_stats(const Request& req, Response& res) {
    REQUIRE_ADMIN(uid);
    string data = stats_system();
    json resp = {{"ok", true}, {"data", json::parse(data)}};
    res.set_content(resp.dump(), "application/json");
}
