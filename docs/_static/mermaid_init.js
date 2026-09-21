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
// 等待DOM加载完成后初始化mermaid
document.addEventListener('DOMContentLoaded', function() {
    // 配置mermaid
    mermaid.initialize({
        startOnLoad: true,
        theme: 'default',
        flowchart: {
            useMaxWidth: true,
            htmlLabels: true
        }
    });

    // 查找所有带有mermaid标记的<pre>或<code>元素并手动渲染
    const mermaidBlocks = document.querySelectorAll('pre code.language-mermaid, div[class*="mermaid"]');
    mermaidBlocks.forEach((block, index) => {
        try {
            // 获取mermaid图表代码
            let code = block.textContent || block.innerText;

            // 创建一个新的div元素来容纳渲染后的图表
            const newDiv = document.createElement('div');
            newDiv.className = 'mermaid';
            newDiv.id = 'mermaid-diagram-' + index;
            newDiv.textContent = code;

            // 替换原元素或在其旁边插入新元素
            block.parentNode.insertBefore(newDiv, block);

            // 隐藏原始代码块
            block.style.display = 'none';

        } catch (error) {
            console.error('Error rendering mermaid diagram:', error);
        }
    });

    // 确保所有mermaid图表都被渲染
    mermaid.run();
});
