#!/bin/bash
# 把 cpp_coverage.sh 的覆盖率报告提交到 qa-test-assets，并把汇总回帖到 MR。
# cpp_coverage.sh 在汇总完成后默认调用本脚本；也可单独执行。
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
# 归档：msinsight-org/qa-test-assets 的 server/cpp-ut/coverage-reports/<日期>_pr<MR>_<短提交>/
# 评论：读取 coverage_summary.md，缺 pr_id 或缺令牌即跳过，默认不影响作业结果。
#   pr_id / GITCODE_PR_ID     目标 MR 序号
#   GITCODE_TOKEN / gitcode_token  个人访问令牌；只经请求头或 git askpass 传递
#   GITCODE_REPO              产品仓库路径，默认从 origin 解析
#   GITCODE_API               默认 https://api.gitcode.com/api/v5
#   COVERAGE_ASSET_REPO       归档仓，默认 msinsight-org/qa-test-assets
#   COVERAGE_ASSET_BRANCH     归档分支，默认 main
#   COVERAGE_ASSET            auto / dry-run / off / required
#   COVERAGE_COMMENT          auto / dry-run / off / required
#   COVERAGE_ASSET_URL        若已有归档地址则跳过提交，只用于评论

set -e
CUR_DIR=$(dirname $(readlink -f $0))
TOP_DIR=${CUR_DIR}/..
COV_DIR=${TOP_DIR}/build_llt/output/cpp_coverage
ASSET_REPO=${COVERAGE_ASSET_REPO:-msinsight-org/qa-test-assets}
ASSET_BRANCH=${COVERAGE_ASSET_BRANCH:-main}
ASSET_PREFIX=server/cpp-ut/coverage-reports
token="${GITCODE_TOKEN:-${gitcode_token:-}}"
pr_id="${pr_id:-${GITCODE_PR_ID:-}}"
asset_mode=$(echo "${COVERAGE_ASSET:-auto}" | tr 'A-Z' 'a-z')
comment_mode=$(echo "${COVERAGE_COMMENT:-auto}" | tr 'A-Z' 'a-z')

git_in_src() {
    git -C "${TOP_DIR}" "$@" 2>/dev/null || true
}

short_sha=$(git_in_src rev-parse --short=9 HEAD)
run_date=$(date +%Y-%m-%d)
if [ -n "${pr_id}" ] && [ -n "${short_sha}" ]; then
    run_id="${run_date}_pr${pr_id}_${short_sha}"
elif [ -n "${short_sha}" ]; then
    run_id="${run_date}_${short_sha}"
else
    run_id="${run_date}"
fi
asset_url="${COVERAGE_ASSET_URL:-}"

# 令牌只进 askpass，不写进 remote URL，避免落进 .git/config。
git_with_token() {
    askpass="$1"
    shift
    GIT_ASKPASS="${askpass}" GIT_TERMINAL_PROMPT=0 \
        GITCODE_ASKPASS_TOKEN="${token}" git "$@"
}

write_askpass() {
    path="$1"
    cat > "${path}" <<'EOF'
#!/bin/sh
case "$1" in
    *[Uu]sername*) echo oauth2 ;;
    *) echo "${GITCODE_ASKPASS_TOKEN}" ;;
esac
EOF
    chmod 700 "${path}"
}

archive_report() {
    if [ "${asset_mode}" = "off" ] || [ "${asset_mode}" = "none" ] || \
        [ "${asset_mode}" = "disable" ] || [ "${asset_mode}" = "disabled" ]; then
        echo "[asset] COVERAGE_ASSET=${asset_mode}，跳过归档"
        return 0
    fi
    if [ -n "${asset_url}" ]; then
        echo "[asset] 已提供 COVERAGE_ASSET_URL，跳过提交"
        return 0
    fi
    if [ ! -f "${COV_DIR}/coverage_summary.md" ]; then
        echo "[asset] 未找到 coverage_summary.md，跳过归档" >&2
        return 1
    fi
    if [ -z "${token}" ]; then
        echo "[asset] 未提供令牌（GITCODE_TOKEN / gitcode_token），跳过归档"
        return 0
    fi

    dest_rel="${ASSET_PREFIX}/${run_id}"
    browse_url="https://gitcode.com/${ASSET_REPO}/tree/${ASSET_BRANCH}/${dest_rel}"
    if [ "${asset_mode}" = "dry-run" ] || [ "${asset_mode}" = "dryrun" ] || \
        [ "${asset_mode}" = "print" ]; then
        echo "[asset] dry-run：将提交到 ${ASSET_REPO}:${dest_rel}"
        asset_url="${browse_url}"
        return 0
    fi

    work=$(mktemp -d)
    askpass="${work}/askpass"
    write_askpass "${askpass}"
    trap 'rm -rf "${work}"' RETURN

    clone_url="https://gitcode.com/${ASSET_REPO}.git"
    echo "[asset] clone ${ASSET_REPO} (${ASSET_BRANCH})"
    if ! git_with_token "${askpass}" clone --depth 1 --branch "${ASSET_BRANCH}" \
        "${clone_url}" "${work}/repo" >/dev/null 2>"${work}/clone.err"; then
        echo "[asset] clone 失败：" >&2
        sed 's/oauth2:[^@]*@/oauth2:***@/g; s/Bearer [^[:space:]]*/Bearer ***/g' \
            "${work}/clone.err" >&2 || true
        return 1
    fi

    dest="${work}/repo/${dest_rel}"
    mkdir -p "${dest}"
    cp "${COV_DIR}/coverage_summary.md" "${dest}/coverage_summary.md"
    python3 - "${COV_DIR}/coverage_summary.md" "${dest}/coverage_summary.csv" <<'PY'
import csv
import sys
rows = []
with open(sys.argv[1], encoding="utf-8") as handle:
    for line in handle:
        if not line.startswith("|") or "---" in line:
            continue
        rows.append([cell.strip() for cell in line.strip().strip("|").split("|")])
with open(sys.argv[2], "w", encoding="utf-8", newline="") as handle:
    csv.writer(handle).writerows(rows)
PY

    tar_args=()
    for item in coverage_summary.md lcov_insight_test.info result modules; do
        if [ -e "${COV_DIR}/${item}" ]; then
            tar_args+=("${item}")
        fi
    done
    tar -czf "${dest}/cpp-ut-coverage_html.tar.gz" -C "${COV_DIR}" "${tar_args[@]}"

    src_sha=$(git_in_src rev-parse HEAD)
    src_branch=$(git_in_src rev-parse --abbrev-ref HEAD)
    {
        echo "# PR ${pr_id:+!}${pr_id:-（无）} 覆盖率报告（${run_date}）"
        echo ""
        echo "归档 msinsight 提交 \`${src_sha:-unknown}\` 的 CPP_UT 模块级覆盖率报告。"
        echo ""
        echo "## 文件入口"
        echo ""
        echo "- [PROVENANCE.md](PROVENANCE.md)：运行环境、代码版本。"
        echo "- [coverage_summary.md](coverage_summary.md)：模块级行、函数与分支覆盖率汇总。"
        echo "- [coverage_summary.csv](coverage_summary.csv)：机器可读汇总。"
        echo "- [cpp-ut-coverage_html.tar.gz](cpp-ut-coverage_html.tar.gz)：HTML 报告与 lcov 数据。"
        echo ""
        echo "## 查看 HTML"
        echo ""
        echo '```bash'
        echo "tar -xzf cpp-ut-coverage_html.tar.gz"
        echo '```'
        echo ""
        echo "解包后打开 \`result/index.html\`。归档报告为生成物，不手工修改。"
        echo ""
        echo "[返回覆盖率报告目录](../README.md)"
    } > "${dest}/README.md"
    {
        echo "# 本次运行溯源（${run_date}${pr_id:+ / PR !${pr_id}}）"
        echo ""
        echo "| 项 | 值 |"
        echo "| --- | --- |"
        echo "| 被测仓 | \`$(git_in_src remote get-url origin)\` |"
        echo "| 提交 | \`${src_sha:-unknown}\` |"
        echo "| 分支 | \`${src_branch:-unknown}\` |"
        echo "| MR | ${pr_id:-（无）} |"
        echo "| 生成脚本 | \`build/cpp_coverage.sh\` + \`build/cpp_coverage_post.sh\` |"
        echo "| 采集 | lcov 扫描 \`insight_test.dir\`，去掉 include/test/third_party |"
        echo "| 运行标识 | \`${run_id}\` |"
        echo ""
        echo "未归档原始 \`.gcda\`。需要更新时重跑作业并归档新的一份。"
    } > "${dest}/PROVENANCE.md"

    git -C "${work}/repo" add "${dest_rel}"
    if git -C "${work}/repo" diff --cached --quiet; then
        echo "[asset] ${dest_rel} 无变更，跳过提交"
        asset_url="${browse_url}"
        return 0
    fi
    author_name=$(git_in_src log -1 --format='%an')
    author_email=$(git_in_src log -1 --format='%ae')
    git -C "${work}/repo" -c user.name="${author_name:-cpp-coverage-bot}" \
        -c user.email="${author_email:-cpp-coverage-bot@noreply.gitcode.com}" \
        commit -m "archive: CPP_UT coverage ${run_id}"
    echo "[asset] push ${ASSET_REPO} ${dest_rel}"
    if ! git_with_token "${askpass}" -C "${work}/repo" push origin "HEAD:${ASSET_BRANCH}"; then
        echo "[asset] push 失败" >&2
        return 1
    fi
    asset_url="${browse_url}"
    echo "[asset] 已归档 ${asset_url}"
}

asset_rc=0
archive_report || asset_rc=$?
if [ "${asset_rc}" -ne 0 ]; then
    if [ "${asset_mode}" = "required" ]; then
        echo "ERROR: 覆盖率报告归档失败（COVERAGE_ASSET=required）" >&2
        exit 1
    fi
    echo "WARNING: 覆盖率报告未归档（退出码 ${asset_rc}），继续尝试评论" >&2
fi
if [ -n "${asset_url}" ]; then
    export COVERAGE_ASSET_URL="${asset_url}"
fi

comment_rc=0
python3 - "${COV_DIR}" "${TOP_DIR}" <<'PY' || comment_rc=$?
"""把覆盖率汇总回帖到 GitCode MR 评论（幂等：有旧评论则编辑，无则新建）。"""
import json
import os
import re
import subprocess
import sys
import urllib.error
import urllib.request

cov_dir, top_dir = sys.argv[1], os.path.abspath(sys.argv[2])
MARKER = "<!-- insight-ci-cpp-ut-coverage -->"
MAX_BODY = 8000
mode = (os.environ.get("COVERAGE_COMMENT") or "auto").strip().lower()
pr_id = (os.environ.get("pr_id") or os.environ.get("GITCODE_PR_ID") or "").strip()
token = (os.environ.get("GITCODE_TOKEN") or os.environ.get("gitcode_token") or "").strip()


def log(message):
    print("[comment] {}".format(message), flush=True)


def git(args):
    try:
        return subprocess.check_output(["git", "-C", top_dir] + args,
                                       universal_newlines=True,
                                       stderr=subprocess.DEVNULL).strip()
    except Exception:  # noqa: BLE001
        return ""


def resolve_repo():
    env_repo = (os.environ.get("GITCODE_REPO") or "").strip().strip("/")
    if env_repo:
        return env_repo
    url = git(["remote", "get-url", "origin"])
    match = re.search(r"[:/]([^/:]+)/([^/]+?)(?:\.git)?$", url) if url else None
    if match:
        return "{}/{}".format(match.group(1), match.group(2))
    log("无法从 origin 解析仓库路径，回退 ascend/msinsight（url={!r}）".format(url))
    return "ascend/msinsight"


def strip_report_column(lines):
    """表格最后一列指向作业产物里的 HTML，MR 评论里点不开，回帖时去掉。"""
    out = []
    for line in lines:
        cells = line.split("|")
        if line.startswith("|") and len(cells) >= 5:
            out.append("|".join(cells[:-2] + [cells[-1]]))
        else:
            out.append(line)
    return out


def build_body(repo):
    lines = ["{}".format(MARKER), "## CPP_UT 模块级覆盖率", ""]
    sha = git(["rev-parse", "--short", "HEAD"])
    branch = git(["rev-parse", "--abbrev-ref", "HEAD"])
    context = []
    if sha:
        context.append("提交 `{}`".format(sha))
    if branch:
        context.append("检出分支 `{}`".format(branch))
    if context:
        lines.append("- " + " ｜ ".join(context))
    lines.append("- MR：https://gitcode.com/{}/merge_requests/{}".format(repo, pr_id))
    asset_url = (os.environ.get("COVERAGE_ASSET_URL") or "").strip()
    if asset_url:
        lines.append("- 完整报告归档（HTML + lcov 数据）：{}".format(asset_url))
    lines.append("")
    summary = os.path.join(cov_dir, "coverage_summary.md")
    if os.path.exists(summary):
        with open(summary, encoding="utf-8") as handle:
            content = handle.read().strip().splitlines()
        if content and content[0].startswith("# "):
            content = content[1:]
        while content and not content[0].strip():
            content.pop(0)
        lines.extend(strip_report_column(content))
    else:
        lines.append("（未找到 coverage_summary.md）")
    lines.append("")
    lines.append("<sub>由 cpp_coverage_post.sh 自动更新；重复运行会就地更新本条评论。</sub>")
    body = "\n".join(lines)
    if len(body) > MAX_BODY:
        body = body[:MAX_BODY] + "\n…（过长已截断，完整报告见作业产物）"
    return body


def request(method, url, payload=None):
    data = json.dumps(payload).encode("utf-8") if payload is not None else None
    req = urllib.request.Request(url, data=data, method=method)
    req.add_header("Accept", "application/json")
    if data is not None:
        req.add_header("Content-Type", "application/json")
    if token:
        req.add_header("Authorization", "Bearer {}".format(token))
    with urllib.request.urlopen(req, timeout=25) as resp:
        return resp.status, resp.read().decode("utf-8", "replace")


if mode in ("off", "none", "disable", "disabled"):
    log("COVERAGE_COMMENT={}，跳过".format(mode))
    sys.exit(0)
if not pr_id:
    log("未提供 pr_id / GITCODE_PR_ID，跳过（非 PR 触发或平台未注入）")
    sys.exit(0)

api = (os.environ.get("GITCODE_API") or "https://api.gitcode.com/api/v5").rstrip("/")
repo = resolve_repo()
comments_url = "{}/repos/{}/pulls/{}/comments".format(api, repo, pr_id)
body = build_body(repo)

login = ""
if token:
    try:
        _status, text = request("GET", "{}/user".format(api))
        login = (json.loads(text).get("login") or "") if text.strip().startswith("{") else ""
    except Exception as exc:  # noqa: BLE001
        log("读取令牌身份失败（{}），改为只按正文标记判定".format(exc))
        login = ""


def is_own_comment(item):
    """只认「正文以标记开头」且（能确定身份时）作者为本令牌账号的评论。"""
    text = (item.get("body") or "").lstrip()
    if not text.startswith(MARKER):
        return False
    if not login:
        return True
    return ((item.get("user") or {}).get("login") or "") == login


existing_id = None
try:
    for page in range(1, 6):
        status, text = request("GET", "{}?per_page=100&page={}".format(comments_url, page))
        items = json.loads(text) if text.strip().startswith("[") else []
        for item in items:
            if is_own_comment(item):
                existing_id = item.get("id")
        if len(items) < 100:
            break
except urllib.error.HTTPError as exc:
    log("读取评论失败：HTTP {} {}".format(exc.code, exc.read().decode("utf-8", "replace")[:200]))
    sys.exit(1)
except Exception as exc:  # noqa: BLE001
    log("读取评论异常：{}".format(exc))
    sys.exit(1)

if mode in ("dry-run", "dryrun", "print"):
    log("dry-run：目标 {}，动作 {}，正文 {} 字符".format(repo + "!" + pr_id,
        "PATCH 评论 {}".format(existing_id) if existing_id else "POST 新评论", len(body)))
    print("---------- 评论正文（dry-run 预览）----------")
    print(body)
    print("---------- 预览结束 ----------")
    sys.exit(0)

if not token:
    log("未提供令牌（GITCODE_TOKEN / gitcode_token），跳过发布；可设 COVERAGE_COMMENT=dry-run 预览正文")
    sys.exit(0)

try:
    if existing_id:
        status, text = request("PATCH", "{}/repos/{}/pulls/comments/{}".format(api, repo, existing_id),
                               {"body": body})
        log("已更新既有评论 id={}（HTTP {}）".format(existing_id, status))
    else:
        status, text = request("POST", comments_url, {"body": body})
        new_id = ""
        try:
            new_id = json.loads(text).get("id", "")
        except Exception:  # noqa: BLE001
            pass
        log("已发布新评论 id={}（HTTP {}）".format(new_id or "?", status))
except urllib.error.HTTPError as exc:
    detail = exc.read().decode("utf-8", "replace")[:300]
    log("发布失败：HTTP {} {}".format(exc.code, detail))
    if exc.code in (401, 403):
        log("提示：令牌无效或该账号无权评论 {} 的 MR".format(repo))
    sys.exit(1)
except Exception as exc:  # noqa: BLE001
    log("发布异常：{}".format(exc))
    sys.exit(1)
PY
if [ "${comment_rc}" -ne 0 ]; then
    if [ "${comment_mode}" = "required" ]; then
        echo "ERROR: 覆盖率评论发布失败（COVERAGE_COMMENT=required）" >&2
        exit 1
    fi
    echo "WARNING: 覆盖率评论未发布（退出码 ${comment_rc}），不影响作业结论" >&2
fi
