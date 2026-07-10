#include "routes_reminder.h"
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

// GET /api/reminders?type=due|review
void handle_get_reminders(const Request& req, Response& res) {
    int user_id = get_uid(req);
    if (user_id <= 0) {
        res.status = 401;
        res.set_content(R"({"ok":false,"error":"请先登录"})", "application/json");
        return;
    }
    string filter = req.has_param("type") ? req.get_param_value("type") : "";
    string data = reminder_list(user_id, filter);
    json resp = {{"ok", true}, {"data", json::parse(data)}};
    res.set_content(resp.dump(), "application/json");
}

// POST /api/reminders/mark_read
void handle_mark_read(const Request& req, Response& res) { try {
    int user_id = get_uid(req);
    if (user_id <= 0) {
        res.status = 401;
        res.set_content(R"({"ok":false,"error":"请先登录"})", "application/json");
        return;
    }
    json body = json::parse(req.body);
    int reminder_id = body.value("reminder_id", 0);
    if (reminder_id == 0) {
        res.status = 400;
        res.set_content(R"({"ok":false,"error":"reminder_id is required"})", "application/json");
        return;
    }
    reminder_mark_read(reminder_id, user_id);
    json resp = {{"ok", true}, {"message", "marked read"}};
    res.set_content(resp.dump(), "application/json");
} CATCH_ERR }
