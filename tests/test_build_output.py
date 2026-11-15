"""Build failures retain the actual child output for Visual Studio diagnostics."""
import contextlib
import io
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
import build


class BuildOutputTests(unittest.TestCase):
    def test_real_subprocess_failure_retains_compiler_diagnostic(self):
        with tempfile.TemporaryDirectory(prefix='Last Light Output ') as directory:
            root = Path(directory)
            with patch.object(build, 'ROOT', root), contextlib.redirect_stdout(io.StringIO()) as visible:
                with self.assertRaisesRegex(RuntimeError, 'code 17'):
                    build.run_build([sys.executable, '-c',
                                     'import sys;print("compiler diagnostic");sys.exit(17)'], root, os.environ.copy())
            self.assertIn('compiler diagnostic', visible.getvalue())
            self.assertIn('compiler diagnostic', (root / 'reports/godot-build.log').read_text())

    def test_real_success_streams_output_and_retains_log(self):
        with tempfile.TemporaryDirectory(prefix='Last Light Output ') as directory:
            root = Path(directory)
            with patch.object(build, 'ROOT', root), contextlib.redirect_stdout(io.StringIO()) as visible:
                build.run_build([sys.executable, '-c', 'print("build completed")'], root, os.environ.copy())
            self.assertIn('build completed', visible.getvalue())
            self.assertEqual((root / 'reports/godot-build.log').read_text().strip(), 'build completed')


if __name__ == '__main__':
    unittest.main()
