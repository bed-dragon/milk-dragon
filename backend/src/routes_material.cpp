#include "routes_material.h"
#include "db.h"
#include "json.hpp"
using namespace std;
using json = nlohmann::json;

// 从 token 或查询参数中取 user_id（抄 routes_tasks.cpp 的）
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

#define CATCH_ERR catch(exception& e) { \
    cerr << "[ERROR] " << e.what() << endl; \
    res.set_content(string(R"({"ok":false,"error":")") + e.what() + "\"}", "application/json"); \
}

// GET /api/materials
void handle_get_materials(const Request& req, Response& res) { try {
    int user_id = get_uid(req);
    if (user_id <= 0) {                          // ← 改：不再默认1，直接拒绝
        res.status = 401;
        res.set_content(R"({"ok":false,"error":"请先登录"})", "application/json");
        return;
    }
    string data = material_list(user_id);
    json resp;
    resp["ok"] = true;
    resp["data"] = json::parse(data);
    res.set_content(resp.dump(), "application/json");
} CATCH_ERR }

// POST /api/materials
void handle_add_material(const Request& req, Response& res) { try {
    int user_id = get_uid(req);                  // ← 改：从 token 取，不从前端 body 取
    if (user_id <= 0) {
        res.status = 401;
        res.set_content(R"({"ok":false,"error":"请先登录"})", "application/json");
        return;
    }
    json body = json::parse(req.body);
    bool ok = material_add(user_id, body["title"], body["url"]);
    json resp;
    resp["ok"] = ok;
    res.set_content(resp.dump(), "application/json");
} CATCH_ERR }

// DELETE /api/materials/1
void handle_delete_material(const Request& req, Response& res) { try {
    int material_id = stoi(req.matches[1]);
    int user_id = get_uid(req);                  // ← 改：不再默认1
    if (user_id <= 0) {
        res.status = 401;
        res.set_content(R"({"ok":false,"error":"请先登录"})", "application/json");
        return;
    }
    bool ok = material_delete(material_id, user_id);
    json resp;
    resp["ok"] = ok;
    res.set_content(resp.dump(), "application/json");
} CATCH_ERR }
