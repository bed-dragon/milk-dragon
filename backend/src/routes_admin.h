#pragma once
#define _WIN32_WINNT 0x0A00
#include "httplib.h"
using namespace httplib;

// 用户管理
void handle_admin_users(const Request& req, Response& res);
void handle_admin_user_role(const Request& req, Response& res);
void handle_admin_user_delete(const Request& req, Response& res);

// 推荐任务库管理
void handle_admin_recommended(const Request& req, Response& res);
void handle_admin_recommended_add(const Request& req, Response& res);
void handle_admin_recommended_update(const Request& req, Response& res);
void handle_admin_recommended_delete(const Request& req, Response& res);

// 系统统计
void handle_admin_stats(const Request& req, Response& res);
