"""Run one redacted secret scan over the actual pre-commit scope.

Copyright (c) 2026 Huawei Technologies Co.,Ltd.
MindStudio is licensed under Mulan PSL v2.
http://license.coscl.org.cn/MulanPSL2
"""

import os
from pathlib import Path
import shutil
import subprocess  # nosec B404: this hook must execute the existing Git and Gitleaks binaries.
import sys
import time


def complete_scan_history(root, git, base, head):
    """A shallow boundary inside the range makes Git report its whole tree as added."""
    for _ in range(8):
        shallow_path = subprocess.check_output(  # nosec B603: resolved Git executable, fixed arguments.
            [git, 'rev-parse', '--git-path', 'shallow'], cwd=root, text=True
        ).strip()
        shallow = root / shallow_path
        if not shallow.exists():
            return
        boundaries = set(shallow.read_text(encoding='utf-8').splitlines())
        commits = subprocess.check_output(  # nosec B603: validated commit hashes, no shell.
            [git, 'rev-list', f'{base}..{head}'], cwd=root, text=True
        ).splitlines()
        if not boundaries.intersection(commits):
            return
        print('Gitleaks: completing shallow PR history before scanning.', flush=True)
        subprocess.run(  # nosec B603: existing origin, validated commit hash, no shell.
            [git, 'fetch', '--no-tags', '--deepen=32', 'origin', head],
            cwd=root,
            check=True,
            capture_output=True,
            text=True,
            timeout=120,
        )
    raise ValueError('PR history is still shallow inside the scan range; refusing an incomplete scan.')


def scan_arguments(root, environment):
    """Use the CI commit range, or the staged diff for a local commit."""
    base = environment.get('PRE_COMMIT_FROM_REF')
    head = environment.get('PRE_COMMIT_TO_REF')
    if bool(base) != bool(head):
        raise ValueError('Both PRE_COMMIT_FROM_REF and PRE_COMMIT_TO_REF are required.')
    if not base:
        return ['protect', '--staged']
    git = shutil.which('git')
    if not git:
        raise ValueError('Git executable not found in PATH.')
    revisions = [
        subprocess.check_output(  # nosec B603: executable resolved above; refs follow --end-of-options; no shell.
            [git, 'rev-parse', '--verify', '--end-of-options', f'{ref}^{{commit}}'], cwd=root, text=True
        ).strip()
        for ref in (base, head)
    ]
    complete_scan_history(root, git, *revisions)
    return ['git', f'--log-opts={revisions[0]}..{revisions[1]}']


def main():
    """Preserve scanner failures and report timeouts explicitly."""
    root = Path(__file__).resolve().parent.parent
    binary = next((str(root / name) for name in ('gitleaks', 'gitleaks.exe') if (root / name).is_file()), None)
    binary = binary or shutil.which('gitleaks')
    if not binary:
        print('Gitleaks executable not found in repository root or PATH.', file=sys.stderr)
        return 127
    try:
        arguments = scan_arguments(root, os.environ)
    except (ValueError, subprocess.CalledProcessError, subprocess.TimeoutExpired) as error:
        print(f'Invalid Gitleaks scan range: {error}', file=sys.stderr)
        return 2
    print(f'Gitleaks scope: {" ".join(arguments)}', flush=True)
    started = time.monotonic()
    try:
        result = subprocess.run(  # nosec B603: trusted binary, fixed flags and Git-resolved hashes; no shell.
            [
                binary,
                *arguments,
                '--verbose',
                '--redact',
                '--no-banner',
                '--config=pre-commit/.gitleaks.toml',
                '--timeout=120',
            ],
            cwd=root,
            check=False,
            timeout=140,
        ).returncode
    except subprocess.TimeoutExpired:
        print('Gitleaks exceeded the 140 second process limit.', file=sys.stderr)
        result = 124
    print(f'Gitleaks finished in {time.monotonic() - started:.1f}s; exit code: {result}', flush=True)
    return result


if __name__ == '__main__':
    sys.exit(main())
