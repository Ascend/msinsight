"""Regression checks for frontend dependency preparation on fresh CI workspaces."""

import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch


def load_frontend_build():
    path = Path(__file__).parents[2] / 'modules' / 'build' / 'build.py'
    spec = importlib.util.spec_from_file_location('insight_frontend_build_test', path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class FrontendDependenciesTest(unittest.TestCase):
    def setUp(self):
        self.build = load_frontend_build()
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.build.MODULES_DIR = self.temp.name
        for module in ('lib', 'framework', *self.build.MODULES_MAP):
            directory = Path(self.temp.name) / module
            directory.mkdir(parents=True)
            (directory / 'package.json').write_text(
                json.dumps({'dependencies': {'fixture': '1.0.0'}}), encoding='utf-8')
        self.platform = patch.object(self.build.platform, 'system', return_value='Windows')
        self.platform.start()
        self.addCleanup(self.platform.stop)

    def provision(self):
        root = Path(self.temp.name)
        for module in ['', 'lib', 'framework', *self.build.MODULES_MAP]:
            (root / module / 'node_modules').mkdir(parents=True, exist_ok=True)
        bin_dir = root / 'node_modules' / '.bin'
        bin_dir.mkdir(exist_ok=True)
        for tool in ('cross-env', 'craco', 'react-scripts', 'esbuild'):
            (bin_dir / f'{tool}.cmd').write_text('fixture', encoding='ascii')

    def test_prepared_offline_build_does_not_install(self):
        self.provision()
        with patch.object(self.build, 'execute_cmd') as execute:
            self.assertEqual(self.build.prepare_dependencies(False), 0)
        execute.assert_not_called()

    def test_packages_without_dependencies_need_no_local_node_modules(self):
        self.provision()
        root = Path(self.temp.name)
        (root / 'lib' / 'package.json').write_text('{}', encoding='utf-8')
        (root / 'lib' / 'node_modules').rmdir()
        with patch.object(self.build, 'execute_cmd') as execute:
            self.assertEqual(self.build.prepare_dependencies(False), 0)
        execute.assert_not_called()

    def test_fresh_workspace_force_installs_complete_workspace(self):
        def install(*_args):
            self.provision()
            return 0

        with patch.object(self.build, 'execute_cmd', side_effect=install) as execute:
            self.assertEqual(self.build.prepare_dependencies(False), 0)
        execute.assert_called_once_with('modules', self.temp.name,
                                        ['pnpm.cmd', 'install', '--force', '--prod=false', '--frozen-lockfile'])

    def test_missing_module_links_or_build_tools_trigger_fallback(self):
        for missing in ('leaks/node_modules', 'framework/node_modules', 'node_modules/.bin/cross-env.cmd'):
            with self.subTest(missing=missing):
                self.provision()
                path = Path(self.temp.name) / missing
                path.rmdir() if path.is_dir() else path.unlink()
                with patch.object(self.build, 'execute_cmd', return_value=1) as execute:
                    self.assertEqual(self.build.prepare_dependencies(False), 1)
                self.assertIn('--force', execute.call_args.args[2])

    def test_install_failure_stops_before_parallel_compilation(self):
        with patch.object(self.build, 'execute_cmd', return_value=1), \
                patch.object(self.build.multiprocessing, 'Pool') as pool:
            self.assertEqual(self.build.parallel_build(False), 1)
        pool.assert_not_called()

    def test_successful_install_without_required_files_still_fails(self):
        with patch.object(self.build, 'execute_cmd', return_value=0), \
                patch.object(self.build.multiprocessing, 'Pool') as pool:
            self.assertEqual(self.build.parallel_build(False), 1)
        pool.assert_not_called()

    def test_normal_install_preserves_existing_command(self):
        self.provision()
        with patch.object(self.build, 'execute_cmd', return_value=0) as execute:
            self.assertEqual(self.build.prepare_dependencies(True), 0)
        execute.assert_called_once_with('modules', self.temp.name, ['pnpm.cmd', 'install'])

    def test_unix_build_checks_executable_names_without_cmd_suffix(self):
        self.provision()
        with patch.object(self.build.platform, 'system', return_value='Linux'):
            self.assertIn('node_modules/.bin/craco',
                          [path.replace('\\', '/') for path in self.build.missing_build_dependencies()])


if __name__ == '__main__':
    unittest.main()
