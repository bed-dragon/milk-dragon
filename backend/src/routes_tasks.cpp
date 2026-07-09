#include "routes_tasks.h"
#include "db.h"
#include "json.hpp"
using namespace std;
using json = nlohmann::json;

// 从 Authorization Bearer token 解析 user_id（声明自 db.h）
extern int user_id_by_token(const string& token);

// 辅助：从请求中解析 user_id，失败返回 -1
static int get_uid_from_req(const Request& req) {
    if (req.has_header("Authorization")) {
        string auth = req.get_header_value("Authorization");
        if (auth.rfind("Bearer ", 0) == 0) {
            return user_id_by_token(auth.substr(7));
        }
    }
    // 兼容 query param
    auto uid = req.get_param_value("user_id");
    if (!uid.empty()) return stoi(uid);
    return 1;  // 默认用户 ID
}

#define CATCH_ERR catch(exception& e) { \
    cerr << "[ERROR] " << e.what() << endl; \
    res.set_content(string(R"({"ok":false,"error":")") + e.what() + "\"}", "application/json"); \
}

// GET /api/tasks
void handle_get_tasks(const Request& req, Response& res) { try {
    int user_id = get_uid_from_req(req);
    if (user_id <= 0) { res.status = 401; res.set_content(R"({"ok":false,"error":"请先登录"})", "application/json"); return; }
    string data = task_get_list(user_id);
    json resp;
    resp["ok"] = true;
    resp["data"] = json::parse(data);
    res.set_content(resp.dump(), "application/json");
} CATCH_ERR }

// GET /api/tasks/1
void handle_get_one_task(const Request& req, Response& res) { try {
    int task_id = stoi(req.matches[1]);
    string data = task_get_one(task_id);
    json resp;
    resp["ok"] = true;
    resp["data"] = json::parse(data);
    res.set_content(resp.dump(), "application/json");
} CATCH_ERR }

// POST /api/tasks
void handle_create_task(const Request& req, Response& res) { try {
    int user_id = get_uid_from_req(req);
    if (user_id <= 0) { res.status = 401; res.set_content(R"({"ok":false,"error":"请先登录"})", "application/json"); return; }
    json body = json::parse(req.body);
    Task t;
    t.title       = body["title"];
    t.topic       = body.value("topic", "");
    t.deadline    = body.value("deadline", "");
    t.priority    = body.value("priority", 1);
    // 兼容前端发 0/1（整数）和 true/false（布尔）
    if (body.contains("need_review")) {
        auto& nr = body["need_review"];
        if (nr.is_boolean())    t.need_review = nr.get<bool>();
        else if (nr.is_number()) t.need_review = (nr.get<int>() != 0);
    }
    int new_id = task_create(user_id, t);
    json resp;
    resp["ok"] = (new_id > 0);
    resp["data"]["id"] = new_id;
    res.set_content(resp.dump(), "application/json");
} CATCH_ERR }

// PUT /api/tasks/1
void handle_update_task(const Request& req, Response& res) { try {
    int user_id = get_uid_from_req(req);
    if (user_id <= 0) { res.status = 401; res.set_content(R"({"ok":false,"error":"请先登录"})", "application/json"); return; }
    int task_id = stoi(req.matches[1]);
    json body = json::parse(req.body);
    Task t;
    t.title       = body.value("title", "");
    t.topic       = body.value("topic", "");
    t.deadline    = body.value("deadline", "");
    t.priority    = body.value("priority", 1);
    t.status      = body.value("status", 0);
    if (body.contains("need_review")) {
        auto& nr = body["need_review"];
        if (nr.is_boolean())    t.need_review = nr.get<bool>();
        else if (nr.is_number()) t.need_review = (nr.get<int>() != 0);
    }
    bool ok = task_update(task_id, user_id, t);
    json resp;
    resp["ok"] = ok;
    res.set_content(resp.dump(), "application/json");
} CATCH_ERR }

// DELETE /api/tasks/1
void handle_delete_task(const Request& req, Response& res) { try {
    int user_id = get_uid_from_req(req);
    if (user_id <= 0) { res.status = 401; res.set_content(R"({"ok":false,"error":"请先登录"})", "application/json"); return; }
    int task_id = stoi(req.matches[1]);
    bool ok = task_delete(task_id, user_id);
    json resp;
    resp["ok"] = ok;
    res.set_content(resp.dump(), "application/json");
} CATCH_ERR }
