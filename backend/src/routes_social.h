#pragma once
#define _WIN32_WINNT 0x0A00
#include "httplib.h"
using namespace httplib;

// 好友
void handle_friend_request(const Request& req, Response& res);
void handle_friend_list(const Request& req, Response& res);
void handle_friend_action(const Request& req, Response& res);
void handle_friend_delete(const Request& req, Response& res);
// 聊天
void handle_send_message(const Request& req, Response& res);
void handle_get_messages(const Request& req, Response& res);
void handle_unread_count(const Request& req, Response& res);
