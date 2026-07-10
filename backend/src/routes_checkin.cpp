#include "routes_checkin.h"
#include "db.h"
#include "json.hpp"
using namespace std;
using json = nlohmann::json;

extern int user_id_by_token(const string& token);
extern string checkin_get_by_task(int user_id, int task_id);

static int get_uid(const Request& req) {
    if (req.has_header("Authorization")) {
        string auth = req.get_header_value("Authorization");
        if (auth.rfind("Bearer ", 0) == 0)
            return user_id_by_token(auth.substr(7));
    }
    return -1;  // 未认证
}

#define CATCH_ERR catch(exception& e) { \
    cerr << "[ERROR] " << e.what() << endl; \
    res.set_content(string(R"({"ok":false,"error":")") + e.what() + "\"}", "application/json"); \
}

// GET /api/checkin?date= 或 ?task_id=
void handle_get_checkin(const Request& req, Response& res) {
    int user_id = get_uid(req);
    if (user_id <= 0) { res.status = 401; res.set_content(R"({"ok":false,"error":"请先登录"})", "application/json"); return; }
    string data;
    if (req.has_param("task_id")) {
        int task_id = stoi(req.get_param_value("task_id"));
        data = checkin_get_by_task(user_id, task_id);
    } else {
        string date = req.has_param("date") ? req.get_param_value("date") : "";
        data = checkin_get(user_id, date);
    }
    json resp = {{"ok", true}, {"data", json::parse(data)}};
    res.set_content(resp.dump(), "application/json");
}

// POST /api/checkin
void handle_do_checkin(const Request& req, Response& res) { try {
    int user_id = get_uid(req);
    if (user_id <= 0) { res.status = 401; res.set_content(R"({"ok":false,"error":"请先登录"})", "application/json"); return; }
    json body = json::parse(req.body);
    int task_id = body.value("task_id", 0);
    string date = body.value("date", "");
    if (task_id == 0 || date.empty()) {
        res.status = 400;
        res.set_content(R"({"ok":false,"error":"task_id and date are required"})", "application/json");
        return;
    }
    if (checkin_do(user_id, task_id, date))
        res.set_content(R"({"ok":true,"message":"checked","data":{"checked":true}})", "application/json");
    else {
        res.status = 409;
        res.set_content(R"({"ok":false,"error":"打卡失败，可能已重复打卡"})", "application/json");
    }
} CATCH_ERR }

// GET /api/signins — 签到历史
void handle_get_signins(const Request& req, Response& res) {
    int user_id = get_uid(req);
    if (user_id <= 0) { res.status = 401; res.set_content(R"({"ok":false,"error":"请先登录"})", "application/json"); return; }
    string data = signin_history(user_id);
    json resp = {{"ok", true}, {"data", json::parse(data)}};
    res.set_content(resp.dump(), "application/json");
}

// POST /api/signins — 每日签到
void handle_do_signin(const Request& req, Response& res) { try {
    int user_id = get_uid(req);
    if (user_id <= 0) { res.status = 401; res.set_content(R"({"ok":false,"error":"请先登录"})", "application/json"); return; }
    json body = json::parse(req.body);
    string date = body.value("date", "");
    if (date.empty()) {
        res.status = 400;
        res.set_content(R"({"ok":false,"error":"date is required"})", "application/json");
        return;
    }
    signin_do(user_id, date);
    int streak = signin_streak(user_id);
    json resp = {{"ok", true}, {"data", {{"streak", streak}}}};
    res.set_content(resp.dump(), "application/json");
} CATCH_ERR }

// GET /api/checkins?date=   （细粒度打卡记录）
void handle_get_checkins(const Request& req, Response& res) {
    int user_id = get_uid(req);
    if (user_id <= 0) { res.status = 401; res.set_content(R"({"ok":false,"error":"请先登录"})", "application/json"); return; }
    string date = req.has_param("date") ? req.get_param_value("date") : "";
    string data = checkin_get(user_id, date);
    json resp = {{"ok", true}, {"data", json::parse(data)}};
    res.set_content(resp.dump(), "application/json");
}

// POST /api/checkins   （创建打卡记录）
void handle_create_checkin_record(const Request& req, Response& res) { try {
    int user_id = get_uid(req);
    if (user_id <= 0) { res.status = 401; res.set_content(R"({"ok":false,"error":"请先登录"})", "application/json"); return; }
    json body = json::parse(req.body);
    int task_id = body.value("task_id", 0);
    string date = body.value("date", "");
    if (task_id == 0 || date.empty()) {
        res.status = 400;
        res.set_content(R"({"ok":false,"error":"task_id and date are required"})", "application/json");
        return;
    }
    if (checkin_do(user_id, task_id, date))
        res.set_content(R"({"ok":true,"message":"checkin record created"})", "application/json");
    else {
        res.status = 409;
        res.set_content(R"({"ok":false,"error":"打卡失败"})", "application/json");
    }
} CATCH_ERR }
