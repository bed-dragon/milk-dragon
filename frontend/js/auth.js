/**
 * auth.js — 认证模块
 * 学习养成计划
 *
 * 提供 token / 用户信息管理、认证守卫、退出登录、侧边栏渲染
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

  /** 是否是管理员 */
  isAdmin() {
    const user = this.getUser();
    return user && user.role === 'admin';
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
   * 管理员守卫：非管理员跳转到首页
   * 管理页面在脚本开头调用
   */
  guardAdmin() {
    if (!this.guard()) return false;
    if (!this.isAdmin()) {
      const prefix = window.location.pathname.includes('/pages/') ? '../' : '';
      window.location.href = prefix + 'index.html';
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
      var profileUrl = (window.location.pathname.includes('/pages/') ? '' : 'pages/') + 'profile.html';
      container.innerHTML =
        '<a class="sidebar-user-name" href="' + profileUrl + '" title="查看个人中心">' + (user.nickname || user.username) + '</a>' +
        '<a class="sidebar-user-logout" onclick="Auth.logout()">退出登录</a>';
    }
  },

  /**
   * 动态渲染侧边栏导航
   * @param {string} activePage - 当前页面标识：dashboard|tasks|checkin|stats|reminders|pomodoro|profile|friends|materials|chat|admin-users|admin-tasks|admin-stats
   */
  renderSidebar(activePage) {
    const isPages = window.location.pathname.includes('/pages/');
    const base = isPages ? '' : 'pages/';

    // 导航项定义
    const navItems = [
      { page: 'dashboard',  href: (isPages ? '../' : '') + 'index.html',     icon: '📊', label: '仪表盘' },
      { page: 'tasks',      href: base + 'tasks.html',      icon: '📋', label: '任务管理' },
      { page: 'checkin',    href: base + 'checkin.html',    icon: '✅', label: '打卡' },
      { page: 'stats',      href: base + 'stats.html',      icon: '📈', label: '数据分析' },
      { page: 'reminders',  href: base + 'reminders.html',  icon: '🔔', label: '提醒中心' },
      { page: 'pomodoro',   href: base + 'pomodoro.html',   icon: '🍅', label: '番茄钟' },
      { page: 'profile',    href: base + 'profile.html',    icon: '👤', label: '个人中心' },
      { page: 'friends',    href: base + 'friends.html',    icon: '👥', label: '好友' },
      { page: 'materials',  href: base + 'materials.html',  icon: '📎', label: '学习资料' },
    ];

    // 管理员专属导航项
    const adminItems = [
      { page: 'admin-users',  href: base + 'admin-users.html',  icon: '👥', label: '用户管理' },
      { page: 'admin-tasks',  href: base + 'admin-tasks.html',  icon: '📋', label: '推荐任务库' },
      { page: 'admin-stats',  href: base + 'admin-stats.html',  icon: '📊', label: '系统统计' },
    ];

    var html = '';
    html += '<div class="sidebar-brand">';
    html += '<span class="brand-icon">📚</span>';
    html += '<span>学习养成计划</span>';
    html += '</div>';
    html += '<nav class="sidebar-nav">';

    // 渲染普通导航项
    for (var i = 0; i < navItems.length; i++) {
      var item = navItems[i];
      var cls = 'nav-item' + (item.page === activePage ? ' active' : '');
      html += '<a class="' + cls + '" href="' + item.href + '">';
      html += '<span class="nav-icon">' + item.icon + '</span>';
      html += '<span class="nav-label">' + item.label + '</span>';
      html += '</a>';
    }

    // 管理员专属导航
    if (this.isAdmin()) {
      html += '<div class="sidebar-divider"></div>';
      html += '<div class="sidebar-section-title">👑 管理后台</div>';
      for (var j = 0; j < adminItems.length; j++) {
        var aItem = adminItems[j];
        var aCls = 'nav-item admin-nav-item' + (aItem.page === activePage ? ' active' : '');
        html += '<a class="' + aCls + '" href="' + aItem.href + '">';
        html += '<span class="nav-icon">' + aItem.icon + '</span>';
        html += '<span class="nav-label">' + aItem.label + '</span>';
        html += '</a>';
      }
    }

    html += '</nav>';
    html += '<div class="sidebar-user" id="sidebarUserArea">';
    html += '<span class="sidebar-user-name">未登录</span>';
    html += '</div>';

    // 写入侧边栏
    var sidebar = document.querySelector('.sidebar');
    if (sidebar) {
      sidebar.innerHTML = html;
    }

    // 渲染用户区域
    this.renderSidebarUser();

    // 检测番茄钟是否在其他页面完成
    var doneMinutes = localStorage.getItem('pomodoro_done');
    if (doneMinutes && activePage !== 'pomodoro') {
      setTimeout(function() {
        if (typeof showToast === 'function') {
          showToast('🍅 番茄钟 ' + doneMinutes + ' 分钟已完成！', 'success');
        }
      }, 500);
    }
  }
};
