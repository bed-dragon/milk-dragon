/**
 * api.js — 后端 API 请求封装 + 模拟数据
 * 学习养成计划
 *
 * 使用说明：
 * 1. 后端未就绪时，页面使用 MOCK 数据自动展示
 * 2. 后端接口完成后，函数会自动请求真实接口
 * 3. 如果请求失败，自动降级到模拟数据
 *
 * 响应格式约定：后端统一返回 { ok: true/false, data: ... }
 * 每个 wrapper 函数负责解包 data 字段，返回页面期望的格式
 */

const API_BASE = '';  // 空 = 自动跟随当前地址，本地/ngrok都能用
const REQUEST_TIMEOUT = 8000; // 8 秒超时，给后端充足时间

/**
 * 通用请求函数（带超时）
 */
async function request(method, path, body = null) {
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), REQUEST_TIMEOUT);

  try {
    const headers = { 'Content-Type': 'application/json' };
    // 自动附加 Authorization header
    if (typeof Auth !== 'undefined' && Auth.getToken()) {
      headers['Authorization'] = 'Bearer ' + Auth.getToken();
    }

    const options = {
      method: method,
      headers: headers,
      signal: controller.signal,
    };
    if (body) options.body = JSON.stringify(body);

    const res = await fetch(API_BASE + path, options);

    // 401 未认证 — 清除登录状态并跳转
    if (res.status === 401) {
      if (typeof Auth !== 'undefined') Auth.clear();
      const prefix = window.location.pathname.includes('/pages/') ? '../' : '';
      window.location.href = prefix + 'login.html';
      throw new Error('请先登录');
    }

    const data = await res.json();

    if (!data.ok && data.ok !== undefined) {
      throw new Error(data.error || data.msg || '请求失败');
    }
    return data;
  } finally {
    clearTimeout(timeout);
  }
}

// ═══════════════════════════════════
// 任务 API
// ═══════════════════════════════════

/** 获取任务列表 → { tasks: [...] } */
async function getTasks() {
  const data = await request('GET', '/api/tasks');
  return { tasks: data.data || [] };
}

function createTask(task) {
  return request('POST', '/api/tasks', task);
}

function updateTask(id, fields) {
  return request('PUT', `/api/tasks/${id}`, fields);
}

function _deleteTask(id) {
  return request('DELETE', `/api/tasks/${id}`);
}

/** 获取推荐任务 → { tasks: [...] } */
async function getRecommended() {
  const data = await request('GET', '/api/recommended_tasks');
  return { tasks: data.data || [] };
}

// ═══════════════════════════════════
// 打卡 API
// ═══════════════════════════════════

function _doCheckin(taskId, date) {
  return request('POST', '/api/checkin', { task_id: taskId, date: date });
}

/** 查询打卡记录 → { checkins: [...] } */
async function getCheckins(dateOrTaskId) {
  let data;
  if (typeof dateOrTaskId === 'number' || /^\d+$/.test(dateOrTaskId)) {
    data = await request('GET', `/api/checkin?task_id=${dateOrTaskId}`);
  } else {
    data = await request('GET', `/api/checkin?date=${dateOrTaskId}`);
  }
  return { checkins: data.data || [] };
}

/** 获取连续打卡天数 → { streak: N } */
async function getStreak() {
  const data = await request('POST', '/api/signins', { date: today() });
  const inner = data.data || {};
  return { streak: inner.streak || 0 };
}

// ═══════════════════════════════════
// 统计分析 API
// ═══════════════════════════════════

/** 获取总览统计 → { total_tasks, completed, rate, streak } */
async function getOverview() {
  const data = await request('GET', '/api/stats/overview');
  const d = data.data || {};
  return {
    total_tasks: d.total_tasks || 0,
    completed: d.completed || 0,
    rate: d.completion_rate || d.rate || 0,
    streak: d.streak || 0,
  };
}

/** 获取每日统计 → { daily: [...] } */
async function getDailyStats(start, end) {
  const data = await request('GET', `/api/stats/daily?start=${start}&end=${end}`);
  return { daily: data.data || [] };
}

/** 获取每周统计 → { daily: [...] } */
async function getWeeklyStats(week) {
  const data = await request('GET', `/api/stats/weekly?week=${week}`);
  return { daily: data.data || [] };
}

/** 获取每月统计 → { daily: [...] } */
async function getMonthlyStats(month) {
  const data = await request('GET', `/api/stats/monthly?month=${month}`);
  return { daily: data.data || [] };
}

// ═══════════════════════════════════
// 提醒 API
// ═══════════════════════════════════

/** 获取提醒列表 → { reminders: [...] } */
async function getReminders(type = '') {
  const data = await request('GET', `/api/reminders?type=${type}`);
  return { reminders: data.data || [] };
}

/** 标记提醒已读 */
function markReminderRead(id) {
  return request('POST', '/api/reminders/mark_read', { reminder_id: id });
}

// ═══════════════════════════════════
// 番茄钟 API
// ═══════════════════════════════════

/** 记录一次番茄钟 */
function recordPomodoro(duration) {
  return request('POST', '/api/pomodoro', { duration: duration });
}

/** 获取今日番茄钟记录 → { records: [...] } */
async function getTodayPomodoros() {
  const data = await request('GET', '/api/pomodoro/today');
  return { records: data.data || [] };
}

// ═══════════════════════════════════
// 名言 API
// ═══════════════════════════════════

/** 获取随机名言 → { content, author } */
async function getRandomQuote() {
  const data = await request('GET', '/api/quotes/random');
  const d = data.data || {};
  return { content: d.content || '', author: d.author || '佚名' };
}

// ═══════════════════════════════════
// 认证 API
// ═══════════════════════════════════

/** 登录 → { token, user_id, ... } */
async function login(username, password) {
  return await request('POST', '/api/auth/login', { username, password });
}

/** 注册（成功后自动返回 token）→ { token, user_id, username } */
async function register(username, password, nickname) {
  return await request('POST', '/api/auth/register', { username, password, nickname });
}

/** 获取当前用户信息 → { id, username, nickname, signature, created_at } */
async function getMe() {
  return await request('GET', '/api/me');
}

/** 更新个人资料 → { nickname, signature } */
async function updateProfile(nickname, signature) {
  return await request('PUT', '/api/me', { nickname, signature });
}

/** 修改密码 → { old_password, new_password } */
async function changePassword(oldPwd, newPwd) {
  return await request('PUT', '/api/me/password', { old_password: oldPwd, new_password: newPwd });
}

// ═══════════════════════════════════
// 好友 API
// ═══════════════════════════════════

/** 搜索用户 → [{ id, username, nickname }] */
async function searchUsers(q) {
  const data = await request('GET', `/api/users/search?q=${encodeURIComponent(q)}`);
  return data.data || [];
}

/** 获取好友列表 → [{ id, status, other_id, other_name, other_nick, ... }] */
async function getFriendList() {
  const data = await request('GET', '/api/friends');
  return data.data || [];
}

/** 发送好友申请 */
function sendFriendRequest(toId) {
  return request('POST', '/api/friends/request', { to_id: toId });
}

/** 处理好友申请（1=接受, 2=拒绝） */
function handleFriendRequest(friendshipId, status) {
  return request('POST', `/api/friends/${friendshipId}/handle`, { status: status });
}

/** 删除好友 */
function deleteFriend(friendshipId) {
  return request('DELETE', `/api/friends/${friendshipId}`);
}

// ═══════════════════════════════════
// 学习资料 API
// ═══════════════════════════════════

/** 获取收藏列表 → [{ id, title, url, created_at }] */
async function getMaterials() {
  const data = await request('GET', '/api/materials');
  return data.data || [];
}

/** 添加收藏 */
function addMaterial(title, url) {
  return request('POST', '/api/materials', { title, url });
}

/** 删除收藏 */
function deleteMaterial(id) {
  return request('DELETE', `/api/materials/${id}`);
}

// ═══════════════════════════════════
// 聊天 API
// ═══════════════════════════════════

/** 发送消息 */
function sendMessage(toId, content) {
  return request('POST', '/api/messages', { to_id: toId, content: content });
}

/** 获取与某好友的消息历史 → [{ id, from_id, to_id, content, is_read, created_at, mine }] */
async function getMessages(friendId) {
  const data = await request('GET', `/api/messages/${friendId}`);
  return data.data || [];
}

/** 获取未读消息数 → { count: N } */
async function getUnreadCount() {
  const data = await request('GET', '/api/messages/unread_count');
  return data.count || 0;
}

// ═══════════════════════════════════
// 📦 模拟数据（后端就绪前使用）
// ═══════════════════════════════════

const MOCK = {

  // ── 任务 ──
  tasks: [
    { id: 1, title: '背 30 个英语单词', topic: '英语', deadline: '2026-07-10T18:00', priority: 2, need_review: 1, status: 0, created_at: '2026-07-07 10:00' },
    { id: 2, title: '复习当天课程笔记', topic: '学习方法', deadline: '2026-07-08T22:00', priority: 1, need_review: 1, status: 0, created_at: '2026-07-07 10:30' },
    { id: 3, title: '完成 LeetCode 每日一题', topic: '编程', deadline: '2026-07-08T23:59', priority: 2, need_review: 0, status: 0, created_at: '2026-07-07 11:00' },
    { id: 4, title: '阅读课外书 30 分钟', topic: '阅读', deadline: '2026-07-09T20:00', priority: 3, need_review: 0, status: 1, created_at: '2026-07-06 09:00' },
    { id: 5, title: '运动 30 分钟', topic: '健康', deadline: '2026-07-08T07:00', priority: 3, need_review: 0, status: 0, created_at: '2026-07-06 08:00' },
  ],

  // ── 推荐任务 ──
  recommended: [
    { id: 101, title: '背 30 个英语单词', topic: '英语', priority: 2 },
    { id: 102, title: '复习当天课程笔记', topic: '学习方法', priority: 1 },
    { id: 103, title: '阅读课外书 30 分钟', topic: '阅读', priority: 3 },
    { id: 104, title: '完成 LeetCode 每日一题', topic: '编程', priority: 2 },
    { id: 105, title: '整理课程思维导图', topic: '学习方法', priority: 2 },
    { id: 106, title: '练习听力 20 分钟', topic: '英语', priority: 2 },
    { id: 107, title: '写学习日记', topic: '学习方法', priority: 3 },
    { id: 108, title: '运动 30 分钟', topic: '健康', priority: 3 },
  ],

  // ── 打卡 ──
  checkins: {
    '2026-07-07': [
      { id: 1, task_id: 1, task_title: '背 30 个英语单词', checkin_date: '2026-07-07', checkin_time: '10:30' },
      { id: 2, task_id: 4, task_title: '阅读课外书 30 分钟', checkin_date: '2026-07-07', checkin_time: '20:05' },
    ],
    '2026-07-06': [
      { id: 3, task_id: 4, task_title: '阅读课外书 30 分钟', checkin_date: '2026-07-06', checkin_time: '20:00' },
    ],
    '2026-07-05': [
      { id: 4, task_id: 4, task_title: '阅读课外书 30 分钟', checkin_date: '2026-07-05', checkin_time: '19:30' },
    ],
  },

  // ── 统计总览 ──
  overview: {
    total_tasks: 20,
    completed: 12,
    rate: 0.6,
    streak: 5
  },

  // ── 每日趋势数据 ──
  dailyStats: [
    { date: '07-01', added: 3, completed: 2, rate: 0.67 },
    { date: '07-02', added: 4, completed: 3, rate: 0.75 },
    { date: '07-03', added: 2, completed: 1, rate: 0.50 },
    { date: '07-04', added: 5, completed: 3, rate: 0.60 },
    { date: '07-05', added: 3, completed: 2, rate: 0.67 },
    { date: '07-06', added: 2, completed: 1, rate: 0.50 },
    { date: '07-07', added: 4, completed: 0, rate: 0.00 },
  ],

  // ── 提醒 ──
  reminders: {
    due: [
      { id: 1, task_title: '复习当天课程笔记', type: 'due', urgency: 'high', remind_date: '2026-07-08', is_read: 0 },
      { id: 2, task_title: '完成 LeetCode 每日一题', type: 'due', urgency: 'medium', remind_date: '2026-07-08', is_read: 0 },
      { id: 3, task_title: '运动 30 分钟', type: 'due', urgency: 'low', remind_date: '2026-07-08', is_read: 0 },
    ],
    review: [
      { id: 4, task_title: '背 30 个英语单词', type: 'review', urgency: 'medium', remind_date: '2026-07-08', is_read: 0 },
      { id: 5, task_title: '阅读课外书 30 分钟', type: 'review', urgency: 'low', remind_date: '2026-07-08', is_read: 1 },
    ],
  },

  // ── 番茄钟 ──
  pomodoroToday: [],

  // ── 名言 ──
  quotes: [
    { content: '学而不思则罔，思而不学则殆。', author: '孔子' },
    { content: '天才是百分之一的灵感，加上百分之九十九的汗水。', author: '爱迪生' },
    { content: '学习知识要善于思考，思考，再思考。', author: '爱因斯坦' },
    { content: '千里之行，始于足下。', author: '老子' },
    { content: '读书破万卷，下笔如有神。', author: '杜甫' },
    { content: '知识就是力量。', author: '培根' },
    { content: '活着就要学习，学习不是为了活着。', author: '培根' },
    { content: '温故而知新，可以为师矣。', author: '孔子' },
  ],
};