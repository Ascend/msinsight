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

document.addEventListener("DOMContentLoaded", function() {
    var network_container = document.getElementById("mynetwork");
    if (!network_container) return;

    var network = window.network;
    if (!network) return;

    var NPU_NODE_PREFIX = "NPU_";
    // 存储原始状态
    var originalEdges = {};
    var originalNodes = {};
    var fixedHighlightedNode = null;
    var edgesInitialized = false;

    // 初始化：隐藏所有 NPU-NPU 边
    function initHideNpuEdges() {
        if (edgesInitialized) return;
        var allEdges = network.body.data.edges.get();
        allEdges.forEach(function(edge) {
            if (isNpuEdge(edge)) {
                originalEdges[edge.id] = {
                    color: edge.color ? edge.color : '#97C2FC',
                    width: edge.width || 1,
                    dashes: edge.dashes || false
                };
                network.body.data.edges.update({
                    id: edge.id,
                    color: {color: '#000000', opacity: 0},
                    width: 0
                });
            }
        });

        var allNodes = network.body.data.nodes.get();
        allNodes.forEach(function(node) {
            if (node.id.toString().startsWith(NPU_NODE_PREFIX)) {
                originalNodes[node.id] = {
                    color: node.color?.background || '#FF5722',
                    size: node.size || 20
                };
            }
        });
        edgesInitialized = true;
    }

    // 判断是否为 NPU-NPU 边
    function isNpuEdge(edge) {
        return edge.from && edge.to &&
            edge.from.toString().startsWith(NPU_NODE_PREFIX) &&
            edge.to.toString().startsWith(NPU_NODE_PREFIX);
    }

    // 显示指定节点的关联边和节点
    function showConnections(nodeId, isFixed) {
        // 高亮相连节点
        var connectedNodes = [];
        var connectedEdges = network.getConnectedEdges(nodeId);

        connectedEdges.forEach(function(edgeId) {
            var edge = network.body.data.edges.get(edgeId);
            if (edge && isNpuEdge(edge)) {
                // 恢复边样式
                var origEdge = originalEdges[edgeId];
                if (origEdge) {
                    network.body.data.edges.update({
                        id: edgeId,
                        color: origEdge.color,
                        width: origEdge.width,
                        dashes: origEdge.dashes
                    });
                }

                // 获取相连节点
                var otherNode = edge.from == nodeId ? edge.to : edge.from;
                connectedNodes.push(otherNode);
            }
        });

        // 高亮相连节点
        connectedNodes.forEach(function(nid) {
            var origNode = originalNodes[nid];
            if (origNode) {
                network.body.data.nodes.update({
                    id: nid,
                    color: {
                        background: isFixed ? '#FF9E80' : '#FFD54F', // 固定高亮 vs 临时高亮
                        border: '#FF5722'
                    },
                    size: origNode.size + 5
                });
            }
        });
    }

    // 隐藏指定节点的关联边和节点
    function hideConnections(nodeId) {
        var connectedEdges = network.getConnectedEdges(nodeId);
        connectedEdges.forEach(function(edgeId) {
            var edge = network.body.data.edges.get(edgeId);
            if (edge && isNpuEdge(edge)) {
                network.body.data.edges.update({
                    id: edgeId,
                    color: {color: '#000000', opacity: 0},
                    width: 0
                });
            }
        });

        var connectedNodes = [];
        connectedEdges.forEach(function(edgeId) {
            var edge = network.body.data.edges.get(edgeId);
            if (edge) {
                var otherNode = edge.from == nodeId ? edge.to : edge.from;
                connectedNodes.push(otherNode);
            }
        });

        connectedNodes.forEach(function(nid) {
            var origNode = originalNodes[nid];
            if (origNode) {
                network.body.data.nodes.update({
                    id: nid,
                    color: {background: origNode.color, border: '#2B7CE9'},
                    size: origNode.size
                });
            }
        });
    }

    // 初始化
    setTimeout(initHideNpuEdges, 30);

    // === 事件绑定 ===
    // 悬停预览（临时）
    network.on("hoverNode", function(params) {
        if (fixedHighlightedNode) return; // 已有固定高亮时禁用悬停
        var nodeId = params.node;
        if (nodeId && nodeId.toString().startsWith(NPU_NODE_PREFIX)) {
            showConnections(nodeId, false);
        }
    });

    network.on("blurNode", function(params) {
        if (fixedHighlightedNode) return;
        var nodeId = params.node;
        if (nodeId && nodeId.toString().startsWith(NPU_NODE_PREFIX)) {
            hideConnections(nodeId);
        }
    });

    // 点击固定高亮
    network.on("click", function(params) {
        // 点击空白处：清除固定高亮
        if (params.nodes.length === 0) {
            if (fixedHighlightedNode) {
                hideConnections(fixedHighlightedNode);
                fixedHighlightedNode = null;
            }
            return;
        }

        var clickedNode = params.nodes[0];
        if (!clickedNode.toString().startsWith(NPU_NODE_PREFIX)) return;

        // 如果点击已固定的节点，取消高亮
        if (fixedHighlightedNode === clickedNode) {
            hideConnections(clickedNode);
            fixedHighlightedNode = null;
            return;
        }

        // 清除之前的固定高亮
        if (fixedHighlightedNode) {
            hideConnections(fixedHighlightedNode);
        }

        // 设置新固定高亮
        showConnections(clickedNode, true);
        fixedHighlightedNode = clickedNode;
    });
});
