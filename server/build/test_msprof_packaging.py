"""Offline synthetic msprof packaging tests; never build or execute third-party code.

Set MSINSIGHT_MSPROF_TEST_ROOT to an explicitly owned fresh fixture parent.
Fixtures are retained for inspection; this suite does not delete existing artifacts.
"""
import importlib.util
import json
import os
from pathlib import Path
import stat
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

BUILD_DIR = Path(__file__).resolve().parent
LAUNCHER = BUILD_DIR / 'msprof_cluster_entry.py'
PROJECT_DIR = BUILD_DIR.parents[1]


class MsprofPackagingTest(unittest.TestCase):
    def setUp(self):
        value = os.environ.get('MSINSIGHT_MSPROF_TEST_ROOT')
        if not value:
            self.fail('Explicit owned MSINSIGHT_MSPROF_TEST_ROOT is required')
        root = Path(value)
        self.assertTrue(root.is_absolute())
        for path in reversed([root, *root.parents]):
            info = os.lstat(path)
            self.assertTrue(stat.S_ISDIR(info.st_mode) and not stat.S_ISLNK(info.st_mode), str(path))
        self.case = Path(tempfile.mkdtemp(prefix=self._testMethodName + '-', dir=root))
        self.assertEqual(self.case.parent.resolve(), root.resolve())
        spec = importlib.util.spec_from_file_location('msinsight_server_build_fixture', BUILD_DIR / 'build.py')
        self.builder = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.builder)  # Product definitions only: main/build/init_log are not called.
        self.source = self.case / 'source/msprof_analyze'
        for name in ('cluster_analyse', 'prof_common', 'prof_exports'):
            subtree = self.source / name
            subtree.mkdir(parents=True)
            self.write(subtree / '__init__.py', '')
            self.write(subtree / 'marker.py', "ORIGIN = 'source-marker'\n")
        self.write(self.source / '__init__.py', "ORIGIN = 'source-marker'\n")
        self.write(self.source / 'cluster_analyse/cluster_analysis.py', '''import json
import sys
import msprof_analyze
from msprof_analyze.prof_common import marker as common
from msprof_analyze.prof_exports import marker as exports
print(json.dumps({'origin': msprof_analyze.ORIGIN, 'origins': [common.ORIGIN, exports.ORIGIN], 'files': [msprof_analyze.__file__, common.__file__, exports.__file__, __file__], 'args': sys.argv[1:]}))
''')
        self.bin = self.case / 'output/fixture/bin'
        self.bin.mkdir(parents=True)
        overrides = {'OUTPUT_DIR': str(self.case / 'output'),
                     'CLUSTER_ANALYSE_DIR': str(self.source / 'cluster_analyse'),
                     'PROF_COMMON_DIR': str(self.source / 'prof_common'),
                     'PROF_EXPORTS_DIR': str(self.source / 'prof_exports')}
        for name, value in overrides.items():
            patcher = mock.patch.object(self.builder, name, value)
            patcher.start()
            self.addCleanup(patcher.stop)
        patcher = mock.patch.object(self.builder, 'get_gxx_type', return_value='fixture')
        patcher.start()
        self.addCleanup(patcher.stop)
        self.site = self.case / 'fake-site'
        for name in ('', 'prof_common', 'prof_exports'):
            package = self.site / 'msprof_analyze' / name
            package.mkdir(parents=True, exist_ok=True)
            self.write(package / '__init__.py', "ORIGIN = 'wheel-marker'\n")
            if name:
                self.write(package / 'marker.py', "ORIGIN = 'wheel-marker'\n")

    @staticmethod
    def write(path, text):
        with path.open('x', encoding='utf-8') as output:
            output.write(text)

    def copy(self):
        self.assertFalse((self.bin / 'msprof_analyze').exists())
        self.assertFalse((self.bin / 'msprof_cluster_entry.py').exists())
        self.builder.copy_msprof2server_dir()

    def launcher_only(self):
        self.assertTrue(LAUNCHER.is_file(), 'Owned launcher is not implemented')
        self.write(self.bin / 'msprof_cluster_entry.py', LAUNCHER.read_text())

    def run_launcher(self, args=()):
        code = "import runpy,sys; sys.path.insert(0,sys.argv[1]); sys.argv=sys.argv[2:]; runpy.run_path(sys.argv[0],run_name='__main__')"
        argv = [sys.executable, '-I', '-B', '-S', '-c', code, str(self.site),
                str(self.bin / 'msprof_cluster_entry.py'), *args]
        return subprocess.run(argv, cwd=self.case, env={'PATH': '/usr/bin:/bin', 'HOME': str(self.case),
                              'PYTHONDONTWRITEBYTECODE': '1'}, capture_output=True, text=True, timeout=30)

    def test_copy_complete_package_root_and_launcher(self):
        self.copy()
        self.assertEqual((self.bin / 'msprof_analyze/__init__.py').read_bytes(),
                         (self.source / '__init__.py').read_bytes())
        self.assertTrue((self.bin / 'msprof_cluster_entry.py').is_file())
        self.assertEqual((self.bin / 'msprof_cluster_entry.py').read_bytes(), LAUNCHER.read_bytes())

    def test_source_subtrees_preserve_bytes(self):
        self.copy()
        for name in ('cluster_analyse', 'prof_common', 'prof_exports'):
            for filename in ('__init__.py', 'marker.py'):
                self.assertEqual((self.bin / 'msprof_analyze' / name / filename).read_bytes(),
                                 (self.source / name / filename).read_bytes())
        self.assertEqual((self.bin / 'msprof_analyze/cluster_analyse/cluster_analysis.py').read_bytes(),
                         (self.source / 'cluster_analyse/cluster_analysis.py').read_bytes())

    def test_owned_source_wins_over_regular_site(self):
        self.copy()
        result = self.run_launcher()
        self.assertEqual(result.returncode, 0, result.stderr)
        observed = json.loads(result.stdout)
        self.assertEqual(observed['origin'], 'source-marker')
        self.assertEqual(observed['origins'], ['source-marker', 'source-marker'])
        for filename in observed['files']:
            self.assertTrue(Path(filename).resolve().is_relative_to(self.bin / 'msprof_analyze'))

    def test_cluster_and_mfu_arguments_preserved(self):
        self.copy()
        args = ['-d', '/synthetic/path with spaces', '-m', 'operator_mfu']
        result = self.run_launcher(args)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)['args'], args)

    def test_missing_source_package_root_fails_before_copy(self):
        empty = self.case / 'missing-source/msprof_analyze'
        for name in ('cluster_analyse', 'prof_common', 'prof_exports'):
            (empty / name).mkdir(parents=True)
        with mock.patch.object(self.builder, 'CLUSTER_ANALYSE_DIR', str(empty / 'cluster_analyse')):
            with self.assertRaises(FileNotFoundError):
                self.builder.copy_msprof2server_dir()
        self.assertFalse((self.bin / 'msprof_analyze').exists())

    def test_launcher_missing_root_never_falls_back_to_wheel(self):
        self.launcher_only()
        result = self.run_launcher()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Owned msprof package', result.stderr)
        self.assertEqual(result.stdout, '')

    def test_launcher_rejects_escaped_package_init(self):
        self.launcher_only()
        package = self.bin / 'msprof_analyze'
        package.mkdir()
        outside = self.case / 'outside-init.py'
        self.write(outside, "raise AssertionError('escaped init must not execute')\n")
        (package / '__init__.py').symlink_to(outside)
        result = self.run_launcher()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Owned msprof package', result.stderr)
        self.assertNotIn('escaped init must not execute', result.stderr)
        self.assertEqual(result.stdout, '')

    def test_launcher_rejects_escaped_entry(self):
        self.launcher_only()
        package = self.bin / 'msprof_analyze'
        (package / 'cluster_analyse').mkdir(parents=True)
        self.write(package / '__init__.py', "ORIGIN = 'source-marker'\n")
        outside = self.case / 'outside-entry.py'
        self.write(outside, "raise AssertionError('escaped entry must not execute')\n")
        (package / 'cluster_analyse/cluster_analysis.py').symlink_to(outside)
        result = self.run_launcher()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Owned msprof entry', result.stderr)
        self.assertNotIn('escaped entry must not execute', result.stderr)
        self.assertEqual(result.stdout, '')

    def test_cpp_cluster_and_mfu_use_owned_launcher(self):
        source = (PROJECT_DIR / 'server/src/modules/communication/parser/ClusterFileParser.cpp').read_text()
        self.assertEqual(source.count('const std::string scriptPath = "msprof_cluster_entry.py";'), 2)
        self.assertNotIn('FILE_SEPARATOR + "cluster_analyse" + FILE_SEPARATOR + "cluster_analysis.py"', source)
        self.assertIn('std::vector<std::string> arguments{"-m", "operator_mfu", "-d", selectedPath}', source)


if __name__ == '__main__':
    unittest.main(verbosity=2)
