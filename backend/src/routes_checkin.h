#pragma once
#define _WIN32_WINNT 0x0A00
#include "httplib.h"
using namespace httplib;

// 打卡
void handle_get_checkin(const Request& req, Response& res);
void handle_do_checkin(const Request& req, Response& res);
// 签到
void handle_get_signins(const Request& req, Response& res);
void handle_do_signin(const Request& req, Response& res);
// 打卡记录（细粒度）
void handle_get_checkins(const Request& req, Response& res);
void handle_create_checkin_record(const Request& req, Response& res);
