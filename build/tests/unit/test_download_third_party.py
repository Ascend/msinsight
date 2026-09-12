"""Contract tests for third-party clone settings used by CI builds."""

import importlib.util
import subprocess
import sys
import tempfile
import types
import unittest
from pathlib import Path
from unittest import mock


THIRD_PARTY_DOWNLOADER = Path(__file__).parents[3] / 'server' / 'build' / 'download_third_party.py'


def load_third_party_downloader():
    fake_build = types.ModuleType('build')
    fake_build.init_log = lambda _name: mock.Mock()
    fake_build.log_output = mock.Mock()
    spec = importlib.util.spec_from_file_location('download_third_party_for_test', THIRD_PARTY_DOWNLOADER)
    module = importlib.util.module_from_spec(spec)
    with mock.patch.dict(sys.modules, {'build': fake_build}):
        spec.loader.exec_module(module)
    return module


class DownloadThirdPartyTest(unittest.TestCase):
    def test_third_party_clone_persists_autocrlf_false(self):
        with tempfile.TemporaryDirectory() as directory:
            downloader = load_third_party_downloader()
            downloader.THIRD_PARTY_DIR = directory
            downloader.OPEN_SOURCE = [['dependency', 'v1.0.0', 'https://example.invalid/dependency.git']]
            process = mock.MagicMock()
            process.__enter__.return_value = mock.Mock()
            process.__exit__.return_value = False
            with mock.patch.object(downloader.subprocess, 'Popen', return_value=process) as popen:
                downloader.download_3rd_party()

            cmd = popen.call_args[0][0]
            self.assertEqual(cmd[:4], ['git', 'clone', '--config', 'core.autocrlf=false'])
            self.assertEqual(popen.call_args.kwargs['cwd'], directory)
            self.assertEqual(popen.call_args.kwargs['stdout'], subprocess.PIPE)


if __name__ == '__main__':
    unittest.main()
