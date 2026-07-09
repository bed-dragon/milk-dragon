#include "routes_material.h"
#include "db.h"
#include "json.hpp"
using namespace std;
using json = nlohmann::json;

#define CATCH_ERR catch(exception& e) { \
    cerr << "[ERROR] " << e.what() << endl; \
    res.set_content(string(R"({"ok":false,"error":")") + e.what() + "\"}", "application/json"); \
}

// GET /api/materials?user_id=1
void handle_get_materials(const Request& req, Response& res) { try {
    auto uid = req.get_param_value("user_id");
    int user_id = uid.empty() ? 1 : stoi(uid);
    string data = material_list(user_id);
    json resp;
    resp["ok"] = true;
    resp["data"] = json::parse(data);
    res.set_content(resp.dump(), "application/json");
} CATCH_ERR }

// POST /api/materials
void handle_add_material(const Request& req, Response& res) { try {
    json body = json::parse(req.body);
    int user_id = body.value("user_id", 1);
    bool ok = material_add(user_id, body["title"], body["url"]);
    json resp;
    resp["ok"] = ok;
    res.set_content(resp.dump(), "application/json");
} CATCH_ERR }

// DELETE /api/materials/1?user_id=1
void handle_delete_material(const Request& req, Response& res) { try {
    int material_id = stoi(req.matches[1]);
    auto uid = req.get_param_value("user_id");
    int user_id = uid.empty() ? 1 : stoi(uid);
    bool ok = material_delete(material_id, user_id);
    json resp;
    resp["ok"] = ok;
    res.set_content(resp.dump(), "application/json");
} CATCH_ERR }
