#pragma once
#define _WIN32_WINNT 0x0A00
#include "httplib.h"
using namespace httplib;

// 任务模块的 HTTP 处理函数声明
void handle_get_tasks(const Request& req, Response& res);
void handle_get_one_task(const Request& req, Response& res);
void handle_create_task(const Request& req, Response& res);
void handle_update_task(const Request& req, Response& res);
void handle_delete_task(const Request& req, Response& res);
