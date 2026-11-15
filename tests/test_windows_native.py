"""Native Windows regression checks; require cmd.exe and installed MSVC."""
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
spec = importlib.util.spec_from_file_location('lastlight_native_build', ROOT / 'scripts/build.py')
build = importlib.util.module_from_spec(spec)
spec.loader.exec_module(build)


@unittest.skipUnless(os.name == 'nt', 'requires native Windows cmd.exe/MSVC')
class WindowsNativeTests(unittest.TestCase):
    def test_real_cmd_echo_marker_and_space_path(self):
        with tempfile.TemporaryDirectory(prefix='Last Light Native ') as directory:
            script = Path(directory) / 'fake tools with spaces.bat'
            script.write_text('@echo off\nset "LASTLIGHT_NATIVE_PROBE=ready"\nexit /b 0\n')
            comspec = os.environ.get('COMSPEC', 'cmd.exe')
            output = subprocess.check_output(build.vcvars_command_line(script, comspec),
                                             executable=comspec, shell=False, text=True)
            marker = next(line for line in output.splitlines() if line.strip() == build.ENV_MARKER)
            self.assertEqual(marker.strip(), build.ENV_MARKER)
            self.assertIn('LASTLIGHT_NATIVE_PROBE=ready', output)

    def test_real_visual_studio_initialization_and_compilation(self):
        script = build.find_vcvars()
        env = build.vs_environment(script)
        self.assertTrue(build.configured_msvc(env))
        with tempfile.TemporaryDirectory(prefix='Last Light Cpp ') as directory:
            directory = Path(directory)
            source = directory / 'hello with spaces.cpp'
            executable = directory / 'hello with spaces.exe'
            source.write_text('#include <iostream>\nint main(){std::cout << "native compiler probe"; return 0;}\n')
            compiler = shutil.which('cl.exe', path=env['PATH'])
            compile_result = subprocess.run([compiler, '/nologo', '/EHsc', '/std:c++17',
                                             str(source), '/Fe' + str(executable)],
                                            cwd=directory, env=env, text=True, capture_output=True)
            self.assertEqual(compile_result.returncode, 0, compile_result.stdout + compile_result.stderr)
            output = subprocess.check_output([str(executable)], text=True, env=env)
            self.assertEqual(output, 'native compiler probe')

    def test_native_domain_actions_lifecycle_and_blends(self):
        env = build.vs_environment(build.find_vcvars())
        with tempfile.TemporaryDirectory(prefix='Last Light Domain ') as directory:
            directory = Path(directory)
            compiler = shutil.which('cl.exe', path=env['PATH'])
            executable = directory / 'domain.exe'
            result = subprocess.run([compiler, '/nologo', '/EHsc', '/std:c++17', '/MT',
                                     str(ROOT / 'tests/domain_tests.cpp'), '/Fe' + str(executable)],
                                    cwd=directory, env=env, text=True, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            output = subprocess.check_output([str(executable)], env=env, text=True)
            self.assertIn('Domain action, inventory, timing, social, lifecycle and blend checks passed', output)

    def test_engine_independent_world_contract(self):
        env=build.vs_environment(build.find_vcvars())
        with tempfile.TemporaryDirectory(prefix='Last Light World Contract ') as directory:
            executable=Path(directory)/'contract.exe'
            compiler=shutil.which('cl.exe',path=env['PATH'])
            result=subprocess.run([compiler,'/nologo','/EHsc','/std:c++17','/MT',str(ROOT/'tests/world_contract_tests.cpp'),'/Fe'+str(executable)],cwd=directory,env=env,text=True,capture_output=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            self.assertIn('Fake-world',subprocess.check_output([str(executable)],env=env,text=True))

    def test_native_strategic_planner(self):
        env=build.vs_environment(build.find_vcvars())
        with tempfile.TemporaryDirectory(prefix='Last Light Strategy ') as directory:
            executable=Path(directory)/'strategy.exe'
            compiler=shutil.which('cl.exe',path=env['PATH'])
            result=subprocess.run([compiler,'/nologo','/EHsc','/std:c++17','/MT',str(ROOT/'tests/strategy_tests.cpp'),'/Fe'+str(executable)],cwd=directory,env=env,text=True,capture_output=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            self.assertIn('Strategic route selection',subprocess.check_output([str(executable)],env=env,text=True))

    def test_native_tactical_policy_and_resource_budgets(self):
        env=build.vs_environment(build.find_vcvars())
        with tempfile.TemporaryDirectory(prefix='Last Light Tactical ') as directory:
            for name in ('hard_policy_tests.cpp','planner_budget_tests.cpp'):
                with self.subTest(scenario=name):
                    executable=Path(directory)/(Path(name).stem+'.exe')
                    compiler=shutil.which('cl.exe',path=env['PATH'])
                    built=subprocess.run([compiler,'/nologo','/EHsc','/std:c++17','/MT',str(ROOT/'tests'/name),'/Fe'+str(executable)],cwd=directory,env=env,text=True,capture_output=True)
                    self.assertEqual(built.returncode,0,built.stdout+built.stderr)
                    run=subprocess.run([str(executable)],env=env,text=True,capture_output=True)
                    self.assertEqual(run.returncode,0,run.stdout+run.stderr)

    def test_registry_free_scons_compiles_cpp_and_resource(self):
        env = build.vs_environment(build.find_vcvars())
        python = ROOT / '.tools/venv/Scripts/python.exe'
        self.assertTrue(python.is_file(), 'Run Setup.bat to install the pinned build tools')
        with tempfile.TemporaryDirectory(prefix='Last Light SCons ') as directory:
            directory = Path(directory)
            (directory / 'probe.cpp').write_text(
                '#include <windows.h>\n#include <iostream>\n'
                'static_assert(sizeof(void*)==8, "x64");\n'
                'int main(){std::cout<<"registry-free MSVC"; return GetCurrentProcessId()==0;}\n')
            (directory / 'probe.rc').write_text('#include <windows.h>\n1 RCDATA {1,2,3}\n')
            (directory / 'SConstruct').write_text(
                "env=Environment(tools=['msvc','mslink','mslib'])\n"
                "env.Append(CCFLAGS=['/EHsc','/std:c++17','/MT'])\n"
                "env.Program('probe',['probe.cpp',env.RES('probe.rc')])\n")
            # Reproduce the user's failure: a working vcvars environment, but
            # SCons installation enumeration reports no Visual Studio tools.
            bootstrap = (
                "import sys;sys.path.insert(0," + repr(str(ROOT / 'scripts')) + ");"
                "import SCons.Tool.MSCommon as c;import SCons.Tool.MSCommon.vc as v;"
                "c.msvc_exists=lambda *a,**k:False;v.get_installed_vcs=lambda *a,**k:[];"
                "from scons_entry import main;main()")
            result = subprocess.run([str(python), '-c', bootstrap, '-Q'],
                                    cwd=directory, env=env, text=True, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(subprocess.check_output([str(directory / 'probe.exe')], text=True, env=env),
                             'registry-free MSVC')

    def test_real_batch_wrapper_inherits_environment_and_forwards_arguments(self):
        script = build.find_vcvars()
        with tempfile.TemporaryDirectory(prefix='Last Light Batch ') as directory:
            directory = Path(directory)
            shutil.copy2(ROOT / 'BuildFromVS.bat', directory / 'BuildFromVS.bat')
            report = directory / 'probe.json'
            build_location = str(ROOT / 'scripts/build.py')
            probe = f'''import importlib.util,json,os,sys
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0, {str(ROOT / 'scripts')!r})
spec=importlib.util.spec_from_file_location('batch_probe_build',{build_location!r})
build=importlib.util.module_from_spec(spec);spec.loader.exec_module(build)
assert sys.argv[1]=='--from-ide',sys.argv
with patch.object(build.subprocess,'check_output',side_effect=AssertionError('IDE must not capture a marker')):
    env=build.vs_environment(sys.argv[3],from_ide=True)
assert build.configured_msvc(env)
Path({str(report)!r}).write_text(json.dumps(dict(arguments=sys.argv[1:],compiler_available=True)))
'''
            (directory / 'probe.py').write_text(probe)
            (directory / 'BuildGame.bat').write_text(
                '@echo off\n"' + sys.executable + '" "%~dp0probe.py" %*\nexit /b %errorlevel%\n')
            args = subprocess.list2cmdline(['--vcvars', str(script), '--jobs', '1', '--debug'])
            comspec = os.environ.get('COMSPEC', 'cmd.exe')
            command = f'"{comspec}" /d /s /v:off /c "call "{directory / "BuildFromVS.bat"}" {args}"'
            result = subprocess.run(command, executable=comspec, shell=False, cwd=directory,
                                    text=True, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            payload = json.loads(report.read_text())
            self.assertEqual(payload['arguments'], ['--from-ide', '--vcvars', str(script), '--jobs', '1', '--debug'])
            self.assertTrue(payload['compiler_available'])


if __name__ == '__main__':
    unittest.main()
