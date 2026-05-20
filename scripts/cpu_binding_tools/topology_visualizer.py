#!/usr/bin/env python
# -*- coding: UTF-8 -*-

"""
-------------------------------------------------------------------------
This file is part of the MindStudio project.
Copyright (c) 2026 Huawei Technologies Co.,Ltd.

MindStudio is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:

        http://license.coscl.org.cn/MulanPSL2

THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details.
-------------------------------------------------------------------------
"""

# pylint: disable=logging-fstring-interpolation,use-implicit-booleaness-not-comparison
# CPU、NPU、NUMA / CPU、NPU Topology Visualizer

import subprocess  # nosec B404
import re
import math
import logging
import networkx as nx
from pyvis.network import Network
from IPython.display import IFrame
from collections import defaultdict
from cpu_binding_utils import LoggerUtils, FileUtils


class AscendTopologyVisualizer:
    def __init__(self):
        self.logger = LoggerUtils.setup_logger(self.__class__.__name__, logging.INFO)
        self.graph = nx.Graph()
        self.system_info = {"numa_nodes": {}, "npus": [], "interconnects": []}
        self._numa_assignment_counter = 0  # 用于均匀分配 NPU 到 NUMA 的计数器

        # === 节点配色方案 ===
        self.NODE_COLORS = {
            "server": "#455A64",  # 深灰蓝
            "numa": "#4CAF50",  # 清新绿
            "npu": "#FF5722",  # 温柔橙红
        }

        # === 连接样式（柔和、低饱和度）===
        self.LINK_STYLES = {
            "HCCS": {"color": "#FF9E80", "width": 3, "dashes": False, "title": "HCCS (High Speed Interconnect)"},
            "HCCS_SW": {"color": "#FFB74D", "width": 2.5, "dashes": False, "title": "HCCS via Switch"},
            "PIX": {"color": "#64B5F6", "width": 2, "dashes": False, "title": "PIX (Single PCIe Switch)"},
            "PXB": {"color": "#81C784", "width": 2, "dashes": False, "title": "PXB (Multiple PCIe Switches)"},
            "PHB": {"color": "#BA68C8", "width": 1.5, "dashes": False, "title": "PHB (PCIe Host Bridge)"},
            "SYS": {"color": "#BDBDBD", "width": 1, "dashes": True, "title": "SYS (Cross NUMA/QPI/SMP - Slow)"},
            "SIO": {"color": "#FFD54F", "width": 1.5, "dashes": False, "title": "SIO Bus"},
            "NA": {"color": "#E0E0E0", "width": 1, "dashes": [5, 5], "title": "Unknown Connection"},
        }

    def _run_cmd(self, cmd):
        try:
            result = subprocess.check_output(  # nosec B603
                cmd,
                timeout=5,
                stderr=subprocess.STDOUT,
            )
            return result.decode("utf-8", errors="ignore").strip()

        except subprocess.TimeoutExpired:
            self.logger.error(f"Command timeout after 5s: {cmd}")
            return None
        except subprocess.CalledProcessError as e:
            self.logger.error(
                f"Command failed (exit {e.returncode}): {cmd}\n"
                f"Output: {e.output.decode('utf-8', errors='ignore').strip()}"
            )
            return None
        except FileNotFoundError:
            self.logger.error(f"Command not found: {cmd}")
            return None
        except Exception as e:
            self.logger.error(f"Unexpected error running command {cmd}: {str(e)}")
            return None

    def get_cpu_numa_info(self):
        """1. 获取 NUMA 信息"""
        output = self._run_cmd(["lscpu"])
        if not output:
            self.logger.error("Failed to run cmd lscpu, please check it!")
            return

        for line in output.split('\n'):
            if "NUMA node" in line and "CPU(s)" in line:
                parts = line.split("CPU(s):")
                if len(parts) == 2 and "node" in parts[0]:
                    try:
                        match = re.search(r'node(\d+)', parts[0])
                        if not match:
                            raise ValueError(f"Invalid node format: {parts[0]}")
                        node_id = int(match.group(1))
                        self.system_info["numa_nodes"][node_id] = parts[1].strip()
                    except (IndexError, ValueError, TypeError) as e:
                        self.logger.error(f"Failed to parse NUMA node info, parts={parts}, error={str(e)}")
                        return

    def _map_affinity_to_numa(self, affinity_str):
        """从 affinity 字符串映射到 NUMA node; 若失败, 返回 None"""
        if not affinity_str or affinity_str.strip() in ("N/A", "-", ""):
            return None  # 表示无法确定
        try:
            first_core = int(re.split(r'[-,\s]', affinity_str)[0])
            for node_id, core_range in self.system_info["numa_nodes"].items():
                if '-' in core_range:
                    s, e = map(int, core_range.split('-'))
                    if s <= first_core <= e:
                        return node_id
        except Exception as e:
            self.logger.error(f"Unexpected error mapping affinity to numa: {str(e)}")
        return None

    def _gen_npu_and_affinity_index(self, header):
        npu_col_indices, affinity_idx = {}, -1
        for i, col in enumerate(header):
            if col.startswith("NPU"):
                try:
                    nid = int(col.replace("NPU", ""))
                    npu_col_indices[nid] = i
                except ValueError:
                    continue
            elif col.startswith("Phy-ID"):
                try:
                    nid = int(col.replace("Phy-ID", ""))
                    npu_col_indices[nid] = i
                except ValueError:
                    continue

        for i, col in enumerate(header):
            if "Affinity" in col or "NUMA" in col:
                affinity_idx = i

        if not npu_col_indices:
            self.logger.warning(
                "Failed to generate npu column index cause npu columns not start with 'NPU' or 'Phy-ID'"
            )
        if affinity_idx == -1:
            self.logger.warning(
                "Failed to generate affinity index cause the current device not include CPU NUMA Affinity, "
                "and the npu will distribute evenly across all cpu cores."
            )
        return npu_col_indices, affinity_idx

    def _get_cur_npu_id(self, first_col):
        # 支持两种前缀：NPU 或 Phy-ID
        curr_id = None

        if first_col.startswith("NPU"):
            try:
                curr_id = int(first_col.replace("NPU", ""))
            except ValueError:
                return None
        elif first_col.startswith("Phy-ID"):
            try:
                curr_id = int(first_col.replace("Phy-ID", "").strip())
            except ValueError:
                return None

        return curr_id

    def get_npu_info(self):
        """2. 解析 npu-smi info -t topo"""
        topo_output = self._run_cmd(["npu-smi", "info", "-t", "topo"])
        if not topo_output:
            self.logger.error("Failed to run cmd npu-smi info -t topo, please check it!")
            return

        lines = [line.strip() for line in topo_output.strip().split('\n') if line.strip()]
        if not lines or len(lines) < 2:
            self.logger.error("Failed to get npu-smi topo info.")
            return

        # 生成topo结果中npu的列索引、id字典 + cpu亲和列索引
        npu_col_indices, affinity_idx = self._gen_npu_and_affinity_index(lines[0].split())

        for line in lines[1:]:
            parts = line.split()
            if not parts:
                continue

            curr_id = self._get_cur_npu_id(parts[0])
            # 如果无法解析出有效 ID，跳过该行
            if curr_id is None:
                continue

            affinity_str = "N/A"
            target_numa = None
            if affinity_idx != -1 and affinity_idx < len(parts):
                affinity_str = parts[affinity_idx]
                target_numa = self._map_affinity_to_numa(affinity_str)

            # 如果没有 affinity，均匀分配到 NUMA 节点 ===
            if target_numa is None:
                numa_ids = sorted(self.system_info["numa_nodes"].keys())
                if numa_ids:
                    # 轮询分配：第 i 个 NPU 分配给 numa_ids[i % len(numa_ids)]
                    target_numa = numa_ids[self._numa_assignment_counter % len(numa_ids)]
                    self._numa_assignment_counter += 1
                else:
                    target_numa = 0  # fallback

            if not any(n['id'] == curr_id for n in self.system_info["npus"]):
                self.system_info["npus"].append(
                    {"id": curr_id, "name": f"NPU-{curr_id}", "affinity": affinity_str, "numa_node": target_numa}
                )

            # 提取连接
            for target_id, col_idx in npu_col_indices.items():
                if curr_id >= target_id:
                    continue  # 只处理上三角
                if col_idx >= len(parts):
                    continue
                link_type = parts[col_idx + 1]
                if link_type in ('X', 'NA'):
                    continue
                self.system_info["interconnects"].append({"src": curr_id, "dst": target_id, "type": link_type})

    def build_graph(self):
        """3. 构建拓扑图"""
        G = self.graph
        G.clear()

        R_NUMA = 250
        R_NPU = 500

        # Server 中心
        G.add_node(
            "Server",
            label="Server",
            color=self.NODE_COLORS["server"],
            shape="star",
            size=30,
            x=0,
            y=0,
            physics=False,
            borderWidth=2,
            borderWidthSelected=3,
            shadow={"enabled": True, "size": 5},
        )

        numa_ids = sorted(self.system_info["numa_nodes"].keys())
        numa_angles = {}
        if numa_ids:
            step = 2 * math.pi / len(numa_ids)
            for i, nid in enumerate(numa_ids):
                angle = i * step - math.pi / 2
                x, y = R_NUMA * math.cos(angle), R_NUMA * math.sin(angle)
                numa_angles[nid] = angle
                G.add_node(
                    f"NUMA_{nid}",
                    label=f"NUMA {nid}\nCores:{self.system_info['numa_nodes'][nid]}",
                    title=f"CPU Cores: {self.system_info['numa_nodes'][nid]}",
                    color=self.NODE_COLORS["numa"],
                    shape="hexagon",
                    size=22,
                    x=x,
                    y=y,
                    physics=False,
                    borderWidth=1.5,
                    shadow={"enabled": True, "size": 3},
                )

                G.add_edge("Server", f"NUMA_{nid}", color="#B2DFDB", width=2, smooth={"type": "continuous"})

        # NPU 按 NUMA 分扇区
        npus_by_numa = defaultdict(list)
        for npu in self.system_info["npus"]:
            npus_by_numa[npu['numa_node']].append(npu)

        if numa_ids:
            sector_angle = 2 * math.pi / len(numa_ids)
        else:
            sector_angle = 2 * math.pi

        for nid, npus in npus_by_numa.items():
            base_angle = numa_angles.get(nid, 0)
            count = len(npus)
            if count == 0:
                continue

            sub_sector = sector_angle * 0.8
            step = sub_sector / max(1, count - 1) if count > 1 else 0

            for i, npu in enumerate(npus):
                offset = (i - (count - 1) / 2) * step
                angle = base_angle + offset
                x, y = R_NPU * math.cos(angle), R_NPU * math.sin(angle)

                n_id = f"NPU_{npu['id']}"
                G.add_node(
                    n_id,
                    label=npu['name'],
                    title=f"CPU Affinity: {npu['affinity']}",
                    color=self.NODE_COLORS["npu"],
                    shape="box",
                    size=18,
                    x=x,
                    y=y,
                    physics=False,
                    font={'color': 'white', 'size': 14},
                    borderWidth=1,
                    borderWidthSelected=2,
                    shadow={"enabled": True, "size": 3},
                )

                if f"NUMA_{nid}" in G.nodes:
                    G.add_edge(
                        f"NUMA_{nid}",
                        n_id,
                        color="#C5E1A5",
                        width=1.5,
                        dashes=True,
                        title=f"Affinity to NUMA {nid}",
                        smooth={"type": "continuous"},
                    )

        # NPU 互联
        for link in self.system_info["interconnects"]:
            src = f"NPU_{link['src']}"
            dst = f"NPU_{link['dst']}"
            ltype = link['type']
            style = self.LINK_STYLES.get(ltype, self.LINK_STYLES["NA"])
            G.add_edge(
                src,
                dst,
                color=style["color"],
                width=style["width"],
                dashes=style["dashes"],
                title=f"{ltype}: {style['title']}",
                smooth={"type": "continuous"},
            )

    def visualize(self, output_file="ascend_topo.html"):
        net = Network(height="800px", width="100%", bgcolor="#fafafa", notebook=True, cdn_resources='remote')
        net.toggle_physics(False)
        net.from_nx(self.graph)

        # 设置基础选项（纯 JSON 兼容）
        net.set_options("""
        {
        "edges": {
            "smooth": {"type": "continuous"},
            "color": {"inherit": true}
        },
        "physics": {"enabled": false},
        "interaction": {
            "hover": true,
            "hoverConnectedEdges": false,
            "tooltipDelay": 200
        }
        }
        """)

        # 文件查看
        try:
            # 生成 HTML 文件
            net.write_html(output_file)
            self.logger.info(f"正在处理拓扑图文件: {output_file}")

            # 读取拓扑文件
            html_content = FileUtils.safe_read_file(self.logger, output_file, operation_name="读取拓扑图HTML")
            if html_content is None:
                return None

            network_assignments = (
                "network = new vis.Network(container, data, options);",
                "var network = new vis.Network(container, data, options);",
            )
            for assignment in network_assignments:
                if assignment in html_content:
                    html_content = html_content.replace(
                        assignment, f"{assignment}\n                  window.network = network;", 1
                    )
                    break

            # 读取拓扑文件样式
            js_content = FileUtils.safe_read_file(self.logger, "topo_interactions.js", operation_name="读取拓扑图样式")
            if js_content is None:
                return None

            # 注入 JS
            if '</body>' not in html_content:
                self.logger.warning("HTML 中未找到 </body> 标签, JS 可能未正确覆盖.")
                final_content = html_content + f'<script>{js_content}</script>'
            else:
                final_content = html_content.replace('</body>', f'<script>{js_content}</script></body>')

            # 写入拓扑文件样式
            if not FileUtils.safe_write_file(self.logger, output_file, final_content):
                return None

            self.logger.info(f"✅ 拓扑图已保存: {output_file}")

            try:
                return IFrame(output_file, width="100%", height="850px")
            except ImportError:
                self.logger.info("非 Jupyter 环境，文件已保存")
                return None
        except Exception as e:
            self.logger.exception(f"❌ 可视化过程失败: {str(e)}")
            return None


def main():
    viz = AscendTopologyVisualizer()
    viz.get_cpu_numa_info()
    viz.get_npu_info()
    viz.build_graph()
    html_file = "ascend_topo.html"
    viz.visualize(html_file)


if __name__ == "__main__":
    main()
