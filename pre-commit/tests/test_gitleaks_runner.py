"""Regression tests for the pre-commit scanner scope and failure handling.

Copyright (c) 2026 Huawei Technologies Co.,Ltd.
MindStudio is licensed under Mulan PSL v2.
http://license.coscl.org.cn/MulanPSL2
"""

import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location(
    'gitleaks_runner', Path(__file__).resolve().parents[1] / 'run_gitleaks.py'
)
RUNNER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(RUNNER)


class GitleaksRunnerTest(unittest.TestCase):
    def test_real_shallow_merge_excludes_unchanged_baseline(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            origin = root / 'origin'
            checkout = root / 'checkout'
            origin.mkdir()

            def git(cwd, *arguments):
                return subprocess.check_output(
                    [
                        RUNNER.shutil.which('git'),
                        '-c',
                        'user.name=ScannerTest',
                        '-c',
                        'user.email=scanner@example.invalid',
                        *arguments,
                    ],
                    cwd=cwd,
                    text=True,
                    stderr=subprocess.DEVNULL,
                ).strip()

            git(origin, 'init', '--initial-branch=master')
            (origin / 'baseline.txt').write_text('existing baseline\n', encoding='utf-8')
            git(origin, 'add', '.')
            git(origin, 'commit', '-m', 'baseline')
            base = git(origin, 'rev-parse', 'HEAD')
            git(origin, 'checkout', '-b', 'feature')
            (origin / 'feature.txt').write_text('new application\n', encoding='utf-8')
            git(origin, 'add', '.')
            git(origin, 'commit', '-m', 'feature')
            git(origin, 'checkout', 'master')
            git(origin, 'merge', '--no-ff', 'feature', '-m', 'merge')
            git(root, 'clone', '--depth=1', origin.as_uri(), str(checkout))
            git(checkout, 'fetch', '--depth=1', 'origin', base)
            head = git(checkout, 'rev-parse', 'HEAD')
            before = git(checkout, 'log', '--format=', '--name-only', f'{base}..{head}')
            self.assertIn('baseline.txt', before)
            RUNNER.scan_arguments(checkout, {'PRE_COMMIT_FROM_REF': base, 'PRE_COMMIT_TO_REF': head})
            after = git(checkout, 'log', '--format=', '--name-only', f'{base}..{head}')
            self.assertIn('feature.txt', after)
            self.assertNotIn('baseline.txt', after)

    def test_local_commit_scans_staged_changes(self):
        self.assertEqual(RUNNER.scan_arguments(Path.cwd(), {}), ['protect', '--staged'])

    def test_ci_range_is_resolved_and_passed_to_git_scan(self):
        with (
            patch.object(RUNNER, 'complete_scan_history'),
            patch.object(RUNNER.subprocess, 'check_output', side_effect=['a' * 40, 'b' * 40]) as resolve,
        ):
            arguments = RUNNER.scan_arguments(Path.cwd(), {'PRE_COMMIT_FROM_REF': 'base', 'PRE_COMMIT_TO_REF': 'HEAD'})
        self.assertEqual(arguments, ['git', f'--log-opts={"a" * 40}..{"b" * 40}'])
        self.assertIn('--end-of-options', resolve.call_args.args[0])

    def test_shallow_commit_inside_range_requires_deepening(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            shallow = root / 'shallow'
            shallow.write_text('merge\n', encoding='utf-8')
            with (
                patch.object(
                    RUNNER.subprocess, 'check_output', side_effect=['shallow', 'merge\n', 'shallow', 'merge\n']
                ),
                patch.object(RUNNER.subprocess, 'run', side_effect=lambda *_args, **_kwargs: shallow.unlink()) as fetch,
            ):
                RUNNER.complete_scan_history(root, 'git', 'base', 'head')
            self.assertEqual(fetch.call_args.args[0], ['git', 'fetch', '--no-tags', '--deepen=32', 'origin', 'head'])

    def test_shallow_boundary_outside_range_does_not_fetch(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'shallow').write_text('base\n', encoding='utf-8')
            with (
                patch.object(RUNNER.subprocess, 'check_output', side_effect=['shallow', 'head\n']),
                patch.object(RUNNER.subprocess, 'run') as fetch,
            ):
                RUNNER.complete_scan_history(root, 'git', 'base', 'head')
            fetch.assert_not_called()

    def test_incomplete_ci_range_fails(self):
        with self.assertRaises(ValueError):
            RUNNER.scan_arguments(Path.cwd(), {'PRE_COMMIT_FROM_REF': 'base'})

    @patch.object(RUNNER, 'scan_arguments', return_value=['protect', '--staged'])
    @patch.object(RUNNER.shutil, 'which', return_value='gitleaks')
    def test_scan_failure_is_preserved_and_redacted(self, _which, _arguments):
        with patch.object(RUNNER.subprocess, 'run', return_value=subprocess.CompletedProcess([], 1)) as scan:
            self.assertEqual(RUNNER.main(), 1)
        self.assertIn('--redact', scan.call_args.args[0])
        self.assertEqual(scan.call_args.kwargs['timeout'], 140)

    @patch.object(RUNNER, 'scan_arguments', return_value=['protect', '--staged'])
    @patch.object(RUNNER.shutil, 'which', return_value='gitleaks')
    def test_timeout_is_a_failure(self, _which, _arguments):
        with patch.object(RUNNER.subprocess, 'run', side_effect=subprocess.TimeoutExpired('gitleaks', 140)):
            self.assertEqual(RUNNER.main(), 124)


if __name__ == '__main__':
    unittest.main()
