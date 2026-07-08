/**
 * navigation.js — 页面导航优化
 * 功能：hover 预加载 + 点击加载动画 + 资源提示
 */
(function() {
  'use strict';

  // ═══════════════════════════════════════
  // 1. Hover 预加载（鼠标悬停时就开始加载页面）
  // ═══════════════════════════════════════
  function initPrefetch() {
    const navLinks = document.querySelectorAll('.nav-item');
    if (!navLinks.length) return;

    navLinks.forEach(link => {
      let prefetched = false;

      link.addEventListener('mouseenter', () => {
        if (prefetched) return;
        prefetched = true;

        const url = link.getAttribute('href');
        if (!url || url === '#' || url.startsWith('javascript:')) return;

        // 方式 1: <link rel="prefetch">（浏览器空闲时加载）
        const prefetchLink = document.createElement('link');
        prefetchLink.rel = 'prefetch';
        prefetchLink.href = url;
        document.head.appendChild(prefetchLink);

        // 方式 2: fetch 预加载（更激进，兼容性更好）
        try {
          const fetchLink = document.createElement('link');
          fetchLink.rel = 'preload';
          fetchLink.as = 'document';
          fetchLink.href = url;
          document.head.appendChild(fetchLink);
        } catch (e) { /* 忽略 */ }
      }, { once: true });
    });
  }

  // ═══════════════════════════════════════
  // 2. 点击加载动画
  // ═══════════════════════════════════════
  function initLoadingOverlay() {
    const navLinks = document.querySelectorAll('.nav-item');
    if (!navLinks.length) return;

    // 注入 fadeIn 动画
    if (!document.getElementById('navOptimizeStyle')) {
      const style = document.createElement('style');
      style.id = 'navOptimizeStyle';
      style.textContent = `
        @keyframes navFadeIn { from { opacity: 0; } to { opacity: 1; } }
        .nav-loading-overlay {
          position: fixed; inset: 0; z-index: 99999;
          background: var(--bg-body, #f0f2f5);
          display: flex; flex-direction: column;
          align-items: center; justify-content: center;
          gap: 16px;
          animation: navFadeIn 0.15s ease;
        }
        .nav-loading-overlay .nav-loading-text {
          font-size: var(--font-size-sm, 13px);
          color: var(--text-muted, #b2bec3);
        }
      `;
      document.head.appendChild(style);
    }

    navLinks.forEach(link => {
      link.addEventListener('click', (e) => {
        // 当前页面的链接，不处理
        if (link.classList.contains('active')) {
          e.preventDefault();
          return;
        }

        // 创建加载遮罩
        const overlay = document.createElement('div');
        overlay.className = 'nav-loading-overlay';
        overlay.innerHTML = `
          <div class="spinner spinner-lg"></div>
          <span class="nav-loading-text">正在加载...</span>
        `;
        document.body.appendChild(overlay);
      });
    });
  }

  // ═══════════════════════════════════════
  // 3. 页面加载完成后添加 prefetch link 标签（资源提示）
  // ═══════════════════════════════════════
  function addPrefetchHints() {
    // 根据当前页面，预加载其他页面
    const currentPath = window.location.pathname;
    const isInPages = currentPath.includes('/pages/');

    const pages = isInPages
      ? ['../index.html', 'checkin.html', 'stats.html', 'reminders.html', 'tasks.html', 'pomodoro.html']
      : ['pages/tasks.html', 'pages/checkin.html', 'pages/stats.html', 'pages/reminders.html', 'pages/pomodoro.html'];

    // 排除当前页面
    const currentPage = currentPath.split('/').pop() || 'index.html';

    pages.forEach(page => {
      const pageName = page.split('/').pop();
      if (pageName === currentPage) return;

      const link = document.createElement('link');
      link.rel = 'prefetch';
      link.href = page;
      document.head.appendChild(link);
    });
  }

  // ═══════════════════════════════════════
  // 启动
  // ═══════════════════════════════════════
  // DOM 就绪后执行
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
      initPrefetch();
      initLoadingOverlay();
      addPrefetchHints();
    });
  } else {
    initPrefetch();
    initLoadingOverlay();
    addPrefetchHints();
  }
})();
