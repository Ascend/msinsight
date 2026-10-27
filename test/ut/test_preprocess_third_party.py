"""
-------------------------------------------------------------------------
This file is part of the MindStudio project.
Copyright (c) 2026 Huawei Technologies Co.,Ltd.

MindStudio is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:

    http://license.coscl.org.cn/MulanPSL2

THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details.
-------------------------------------------------------------------------
"""

# Offline sqlite preparation contract tests; no third-party downloads or compiler required.

import hashlib
import io
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
from concurrent.futures import ThreadPoolExecutor
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'server' / 'build'))
import preprocess_third_party as prep


class SqlitePreparationTest(unittest.TestCase):
    def setUp(self):
        # unittest enterContext registers __exit__ as cleanup, including on test failure.
        self.root = Path(self.enterContext(tempfile.TemporaryDirectory()))  # pylint: disable=consider-using-with
        self.workspace = self.root / 'workspace'
        self.workspace.mkdir()
        self.cache = self.root / 'cache'
        self.cache.mkdir()
        buffer = io.BytesIO()
        with tarfile.open(fileobj=buffer, mode='w:gz') as archive:
            for name in ('sqlite3.c', 'sqlite3.h', 'sqlite3ext.h', 'shell.c'):
                data = b'fixture source\n'
                member = tarfile.TarInfo(f'{prep.SQLITE3_AUTOCONF_DIR}/{name}')
                member.size = len(data)
                archive.addfile(member, io.BytesIO(data))
        self.payload = buffer.getvalue()
        self.digest = hashlib.sha256(self.payload).hexdigest()
        # Preserve Windows runtime variables (e.g. SystemRoot) for child-process tests.
        environment = {
            key: value
            for key, value in os.environ.items()
            if key not in ('MSINSIGHT_SQLITE3_CACHE_DIR', 'MSINSIGHT_SQLITE3_URL')
        }
        environment['MSINSIGHT_SQLITE3_CACHE_DIR'] = str(self.cache)
        for mocker in (
            patch.object(prep, 'THIRD_PARTY_DIR', str(self.workspace)),
            patch.object(prep.platform, 'system', return_value='Windows'),
            patch.object(prep, 'is_mingw_available', return_value=(True, 'fixture MinGW')),
            patch.object(prep, 'SQLITE3_SOURCE_SHA256', self.digest),
            patch.dict(os.environ, environment, clear=True),
        ):
            mocker.start()
            self.addCleanup(mocker.stop)
        self.download = self.enterContext(patch.object(prep.urllib.request, 'urlretrieve', side_effect=self.fetch))

    @property
    def cache_file(self):
        return self.cache / f'{prep.SQLITE3_AUTOCONF_DIR}.tar.gz'

    def fetch(self, url, path):
        Path(path).write_bytes(self.payload)
        return path, None

    def assert_prepared(self):
        self.assertEqual((self.workspace / prep.SQLITE3_AUTOCONF_DIR / 'sqlite3.c').read_bytes(), b'fixture source\n')
        self.assertEqual(list(self.workspace.glob('*.tar.gz')), [])

    def test_hit_forbids_download(self):
        self.cache_file.write_bytes(self.payload)
        self.download.side_effect = AssertionError('network forbidden')
        with self.assertLogs(level='INFO') as logs:
            self.assertTrue(prep.prepare_sqlite_src())
        self.download.assert_not_called()
        self.assert_prepared()
        output = '\n'.join(logs.output)
        for expected in (
            'cache_read_verify status=hit',
            'download status=skipped',
            'extract status=success',
            'elapsed=',
        ):
            self.assertIn(expected, output)

    def test_cold_then_fresh_workspace_hit(self):
        with self.assertLogs(level='INFO') as logs:
            self.assertTrue(prep.prepare_sqlite_src())
        self.assertEqual(self.cache_file.read_bytes(), self.payload)
        self.assert_prepared()
        for phase in (
            'cache_read_verify status=missing',
            'download status=success',
            'download_verify status=valid',
            'cache_write status=success',
        ):
            self.assertIn(phase, '\n'.join(logs.output))
        fresh = self.root / 'fresh'
        fresh.mkdir()
        with patch.object(prep, 'THIRD_PARTY_DIR', str(fresh)):
            self.download.side_effect = AssertionError('network forbidden')
            self.assertTrue(prep.prepare_sqlite_src())
        self.assertEqual(self.download.call_count, 1)
        self.assertTrue((fresh / prep.SQLITE3_AUTOCONF_DIR / 'sqlite3.c').exists())

    def test_corrupt_cache_falls_back(self):
        self.cache_file.write_bytes(b'corrupt')
        with self.assertLogs(level='INFO') as logs:
            self.assertTrue(prep.prepare_sqlite_src())
        self.assertIn('cache_read_verify status=invalid', '\n'.join(logs.output))
        self.download.assert_called_once()
        self.assertEqual(self.cache_file.read_bytes(), self.payload)
        self.assert_prepared()

    def test_unreadable_cache_falls_back(self):
        self.cache_file.write_bytes(self.payload)
        original = prep.shutil.copyfile

        def copy(source, target):
            if Path(source) == self.cache_file:
                raise PermissionError('private path')
            return original(source, target)

        with patch.object(prep.shutil, 'copyfile', side_effect=copy), self.assertLogs(level='INFO') as logs:
            self.assertTrue(prep.prepare_sqlite_src())
        self.assertIn('cache_read_verify status=unreadable', '\n'.join(logs.output))
        self.assert_prepared()

    def test_wrong_hash_is_not_extracted_or_published(self):
        self.download.side_effect = lambda url, path: Path(path).write_bytes(b'wrong')
        with patch.object(prep.tarfile, 'open') as extract, self.assertLogs(level='ERROR'):
            self.assertFalse(prep.prepare_sqlite_src())
        extract.assert_not_called()
        self.assertFalse(self.cache_file.exists())
        self.assertEqual(list(self.workspace.iterdir()), [])
        self.assertEqual(list(self.cache.iterdir()), [])

    def test_failed_download_preserves_existing_invalid_cache(self):
        self.cache_file.write_bytes(b'old invalid cache')
        self.download.side_effect = OSError('download failed')
        with patch.object(prep.tarfile, 'open') as extract, self.assertLogs(level='ERROR'):
            self.assertFalse(prep.prepare_sqlite_src())
        extract.assert_not_called()
        self.assertEqual(self.cache_file.read_bytes(), b'old invalid cache')
        self.assertEqual(list(self.workspace.iterdir()), [])

    def test_download_verification_io_error_fails_with_phase(self):
        with (
            patch.object(prep, 'verify_sqlite_archive', side_effect=PermissionError('private path')),
            patch.object(prep.tarfile, 'open') as extract,
            self.assertLogs(level='ERROR') as logs,
        ):
            self.assertFalse(prep.prepare_sqlite_src())
        self.assertIn('download_verify status=failed', '\n'.join(logs.output))
        extract.assert_not_called()
        self.assertEqual(list(self.workspace.iterdir()), [])
        self.assertFalse(self.cache_file.exists())

    def test_extract_failure_cleans_workspace_archive(self):
        self.cache_file.write_bytes(self.payload)
        with (
            patch.object(prep.tarfile, 'open', side_effect=tarfile.ReadError('bad archive')),
            self.assertLogs(level='ERROR') as logs,
        ):
            self.assertFalse(prep.prepare_sqlite_src())
        self.assertIn('extract status=failed', '\n'.join(logs.output))
        self.download.assert_not_called()
        self.assertEqual(list(self.workspace.iterdir()), [])

    def test_cache_tempfile_denied_still_builds(self):
        original = prep.tempfile.NamedTemporaryFile

        def temporary(*args, **kwargs):
            if Path(kwargs['dir']) == self.cache:
                raise PermissionError('read-only cache directory')
            return original(*args, **kwargs)

        with (
            patch.object(prep.tempfile, 'NamedTemporaryFile', side_effect=temporary),
            self.assertLogs(level='WARNING') as logs,
        ):
            self.assertTrue(prep.prepare_sqlite_src())
        self.assertIn('cache_write status=failed', '\n'.join(logs.output))
        self.assertEqual(list(self.cache.iterdir()), [])
        self.assert_prepared()

    def test_download_error_cleans_partial_and_redacts(self):
        secret_url = 'https://user:password@example.invalid/pkg?token=secret'
        os.environ['MSINSIGHT_SQLITE3_URL'] = secret_url

        def fail(url, path):
            Path(path).write_bytes(b'partial')
            raise OSError(secret_url)

        self.download.side_effect = fail
        with patch.object(prep.tarfile, 'open') as extract, self.assertLogs(level='INFO') as logs:
            self.assertFalse(prep.prepare_sqlite_src())
        extract.assert_not_called()
        self.assertEqual(list(self.workspace.iterdir()), [])
        self.assertEqual(list(self.cache.iterdir()), [])
        output = '\n'.join(logs.output)
        self.assertIn('download status=failed', output)
        for sensitive in (secret_url, 'password', 'token=secret'):
            self.assertNotIn(sensitive, output)

    def test_unconfigured_cache(self):
        del os.environ['MSINSIGHT_SQLITE3_CACHE_DIR']
        with self.assertLogs(level='INFO') as logs:
            self.assertTrue(prep.prepare_sqlite_src())
        self.assertIn('cache_read_verify status=unconfigured', '\n'.join(logs.output))
        self.assertEqual(list(self.cache.iterdir()), [])
        self.assert_prepared()

    def test_override_uses_same_hash_and_redacts_success(self):
        secret_url = 'https://user:password@example.invalid/pkg?token=secret'
        os.environ['MSINSIGHT_SQLITE3_URL'] = secret_url
        with self.assertLogs(level='INFO') as logs:
            self.assertTrue(prep.prepare_sqlite_src())
        self.assertEqual(self.download.call_args.args[0], secret_url)
        self.assertNotIn(secret_url, '\n'.join(logs.output))
        self.assertEqual(self.cache_file.read_bytes(), self.payload)

    def test_override_wrong_hash_is_rejected(self):
        os.environ['MSINSIGHT_SQLITE3_URL'] = 'https://example.invalid/pkg'
        self.download.side_effect = lambda url, path: Path(path).write_bytes(b'wrong')
        with self.assertLogs(level='ERROR'):
            self.assertFalse(prep.prepare_sqlite_src())
        self.assertFalse(self.cache_file.exists())

    def test_default_source(self):
        self.assertTrue(prep.prepare_sqlite_src())
        self.assertEqual(self.download.call_args.args[0], prep.SQLITE3_SOURCE_URL)

    def test_write_failures_warn_but_build(self):
        original = prep.shutil.copyfile
        original_makedirs = prep.os.makedirs

        def copy(source, target):
            if Path(target).parent == self.cache:
                Path(target).write_bytes(b'partial')
                raise PermissionError('write denied')
            return original(source, target)

        def makedirs(path, *args, **kwargs):
            if Path(path) == self.cache:
                raise PermissionError('write denied')
            return original_makedirs(path, *args, **kwargs)

        for operation in ('mkdir', 'copy', 'replace'):
            with self.subTest(operation=operation), tempfile.TemporaryDirectory(dir=self.root) as workspace:
                target = {'mkdir': 'os.makedirs', 'copy': 'shutil.copyfile', 'replace': 'os.replace'}[operation]
                effect = {'mkdir': makedirs, 'copy': copy, 'replace': PermissionError('write denied')}[operation]
                with (
                    patch.object(prep, 'THIRD_PARTY_DIR', workspace),
                    patch(f'preprocess_third_party.{target}', side_effect=effect),
                    self.assertLogs(level='WARNING'),
                ):
                    self.assertTrue(prep.prepare_sqlite_src())
                self.assertTrue((Path(workspace) / prep.SQLITE3_AUTOCONF_DIR / 'sqlite3.c').exists())
                self.assertEqual(list(self.cache.iterdir()), [])

    def test_existing_source_preserves_mingw_check(self):
        (self.workspace / prep.SQLITE3_AUTOCONF_DIR).mkdir()
        self.assertTrue(prep.prepare_sqlite_src())
        self.download.assert_not_called()
        with (
            patch.object(prep, 'is_mingw_available', return_value=(False, 'not MinGW')),
            self.assertLogs(level='ERROR'),
        ):
            self.assertFalse(prep.prepare_sqlite_src())

    def test_non_windows_calls_configure_and_make(self):
        source = self.workspace / prep.SQLITE3_SRC_DIR
        source.mkdir()
        (source / 'configure').write_bytes(b'fixture')
        build = source / 'build'
        build.mkdir()
        (build / 'stale').write_bytes(b'stale')
        with (
            patch.object(prep.platform, 'system', return_value='Linux'),
            patch.object(prep.os, 'chdir') as chdir,
            patch.object(prep, 'exec_command', return_value=0) as execute,
        ):
            self.assertTrue(prep.prepare_sqlite_src())
        self.assertFalse((build / 'stale').exists())
        chdir.assert_called_once_with(str(build))
        self.assertEqual([call.args[0] for call in execute.call_args_list], [['../configure'], ['make', 'sqlite3.c']])
        self.download.assert_not_called()
        self.assertEqual(list(self.cache.iterdir()), [])

    def test_cache_snapshot_prevents_change_after_verification(self):
        self.cache_file.write_bytes(self.payload)
        original = prep.tarfile.open

        def extract(path, *args, **kwargs):
            self.cache_file.write_bytes(b'concurrent change')
            return original(path, *args, **kwargs)

        with patch.object(prep.tarfile, 'open', side_effect=extract):
            self.assertTrue(prep.prepare_sqlite_src())
        self.download.assert_not_called()
        self.assert_prepared()

    def test_unique_download_paths_and_owned_cleanup(self):
        other = self.workspace / 'sqlite-download-other.tar.gz'
        other.write_bytes(b'other invocation')
        paths = []
        for number in range(2):
            workspace = self.root / f'job-{number}'
            workspace.mkdir()
            with (
                patch.object(prep, 'THIRD_PARTY_DIR', str(workspace)),
                patch.dict(os.environ, {'MSINSIGHT_SQLITE3_CACHE_DIR': ''}),
            ):
                self.assertTrue(prep.prepare_sqlite_src())
                paths.append(self.download.call_args.args[1])
        self.assertNotEqual(Path(paths[0]).name, Path(paths[1]).name)
        self.assertEqual(other.read_bytes(), b'other invocation')

    def test_parallel_publication_is_atomic_and_preserves_other_files(self):
        source = self.root / 'verified.tar.gz'
        source.write_bytes(self.payload)
        other = self.cache / '.sqlite-cache-other.tmp'
        other.write_bytes(b'other invocation')
        original = prep.os.replace
        published = []

        def replace(temporary, final):
            self.assertEqual(Path(temporary).parent, self.cache)
            self.assertEqual(Path(temporary).read_bytes(), self.payload)
            published.append(str(temporary))
            return original(temporary, final)

        with patch.object(prep.os, 'replace', side_effect=replace), ThreadPoolExecutor(max_workers=8) as pool:
            list(pool.map(lambda _: prep.publish_sqlite_cache(str(source), str(self.cache)), range(16)))
        self.assertEqual(len(set(published)), 16)
        self.assertEqual(self.cache_file.read_bytes(), self.payload)
        self.assertEqual(other.read_bytes(), b'other invocation')
        self.assertEqual(set(self.cache.iterdir()), {self.cache_file, other})

    def test_force_exit_during_copy_keeps_formal_cache_valid(self):
        source = self.root / 'verified.tar.gz'
        source.write_bytes(self.payload)
        script = (
            'import os, pathlib, sys; '
            'sys.path.insert(0, sys.argv[1]); import preprocess_third_party as p; '
            '\n'
            'def interrupted(source, target):\n'
            '    pathlib.Path(target).write_bytes(b"partial"); os._exit(23)\n'
            'p.shutil.copyfile = interrupted\n'
            'p.publish_sqlite_cache(sys.argv[2], sys.argv[3])\n'
        )
        for existing in (False, True):
            with self.subTest(existing=existing):
                if existing:
                    self.cache_file.write_bytes(self.payload)
                result = subprocess.run(
                    [sys.executable, '-c', script, str(Path(prep.__file__).parent), str(source), str(self.cache)],
                    check=False,
                    capture_output=True,
                    timeout=15,
                )
                self.assertEqual(result.returncode, 23, result.stderr)
                if existing:
                    self.assertEqual(self.cache_file.read_bytes(), self.payload)
                else:
                    self.assertFalse(self.cache_file.exists())
        self.assertTrue(any(path.read_bytes() == b'partial' for path in self.cache.iterdir()))
        self.assertTrue(prep.prepare_sqlite_src())
        self.download.assert_not_called()
        self.assert_prepared()


if __name__ == '__main__':
    unittest.main()
