#include "routes_tasks.h"
#include "db.h"
#include "json.hpp"
using namespace std;
using json = nlohmann::json;

// ============================================================
//  routes_tasks.cpp — 任务 HTTP 处理函数
//  只管：解析 JSON → 调 db.cpp → 返回 JSON
//  不管：SQL 怎么写（那是 db.cpp 的事）
//  不管：路由怎么注册（那是 main.cpp 的事）
// ============================================================

// GET /api/tasks?user_id=1
void handle_get_tasks(const Request& req, Response& res) {
    int user_id = stoi(req.get_param_value("user_id"));
    string data = task_get_list(user_id);

    json resp;
    resp["ok"] = true;
    resp["data"] = json::parse(data);
    res.set_content(resp.dump(), "application/json");
}

// GET /api/tasks/1
void handle_get_one_task(const Request& req, Response& res) {
    int task_id = stoi(req.matches[1]);
    string data = task_get_one(task_id);

    json resp;
    resp["ok"] = true;
    resp["data"] = json::parse(data);
    res.set_content(resp.dump(), "application/json");
}

// POST /api/tasks
void handle_create_task(const Request& req, Response& res) {
    json body = json::parse(req.body);

    Task t;
    t.title       = body["title"];
    t.topic       = body.value("topic", "");
    t.deadline    = body.value("deadline", "");
    t.priority    = body.value("priority", 1);
    t.need_review = body.value("need_review", false);

    int new_id = task_create(body["user_id"], t);

    json resp;
    resp["ok"] = (new_id > 0);
    resp["data"]["id"] = new_id;
    res.set_content(resp.dump(), "application/json");
}

// PUT /api/tasks/1
void handle_update_task(const Request& req, Response& res) {
    int task_id = stoi(req.matches[1]);
    json body = json::parse(req.body);

    Task t;
    t.title       = body.value("title", "");
    t.topic       = body.value("topic", "");
    t.deadline    = body.value("deadline", "");
    t.priority    = body.value("priority", 1);
    t.status      = body.value("status", 0);
    t.need_review = body.value("need_review", false);

    bool ok = task_update(task_id, body["user_id"], t);

    json resp;
    resp["ok"] = ok;
    res.set_content(resp.dump(), "application/json");
}

// DELETE /api/tasks/1?user_id=1
void handle_delete_task(const Request& req, Response& res) {
    int task_id = stoi(req.matches[1]);
    int user_id = stoi(req.get_param_value("user_id"));

    bool ok = task_delete(task_id, user_id);

    json resp;
    resp["ok"] = ok;
    res.set_content(resp.dump(), "application/json");
}
