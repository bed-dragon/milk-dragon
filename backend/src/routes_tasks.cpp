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
    return -1;  // 未认证
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
    int user_id = get_uid_from_req(req);
    if (user_id <= 0) { res.status = 401; res.set_content(R"({"ok":false,"error":"请先登录"})", "application/json"); return; }
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
    if (t.title.length() > 50) {
        res.status = 400; res.set_content(R"({"ok":false,"error":"任务标题最多50个字符"})", "application/json"); return;
    }
    if (t.topic.length() > 50) {
        res.status = 400; res.set_content(R"({"ok":false,"error":"主题最多50个字符"})", "application/json"); return;
    }
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
    if (t.title.length() > 50) {
        res.status = 400; res.set_content(R"({"ok":false,"error":"任务标题最多50个字符"})", "application/json"); return;
    }
    if (t.topic.length() > 50) {
        res.status = 400; res.set_content(R"({"ok":false,"error":"主题最多50个字符"})", "application/json"); return;
    }
    t.deadline    = body.value("deadline", "");
    t.priority    = body.value("priority", 1);
    // 只有前端显式传了 status 才更新，否则保留原状态（防打卡状态被覆盖）
    if (body.contains("status"))
        t.status = body["status"];
    else
        t.status = -1;  // -1 = 不更新此字段
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

// ========== 收藏任务模板 ==========

extern int user_id_by_token(const string& token);
static int get_uid(const Request& req) {
    if (req.has_header("Authorization")) {
        string auth = req.get_header_value("Authorization");
        if (auth.rfind("Bearer ", 0) == 0)
            return user_id_by_token(auth.substr(7));
    }
    auto uid = req.get_param_value("user_id");
    return uid.empty() ? -1 : stoi(uid);
}

// POST /api/favorite_tasks
void handle_favorite_task_add(const Request& req, Response& res) { try {
    int user_id = get_uid(req);
    if (user_id <= 0) { res.status = 401; res.set_content(R"({"ok":false,"error":"请先登录"})", "application/json"); return; }
    json body = json::parse(req.body);
    Task t;
    t.title       = body["title"];
    t.topic       = body.value("topic", "");
    t.deadline    = body.value("deadline", "");
    t.priority    = body.value("priority", 1);
    t.need_review = body.value("need_review", 0) != 0;
    bool ok = favorite_task_add(user_id, t);
    json resp;
    resp["ok"] = ok;
    res.set_content(resp.dump(), "application/json");
} CATCH_ERR }

// GET /api/favorite_tasks
void handle_favorite_task_list(const Request& req, Response& res) { try {
    int user_id = get_uid(req);
    if (user_id <= 0) { res.status = 401; res.set_content(R"({"ok":false,"error":"请先登录"})", "application/json"); return; }
    string data = favorite_task_list(user_id);
    json resp;
    resp["ok"] = true;
    resp["data"] = json::parse(data);
    res.set_content(resp.dump(), "application/json");
} CATCH_ERR }

// DELETE /api/favorite_tasks/:id
void handle_favorite_task_delete(const Request& req, Response& res) { try {
    int user_id = get_uid(req);
    if (user_id <= 0) { res.status = 401; res.set_content(R"({"ok":false,"error":"请先登录"})", "application/json"); return; }
    int fav_id = stoi(req.matches[1]);
    bool ok = favorite_task_delete(fav_id, user_id);
    json resp;
    resp["ok"] = ok;
    res.set_content(resp.dump(), "application/json");
} CATCH_ERR }
