#pragma once
#define _WIN32_WINNT 0x0A00
#include "httplib.h"
using namespace httplib;

void handle_get_materials(const Request& req, Response& res);
void handle_add_material(const Request& req, Response& res);
void handle_delete_material(const Request& req, Response& res);
