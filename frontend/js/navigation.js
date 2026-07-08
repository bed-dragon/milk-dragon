/**
 * navigation.js — 页面导航优化
 * 功能：hover 预加载 + 点击加载动画 + 资源提示
 */
(function() {
  'use strict';

  // 检测是否以 file:// 协议打开（本地文件），
  // 此时跳过 prefetch/preload，避免浏览器安全警告
  var isFileProtocol = window.location.protocol === 'file:';

  // ═══════════════════════════════════════
  // 1. Hover 预加载（鼠标悬停时就开始加载页面）
  // ═══════════════════════════════════════
  function initPrefetch() {
    // file:// 协议下跳过，prefetch 会触发浏览器安全警告
    if (isFileProtocol) return;

    var navLinks = document.querySelectorAll('.nav-item');
    if (!navLinks.length) return;

    navLinks.forEach(function(link) {
      var prefetched = false;

      link.addEventListener('mouseenter', function() {
        if (prefetched) return;
        prefetched = true;

        var url = link.getAttribute('href');
        if (!url || url === '#' || url.indexOf('javascript:') === 0) return;

        // <link rel="prefetch"> 让浏览器空闲时预加载
        try {
          var prefetchLink = document.createElement('link');
          prefetchLink.rel = 'prefetch';
          prefetchLink.href = url;
          document.head.appendChild(prefetchLink);
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
    // file:// 协议下跳过
    if (isFileProtocol) return;
    // 根据当前页面，预加载其他页面
    const currentPath = window.location.pathname;
    const isInPages = currentPath.includes('/pages/');

    const pages = isInPages
      ? ['../index.html', 'checkin.html', 'stats.html', 'reminders.html', 'tasks.html']
      : ['pages/tasks.html', 'pages/checkin.html', 'pages/stats.html', 'pages/reminders.html'];

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
