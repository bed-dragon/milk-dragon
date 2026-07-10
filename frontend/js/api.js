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
// 收藏任务 API
// ═══════════════════════════════════

function favoriteTaskAdd(task) {
  return request('POST', '/api/favorite_tasks', task);
}

async function getFavoriteTasks() {
  const data = await request('GET', '/api/favorite_tasks');
  return { tasks: data.data || [] };
}

function _deleteFavorite(id) {
  return request('DELETE', `/api/favorite_tasks/${id}`);
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

/** 获取未读消息数 → 传 friend_id 则只查该好友 */
async function getUnreadCount(friendId) {
  const qs = friendId ? `?friend_id=${friendId}` : '';
  const data = await request('GET', '/api/messages/unread_count' + qs);
  return data.count || 0;
}

// ═══════════════════════════════════
// 管理员 API
// ═══════════════════════════════════

/** 获取全部用户列表 → [{ id, username, nickname, signature, role, created_at }] */
async function getAdminUsers() {
  const data = await request('GET', '/api/admin/users');
  return data.data || [];
}

/** 修改用户角色 */
function updateUserRole(userId, role) {
  return request('PUT', `/api/admin/users/${userId}/role`, { role });
}

/** 删除用户 */
function deleteUser(userId) {
  return request('DELETE', `/api/admin/users/${userId}`);
}

/** 获取推荐任务库全部数据 → [{ id, title, topic, priority, created_at }] */
async function getAdminRecommended() {
  const data = await request('GET', '/api/admin/recommended');
  return data.data || [];
}

/** 添加推荐任务 */
function addRecommended(title, topic, priority) {
  return request('POST', '/api/admin/recommended', { title, topic, priority });
}

/** 更新推荐任务 */
function updateRecommended(id, title, topic, priority) {
  return request('PUT', `/api/admin/recommended/${id}`, { title, topic, priority });
}

/** 删除推荐任务 */
function deleteRecommended(id) {
  return request('DELETE', `/api/admin/recommended/${id}`);
}

/** 获取系统全局统计 → { total_users, total_tasks, ... } */
async function getSystemStats() {
  const data = await request('GET', '/api/admin/stats');
  return data.data || {};
}

// ═══════════════════════════════════
// 📦 模拟数据（后端就绪前使用）
// ═══════════════════════════════════

const MOCK = {

  // ── 任务 ──（后端离线降级用，仅保留最小样本）
  tasks: [
    { id: 1, title: '示例任务', topic: '学习', deadline: '', priority: 2, need_review: 0, status: 0, created_at: '' },
  ],

  // ── 推荐任务 ──
  recommended: [
    { id: 1, title: '背 30 个英语单词', topic: '英语', priority: 2 },
    { id: 2, title: '完成 LeetCode 每日一题', topic: '编程', priority: 2 },
  ],

  // ── 收藏任务 ──
  favorites: [
    { id: 1, title: '背 30 个英语单词', topic: '英语', priority: 2 },
  ],

  // ── 打卡 ──
  checkins: {},

  // ── 统计总览 ──
  overview: {
    total_tasks: 0,
    completed: 0,
    rate: 0,
    streak: 0
  },

  // ── 每日趋势数据 ──
  dailyStats: [],

  // ── 提醒 ──
  reminders: {
    due: [],
    review: [],
  },

  // ── 番茄钟 ──
  pomodoroToday: [],

  // ── 名言 ──
  quotes: [
    { content: '学而不思则罔，思而不学则殆。', author: '孔子' },
    { content: '千里之行，始于足下。', author: '老子' },
    { content: '知识就是力量。', author: '培根' },
    { content: '温故而知新，可以为师矣。', author: '孔子' },
  ],
};