"""Smoke tests for the shared CI build library."""

import subprocess
import unittest

from ci_build_test_helpers import SCRIPT, isolated_env, run_bash


class CiBuildCommonSmokeTest(unittest.TestCase):
    def test_common_rejects_direct_execution(self):
        result = subprocess.run(
            ['bash', str(SCRIPT)],
            check=False,
            capture_output=True,
            text=True,
            env=isolated_env(),
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('must be sourced', result.stderr)
        self.assertNotIn('Build pipeline completed', result.stderr)

    def test_sourcing_common_does_not_run_the_pipeline(self):
        result = run_bash(
            'source "$1"; printf "sourced\\n"; '
            'declare -F run_pipeline >/dev/null; declare -F handle_cli_args >/dev/null'
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, 'sourced\n')
        self.assertNotIn('Build pipeline completed', result.stderr)
        self.assertNotIn('Downloading third-party', result.stderr)

    def test_resolve_environment_defaults_the_pip_index_to_the_aliyun_mirror(self):
        # build.py reads PIP_INDEX_URL; without the mirror a single 11.7 MB
        # wheel download from pypi.org took 8 minutes inside the CI network.
        result = run_bash(
            'source "$1"; set_platform Linux x86_64; HOME=/nonexistent; '
            'PATH=/usr/bin:/bin; unset PIP_INDEX_URL; resolve_environment; '
            'printf "%s" "$PIP_INDEX_URL"'
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, 'https://mirrors.aliyun.com/pypi/simple/')

    def test_resolve_environment_keeps_an_explicit_pip_index(self):
        result = run_bash(
            'source "$1"; set_platform Linux x86_64; HOME=/nonexistent; '
            'PATH=/usr/bin:/bin; PIP_INDEX_URL=https://pypi.org/simple; '
            'resolve_environment; printf "%s" "$PIP_INDEX_URL"'
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, 'https://pypi.org/simple')


if __name__ == '__main__':
    unittest.main()
