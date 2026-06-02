/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 *
 * PyTorch Flame Graph — Interactive Canvas Renderer
 *
 * Reads flame graph data from the inline <script id="flamegraph-data"> element
 * in the host HTML and renders an interactive, zoomable flame graph on <canvas>.
 */
(() => {
  // Show a friendly message instead of a blank page when initialization fails.
  const showFatalError = (message) => {
    document.body.innerHTML =
      `<div style="padding:24px;font-family:monospace;color:#d4d4d4;background:#1e1e1e;">`
      + `Failed to load flame graph: ${message}</div>`;
  };

  const dataEl = document.getElementById('flamegraph-data');
  if (!dataEl) {
    showFatalError('flame graph data element not found.');
    throw new Error('flamegraph-data element missing');
  }

  let DATA;
  try {
    DATA = JSON.parse(dataEl.textContent);
  } catch (err) {
    showFatalError('flame graph data is not valid JSON.');
    throw err;
  }

  if (!DATA || !DATA.flamegraph) {
    showFatalError('flame graph data is empty or malformed.');
    throw new Error('DATA.flamegraph missing');
  }

  const CATEGORY_COLORS = {
    root: '#5d4037',
    python: '#ef6c00',
    python_framework: '#f9a825',
    cann: '#c62828',
    unknown: '#8d6e63',
  };
  const ROW_HEIGHT = 22;
  const FONT_SIZE = 12;
  const MIN_LABEL_WIDTH = 60;
  const ANCESTOR_OPACITY = 0.25;
  const MAX_API_STACK_DEPTH = 1000;

  const canvas = document.getElementById('canvas');
  const ctx = canvas.getContext('2d');
  const container = document.getElementById('container');
  const tooltip = document.getElementById('tooltip');
  const searchInput = document.getElementById('searchInput');
  const searchCount = document.getElementById('searchCount');
  const resetBtn = document.getElementById('resetBtn');
  const resetSearchBtn = document.getElementById('resetSearchBtn');
  const threadDropdown = document.getElementById('threadDropdown');
  const threadDropdownToggle = document.getElementById('threadDropdownToggle');
  const threadDropdownMenu = document.getElementById('threadDropdownMenu');

  const metadata = DATA.metadata || {};
  const allRoot = DATA.flamegraph;
  let activeRoot = allRoot;
  let totalDuration = activeRoot.value || metadata.total_duration_us || 0;
  let searchText = '';
  let focusPath = [activeRoot];
  let layoutCache = null;
  let hoveredNode = null;
  let renderScheduled = false;
  let maxDepthWarningShown = false;

  const showMaxDepthWarning = () => {
    if (maxDepthWarningShown) return;
    maxDepthWarningShown = true;
    console.warn(`The flame graph exceeds the max API call stack depth (${MAX_API_STACK_DEPTH}); deeper frames will not be rendered.`);
  };

  const focused = () => focusPath[focusPath.length - 1];

  const scheduleRender = () => {
    if (renderScheduled) return;
    renderScheduled = true;
    requestAnimationFrame(() => {
      renderScheduled = false;
      render();
    });
  };

  const addThreadDropdownItem = (value, label) => {
    const item = document.createElement('div');
    item.className = 'dropdown-item';
    item.dataset.value = value;
    item.textContent = label;
    item.addEventListener('click', () => {
      setActiveThread(value);
      threadDropdown.classList.remove('open');
    });
    threadDropdownMenu.appendChild(item);
  };

  const populateThreadDropdown = () => {
    threadDropdownMenu.innerHTML = '';
    addThreadDropdownItem('__all__', 'All');
    if (!allRoot.children) return;

    const threads = [];
    for (const child of allRoot.children) {
      if (!child.name.startsWith('thread_')) continue;
      const match = child.name.match(/^thread_(\d+)/);
      const tid = match ? parseInt(match[1], 10) : 0;
      threads.push({ name: child.name, tid });
    }

    threads.sort((a, b) => a.tid - b.tid);
    for (const thread of threads) {
      addThreadDropdownItem(thread.name, thread.name);
    }
  };

  const updateThreadDropdownActive = (value) => {
    const items = threadDropdownMenu.querySelectorAll('.dropdown-item');
    for (const item of items) {
      item.classList.toggle('active', item.dataset.value === value);
    }
  };

  const setActiveThread = (threadName) => {
    activeRoot = allRoot;
    if (threadName !== '__all__' && allRoot.children) {
      const thread = allRoot.children.find((child) => child.name === threadName);
      if (thread) activeRoot = thread;
    }

    totalDuration = activeRoot.value || metadata.total_duration_us || 0;
    threadDropdownToggle.textContent = threadName === '__all__' ? 'All' : threadName;
    updateThreadDropdownActive(threadName);
    focusPath = [activeRoot];
    hoveredNode = null;
    tooltip.style.display = 'none';
    layoutCache = null;
    resize(true);
  };

  // Store parent links so zoom paths can be rebuilt without tree-wide searches.
  const initParentLinks = (root) => {
    root._parent = null;

    const stack = [root];
    while (stack.length > 0) {
      const node = stack.pop();
      if (!node.children) continue;

      for (const child of node.children) {
        child._parent = node;
        stack.push(child);
      }
    }
  };

  const isAncestor = (node) => focusPath.slice(0, -1).includes(node);

  // Recreate the backing canvas at device-pixel resolution after viewport changes.
  const resize = (alignBottom) => {
    let width = container.clientWidth;
    if (container.scrollHeight > container.clientHeight) {
      width -= container.offsetWidth - container.clientWidth;
    }

    const depth = getMaxDepth(activeRoot);
    const height = Math.max(depth * ROW_HEIGHT + 20, container.clientHeight, 300);
    const dpr = window.devicePixelRatio || 1;

    canvas.width = width * dpr;
    canvas.height = height * dpr;
    canvas.style.width = `${width}px`;
    canvas.style.height = `${height}px`;
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    layoutCache = null;
    render();

    if (alignBottom) {
      container.scrollTop = Math.max(0, container.scrollHeight - container.clientHeight);
    }
  };

  // Use an explicit stack to avoid recursion limits on very deep flame graphs.
  const getMaxDepth = (root) => {
    let max = 1;
    const stack = [{ node: root, depth: 1 }];

    while (stack.length > 0) {
      const { node, depth } = stack.pop();
      if (depth > max) max = depth;
      if (!node.children) continue;

      for (const child of node.children) {
        stack.push({ node: child, depth: depth + 1 });
      }
    }

    return max;
  };

  // Ancestors in the focus path keep full width; descendants are proportional to duration.
  const layoutTree = (node, x, width, depth) => {
    node._x = x;
    node._w = width;
    node._d = depth;

    if (depth >= MAX_API_STACK_DEPTH) {
      if (node.children && node.children.length > 0) showMaxDepthWarning();
      return;
    }

    for (let i = 0; i < focusPath.length - 1; i++) {
      if (focusPath[i] === node) {
        layoutTree(focusPath[i + 1], x, width, depth + 1);
        return;
      }
    }

    if (!node.children) return;

    let childX = x;
    const totalChildValue = node.children.reduce((sum, child) => sum + child.value, 0);
    const layoutValue = Math.max(node.value, totalChildValue);

    for (const child of node.children) {
      const childWidth = layoutValue > 0 ? (child.value / layoutValue) * width : 0;
      layoutTree(child, childX, childWidth, depth + 1);
      childX += childWidth;
    }
  };

  const getLayout = () => {
    if (layoutCache) return;
    layoutTree(activeRoot, 0, 1.0, 0);
    layoutCache = true;
  };

  // Add a stable name-based variation so adjacent frames in the same category are distinguishable.
  const getColor = (node) => {
    const base = CATEGORY_COLORS[node.category] || CATEGORY_COLORS.unknown;
    let hash = 0;
    for (const char of node.name) {
      hash = ((hash << 5) - hash + char.charCodeAt(0)) | 0;
    }

    const variation = (Math.abs(hash) % 30) - 15;
    const clamp = (value) => Math.max(0, Math.min(255, value));
    const r = clamp(parseInt(base.slice(1, 3), 16) + variation);
    const g = clamp(parseInt(base.slice(3, 5), 16) + variation);
    const b = clamp(parseInt(base.slice(5, 7), 16) + variation);
    return `rgb(${r},${g},${b})`;
  };

  const matchesSearch = (node) => {
    if (!searchText) return true;
    return node.name.toLowerCase().includes(searchText.toLowerCase());
  };

  const countMatches = (node) => {
    if (!searchText) return { total: 0, matched: 0 };

    let total = 0;
    let matched = 0;
    const walk = (current) => {
      total++;
      if (matchesSearch(current)) matched++;
      if (current.children) {
        for (const child of current.children) walk(child);
      }
    };

    walk(node);
    return { total, matched };
  };

  const updateSearchCount = () => {
    if (!searchText) {
      searchCount.style.display = 'none';
      return;
    }

    const counts = countMatches(activeRoot);
    searchCount.textContent = `${counts.matched} / ${counts.total} matched`;
    searchCount.style.display = 'inline-block';
    searchCount.className = counts.matched === 0 ? 'no-match' : '';
  };

  const render = () => {
    getLayout();
    const width = canvas.clientWidth;
    const height = canvas.clientHeight;
    ctx.clearRect(0, 0, width, height);
    ctx.globalAlpha = 1.0;
    drawNode(activeRoot, width, 0);

    if (!hoveredNode) return;

    const rect = getNodeRect(hoveredNode, width);
    if (!rect) return;
    ctx.strokeStyle = '#fff';
    ctx.lineWidth = 2;
    ctx.strokeRect(rect.x, rect.y, rect.w, rect.h);
  };

  // Draw only frames that are large enough and inside the current canvas bounds.
  const drawNode = (node, canvasWidth, depth = 0) => {
    if (depth >= MAX_API_STACK_DEPTH) {
      if (node.children && node.children.length > 0) showMaxDepthWarning();
      return;
    }

    const rect = getNodeRect(node, canvasWidth);
    if (!rect) return;

    const canvasHeight = canvas.clientHeight;
    if (rect.x + rect.w < 0 || rect.x > canvasWidth) return;
    if (rect.y + rect.h < 0 || rect.y > canvasHeight) return;
    if (rect.w < 0.5) return;

    const isAncestorNode = isAncestor(node);
    ctx.globalAlpha = isAncestorNode ? ANCESTOR_OPACITY : 1.0;
    ctx.fillStyle = getColor(node);
    ctx.fillRect(rect.x, rect.y, rect.w, rect.h);

    if (searchText && !matchesSearch(node)) {
      ctx.fillStyle = 'rgba(0,0,0,0.5)';
      ctx.fillRect(rect.x, rect.y, rect.w, rect.h);
    }

    ctx.strokeStyle = '#1e1e1e';
    ctx.lineWidth = 0.5;
    ctx.strokeRect(rect.x, rect.y, rect.w, rect.h);

    if (rect.w >= MIN_LABEL_WIDTH) {
      let label = node.name;
      while (label.length > 3 && ctx.measureText(`${label}...`).width > rect.w - 8) {
        label = label.slice(0, -1);
      }
      if (label.length < node.name.length) label += '...';
      ctx.fillStyle = '#fff';
      ctx.font = `${FONT_SIZE}px monospace`;
      ctx.textBaseline = 'middle';
      ctx.fillText(label, rect.x + 4, rect.y + rect.h / 2);
    }

    if (isAncestorNode) {
      const pathIndex = focusPath.indexOf(node);
      if (pathIndex >= 0 && pathIndex < focusPath.length - 1) {
        drawNode(focusPath[pathIndex + 1], canvasWidth, depth + 1);
      }
      return;
    }

    if (!node.children) return;
    for (const child of node.children) {
      drawNode(child, canvasWidth, depth + 1);
    }
  };

  // Convert normalized layout coordinates into canvas pixels, with depth 0 at the bottom.
  const getNodeRect = (node, canvasWidth) => {
    if (node._w === undefined || node._w < 1e-12) return null;
    const x = node._x * canvasWidth;
    const width = Math.max(node._w * canvasWidth, 1);
    const canvasHeight = canvas.clientHeight;
    return { x, y: canvasHeight - (node._d + 1) * ROW_HEIGHT, w: width, h: ROW_HEIGHT - 1 };
  };

  // Restrict hit testing to the target depth, then follow the focused path when zoomed in.
  const findNodeAt = (x, y, root, canvasWidth) => {
    const canvasHeight = canvas.clientHeight;
    const targetDepth = Math.floor((canvasHeight - y) / ROW_HEIGHT);

    const search = (node, depth = 0) => {
      if (depth >= MAX_API_STACK_DEPTH) {
        if (node.children && node.children.length > 0) showMaxDepthWarning();
        return null;
      }
      if (node._d > targetDepth) return null;
      if (node._d === targetDepth) {
        const rect = getNodeRect(node, canvasWidth);
        return rect && x >= rect.x && x <= rect.x + rect.w ? node : null;
      }

      const pathIndex = focusPath.indexOf(node);
      if (pathIndex >= 0 && pathIndex < focusPath.length - 1) {
        return search(focusPath[pathIndex + 1], depth + 1);
      }

      if (!node.children) return null;
      for (const child of node.children) {
        const result = search(child, depth + 1);
        if (result) return result;
      }
      return null;
    };

    return search(root);
  };

  const formatDuration = (us) => {
    if (us >= 1000000) return `${(us / 1000000).toFixed(2)} s`;
    if (us >= 1000) return `${(us / 1000).toFixed(2)} ms`;
    return `${us.toFixed(2)} us`;
  };

  const formatPercent = (value, total) => {
    if (total === 0) return '0%';
    return `${((value / total) * 100).toFixed(2)}%`;
  };

  const showTooltip = (node, cx, cy) => {
    tooltip.innerHTML = `
      <div class="fname">${escapeHtml(node.name)}<span class="cat cat-${node.category}">${node.category}</span></div>
      <div class="detail">Calls: ${node.count.toLocaleString()}</div>
      <div class="detail">Total: ${formatDuration(node.value)} (${formatPercent(node.value, totalDuration)})</div>
      <div class="detail">Self: ${formatDuration(node.self_time)} (${formatPercent(node.self_time, totalDuration)})</div>
    `;
    tooltip.style.display = 'block';

    let tx = cx + 12;
    let ty = cy - 10;
    if (tx + 300 > window.innerWidth) tx = cx - 300;
    if (ty + 100 > window.innerHeight) ty = cy - 100;
    tooltip.style.left = `${tx}px`;
    tooltip.style.top = `${ty}px`;
  };

  const escapeHtml = (text) => text
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;');

  canvas.addEventListener('mousemove', (event) => {
    const rect = canvas.getBoundingClientRect();
    const node = findNodeAt(
      event.clientX - rect.left,
      event.clientY - rect.top,
      activeRoot,
      canvas.clientWidth,
    );

    if (node) {
      hoveredNode = node;
      canvas.style.cursor = 'pointer';
      showTooltip(node, event.clientX, event.clientY);
    } else {
      hoveredNode = null;
      canvas.style.cursor = 'default';
      tooltip.style.display = 'none';
    }
    scheduleRender();
  });

  canvas.addEventListener('mouseleave', () => {
    hoveredNode = null;
    tooltip.style.display = 'none';
    scheduleRender();
  });

  canvas.addEventListener('click', (event) => {
    const rect = canvas.getBoundingClientRect();
    const node = findNodeAt(
      event.clientX - rect.left,
      event.clientY - rect.top,
      activeRoot,
      canvas.clientWidth,
    );
    if (!node) return;

    if (node === activeRoot) {
      focusPath = [activeRoot];
      layoutCache = null;
      resize(true);
      return;
    }

    const ancestorIndex = focusPath.slice(0, -1).indexOf(node);
    if (ancestorIndex >= 0) {
      focusPath = focusPath.slice(0, ancestorIndex + 1);
      layoutCache = null;
      resize(true);
      return;
    }

    focusPath = buildPath(node);
    layoutCache = null;
    resize(true);
  });

  // Rebuild the zoom path from parent links instead of scanning the full tree.
  const buildPath = (target) => {
    const path = [];
    let node = target;

    while (node) {
      path.push(node);
      if (node === activeRoot) break;
      node = node._parent;
    }

    if (path[path.length - 1] !== activeRoot) return [activeRoot];
    return path.reverse();
  };

  threadDropdownToggle.addEventListener('click', (event) => {
    event.stopPropagation();
    threadDropdown.classList.toggle('open');
  });

  document.addEventListener('click', (event) => {
    if (!threadDropdown.contains(event.target)) {
      threadDropdown.classList.remove('open');
    }
  });

  resetBtn.addEventListener('click', () => {
    focusPath = [activeRoot];
    layoutCache = null;
    resize(true);
  });

  searchInput.addEventListener('input', () => {
    searchText = searchInput.value;
    updateSearchCount();
    scheduleRender();
  });

  resetSearchBtn.addEventListener('click', () => {
    searchInput.value = '';
    searchText = '';
    updateSearchCount();
    scheduleRender();
  });

  document.addEventListener('keydown', (event) => {
    if (event.key !== 'Escape') return;

    if (searchText) {
      searchInput.value = '';
      searchText = '';
      updateSearchCount();
      scheduleRender();
    } else {
      resetBtn.click();
    }
  });

  window.addEventListener('resize', () => resize(true));

  initParentLinks(allRoot);
  populateThreadDropdown();
  updateThreadDropdownActive('__all__');
  resize(true);
})();
