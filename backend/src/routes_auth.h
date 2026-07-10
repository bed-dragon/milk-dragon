#pragma once
#define _WIN32_WINNT 0x0A00
#include "httplib.h"
using namespace httplib;

// 认证模块 HTTP 处理函数
void handle_register(const Request& req, Response& res);
void handle_login(const Request& req, Response& res);
void handle_get_me(const Request& req, Response& res);
void handle_search_users(const Request& req, Response& res);
void handle_update_profile(const Request& req, Response& res);
void handle_change_password(const Request& req, Response& res);
