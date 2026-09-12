#!/usr/bin/env bash

# Shared CI build and upload functions for Linux, macOS, and Windows Bash
# environments (Git Bash, MSYS2, or Cygwin on Windows). This file must be
# sourced by a platform entry. Direct execution is rejected.
#
# Pipeline modes:
#   build-upload  Download dependencies, build the current checkout, validate
#                 the platform artifact, and upload it. This is the default on
#                 Linux, Windows, and macOS.
#   upload-only   Validate and upload a product built by an earlier job.
#
# Common environment variables:
#   CI_BUILD_ACTION          build-upload or upload-only.
#   WORKSPACE                CI scratch directory; defaults to the repository.
#   SOURCE_DIR               Source checkout; defaults to the repository root.
#   ARTIFACT_DIR             Explicit product directory. Otherwise SOURCE_DIR/out
#                            is used, with CODE_DIR/TIMESTAMP compatibility for
#                            legacy upload-only macOS jobs.
#   REPO_URL / REMOTE_URL    Repository identity used in the OBS path.
#   BRANCH                   Source branch or release tag; defaults to master.
#   BUILD_TYPE               OBS category such as daily; defaults to daily.
#   BUILD_VERSION            Optional product version override.
#   WHL_VERSION              Optional JupyterLab wheel version override.
#   UNIFIED_BUILD_TIMESTAMP  YYYYMMDDHHMM[SS] or YYYYMMDD_HHMM. Its presence
#                            enables the second-generation OBS directory
#                            layout. A lowercase unified_build_timestamp is
#                            also accepted for CI systems that normalize
#                            parameter names to lowercase.
#   AK / SK                  Credentials consumed by upload_to_obs.sh, either
#                            positionally (default) or, with
#                            CI_UPLOAD_CREDENTIALS=environment, through the
#                            child process environment.
#
# CI tools supply extract_version_from_tag.sh and upload_to_obs.sh. They may be
# provided through CI_TOOLS_DIR or fetched from CI_TOOLS_URL/CI_TOOLS_REF. The
# external tools repository is trusted as-is: an existing directory is used
# directly after a file-presence check, and a missing one is cloned on demand.

# Direct execution would look like an entry point. Fail before defining any
# pipeline state so this file cannot be mistaken for ci-build.sh.
if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
    printf '[ci-build] ERROR: ci-build-common.sh must be sourced, not executed\n' >&2
    exit 1
fi

# -E propagates ERR traps into functions/subshells, -u rejects missing values,
# and pipefail prevents an early pipeline failure from being hidden.
set -Eeuo pipefail

# Resolve paths from this file rather than the caller's current directory so
# the script behaves identically when invoked by different CI runners.
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/../.." && pwd)

# Do not assign injected CI variables here. BRANCH, WORKSPACE, SOURCE_DIR,
# CI_BUILD_ACTION, and the rest of the pipeline-exported names must keep the
# caller's value; defaults are applied later with ${VAR:-default}.
# Only arrays that cleanup and collect_artifacts expand under nounset are
# created empty. Scalar internals are written by set_platform / resolve_*.
ARTIFACTS=()
TEMP_DIRS=()

# Immutable commit behind uWebSockets v20.48.0, verified against both the
# upstream GitHub repository and the GitCode mirror used by the downloader.
# Keep this aligned with server/build/download_third_party.py. When bumping
# this pin: re-run build/tests/unit/test_ci_build_script.py, re-check the new
# tree for 100755 entries (Windows cannot verify execute bits, see the mode
# check below), symlink targets, and submodules, and confirm the App.h context
# blocks still match exactly once.
UWEBSOCKETS_COMMIT="bc2815d95b6ff4cad0973226d0058eadaaec99ad"

log() {
    # Warnings and diagnostics belong on stderr so stdout stays machine-parseable.
    printf '[ci-build] %s\n' "$*" >&2
}

die() {
    printf '[ci-build] ERROR: %s\n' "$*" >&2
    exit 1
}

cleanup() {
    local path

    # Delete only directories created by this process and recorded immediately
    # after mktemp succeeds. Source checkouts and caller-owned artifacts are
    # never registered here.
    for path in "${TEMP_DIRS[@]}"; do
        if [[ -n "${path}" && -d "${path}" ]]; then
            rm -rf "${path}"
        fi
    done
}

on_error() {
    local status=$1
    local line=$2
    printf '[ci-build] ERROR: command failed at line %s (exit %s)\n' "${line}" "${status}" >&2
}

# Do not include BASH_COMMAND in failure output: the upload command contains
# credentials required by the legacy upload helper interface. Signals exit with
# conventional statuses; the EXIT trap then performs cleanup exactly once.
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
trap 'exit 129' HUP
trap 'on_error "$?" "$LINENO"' ERR

require_command() {
    command -v "$1" >/dev/null 2>&1 || die "required command not found: $1"
}

require_value() {
    local name=$1
    local value=$2
    [[ -n "${value}" ]] || die "${name} must be set"
}

validate_path_value() {
    local name=$1
    local value=$2
    # These values become OBS path components. Reject control-flow and parent
    # path syntax instead of silently rewriting a valid branch/repository name.
    [[ "${value}" != *$'\n'* && "${value}" != *$'\r'* ]] || die "${name} contains a line break"
    [[ ! "${value}" =~ (^|/)\.\.(/|$) ]] || die "${name} contains a parent-directory segment"
}

validate_unified_timestamp() {
    local value=$1

    # Empty selects the legacy OBS layout. Otherwise enforce the same format
    # contract as build_obs_url. Returns non-zero instead of exiting so the
    # caller can decide how to recover from a corrupted injection.
    if [[ -z "${value}" ]]; then
        return 0
    fi
    [[ "${value}" =~ ^[0-9]{12}([0-9]{2})?$ || "${value}" =~ ^[0-9]{8}_[0-9]{4}$ ]]
}

set_platform() {
    local system_name=$1
    local machine=$2

    # Normalize architecture aliases to the names used by build.py's artifact
    # contract. Parameters are accepted directly to make this pure logic
    # testable without requiring all target operating systems.
    machine=$(printf '%s' "${machine}" | tr '[:upper:]' '[:lower:]')
    case "${machine}" in
        amd64) machine="x86_64" ;;
        arm64) machine="aarch64" ;;
    esac

    case "${system_name}" in
        Linux)
            PLATFORM="linux"
            PACKAGE_SUFFIX=".zip"
            PYTHON_CMD="python3"
            ;;
        Darwin)
            PLATFORM="macos"
            PACKAGE_SUFFIX=".dmg"
            PYTHON_CMD="python3"
            ;;
        MINGW*|MSYS*|CYGWIN*)
            PLATFORM="windows"
            PACKAGE_SUFFIX=".exe"
            PYTHON_CMD="python"
            ;;
        *)
            die "unsupported operating system: ${system_name}"
            ;;
    esac

    case "${machine}" in
        x86_64|aarch64) ARCH="${machine}" ;;
        *) die "unsupported architecture: ${machine}" ;;
    esac

    # Product names use "win", while the historical OBS tree uses "windows".
    # Keep both values rather than forcing one convention onto both contracts.
    if [[ "${PLATFORM}" == "windows" ]]; then
        OS_TAG="win"
        OBS_ARCH="windows"
    else
        OS_TAG="${PLATFORM}_${ARCH}"
        OBS_ARCH="${ARCH}"
    fi
}

detect_platform() {
    set_platform "$(uname -s)" "$(uname -m)"
}

resolve_checkout_ref() {
    local repo_dir=${1:-${REPO_ROOT}}
    local ref

    # The ref the checkout actually sits on is the ground truth of what gets
    # built and archived: the injected BRANCH has been observed resolving to
    # "master" on Windows while the job had cloned a feature branch, silently
    # redirecting artifacts into the official daily tree. Branch checkouts are
    # read directly; tag checkouts are detached, so recover the exact tag name
    # that the legacy tag_* contracts parse.
    if ref=$(git -C "${repo_dir}" branch --show-current 2>/dev/null) && [[ -n "${ref}" ]]; then
        printf '%s\n' "${ref}"
        return 0
    fi
    if ref=$(git -C "${repo_dir}" describe --tags --exact-match 2>/dev/null) && [[ -n "${ref}" ]]; then
        printf '%s\n' "${ref}"
        return 0
    fi
    return 1
}

resolve_environment() {
    local repo_url=${REPO_URL:-}
    local remote_url=${REMOTE_URL:-}
    local repo_path
    local ref

    # Linux/Windows jobs historically supplied REPO_URL while macOS supplied
    # REMOTE_URL. Accept both, but fail on ambiguity to avoid a wrong OBS path.
    if [[ -n "${repo_url}" && -n "${remote_url}" && "${repo_url}" != "${remote_url}" ]]; then
        die "REPO_URL and REMOTE_URL specify different repositories"
    fi

    SOURCE_REPO_URL=${repo_url:-${remote_url:-https://gitcode.com/Ascend/msinsight.git}}
    SOURCE_REPO_URL=${SOURCE_REPO_URL%/}
    REPO_NAME=${SOURCE_REPO_URL##*/}
    REPO_NAME=${REPO_NAME%.git}
    require_value "repository name" "${REPO_NAME}"

    # Personal-branch archiving identifies forks by owner
    # (personal/<owner>/<repo>/<branch>/<arch>), taken from the URL path
    # segment before the repository name. Hosts without that segment (flat
    # URLs) leave it empty; only a personal build then requires it.
    repo_path=${SOURCE_REPO_URL#*://}
    repo_path=${repo_path#*/}
    REPO_OWNER=${repo_path%/*}
    [[ "${REPO_OWNER}" == "${repo_path}" ]] && REPO_OWNER=""
    REPO_OWNER=${REPO_OWNER##*/}

    # Resolve the source tree before reading its checkout ref so an omitted
    # SOURCE_DIR still archives the repository that this script lives in.
    WORKSPACE=${WORKSPACE:-${REPO_ROOT}}
    SOURCE_DIR=${SOURCE_DIR:-${REPO_ROOT}}
    BRANCH=${BRANCH:-master}
    # The checkout's own ref outranks the injected BRANCH: the same Windows
    # pipeline that corrupts the timestamp has also been seen resolving BRANCH
    # to "master" while the job had cloned a feature branch, silently sending
    # the artifacts to the official daily tree instead of the personal one.
    if ref=$(resolve_checkout_ref "${SOURCE_DIR}") && [[ -n "${ref}" && "${ref}" != "${BRANCH}" ]]; then
        log "WARNING: injected BRANCH '${BRANCH}' does not match the checkout ref '${ref}'; using the checkout ref"
        BRANCH=${ref}
    fi
    BUILD_TYPE=${BUILD_TYPE:-daily}
    CI_BUILD_ACTION=${CI_BUILD_ACTION:-build-upload}
    case "${CI_BUILD_ACTION}" in
        build-upload|upload-only) ;;
        *) die "CI_BUILD_ACTION must be build-upload or upload-only" ;;
    esac
    # Some CI parameter systems lowercase variable names; accept both forms.
    UNIFIED_BUILD_TIMESTAMP=${UNIFIED_BUILD_TIMESTAMP:-${unified_build_timestamp:-}}
    # Pipeline parameter injection pollutes the value with whitespace (CRLF
    # line endings, UI padding) and sometimes quoting characters. The
    # timestamp contract allows only digits and one underscore separator, so
    # whitespace and quotes anywhere in the value are injection noise and are
    # removed rather than failing the format check.
    UNIFIED_BUILD_TIMESTAMP=${UNIFIED_BUILD_TIMESTAMP//[[:space:]]/}
    UNIFIED_BUILD_TIMESTAMP=${UNIFIED_BUILD_TIMESTAMP//\"/}
    UNIFIED_BUILD_TIMESTAMP=${UNIFIED_BUILD_TIMESTAMP//\'/}
    # A corrupted injection (observed as a bare "1" on Windows) must not kill
    # the 20-minute product build: fall back to the current local time, which
    # is the same clock the legacy jobs used, and log the rejected bytes so the
    # pipeline-side injection fault stays diagnosable.
    if ! validate_unified_timestamp "${UNIFIED_BUILD_TIMESTAMP}"; then
        log "WARNING: UNIFIED_BUILD_TIMESTAMP must use YYYYMMDDHHMM[SS] or YYYYMMDD_HHMM, got bytes: $(printf '%s' "${UNIFIED_BUILD_TIMESTAMP}" | od -An -c | tr -s ' ' | tr -d '\n'); falling back to current time"
        UNIFIED_BUILD_TIMESTAMP=$(date +%Y%m%d%H%M)
    fi
    CI_TOOLS_DIR=${CI_TOOLS_DIR:-${WORKSPACE}/ci}
    # The executed uploader comes from the CI tools directory resolved by
    # ensure_ci_tools, keeping a single source of truth for its location.
    UPLOAD_SCRIPT="${CI_TOOLS_DIR}/build/upload_to_obs.sh"

    # Prefer an explicit artifact directory. CODE_DIR and TIMESTAMP retain the
    # directory convention used by the former macOS upload scripts.
    if [[ -n "${ARTIFACT_DIR:-}" ]]; then
        OUT_DIR=${ARTIFACT_DIR}
    elif [[ "${CI_BUILD_ACTION}" == "upload-only" && -n "${CODE_DIR:-}" ]]; then
        OUT_DIR="${CODE_DIR}/out"
    elif [[ "${CI_BUILD_ACTION}" == "upload-only" && -n "${TIMESTAMP:-}" ]]; then
        OUT_DIR="${WORKSPACE}/CODE_${TIMESTAMP}/out"
    else
        OUT_DIR="${SOURCE_DIR}/out"
    fi

    # The CI network reaches the aliyun PyPI mirror much faster than pypi.org
    # (the former Linux job rewrote build.py's index for this reason, where a
    # single 11.7 MB wheel took 8 minutes). build.py reads PIP_INDEX_URL, so
    # the mirror is supplied through the environment and can be overridden.
    export PIP_INDEX_URL="${PIP_INDEX_URL:-https://mirrors.aliyun.com/pypi/simple/}"

    validate_path_value "BRANCH" "${BRANCH}"
    validate_path_value "BUILD_TYPE" "${BUILD_TYPE}"
    validate_path_value "repository name" "${REPO_NAME}"
    validate_path_value "repository owner" "${REPO_OWNER}"
}

ensure_ci_tools() {
    local extract_script="${CI_TOOLS_DIR}/build/extract_version_from_tag.sh"
    local verified_upload_script="${CI_TOOLS_DIR}/build/upload_to_obs.sh"
    local tools_url=${CI_TOOLS_URL:-https://codehub.devcloud.cn-north-4.huaweicloud.com/mindstudio_test_zhaozepeng12300001/ci.git}
    local tools_ref=${CI_TOOLS_REF:-ci_gitcode}
    local clone_root

    [[ "${UPLOAD_SCRIPT}" == "${verified_upload_script}" ]] \
        || die "UPLOAD_SCRIPT must reference the CI tools checkout"

    # The external tools repository is trusted as-is. Reuse it when the scripts
    # this pipeline needs are present, and clone it on demand otherwise.
    if [[ -f "${UPLOAD_SCRIPT}" && ( "${CI_BUILD_ACTION}" == "upload-only" || -f "${extract_script}" ) ]]; then
        return
    fi
    [[ ! -e "${CI_TOOLS_DIR}" ]] || die "CI tools directory is incomplete: ${CI_TOOLS_DIR}"

    require_command git
    mkdir -p "${WORKSPACE}" "$(dirname "${CI_TOOLS_DIR}")"
    # Clone into a temporary sibling and move the complete checkout into place,
    # so a failed download cannot leave a partial tools directory behind.
    clone_root=$(mktemp -d "${WORKSPACE%/}/ci-tools.XXXXXX")
    TEMP_DIRS+=("${clone_root}")
    log "Fetching CI tools (${tools_ref})"
    git clone --depth 1 --branch "${tools_ref}" --config core.autocrlf=false "${tools_url}" "${clone_root}/ci"
    [[ -f "${clone_root}/ci/build/upload_to_obs.sh" ]] || die "CI tools do not contain upload_to_obs.sh"
    if [[ "${CI_BUILD_ACTION}" == "build-upload" ]]; then
        [[ -f "${clone_root}/ci/build/extract_version_from_tag.sh" ]] || die "CI tools do not contain extract_version_from_tag.sh"
    fi
    mv "${clone_root}/ci" "${CI_TOOLS_DIR}"
}

resolve_versions() {
    local extract_script="${CI_TOOLS_DIR:-}/build/extract_version_from_tag.sh"

    # Explicit versions allow controlled rebuilds. Missing values are derived
    # by the pinned CI helper so product and wheel version rules stay aligned.
    if [[ -z "${BUILD_VERSION:-}" ]]; then
        BUILD_VERSION=$(bash "${extract_script}" --tag "${BRANCH:-}")
    fi
    if [[ -z "${WHL_VERSION:-}" ]]; then
        WHL_VERSION=$(bash "${extract_script}" --tag "${BRANCH:-}" --whl)
    fi
    # The helper returns the branch name itself for non-tag branches such as
    # "fix/kernel-overload". Slashes and backslashes inside it would become
    # directory separators in package names (NSIS OutFile, wheel names) and
    # break packaging, so they are normalized to underscores here.
    BUILD_VERSION=${BUILD_VERSION//[\/\\]/_}
    WHL_VERSION=${WHL_VERSION//[\/\\]/_}
    require_value "BUILD_VERSION" "${BUILD_VERSION}"
    require_value "WHL_VERSION" "${WHL_VERSION}"
}

prepare_uwebsockets_dependency() {
    local dependency_dir="${SOURCE_DIR}/server/third_party/uWebSockets"
    local app_header="${dependency_dir}/src/App.h"
    local actual_commit
    local patch_enabled="false"
    local patched_line='for (char digit : userAgent.substr(posStart, posEnd - posStart)) {'

    # Verification is mandatory on every build platform because the downloader
    # reuses any directory with the expected name. Only the compatibility
    # rewrite remains Linux-specific.
    if [[ "${PLATFORM}" == "linux" && "${APPLY_UWEBSOCKETS_PATCH:-true}" == "true" ]]; then
        patch_enabled="true"
    fi
    require_command git
    [[ -d "${dependency_dir}/.git" ]] \
        || die "cannot verify cached uWebSockets revision: ${dependency_dir} is not a Git checkout"

    actual_commit=$(GIT_NO_REPLACE_OBJECTS=1 git -C "${dependency_dir}" rev-parse HEAD)
    [[ "${actual_commit}" == "${UWEBSOCKETS_COMMIT}" ]] \
        || die "cached uWebSockets revision mismatch: expected ${UWEBSOCKETS_COMMIT}, got ${actual_commit}"

    # Validate files directly against the pinned commit tree instead of trusting
    # git status/index metadata. This catches ignored files, mode changes, and
    # assume-unchanged/skip-worktree tricks (execute-bit comparison is skipped
    # on Windows, where stat cannot observe it). Git replacement objects are
    # disabled.
    # On Linux with patching enabled, App.h may be pristine, legacy-patched, or
    # current-patched and is normalized to the current patch. Other builds only
    # accept the pristine pinned content and never rewrite it.
    "${PYTHON_CMD}" - "${dependency_dir}" "${UWEBSOCKETS_COMMIT}" "${patch_enabled}" "${PLATFORM}" <<'PY'
import hashlib
import os
import pathlib
import stat
import subprocess
import sys

root = pathlib.Path(sys.argv[1])
commit = sys.argv[2]
patch_enabled = sys.argv[3] == 'true'
platform_name = sys.argv[4]
app_rel = 'src/App.h'
app_path = root / app_rel
git_env = dict(os.environ, GIT_NO_REPLACE_OBJECTS='1')


def git_output(*args):
    # stderr is inherited so a failing git command keeps its diagnostics in
    # the CI log next to the Python traceback instead of being swallowed.
    return subprocess.check_output(
        ['git', '-C', str(root), *args], env=git_env
    )


def blob_hash(content):
    header = f'blob {len(content)}\0'.encode()
    return hashlib.sha1(header + content).hexdigest()


def crlf_variant(content):
    # Match only Git's ordinary LF-to-CRLF text conversion. Existing CRLF or
    # NUL-containing blobs are not broadened into additional accepted states.
    if b'\0' in content or b'\n' not in content or b'\r\n' in content:
        return None
    return content.replace(b'\n', b'\r\n')


tree = {}
for record in git_output('ls-tree', '-rz', '--full-tree', commit).split(b'\0'):
    if not record:
        continue
    metadata, raw_path = record.split(b'\t', 1)
    mode, object_type, object_id = metadata.decode().split()
    path = raw_path.decode('utf-8', 'surrogateescape')
    tree[path] = (mode, object_type, object_id)

if app_rel not in tree:
    raise SystemExit('pinned uWebSockets commit does not contain src/App.h')

allowed_dirs = set()
submodules = set()
for rel_path, (mode, object_type, _object_id) in tree.items():
    parent = pathlib.PurePosixPath(rel_path).parent
    while str(parent) != '.':
        allowed_dirs.add(str(parent))
        parent = parent.parent
    if mode == '160000' and object_type == 'commit':
        submodules.add(rel_path)


def verify_directory(path, display_path):
    try:
        directory_stat = path.lstat()
    except FileNotFoundError:
        raise SystemExit(f'missing uWebSockets directory: {display_path}')
    if not stat.S_ISDIR(directory_stat.st_mode):
        raise SystemExit(f'uWebSockets directory type mismatch: {display_path}')
    if directory_stat.st_mode & (stat.S_IRUSR | stat.S_IXUSR) != (stat.S_IRUSR | stat.S_IXUSR):
        raise SystemExit(f'uWebSockets directory is not traversable: {display_path}')
    try:
        with os.scandir(path) as entries:
            for _entry in entries:
                pass
    except OSError as error:
        raise SystemExit(f'cannot traverse uWebSockets directory {display_path}: {error}')


# Validate every tracked parent independently; os.walk alone may suppress a
# scandir failure and would otherwise skip files hidden below that directory.
verify_directory(root, '.')
for rel_path in sorted(allowed_dirs):
    verify_directory(root / rel_path, rel_path)

for rel_path in submodules:
    path = root / rel_path
    if path.exists() or path.is_symlink():
        verify_directory(path, rel_path)
        if any(path.iterdir()):
            raise SystemExit(f'populated uWebSockets submodule is not allowed: {rel_path}')


def walk_error(error):
    raise SystemExit(f'cannot traverse uWebSockets directory: {error}')


# Reject every filesystem entry not represented by the pinned tree. An
# uninitialized submodule may be absent or empty, but populated submodules are
# not used by this build and are rejected rather than trusted recursively.
for current_root, dir_names, file_names in os.walk(root, followlinks=False, onerror=walk_error):
    current = pathlib.Path(current_root)
    if current == root and '.git' in dir_names:
        dir_names.remove('.git')
    if current == root and '.git' in file_names:
        file_names.remove('.git')

    for name in list(dir_names):
        path = current / name
        rel_path = path.relative_to(root).as_posix()
        if path.is_symlink():
            dir_names.remove(name)
            if rel_path not in tree:
                raise SystemExit(f'untracked uWebSockets path: {rel_path}')
        elif rel_path in submodules:
            dir_names.remove(name)
        elif rel_path not in allowed_dirs:
            raise SystemExit(f'untracked uWebSockets directory: {rel_path}')

    for name in file_names:
        rel_path = (current / name).relative_to(root).as_posix()
        if rel_path not in tree:
            raise SystemExit(f'untracked uWebSockets path: {rel_path}')

pristine_app = git_output('cat-file', 'blob', f'{commit}:{app_rel}')
include = b'#include <charconv>\n'
old_block = (
    b'        unsigned int minorVersion = 0;\n'
    b'        auto result = std::from_chars(userAgent.data() + posStart, userAgent.data() + posEnd, minorVersion);\n'
    b'        if (result.ec != std::errc()) return false;\n'
    b'        if (result.ptr != userAgent.data() + posEnd) return false; // do not accept trailing chars\n'
)
new_block = (
    b'        if (posStart == posEnd) return false;\n'
    b'        unsigned int minorVersion = 0;\n'
    b'        for (char digit : userAgent.substr(posStart, posEnd - posStart)) {\n'
    b"            if (digit < '0' || digit > '9') return false;\n"
    b"            minorVersion = minorVersion * 10 + static_cast<unsigned int>(digit - '0');\n"
    b'            if (minorVersion > 3) return false;\n'
    b'        }\n'
)
legacy_block = b'        unsigned minorVersion = std::stoi(std::string(userAgent.substr(posStart, posEnd - posStart)));\n'
legacy_app = pristine_app.replace(include, b'', 1).replace(old_block, legacy_block, 1)
patched_app = pristine_app.replace(include, b'', 1).replace(old_block, new_block, 1)

if pristine_app.count(include) != 1 or pristine_app.count(old_block) != 1:
    raise SystemExit('pinned uWebSockets App.h does not match the expected v20.48.0 context')

# Independently validate every tracked entry and its Git file mode. App.h gets
# an explicit three-state content check so arbitrary edits cannot use its patch
# exception. Other blobs must hash exactly to the pinned tree object.
restore_files = {}
for rel_path, (mode, object_type, object_id) in tree.items():
    path = root / rel_path
    if mode == '160000' and object_type == 'commit':
        continue
    try:
        file_stat = path.lstat()
    except FileNotFoundError:
        raise SystemExit(f'missing uWebSockets path: {rel_path}')

    if mode == '120000':
        if not stat.S_ISLNK(file_stat.st_mode):
            raise SystemExit(f'uWebSockets path type mismatch: {rel_path}')
        content = os.readlink(path).encode('utf-8', 'surrogateescape')
    elif mode in ('100644', '100755'):
        if not stat.S_ISREG(file_stat.st_mode):
            raise SystemExit(f'uWebSockets path type mismatch: {rel_path}')
        expected_executable = mode == '100755'
        # Native Windows Python derives the execute bit from the file
        # extension only, so a legitimate 100755 entry would always report a
        # mismatch there. Keep the strict comparison only where stat reflects
        # the actual permission bits.
        if platform_name != 'windows' and bool(file_stat.st_mode & 0o111) != expected_executable:
            raise SystemExit(f'uWebSockets file mode mismatch: {rel_path}')
        content = path.read_bytes()
    else:
        raise SystemExit(f'unsupported uWebSockets tree mode {mode}: {rel_path}')

    if rel_path == app_rel:
        accepted_app_states = (pristine_app, legacy_app, patched_app) if patch_enabled else (pristine_app,)
        if content in accepted_app_states:
            pass
        elif any(content == crlf_variant(state) for state in accepted_app_states):
            # Normalize a proven CRLF-only cache after every path validates.
            restore_files[path] = pristine_app if not patch_enabled else patched_app
        else:
            raise SystemExit('uWebSockets App.h is not a recognized compatibility state')
    elif blob_hash(content) != object_id:
        pristine_content = git_output('cat-file', 'blob', object_id)
        if content == crlf_variant(pristine_content):
            restore_files[path] = pristine_content
        else:
            raise SystemExit(f'uWebSockets tracked content differs from pinned commit: {rel_path}')

for path, content in restore_files.items():
    path.write_bytes(content)
if patch_enabled:
    app_path.write_bytes(patched_app)
PY

    # Prevent a migrated cache from being converted again by a later checkout.
    git -C "${dependency_dir}" config --local core.autocrlf false
    if [[ "${patch_enabled}" == "true" ]]; then
        grep -Fq "${patched_line}" "${app_header}" || die "failed to apply the uWebSockets compatibility patch"
        ! grep -Fq '#include <charconv>' "${app_header}" || die "failed to remove the uWebSockets charconv include"
        ! grep -Fq 'std::from_chars' "${app_header}" || die "failed to replace the uWebSockets parser"
    fi
}

run_build() {
    [[ -f "${SOURCE_DIR}/build/build.py" ]] || die "build entry point not found under SOURCE_DIR: ${SOURCE_DIR}"
    [[ -f "${SOURCE_DIR}/server/build/download_third_party.py" ]] \
        || die "third-party downloader not found under SOURCE_DIR: ${SOURCE_DIR}"
    require_command "${PYTHON_CMD}"
    # build.py shells out to cargo only at the final packaging step, so a
    # missing cargo would otherwise surface after the full product build.
    require_command cargo

    # build/build.py already invokes preprocess_third_party.py, so only the
    # downloader runs here. This avoids the duplicate preprocessing performed
    # by the former platform-specific scripts.
    log "Downloading third-party dependencies"
    (
        cd "${SOURCE_DIR}/server/build"
        "${PYTHON_CMD}" download_third_party.py
    )
    prepare_uwebsockets_dependency

    log "Building ${PLATFORM}/${ARCH} version ${BUILD_VERSION}"
    (
        cd "${SOURCE_DIR}"
        "${PYTHON_CMD}" build/build.py --build_version="${BUILD_VERSION}" --whl_version="${WHL_VERSION}"
    )
}

collect_artifacts() {
    local expected
    local matches=()
    local wheels=()

    # Never upload a broad directory glob. A fresh build has one deterministic
    # package name; upload-only mode must also resolve to exactly one package so
    # stale products in a reused workspace cannot be selected accidentally.
    [[ -d "${OUT_DIR}" ]] || die "artifact directory does not exist: ${OUT_DIR}"
    ARTIFACTS=()
    if [[ "${CI_BUILD_ACTION}" == "build-upload" ]]; then
        expected="${OUT_DIR}/MindStudio-Insight_${BUILD_VERSION}_${OS_TAG}${PACKAGE_SUFFIX}"
        [[ -f "${expected}" ]] || die "expected build artifact not found: ${expected}"
        ARTIFACTS+=("${expected}")
    else
        shopt -s nullglob
        matches=("${OUT_DIR}"/*"_${OS_TAG}${PACKAGE_SUFFIX}")
        shopt -u nullglob
        [[ ${#matches[@]} -eq 1 ]] \
            || die "expected exactly one ${OS_TAG}${PACKAGE_SUFFIX} artifact in ${OUT_DIR}, found ${#matches[@]}"
        ARTIFACTS+=("${matches[0]}")
    fi

    # JupyterLab is optional and Linux-only in the existing release contract.
    # Multiple wheels are ambiguous and therefore treated as a hard failure.
    if [[ "${PLATFORM}" == "linux" ]]; then
        shopt -s nullglob
        wheels=("${OUT_DIR}"/*_jupyterlab*.whl)
        shopt -u nullglob
        [[ ${#wheels[@]} -le 1 ]] || die "multiple JupyterLab wheels found in ${OUT_DIR}"
        if [[ ${#wheels[@]} -eq 1 ]]; then
            ARTIFACTS+=("${wheels[0]}")
        fi
    fi
}

build_obs_url() {
    local build_type=$1
    local branch=$2
    local repo_name=$3
    local obs_arch=$4
    local unified_timestamp=$5
    local current_hour=$6
    local repo_owner=${7:-}
    local obs_base=${OBS_BASE_URL:-obs://ascend-package}
    local unified_base=${UNIFIED_OBS_BASE_URL:-obs://mindstudio-pkg}
    local compact_timestamp=""
    local release_version=""
    local beta_version=""
    local lingqu_suffix=""
    local parts=()
    local safe_branch=""

    # Keep URL generation free of filesystem/network side effects so all path
    # variants can be contract-tested on one host.
    obs_base=${obs_base%/}
    unified_base=${unified_base%/}

    # Only master and release tags belong in the shared official tree. Any
    # other ref is a personal build archived under
    # personal/<owner>/<repo>/<branch>/<arch>/; slashes in the branch name are
    # flattened to dashes so they cannot split OBS path segments. The timestamp
    # is deliberately absent: personal builds are addressed by owner and
    # branch, not by build time.
    if [[ "${branch}" != "master" && "${branch}" != *tag_* ]]; then
        [[ -n "${repo_owner}" ]] \
            || die "personal archive for branch '${branch}' requires a repository owner in the repository URL"
        safe_branch=${branch//\//-}
        printf '%s/personal/%s/%s/%s/%s/\n' \
            "${unified_base}" "${repo_owner}" "${repo_name}" "${safe_branch}" "${obs_arch}"
        return
    fi

    # A unified timestamp opts into the new archive hierarchy. Seconds, when
    # supplied, are intentionally discarded because the directory contract has
    # minute precision.
    if [[ -n "${unified_timestamp}" ]]; then
        if [[ "${unified_timestamp}" =~ ^[0-9]{12}([0-9]{2})?$ ]]; then
            compact_timestamp=${unified_timestamp:0:12}
        elif [[ "${unified_timestamp}" =~ ^[0-9]{8}_[0-9]{4}$ ]]; then
            compact_timestamp=${unified_timestamp:0:8}${unified_timestamp:9:4}
        else
            die "UNIFIED_BUILD_TIMESTAMP must use YYYYMMDDHHMM[SS] or YYYYMMDD_HHMM"
        fi

        # Release tags omit the timestamp hierarchy and are grouped by tag name.
        if [[ "${branch}" == *tag_* ]]; then
            printf '%s/tag/%s/%s/%s/\n' "${unified_base}" "${branch}" "${repo_name}" "${obs_arch}"
        else
            printf '%s/%s/%s/%s-%s/%s_%s/%s/%s/\n' \
                "${unified_base}" "${build_type}" "${branch}" \
                "${compact_timestamp:0:4}" "${compact_timestamp:4:2}" \
                "${compact_timestamp:0:8}" "${compact_timestamp:8:4}" "${repo_name}" "${obs_arch}"
        fi
        return
    fi

    # Without a unified timestamp, preserve the legacy OBS structure including
    # the separate lingqu buckets and historical tag version parsing.
    [[ "${repo_name}" == *for_lingqu ]] && lingqu_suffix="_lingqu"
    if [[ "${branch}" == *tag_* ]]; then
        IFS='._' read -r -a parts <<< "${branch}"
        [[ ${#parts[@]} -ge 6 ]] || die "tag branch does not contain release and beta versions: ${branch}"
        release_version="${parts[2]}.${parts[3]}.${parts[4]}"
        beta_version=${parts[5]}
        printf '%s/mindstudio_version%s/%s/%s/%s/\n' \
            "${obs_base}" "${lingqu_suffix}" "${release_version}" "${beta_version}" "${obs_arch}"
    else
        printf '%s/mindstudio_daily_version%s/%s/%s/%s/%s/\n' \
            "${obs_base}" "${lingqu_suffix}" "${branch}" "${current_hour}" "${repo_name}" "${obs_arch}"
    fi
}

stage_artifacts() {
    local artifact

    # Upload a private, short-lived directory containing only validated files.
    # cleanup removes it on success, failure, or an interrupt.
    mkdir -p "${WORKSPACE}"
    STAGING_DIR=$(mktemp -d "${WORKSPACE%/}/ci-artifacts.XXXXXX")
    TEMP_DIRS+=("${STAGING_DIR}")
    for artifact in "${ARTIFACTS[@]}"; do
        cp "${artifact}" "${STAGING_DIR}/"
    done
}

upload_artifacts() {
    local credential_mode=${CI_UPLOAD_CREDENTIALS:-positional}

    require_value "AK" "${AK:-}"
    require_value "SK" "${SK:-}"
    [[ -f "${UPLOAD_SCRIPT}" ]] || die "upload helper not found: ${UPLOAD_SCRIPT}"

    # The external helper's historical interface requires positional
    # credentials, which are visible in the process list. Helpers that read
    # AK/SK from their (inherited) environment can opt into
    # CI_UPLOAD_CREDENTIALS=environment to remove that exposure; positional
    # stays the default for compatibility with the deployed helper.
    log "Uploading ${#ARTIFACTS[@]} artifact(s) to ${OBS_URL}"
    case "${credential_mode}" in
        positional)
            bash "${UPLOAD_SCRIPT}" "${STAGING_DIR}" "${OBS_URL}" "${AK}" "${SK}"
            ;;
        environment)
            export AK SK
            bash "${UPLOAD_SCRIPT}" "${STAGING_DIR}" "${OBS_URL}"
            ;;
        *)
            die "CI_UPLOAD_CREDENTIALS must be positional or environment"
            ;;
    esac
}

usage() {
    printf '%s\n' \
        'Usage: ci-build.sh' \
        '' \
        'Environment:' \
        '  CI_BUILD_ACTION=build-upload|upload-only' \
        '  WORKSPACE, SOURCE_DIR, ARTIFACT_DIR, REPO_URL, BRANCH, BUILD_TYPE' \
        '  UNIFIED_BUILD_TIMESTAMP, CI_TOOLS_DIR, CI_TOOLS_URL, CI_TOOLS_REF, AK, SK' \
        '  CI_UPLOAD_CREDENTIALS=positional|environment selects how AK/SK are passed' \
        '' \
        'Defaults: Linux, Windows, and macOS all run build-upload.'
}

handle_cli_args() {
    if [[ ${1:-} == "--help" || ${1:-} == "-h" ]]; then
        usage
        exit 0
    fi
    [[ $# -eq 0 ]] || die "unexpected argument: $1"
}

resolve_ci_build_action() {
    CI_BUILD_ACTION=${CI_BUILD_ACTION:-build-upload}
    case "${CI_BUILD_ACTION}" in
        build-upload|upload-only) ;;
        *) die "CI_BUILD_ACTION must be build-upload or upload-only" ;;
    esac
}

run_pipeline() {
    # Platform detection and host preparation stay in the platform entries.
    # This function only runs the shared post-prepare stages.
    resolve_environment
    log "Platform: ${PLATFORM}/${ARCH}; action: ${CI_BUILD_ACTION}"
    ensure_ci_tools
    if [[ "${CI_BUILD_ACTION}" == "build-upload" ]]; then
        resolve_versions
        run_build
    fi
    collect_artifacts
    OBS_URL=$(build_obs_url \
        "${BUILD_TYPE}" "${BRANCH}" "${REPO_NAME}" "${OBS_ARCH}" \
        "${UNIFIED_BUILD_TIMESTAMP}" "$(date +%Y%m%d%H)" "${REPO_OWNER}")
    stage_artifacts
    upload_artifacts
    log "Build pipeline completed"
}
