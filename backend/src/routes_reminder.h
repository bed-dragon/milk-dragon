#pragma once
#define _WIN32_WINNT 0x0A00
#include "httplib.h"
using namespace httplib;

void handle_get_reminders(const Request& req, Response& res);
void handle_mark_read(const Request& req, Response& res);
