#include "routes_tasks.h"
#include "db.h"
#include "json.hpp"
using namespace std;
using json = nlohmann::json;

#define CATCH_ERR catch(exception& e) { \
    cerr << "[ERROR] " << e.what() << endl; \
    res.set_content(string(R"({"ok":false,"error":")") + e.what() + "\"}", "application/json"); \
}

// GET /api/tasks
void handle_get_tasks(const Request& req, Response& res) { try {
    auto uid = req.get_param_value("user_id");
    int user_id = uid.empty() ? 1 : stoi(uid);
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
    int user_id = body.value("user_id", 1);
    int new_id = task_create(user_id, t);
    json resp;
    resp["ok"] = (new_id > 0);
    resp["data"]["id"] = new_id;
    res.set_content(resp.dump(), "application/json");
} CATCH_ERR }

// PUT /api/tasks/1
void handle_update_task(const Request& req, Response& res) { try {
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
    int user_id = body.value("user_id", 1);
    bool ok = task_update(task_id, user_id, t);
    json resp;
    resp["ok"] = ok;
    res.set_content(resp.dump(), "application/json");
} CATCH_ERR }

// DELETE /api/tasks/1
void handle_delete_task(const Request& req, Response& res) { try {
    int task_id = stoi(req.matches[1]);
    auto uid = req.get_param_value("user_id");
    int user_id = uid.empty() ? 1 : stoi(uid);
    bool ok = task_delete(task_id, user_id);
    json resp;
    resp["ok"] = ok;
    res.set_content(resp.dump(), "application/json");
} CATCH_ERR }
