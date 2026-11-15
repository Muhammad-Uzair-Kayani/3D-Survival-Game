import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
spec = importlib.util.spec_from_file_location('lastlight_build', ROOT / 'scripts/build.py')
build = importlib.util.module_from_spec(spec)
spec.loader.exec_module(build)


class BuildEnvironmentTests(unittest.TestCase):
    def setUp(self):
        # A user's remembered IDE selection must not influence discovery fixtures.
        self.directory = tempfile.TemporaryDirectory(prefix='Last Light Discovery ')
        self.addCleanup(self.directory.cleanup)
        self.root_patch = patch.object(build, 'ROOT', Path(self.directory.name))
        self.root_patch.start()
        self.addCleanup(self.root_patch.stop)

    def test_remembers_ide_selection_without_installer_registration(self):
        root = Path(self.directory.name)
        script = root / 'Custom VS/VC/Auxiliary/Build/vcvars64.bat'
        script.parent.mkdir(parents=True)
        script.write_text('@echo off')
        cache = root / '.tools/msvc-tools.json'
        cache.parent.mkdir()
        cache.write_text(json.dumps({'vcvars': str(script)}))
        self.assertEqual(build.find_vcvars(env={'PROGRAMFILES(X86)':str(root/'missing')}), script)

    def test_unregistered_standard_visual_studio_path(self):
        root = Path(self.directory.name)
        script = root / 'Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat'
        script.parent.mkdir(parents=True)
        script.write_text('@echo off')
        self.assertEqual(build.find_vcvars(env={'PROGRAMFILES':str(root),'PROGRAMFILES(X86)':str(root/'missing')}), script)


    def test_existing_developer_environment_skips_vswhere(self):
        env = {'PATH': 'compiler-bin', 'INCLUDE': 'include-path', 'LIB': 'lib-path', 'VSCMD_ARG_TGT_ARCH': 'x64'}
        with patch.dict(build.os.environ, env, clear=True), patch.object(build.shutil, 'which', return_value='cl.exe'), patch.object(build, 'find_vcvars') as lookup:
            self.assertEqual(build.vs_environment(), env)
            lookup.assert_not_called()

    def test_visual_studio_supplied_tools_path_with_spaces(self):
        with tempfile.TemporaryDirectory(prefix='Visual Studio ') as directory:
            vcvars = Path(directory) / 'VC/Auxiliary/Build/vcvars64.bat'
            vcvars.parent.mkdir(parents=True)
            vcvars.write_text('@echo off')
            output = 'Visual Studio tools banner\n'+build.ENV_MARKER+'\nPath=compiler-bin\nInclude=include-path\nLib=lib-path\nVSCMD_ARG_TGT_ARCH=x64\n'
            with patch.object(build.subprocess, 'check_output', return_value=output) as call, patch.object(build.shutil, 'which', return_value='cl.exe'):
                env = build.vs_environment(vcvars)
            self.assertEqual(env['INCLUDE'], 'include-path')
            self.assertEqual(env['PATH'], 'compiler-bin')
            self.assertNotIn('Path', env)
            command = call.call_args.args[0]
            self.assertIsInstance(command, str)
            self.assertIn('/d /s /v:off /c "call "' + str(vcvars) + '"', command)
            self.assertIn('&& echo ' + build.ENV_MARKER + ' && set"', command)
            self.assertNotIn('\\"', command)
            self.assertFalse(call.call_args.kwargs['shell'])
            self.assertIn('executable', call.call_args.kwargs)

    def test_trailing_marker_whitespace_from_echo_is_accepted(self):
        with tempfile.TemporaryDirectory() as directory:
            script = Path(directory) / 'vcvars64.bat'
            script.write_text('@echo off')
            output = 'Banner\r\n'+build.ENV_MARKER+'  \r\nPath=bin\r\nINCLUDE=inc\r\nLIB=lib\r\nVSCMD_ARG_TGT_ARCH=x64\r\n'
            with patch.object(build.subprocess, 'check_output', return_value=output), patch.object(build.shutil, 'which', return_value='cl.exe'):
                self.assertEqual(build.vs_environment(script)['PATH'], 'bin')

    def test_ide_path_uses_inherited_environment_without_marker_capture(self):
        env = {'PATH':'compiler-bin','INCLUDE':'inc','LIB':'lib','VSCMD_ARG_TGT_ARCH':'x64'}
        with patch.dict(build.os.environ, env, clear=True), patch.object(build.shutil, 'which', return_value='cl.exe'), patch.object(build.subprocess, 'check_output') as capture:
            self.assertEqual(build.vs_environment('already-called-by-batch.bat', from_ide=True), env)
            capture.assert_not_called()

    def test_raw_windows_command_line_preserves_exact_quoting(self):
        script = r'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat'
        comspec = r'C:\Windows\system32\cmd.exe'
        expected = f'"{comspec}" /d /s /v:off /c "call "{script}" && echo {build.ENV_MARKER} && set"'
        self.assertEqual(build.vcvars_command_line(script, comspec), expected)
        self.assertNotIn('\\"', expected)

    def test_tools_error_is_kept_in_build_output(self):
        with tempfile.TemporaryDirectory() as directory:
            script = Path(directory) / 'vcvars64.bat'
            script.write_text('@echo off')
            error = build.subprocess.CalledProcessError(1, 'cmd.exe', output='SDK initialization diagnostic')
            with patch.object(build.subprocess, 'check_output', side_effect=error):
                with self.assertRaisesRegex(RuntimeError, 'SDK initialization diagnostic'):
                    build.vs_environment(script)

    def test_failure_does_not_print_captured_environment_values(self):
        with tempfile.TemporaryDirectory() as directory:
            script = Path(directory) / 'vcvars64.bat'
            script.write_text('@echo off')
            output = 'Tools stopped\n'+build.ENV_MARKER+'\nPRIVATE_TOKEN=do-not-print\n'
            error = build.subprocess.CalledProcessError(1, 'cmd.exe', output=output)
            with patch.object(build.subprocess, 'check_output', side_effect=error):
                with self.assertRaises(RuntimeError) as result:
                    build.vs_environment(script)
            self.assertIn('Tools stopped', str(result.exception))
            self.assertNotIn('PRIVATE_TOKEN', str(result.exception))
            self.assertNotIn('do-not-print', str(result.exception))

    def test_missing_environment_marker_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            script = Path(directory) / 'vcvars64.bat'
            script.write_text('@echo off')
            with patch.object(build.subprocess, 'check_output', return_value='unexpected output'):
                with self.assertRaisesRegex(RuntimeError, 'environment marker'):
                    build.vs_environment(script)

    def test_preview_and_build_tools_are_not_filtered_out(self):
        with tempfile.TemporaryDirectory(prefix='VS installations ') as directory:
            root = Path(directory)
            locator = root / 'Microsoft Visual Studio/Installer/vswhere.exe'
            locator.parent.mkdir(parents=True)
            locator.write_text('locator placeholder')
            install = root / 'Build Tools Preview'
            script = install / 'VC/Auxiliary/Build/vcvars64.bat'
            script.parent.mkdir(parents=True)
            script.write_text('tools placeholder')
            response = json.dumps([{'installationPath': str(install), 'installationVersion': '17.14.1', 'isPrerelease': True}])
            with patch.object(build.subprocess, 'check_output', return_value=response) as call:
                result = build.find_vcvars(env={'ProgramFiles(x86)': str(root)})
            self.assertEqual(result, script)
            args = call.call_args.args[0]
            self.assertIn('-prerelease', args)
            self.assertIn('*', args)
            self.assertNotIn('-version', args)
            self.assertNotIn('-requires', args)

    def test_missing_compiler_is_reported_without_claiming_premake_supplies_it(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(RuntimeError, 'Premake does not include a compiler'):
                build.find_vcvars(env={'ProgramFiles(x86)': directory, 'ProgramFiles': directory})

    def test_x86_environment_requires_x64_initialization(self):
        with patch.object(build.shutil, 'which', return_value='cl.exe'):
            self.assertFalse(build.configured_msvc({'PATH': 'bin', 'LIB': 'lib', 'INCLUDE': 'include', 'VSCMD_ARG_TGT_ARCH': 'x86'}))


if __name__ == '__main__':
    unittest.main()
