"""Contract tests for the macOS bundled Python interpreter permissions."""

import importlib.util
import os
import shutil
import stat
import sys
import tempfile
import unittest
from pathlib import Path


BUILD_DIR = Path(__file__).parents[2]
BUILD_SCRIPT = BUILD_DIR / 'build.py'


def load_build_module():
    if str(BUILD_DIR) not in sys.path:
        sys.path.insert(0, str(BUILD_DIR))
    spec = importlib.util.spec_from_file_location('build_script_for_test', BUILD_SCRIPT)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class MacBundledPythonPermissionTest(unittest.TestCase):
    """复现签名后权限回收，并校验恢复函数与打包守卫的行为。"""

    @classmethod
    def setUpClass(cls):
        cls.module = load_build_module()

    def setUp(self):
        temporary = Path(tempfile.mkdtemp(prefix='mac-bundled-python-'))
        self.addCleanup(shutil.rmtree, temporary, True)
        self.resources_dir = temporary / 'MindStudioInsight.app' / 'Contents' / 'Resources'
        (self.resources_dir / 'python' / 'bin').mkdir(parents=True)
        self.app_path = self.resources_dir.parents[1]
        self.python3 = self.resources_dir / 'python' / 'bin' / 'python3'
        shutil.copyfile('/bin/echo', self.python3)
        os.chmod(self.python3, 0o550)

    def test_verify_rejects_missing_interpreter(self):
        self.python3.unlink()
        self.assertFalse(self.module.verify_mac_bundled_python_executable(str(self.app_path)))

    def test_verify_rejects_non_executable_interpreter(self):
        os.chmod(self.python3, 0o440)
        self.assertFalse(self.module.verify_mac_bundled_python_executable(str(self.app_path)))

    def test_verify_accepts_executable_interpreter(self):
        self.assertTrue(self.module.verify_mac_bundled_python_executable(str(self.app_path)))

    def test_restore_grants_execute_after_signature_permission_recall(self):
        # 复现 resign_mac_app 的签名后回收：Resources 全树 0440，仅 server 子树恢复 0550
        server_dir = self.resources_dir / 'profiler' / 'server'
        server_dir.mkdir(parents=True)
        self.module.traverse_folder_and_chmod(str(self.resources_dir), 0o750, 0o440)
        self.module.traverse_folder_and_chmod(str(server_dir), 0o750, 0o550)
        self.assertFalse(os.access(self.python3, os.X_OK))

        self.module.restore_mac_bundled_python_permissions(str(self.resources_dir))

        self.assertTrue(os.access(self.python3, os.X_OK))
        self.assertEqual(stat.S_IMODE(self.python3.stat().st_mode), 0o550)
        self.assertTrue(self.module.verify_mac_bundled_python_executable(str(self.app_path)))

    def test_restore_is_noop_without_bundled_interpreter(self):
        shutil.rmtree(self.resources_dir / 'python')
        self.module.restore_mac_bundled_python_permissions(str(self.resources_dir))
        self.assertFalse((self.resources_dir / 'python').exists())


if __name__ == '__main__':
    unittest.main()
