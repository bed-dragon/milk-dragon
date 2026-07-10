#include "routes_pomodoro.h"
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

#define CATCH_ERR catch(exception& e) { \
    cerr << "[ERROR] " << e.what() << endl; \
    res.set_content(string(R"({"ok":false,"error":")") + e.what() + "\"}", "application/json"); \
}

// POST /api/pomodoro
void handle_record_pomodoro(const Request& req, Response& res) { try {
    int user_id = get_uid(req);
    if (user_id <= 0) {
        res.status = 401;
        res.set_content(R"({"ok":false,"error":"请先登录"})", "application/json");
        return;
    }
    json body = json::parse(req.body);
    int duration = body.value("duration", 25);
    if (pomodoro_record(user_id, duration))
        res.set_content(R"({"ok":true,"message":"pomodoro recorded"})", "application/json");
    else {
        res.status = 500;
        res.set_content(R"({"ok":false,"error":"record failed"})", "application/json");
    }
} CATCH_ERR }

// GET /api/pomodoro/today
void handle_pomodoro_today(const Request& req, Response& res) {
    int user_id = get_uid(req);
    if (user_id <= 0) {
        res.status = 401;
        res.set_content(R"({"ok":false,"error":"请先登录"})", "application/json");
        return;
    }
    string data = pomodoro_today(user_id);
    json resp = {{"ok", true}, {"data", json::parse(data)}};
    res.set_content(resp.dump(), "application/json");
}
