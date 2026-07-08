/**
 * utils.js — 公共工具函数
 * 学习养成计划
 */

// ── 日期处理 ──

/** 格式化日期为 YYYY-MM-DD */
function formatDate(date) {
  const d = new Date(date);
  const y = d.getFullYear();
  const m = String(d.getMonth() + 1).padStart(2, '0');
  const day = String(d.getDate()).padStart(2, '0');
  return `${y}-${m}-${day}`;
}

/** 格式化日期时间为 YYYY-MM-DD HH:MM */
function formatDateTime(dateStr) {
  if (!dateStr) return '-';
  const d = new Date(dateStr);
  if (isNaN(d.getTime())) return dateStr;
  const y = d.getFullYear();
  const m = String(d.getMonth() + 1).padStart(2, '0');
  const day = String(d.getDate()).padStart(2, '0');
  const h = String(d.getHours()).padStart(2, '0');
  const min = String(d.getMinutes()).padStart(2, '0');
  return `${y}-${m}-${day} ${h}:${min}`;
}

/** 获取今天的日期字符串 */
function today() {
  return formatDate(new Date());
}

/** 获取本周一的日期 */
function getMonday() {
  const now = new Date();
  const dayOfWeek = now.getDay();
  const diff = dayOfWeek === 0 ? 6 : dayOfWeek - 1;
  const monday = new Date(now);
  monday.setDate(now.getDate() - diff);
  return formatDate(monday);
}

// ── 优先级处理 ──

const PRIORITY_MAP = { 1: '高', 2: '中', 3: '低' };
const PRIORITY_CLASS = { 1: 'tag-high', 2: 'tag-medium', 3: 'tag-low' };

/** 优先级数字转文字 */
function priorityLabel(p) {
  return PRIORITY_MAP[p] || '未知';
}

/** 优先级数字转 CSS 类名 */
function priorityClass(p) {
  return PRIORITY_CLASS[p] || 'tag-medium';
}

// ── 紧急程度 ──

const URGENCY_MAP = { high: '紧急', medium: '中等', low: '一般' };
const URGENCY_CLASS = { high: 'tag-due', medium: 'tag-medium', low: 'tag-low' };

function urgencyLabel(u) {
  return URGENCY_MAP[u] || '未知';
}

function urgencyClass(u) {
  return URGENCY_CLASS[u] || 'tag-medium';
}

// ── 数学计算 ──

/** 计算完成率百分比 */
function calcRate(completed, total) {
  if (total === 0) return 0;
  return Math.round((completed / total) * 100);
}

// ── 确认对话框 ──

/** 显示确认对话框，返回 true/false */
function showConfirm(message) {
  return new Promise((resolve) => {
    const overlay = document.createElement('div');
    overlay.className = 'modal-overlay';
    overlay.innerHTML = `
      <div class="modal-box">
        <h3>⚠️ 确认操作</h3>
        <p style="color:var(--text-secondary);">${message}</p>
        <div class="modal-actions">
          <button class="btn btn-outline" id="confirmCancel">取消</button>
          <button class="btn btn-danger" id="confirmOk">确定</button>
        </div>
      </div>
    `;
    document.body.appendChild(overlay);

    const close = (result) => {
      overlay.remove();
      resolve(result);
    };

    overlay.querySelector('#confirmOk').addEventListener('click', () => close(true));
    overlay.querySelector('#confirmCancel').addEventListener('click', () => close(false));
    overlay.addEventListener('click', (e) => {
      if (e.target === overlay) close(false);
    });
  });
}

// ── Toast 通知 ──

/** 显示 Toast 通知（3 秒自动消失） */
function showToast(message, type = 'info') {
  // 移除已存在的 toast
  const existing = document.querySelector('.toast');
  if (existing) existing.remove();

  const toast = document.createElement('div');
  toast.className = `toast toast-${type}`;
  toast.textContent = message;
  document.body.appendChild(toast);

  // 点击关闭
  toast.addEventListener('click', () => toast.remove());

  // 3 秒后自动消失
  setTimeout(() => {
    if (toast.parentNode) {
      toast.style.opacity = '0';
      toast.style.transition = 'opacity 0.3s ease';
      setTimeout(() => toast.remove(), 300);
    }
  }, 3000);
}