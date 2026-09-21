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
 */
// 在页面加载完成后执行
window.addEventListener('DOMContentLoaded', function () {
    // 从 Sphinx 上下文获取 GitCode URL
    const gitcodeUrl = window.SPHINX_CONTEXT?.gitcode_url;

    // 如果未配置，则不创建链接
    if (!gitcodeUrl) return;

    // 创建链接容器
    const linksContainer = document.createElement('div');
    linksContainer.className = 'project-links';

    // 创建GitCode仓库链接
    const gitcodeLink = document.createElement('a');
    gitcodeLink.href = gitcodeUrl; // GitCode仓库地址
    gitcodeLink.target = '_blank';
    gitcodeLink.className = 'project-link';
    gitcodeLink.innerHTML = '<i class="fa fa-git"></i> GitCode';

    // 添加链接到容器
    linksContainer.appendChild(gitcodeLink);

    // 查找放置链接的位置 - 在project信息下方
    const projectNameElement = document.querySelector('.wy-side-nav-search > a');
    if (projectNameElement) {
        projectNameElement.parentNode.appendChild(linksContainer);
    } else {
        // 如果找不到特定位置，则尝试添加到页面顶部
        const headerElement = document.querySelector('.wy-side-nav-search');
        if (headerElement) {
            headerElement.appendChild(linksContainer);
        }
    }
});
