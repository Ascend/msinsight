#!/usr/bin/env python3
# -------------------------------------------------------------------------
# This file is part of the MindStudio project.
# Copyright (c) 2026 Huawei Technologies Co.,Ltd.
#
# MindStudio is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#
#          http://license.coscl.org.cn/MulanPSL2
#
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.
# -------------------------------------------------------------------------
"""
容器内 PID 映射工具类
功能：将宿主机 PID 列表映射为容器内对应进程的 PID 列表
特点：
  - 自动识别 --pid=host 模式 vs 独立 PID 命名空间
  - 环境验证前置（内核/NSpid/ps 命令）
  - 批量映射（输入宿主机 PID 列表 → 输出容器内 PID 列表）
  - 详细异常诊断（含修复建议）
  - 静默跳过瞬时失效进程（无冗余日志）
  - 保留原始输入顺序，缺失项返回 None
"""

import sys
import os
import re
import subprocess  # nosec B404
from typing import List, Optional, Dict, Union


class ContainerPidMapper:
    """容器内宿主机 PID → 容器内 PID 映射器"""

    def __init__(self, verbose: bool = False):
        """
        初始化映射器
        :param validate: 是否执行环境验证（建议保持 True）
        :param verbose: 是否输出构建过程诊断信息到 stderr
        :raises RuntimeError: 环境不满足要求时抛出（含详细修复指引）
        """
        self.verbose = verbose
        self.is_pid_host_mode: Optional[bool] = None
        self.host_to_container_map: Dict[str, int] = {}  # {宿主机PID_str: 容器内PID_int}

        self._build_mapping()

    def _validate_environment(self, pid) -> None:
        """关键环境验证（失败时抛出含修复建议的 RuntimeError）"""
        # === 1. 验证 NSpid 字段存在性与命名空间模式 ===
        try:
            with open(f'/proc/{pid}/status', 'r') as f:  # pylint: disable=unspecified-encoding
                for line in f:
                    if line.startswith('NSpid:'):
                        parts = line.split()
                        # 单列 = --pid=host 模式；多列 = 独立 PID 命名空间
                        self.is_pid_host_mode = len(parts) == 1
                        if self.verbose:
                            mode = "PID host 模式（共享命名空间）" if self.is_pid_host_mode else "独立 PID 命名空间"
                            print(f"✅ 检测到: {mode}", file=sys.stderr)
                        break
                else:  # 未找到 NSpid 行
                    self._diagnose_missing_nspid()
        except PermissionError:
            raise RuntimeError(
                "❌ 无权限读取 /proc/1/status\n   请确保容器以足够权限运行（非 dropped CAP_SYS_PTRACE 等）"
            )
        except FileNotFoundError:
            raise RuntimeError("❌ /proc 文件系统异常（容器环境损坏）")

        # === 2. 验证 ps 命令可用性 ===
        try:
            subprocess.run(['ps', '--version'], capture_output=True, timeout=2, check=True, text=True)  # nosec B603 B607
        except FileNotFoundError:
            raise RuntimeError(
                "❌ 'ps' 命令未找到\n"
                "   请在容器内安装 procps:\n"
                "     • Alpine: apk add procps\n"
                "     • Debian/Ubuntu: apt-get update && apt-get install -y procps\n"
                "     • CentOS/RHEL: yum install -y procps-ng"
            )
        except subprocess.TimeoutExpired:
            raise RuntimeError("❌ 'ps' 命令执行超时（容器资源受限）")
        except subprocess.CalledProcessError as e:
            raise RuntimeError(f"❌ 'ps' 命令执行失败: {e}")

    def _diagnose_missing_nspid(self) -> None:
        """NSpid 字段缺失时的深度诊断"""
        try:
            with open('/proc/version', 'r') as f:  # pylint: disable=unspecified-encoding
                ver_line = f.read()
            match = re.search(r'Linux version (\d+)\.(\d+)', ver_line)
            if match:
                major, minor = int(match.group(1)), int(match.group(2))
                if major < 4 or (major == 4 and minor < 1):
                    raise RuntimeError(
                        f"❌ 内核版本过低 ({major}.{minor})\n"
                        "   NSpid 字段需 Linux ≥4.1（2015年发布）\n"
                        "   建议：升级宿主机内核或使用宿主机工具查询（如 docker top）"
                    )
        except Exception:  # nosec B110
            pass
        raise RuntimeError(
            "❌ /proc/1/status 中无 NSpid 字段\n"
            "   可能原因:\n"
            "     • 内核 <4.1（已尝试检测）\n"
            "     • 容器以 --pid=host 启动但内核异常（罕见）\n"
            "     • 安全模块限制（SELinux/AppArmor）\n"
            "   建议：宿主机执行 `docker inspect <容器> | grep -i pidmode` 确认"
        )

    def _get_all_pids(self) -> List[str]:
        """安全获取容器内所有进程 PID（字符串列表）"""
        # 优先使用 --no-headers 避免标题行
        cmds = [
            ['ps', '-eo', 'pid,comm', '--no-headers'],
            ['ps', '-eo', 'pid,comm'],  # 回退方案
        ]

        output = ""
        for cmd in cmds:
            try:
                result = subprocess.run(cmd, capture_output=True, text=True, timeout=3, check=True)  # nosec B603
                output = result.stdout
                if '--no-headers' not in cmd:
                    output = '\n'.join(output.strip().splitlines()[1:])  # 跳过标题
                break
            except (subprocess.TimeoutExpired, subprocess.CalledProcessError, FileNotFoundError):
                continue
        else:
            raise RuntimeError("无法执行 ps 命令获取进程列表（尽管验证通过）")

        all_pids = []
        for line in output.strip().splitlines():
            parts = line.split(maxsplit=1)
            if len(parts) == 2 and parts[0].isdigit():
                all_pids.append(parts[0])
        return all_pids

    def _build_mapping(self) -> None:
        """构建宿主机 PID → 容器内 PID 映射字典"""
        try:
            all_pids = self._get_all_pids()
        except Exception as e:
            if self.verbose:
                print(f"⚠️  获取进程列表警告: {e}（映射可能不完整）", file=sys.stderr)
            all_pids = []

        if len(all_pids) > 0:
            self._validate_environment(all_pids[0])

        if self.is_pid_host_mode:
            # --pid=host 模式：容器内 PID = 宿主机 PID
            self.host_to_container_map = {pid: int(pid) for pid in all_pids}
            if self.verbose:
                print(f"🛠️  构建映射 (PID host 模式): {len(all_pids)} 个进程", file=sys.stderr)
        else:
            # 独立 PID 命名空间：解析 NSpid 最后一列
            mapping = {}
            for host_pid in all_pids:
                status_path = f'/proc/{host_pid}/status'
                if not os.path.exists(status_path):
                    continue
                try:
                    with open(status_path, 'r') as f:  # pylint: disable=unspecified-encoding
                        for line in f:
                            if line.startswith('NSpid:'):
                                parts = line.split()
                                if len(parts) >= 2:
                                    container_pid = parts[-1].strip()
                                    mapping[host_pid] = int(container_pid)  # 后覆盖（理论无重复）
                                break
                except (OSError, PermissionError):
                    continue  # 静默跳过（进程退出/权限问题属正常现象）
            self.host_to_container_map = mapping
            if self.verbose:
                print(f"🛠️  构建映射 (独立命名空间): {len(mapping)} 个进程映射", file=sys.stderr)

    def map_container_pids(self, host_pids: List[Union[int, str]]) -> List[Optional[int]]:  # pylint: disable=redefined-outer-name
        """
        批量映射宿主机 PID → 容器内 PID
        :param container_pids: 宿主机 PID 列表（支持 int 或 str 类型）
        :return: 容器内 PID 列表（顺序与输入一致），未匹配项返回 None
        """
        result = []
        for hp in host_pids:
            hp_clean = str(hp).strip()
            result.append(self.host_to_container_map.get(hp_clean))
        return result

    def get_full_mapping(self) -> Dict[str, int]:
        """返回当前构建的完整映射字典（宿主机PID_str → 容器内PID_str）"""
        return self.host_to_container_map.copy()

    def refresh(self) -> None:
        """重新构建映射（应对容器内进程动态变化）"""
        self._build_mapping()


# ==================== 命令行使用示例（保留脚本调用能力） ====================
if __name__ == '__main__':
    if len(sys.argv) < 2:
        print(f"用法: {sys.argv[0]} <宿主机_PID1> [宿主机_PID2 ...]\n", file=sys.stderr)
        print("功能: 批量查询宿主机 PID 对应的容器内进程 PID", file=sys.stderr)
        print("示例: python mapper.py 12345 67890", file=sys.stderr)
        sys.exit(1)

    try:
        # 创建映射器（verbose=True 便于调试）
        mapper = ContainerPidMapper(verbose=True)

        # 执行映射
        host_pids = sys.argv[1:]
        container_pids = mapper.map_container_pids(host_pids)

        # 标准输出：严格按输入顺序，每行一个结果（便于脚本解析）
        # 成功: 容器内PID；失败: 空行（或按需改为 "None"）
        for cp in container_pids:
            print(cp if cp is not None else "")

        # 可选：输出统计到 stderr（不影响 stdout 解析）
        matched = sum(1 for p in container_pids if p is not None)
        if matched < len(host_pids):
            print(f"\n⚠️  提示: {matched}/{len(host_pids)} 个 PID 匹配成功", file=sys.stderr)
            if matched == 0:
                print(
                    "   可能原因:\n"
                    "     • 进程 /proc/*/status 内 NSpid 为 0\n"
                    "     • 进程已退出\n"
                    "     • PID 不属于当前容器\n"
                    "   调试: 在容器内执行 `grep NSpid /proc/*/status 2>/dev/null | grep <宿主机PID>`",
                    file=sys.stderr,
                )

    except RuntimeError as e:
        print(f"\n{str(e)}", file=sys.stderr)
        sys.exit(1)
    except KeyboardInterrupt:
        print("\n❌ 操作被用户中断", file=sys.stderr)
        sys.exit(130)
