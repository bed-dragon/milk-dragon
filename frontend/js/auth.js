/**
 * auth.js — 认证模块
 * 学习养成计划
 *
 * 提供 token / 用户信息管理、认证守卫、退出登录
 */
const Auth = {
  TOKEN_KEY: 'auth_token',
  USER_KEY: 'auth_user',

  /** 保存登录凭证 */
  set(token, user) {
    localStorage.setItem(this.TOKEN_KEY, token);
    localStorage.setItem(this.USER_KEY, JSON.stringify(user));
  },

  /** 获取 token */
  getToken() {
    return localStorage.getItem(this.TOKEN_KEY);
  },

  /** 获取当前用户信息 */
  getUser() {
    try {
      const u = localStorage.getItem(this.USER_KEY);
      return u ? JSON.parse(u) : null;
    } catch (e) {
      return null;
    }
  },

  /** 清除登录状态 */
  clear() {
    localStorage.removeItem(this.TOKEN_KEY);
    localStorage.removeItem(this.USER_KEY);
  },

  /**
   * 认证守卫：未登录跳转到登录页
   * 每个受保护页面在脚本最开头调用
   */
  guard() {
    if (!this.getToken()) {
      const prefix = window.location.pathname.includes('/pages/') ? '../' : '';
      window.location.href = prefix + 'login.html';
      return false;
    }
    return true;
  },

  /**
   * 退出登录并跳转
   */
  logout() {
    this.clear();
    const prefix = window.location.pathname.includes('/pages/') ? '../' : '';
    window.location.href = prefix + 'login.html';
  },

  /**
   * 在侧边栏注入用户信息
   */
  renderSidebarUser() {
    const container = document.getElementById('sidebarUserArea');
    if (!container) return;
    const user = this.getUser();
    if (user) {
      container.innerHTML =
        '<span class="sidebar-user-name">' + (user.nickname || user.username) + '</span>' +
        '<a class="sidebar-user-logout" onclick="Auth.logout()">退出登录</a>';
    }
  }
};
