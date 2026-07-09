#pragma once
#define _WIN32_WINNT 0x0A00
#include "httplib.h"
using namespace httplib;

void handle_record_pomodoro(const Request& req, Response& res);
void handle_pomodoro_today(const Request& req, Response& res);
