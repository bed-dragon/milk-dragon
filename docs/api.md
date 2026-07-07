# API 接口文档（扩展草稿）

说明：下列接口为路由框架草稿，返回示例为占位数据。每个接口包含：请求方法、URL、参数说明（Query/Path/Body）与返回 JSON 示例。后续可在每个接口中补充字段验证、鉴权和错误码。

## 目录
- 1. 认证与用户
- 2. 任务模块（Tasks）
- 3. 推荐任务（Recommended）
- 4. 打卡 / 签到（Checkin / Signin）
- 5. 打卡记录（Checkins）
- 6. 提醒（Reminders）
- 7. 统计（Stats）
- 8. 学习资料（Materials）
- 9. 番茄钟（Pomodoro）
- 10. 好友与社交（Friends）
- 11. 聊天（Messages）
- 12. 名言（Quotes）

## 1. 认证与用户

### POST /api/auth/register
- 方法：POST
- 路径：`/api/auth/register`
- Body 示例：
```json
{ "username": "alice", "password": "secret" }
```
- 返回示例：
```json
{ "ok": true, "message": "registered (stub)", "data": { "user_id": 1 } }
```

### POST /api/auth/login
- 方法：POST
- 路径：`/api/auth/login`
- Body 示例：
```json
{ "username": "alice", "password": "secret" }
```
- 返回示例：
```json
{ "ok": true, "token": "stub-token" }
```

### GET /api/users/:id
- 方法：GET
- 路径：`/api/users/:id`
- Path：`id`（用户 ID）
- 返回示例：
```json
{ "ok": true, "data": { "id": 1, "username": "alice", "nickname": "A" } }
```

## 2. 任务模块（Tasks）

说明：任务支持用户自定义和系统推荐；字段示例：`title`、`topic`、`deadline`（YYYY-MM-DD）、`priority`（1/2/3）、`need_review`（boolean）、`status`（0待完成/1已完成）。

### GET /api/tasks
- 方法：GET
- 路径：`/api/tasks`
- Query：可选 `user_id`, `status`, `topic`
- 返回示例：
```json
{ "ok": true, "data": [] }
```

### GET /api/tasks/:id
- 方法：GET
- 路径：`/api/tasks/:id`
- Path：`id`
- 返回示例：
```json
{ "ok": true, "data": { "id":1, "title":"背单词", "topic":"英语", "deadline":"2026-07-10", "priority":2, "need_review":true, "status":0 } }
```

### POST /api/tasks
- 方法：POST
- 路径：`/api/tasks`
- Body 示例：
```json
{ "user_id":1, "title":"背单词", "topic":"英语", "deadline":"2026-07-10", "priority":2, "need_review":true }
```
- 返回示例：
```json
{ "ok": true, "data": { "id": 123 } }
```

### PUT /api/tasks/:id
- 方法：PUT
- 路径：`/api/tasks/:id`
- Body：支持部分字段更新（例如 `title`/`deadline`/`priority`/`status`/`need_review`）
- 返回示例：
```json
{ "ok": true, "message": "updated" }
```

### DELETE /api/tasks/:id
- 方法：DELETE
- 路径：`/api/tasks/:id`
- Query：可选 `user_id` 用于校验所有者
- 返回示例：
```json
{ "ok": true }
```

## 3. 推荐任务（Recommended）

### GET /api/recommended_tasks
- 方法：GET
- 路径：`/api/recommended_tasks`
- 返回示例：
```json
{ "ok": true, "data": [{ "id":1, "title":"Learn 50 words", "topic":"英语", "priority":2 }] }
```

## 4. 打卡 / 签到（Checkin / Signin）

### POST /api/checkin
- 方法：POST
- 路径：`/api/checkin`
- Body 示例：
```json
{ "user_id":1, "task_id":123, "date":"2026-07-07" }
```
- 返回示例：
```json
{ "ok": true, "message": "checked", "data": { "checked": true } }
```

### GET /api/checkin
- 方法：GET
- 路径：`/api/checkin`
- Query：可选 `user_id`, `date`
- 返回示例：
```json
{ "ok": true, "data": [] }
```

### POST /api/signins
- 方法：POST
- 路径：`/api/signins`
- Body：{ "user_id":1, "date":"YYYY-MM-DD" }
- 返回示例：
```json
{ "ok": true, "data": { "streak": 5 } }
```

### GET /api/signins
- 方法：GET
- 路径：`/api/signins`
- Query：可选 `user_id`, `start_date`, `end_date`

## 5. 打卡记录（Checkins）

### GET /api/checkins
- 方法：GET
- 路径：`/api/checkins`
- Query：可按 `user_id`/`task_id`/`date` 过滤
- 返回示例：
```json
{ "ok": true, "data": [] }
```

### POST /api/checkins
- 方法：POST
- 路径：`/api/checkins`
- 用途：创建单条打卡记录（内部或前端调用）

## 6. 提醒（Reminders）

### GET /api/reminders
- 方法：GET
- 路径：`/api/reminders`
- Query：可选 `user_id`, `type`（"due"/"review"）
- 返回示例：
```json
{ "ok": true, "data": [] }
```

### POST /api/reminders/mark_read
- 方法：POST
- 路径：`/api/reminders/mark_read`
- Body：{ "reminder_id": 123, "user_id": 1 }

## 7. 统计与任务分析（Stats）

### GET /api/stats/overview
- 方法：GET
- 路径：`/api/stats/overview`
- Query：`user_id`, `start_date`, `end_date`
- 返回（示例）：
```json
{ "ok": true, "data": { "total_tasks": 10, "completed": 7, "completion_rate": 0.7 } }
```

### GET /api/stats/daily
- 方法：GET
- 路径：`/api/stats/daily`
- Query：`user_id`, `date`

## 8. 学习资料（Materials / 收藏）

### GET /api/materials
- 方法：GET
- 路径：`/api/materials`
- Query：可选 `user_id`

### POST /api/materials
- 方法：POST
- 路径：`/api/materials`
- Body 示例：
```json
{ "user_id":1, "title":"Lecture notes", "url":"https://..." }
```

### DELETE /api/materials/:id
- 方法：DELETE
- 路径：`/api/materials/:id`

## 9. 番茄钟（Pomodoro）

### POST /api/pomodoro
- 方法：POST
- 路径：`/api/pomodoro`
- Body：{ "user_id":1, "duration":25 }

### GET /api/pomodoro/today
- 方法：GET
- 路径：`/api/pomodoro/today`

## 10. 好友与社交（Friends）

### POST /api/friends/request
- 方法：POST
- Body：{ "from_id":1, "to_id":2 }

### GET /api/friends
- 方法：GET
- 返回：好友列表或请求列表

### POST /api/friends/:id/handle
- 方法：POST
- Body：{ "status": 1 } (1=accept,2=reject)

### DELETE /api/friends/:id
- 方法：DELETE

## 11. 聊天（Messages）

### POST /api/messages
- 方法：POST
- Body：{ "from_id":1, "to_id":2, "content":"..." }

### GET /api/messages/:friend_id
- 方法：GET

### GET /api/messages/unread_count
- 方法：GET

## 12. 名言（Quotes）

### GET /api/quotes/random
- 方法：GET
- 返回示例：
```json
{ "ok": true, "data": { "content": "学习使人进步", "author": "佚名" } }
```

---

备注：文档为草稿，下一步可以：
- 为每个接口补充请求/响应字段说明和示例（按需展开）；
- 添加鉴权（`Authorization: Bearer <token>`）示例；
- 明确错误码和常见错误响应结构。
