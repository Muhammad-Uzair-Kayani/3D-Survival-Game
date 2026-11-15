"""CI acceptance: build the delivered Godot/game sources on native Windows.

Run manually with Python 3.12 on a Windows machine with VS2022 and Git.
Outputs are retained in reports/. This is intentionally separate from unit tests.
"""
import argparse
import json
import os
import shutil
import urllib.request
import zipfile
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
import build


def execute(name, command, env=None, timeout=3600):
    reports = ROOT / 'reports'
    reports.mkdir(exist_ok=True)
    log = reports / (name + '.log')
    start = time.monotonic()
    print(name + ': starting', flush=True)
    with log.open('w', encoding='utf-8') as stream:
        result = subprocess.run(command, cwd=ROOT, env=env, stdout=stream,
                                stderr=subprocess.STDOUT, timeout=timeout)
    if result.returncode:
        raise RuntimeError(name + ' failed:\n' + log.read_text(encoding='utf-8', errors='replace')[-24000:])
    print(f'{name}: passed in {time.monotonic()-start:.1f}s', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jobs', type=int, choices=range(1, 5), default=1, help='Use 1 on an 8 GB machine; CI uses 4')
    args = parser.parse_args()
    if os.name != 'nt':
        raise RuntimeError('Native Windows acceptance requires Windows')
    execute('setup', [sys.executable, str(ROOT / 'scripts/setup.py')])
    vcvars = build.find_vcvars()
    # Use the same batch wrapper and vcvars argument the Premake project runs.
    comspec = os.environ.get('COMSPEC', 'cmd.exe')
    wrapper = ROOT / 'BuildFromVS.bat'
    prefix = f'"{comspec}" /d /s /v:off /c "call "{wrapper}" --vcvars "{vcvars}" --jobs {args.jobs}'
    execute('godot-configure', prefix + ' --configure-only"', timeout=600)
    execute('godot-full-build', prefix + '"', timeout=5400)
    execute('windows-regression-tests', [sys.executable, '-m', 'unittest', 'discover', '-s', 'tests', '-v'], timeout=600)
    # Exercise the actual generated Visual Studio project incrementally, after
    # the four-job CI engine build. The user's project retains one compile job.
    premake_zip = ROOT / '.tools/premake.zip'
    urllib.request.urlretrieve('https://github.com/premake/premake-core/releases/download/v5.0.0-beta2/premake-5.0.0-beta2-windows.zip', premake_zip)
    with zipfile.ZipFile(premake_zip) as archive:
        archive.extractall(ROOT / '.tools/premake')
    execute('premake-generate', [str(ROOT / '.tools/premake/premake5.exe'), 'vs2022'])
    env = build.vs_environment(vcvars)
    msbuild = Path(env['VSINSTALLDIR']) / 'MSBuild/Current/Bin/MSBuild.exe'
    execute('premake-msbuild', [str(msbuild), str(ROOT / 'build/premake/vs2022/LastLightGodot.sln'),
                               '/p:Configuration=Release', '/p:Platform=x64', '/m:1', '/nologo'], env=env)

    executable = ROOT / 'dist/LastLight.exe'
    execute('native-gameplay', [sys.executable, str(ROOT / 'scripts/test_engine.py'),
                               '--engine', str(executable), '--autoplay'], timeout=900)
    execute('neural-training', [sys.executable, str(ROOT / 'python/train.py'), 'train',
                                '--engine', str(executable), '--population', '2', '--generations', '1',
                                '--world','lab','--bots','2','--output', str(ROOT / 'reports/training-smoke')], timeout=900)
    execute('neural-evaluation', [sys.executable, str(ROOT / 'python/train.py'), 'evaluate',
                                  '--engine', str(executable), '--model', str(ROOT / 'reports/training-smoke/champion.json'),
                                  '--world','lab','--bots','2','--output', str(ROOT / 'reports/evaluation-smoke')], timeout=900)
    # Launch from dist without --path: validates the shipped separate PCK.
    report = ROOT / 'reports/packaged.json'
    execute('packaged-pck', [str(executable), '--headless', '--fixed-fps', '60',
                            '--', '--smoke', '--case', 'physics', '--frames', '360',
                            '--report', str(report)], timeout=120)
    if not json.loads(report.read_text()).get('test_passed'):
        raise RuntimeError('Packaged executable did not pass gameplay smoke test')
    packaged_python = ROOT / 'reports/packaged-python.json'
    execute('packaged-python', [str(executable), '--headless', '--fixed-fps', '60', '--',
                                '--smoke', '--case', 'python', '--python', sys.executable,
                                '--frames', '120', '--report', str(packaged_python)], timeout=120)
    python_result = json.loads(packaged_python.read_text())
    if not python_result.get('test_passed') or python_result['python_failures']:
        raise RuntimeError('Packaged Python service failed')
    from pack import embed
    embedded = ROOT / 'dist/LastLight-Embedded.exe'
    embed(executable, ROOT / 'dist/LastLight.pck', embedded)
    embedded_report = ROOT / 'reports/embedded.json'
    execute('embedded-pck', [str(embedded), '--headless', '--fixed-fps', '60', '--', '--smoke',
                             '--case', 'physics', '--frames', '360', '--report', str(embedded_report)], timeout=120)
    if not json.loads(embedded_report.read_text()).get('test_passed'):
        raise RuntimeError('Embedded PCK smoke test failed')
    # Verify Windows PE subsystem and the static CRT at link level.
    env = build.vs_environment(vcvars)
    # Windows CreateProcess searches the parent's PATH, not the replacement
    # child environment's PATH. Resolve developer tools before spawning them.
    dumpbin = shutil.which('dumpbin.exe', path=env['PATH'])
    if not dumpbin:
        raise RuntimeError('MSVC dumpbin.exe is required for PE acceptance checks')
    execute('pe-headers', [dumpbin, '/headers', str(executable)], env=env)
    execute('pe-imports', [dumpbin, '/dependents', str(executable)], env=env)
    headers = (ROOT / 'reports/pe-headers.log').read_text(errors='replace')
    imports = (ROOT / 'reports/pe-imports.log').read_text(errors='replace').lower()
    if 'Windows GUI' not in headers:
        raise RuntimeError('Expected Windows GUI PE subsystem')
    if any(name in imports for name in ('vcruntime140', 'msvcp140', 'ucrtbase.dll')):
        raise RuntimeError('Unexpected dynamically linked MSVC C++ runtime')
    print('Full native Windows source build, Arctic and regression gameplay checks, packaging, GUI subsystem and static CRT passed.', flush=True)


if __name__ == '__main__':
    main()
