import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('lastlight_setup', ROOT / 'scripts/setup.py')
setup = importlib.util.module_from_spec(spec)
spec.loader.exec_module(setup)


def git(path, *args):
    return subprocess.run(['git', '-c', 'user.name=Setup Test', '-c', 'user.email=setup@example.invalid', *args], cwd=path, check=True, capture_output=True).stdout


def fixture(path):
    path.mkdir(parents=True)
    git(path, 'init', '-q')
    (path / 'missing [1].txt').write_text('original missing file')
    (path / 'edited.txt').write_text('original edit file')
    git(path, 'add', '.')
    git(path, 'commit', '-qm', 'initial')
    return git(path, 'rev-parse', 'HEAD').decode().strip()


class SetupTests(unittest.TestCase):
    def test_missing_only_repair_preserves_worktree_and_index_edits(self):
        with tempfile.TemporaryDirectory(prefix='Long project path ') as directory:
            engine = Path(directory) / 'engine'
            fixture(engine)
            git(engine, 'rm', '--', 'missing [1].txt')
            (engine / 'edited.txt').write_text('my own staged engine edit')
            git(engine, 'add', 'edited.txt')
            self.assertEqual(setup.repair_missing_files(engine), 1)
            self.assertEqual((engine / 'missing [1].txt').read_text(), 'original missing file')
            self.assertEqual((engine / 'edited.txt').read_text(), 'my own staged engine edit')
            self.assertEqual(git(engine, 'show', ':edited.txt').decode(), 'my own staged engine edit')
            self.assertEqual(setup.repair_missing_files(engine), 0)

    def test_failed_clone_with_empty_index_is_repaired_without_overwriting_files(self):
        with tempfile.TemporaryDirectory() as directory:
            engine = Path(directory) / 'engine'
            fixture(engine)
            (engine / '.git/index').unlink()
            (engine / 'missing [1].txt').unlink()
            (engine / 'edited.txt').write_text('local edit after failed checkout')
            self.assertEqual(setup.repair_missing_files(engine), 1)
            self.assertEqual((engine / 'edited.txt').read_text(), 'local edit after failed checkout')
            self.assertEqual((engine / 'missing [1].txt').read_text(), 'original missing file')
            self.assertEqual(git(engine, 'diff', '--cached', '--name-only').strip(), b'')
            self.assertEqual(git(engine, 'diff', '--name-only').decode().strip(), 'edited.txt')

    def test_long_nested_filename_and_git_configuration(self):
        with tempfile.TemporaryDirectory() as directory:
            engine = Path(directory) / 'engine'
            fixture(engine)
            relative = '/'.join(['nested-directory-' + str(i) for i in range(14)]) + '/file.txt'
            file = engine / relative
            extended_file = Path(setup.fs_path(file))
            extended_file.parent.mkdir(parents=True)
            extended_file.write_text('long path')
            git(engine, '-c', 'core.longpaths=true', 'add', '.')
            git(engine, 'commit', '-qm', 'long file')
            extended_file.unlink()
            self.assertEqual(setup.repair_missing_files(engine), 1)
            self.assertEqual(extended_file.read_text(), 'long path')
            self.assertEqual(setup.GIT[:3], ['git', '-c', 'core.longpaths=true'])

    def test_setup_refuses_edits_after_restoring_missing_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            git(root, 'init', '-q')
            engine = root / 'vendor/godot'
            commit = fixture(engine)
            git(root, 'update-index', '--add', '--cacheinfo', f'160000,{commit},vendor/godot')
            (engine / 'missing [1].txt').unlink()
            (engine / 'edited.txt').write_text('preserve this change')
            with patch.object(setup, 'ROOT', root), patch.object(setup, 'LOCK', {'commit': commit}):
                with self.assertRaisesRegex(RuntimeError, 'local changes'):
                    setup.engine_setup(repair_missing=True)
            self.assertEqual((engine / 'edited.txt').read_text(), 'preserve this change')
            self.assertTrue((engine / 'missing [1].txt').exists())
            self.assertEqual(git(root, 'config', '--local', 'core.longpaths').strip(), b'true')
            self.assertEqual(git(engine, 'config', '--local', 'core.longpaths').strip(), b'true')

    def test_repair_and_setup_resume_without_recloning(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            git(root, 'init', '-q')
            engine = root / 'vendor/godot'
            commit = fixture(engine)
            git(root, 'update-index', '--add', '--cacheinfo', f'160000,{commit},vendor/godot')
            (engine / 'missing [1].txt').unlink()
            with patch.object(setup, 'ROOT', root), patch.object(setup, 'LOCK', {'commit': commit}):
                with self.assertRaisesRegex(RuntimeError, 'missing tracked files'):
                    setup.engine_setup()
                setup.engine_setup(repair_missing=True)
                setup.engine_setup()
            self.assertFalse(git(engine, 'status', '--porcelain').strip())
            self.assertEqual(git(engine, 'rev-parse', 'HEAD').decode().strip(), commit)


if __name__ == '__main__':
    unittest.main()
