/**
 * api.js — 后端 API 请求封装 + 模拟数据
 * 学习养成计划
 *
 * 使用说明：
 * 1. 后端未就绪时，页面使用 MOCK 数据自动展示
 * 2. 后端接口完成后，函数会自动请求真实接口
 * 3. 如果请求失败，自动降级到模拟数据
 */

const API_BASE = 'http://localhost:8080';
const REQUEST_TIMEOUT = 2000; // 2 秒超时，快速降级到 MOCK

/**
 * 通用请求函数（带超时）
 */
async function request(method, path, body = null) {
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), REQUEST_TIMEOUT);

  try {
    const options = {
      method: method,
      headers: { 'Content-Type': 'application/json' },
      signal: controller.signal,
    };
    if (body) options.body = JSON.stringify(body);

    const res = await fetch(API_BASE + path, options);
    const data = await res.json();

    if (!data.ok && data.ok !== undefined) {
      throw new Error(data.error || '请求失败');
    }
    return data;
  } finally {
    clearTimeout(timeout);
  }
}

// ═══════════════════════════════════
// 任务 API
// ═══════════════════════════════════

function getTasks() {
  return request('GET', '/api/tasks');
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

function getRecommended() {
  return request('GET', '/api/tasks/recommended');
}

// ═══════════════════════════════════
// 打卡 API
// ═══════════════════════════════════

function _doCheckin(taskId, date) {
  return request('POST', '/api/checkin', { task_id: taskId, date: date });
}

function getCheckins(dateOrTaskId) {
  // 判断传入的是日期还是 task_id
  if (typeof dateOrTaskId === 'number' || /^\d+$/.test(dateOrTaskId)) {
    return request('GET', `/api/checkin?task_id=${dateOrTaskId}`);
  }
  return request('GET', `/api/checkin?date=${dateOrTaskId}`);
}

function getStreak() {
  return request('GET', '/api/checkin/streak');
}

// ═══════════════════════════════════
// 统计分析 API
// ═══════════════════════════════════

function getOverview() {
  return request('GET', '/api/stats/overview');
}

function getDailyStats(start, end) {
  return request('GET', `/api/stats/daily?start=${start}&end=${end}`);
}

function getWeeklyStats(week) {
  return request('GET', `/api/stats/weekly?week=${week}`);
}

function getMonthlyStats(month) {
  return request('GET', `/api/stats/monthly?month=${month}`);
}

// ═══════════════════════════════════
// 提醒 API
// ═══════════════════════════════════

function getReminders(type = '') {
  return request('GET', `/api/reminders?type=${type}`);
}

function markReminderRead(id) {
  return request('PUT', `/api/reminders/${id}/read`);
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
};