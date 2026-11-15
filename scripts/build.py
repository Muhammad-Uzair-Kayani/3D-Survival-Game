"""Build the built-in game module with Godot, then package the Windows GUI app."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from pack import pack,embed
ROOT=Path(__file__).resolve().parents[1]
LOCK=json.loads((ROOT/'godot.lock.json').read_text())

def normalized_environment(env):
    # Windows treats environment names case-insensitively. cmd's `set` often
    # returns `Path`, whereas Python's os.environ copy contains `PATH`.
    return {key.upper(): value for key, value in env.items()}


def configured_msvc(env):
    """Accept a working x64 Developer/IDE environment without probing vswhere."""
    env = normalized_environment(env)
    arch = env.get('VSCMD_ARG_TGT_ARCH', '').lower()
    return (arch in ('', 'x64', 'amd64') and bool(env.get('INCLUDE'))
            and bool(env.get('LIB')) and shutil.which('cl.exe', path=env.get('PATH', '')) is not None)


def find_vcvars(explicit=None, env=None):
    env = normalized_environment(os.environ if env is None else env)
    if explicit:
        path = Path(explicit)
        if not path.is_file():
            raise RuntimeError('Visual Studio did not supply a valid x64 tools script: ' + str(path)
                               + '. Check the installed C++ desktop workload/toolset.')
        return path
    # Visual Studio Makefile projects pass this path explicitly; Developer
    # Command Prompts and other IDEs can supply these existing environment roots.
    for key, suffix in [('VCINSTALLDIR', 'Auxiliary/Build/vcvars64.bat'),
                        ('VSINSTALLDIR', 'VC/Auxiliary/Build/vcvars64.bat')]:
        value = env.get(key)
        if value:
            candidate = Path(value) / suffix
            if candidate.is_file():
                return candidate
    cache = ROOT / '.tools/msvc-tools.json'
    try:
        selected = Path(json.loads(cache.read_text(encoding='utf-8'))['vcvars'])
        if selected.is_file():
            return selected
    except (OSError, ValueError, KeyError, TypeError):
        pass
    locator = Path(env.get('PROGRAMFILES(X86)', 'C:/Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
    if locator.is_file():
        # Preview installs and standalone Build Tools are valid. The old strict
        # VS2022/component filter hid installations that could contain the tools.
        output = subprocess.check_output(
            [str(locator), '-all', '-prerelease', '-products', '*', '-format', 'json'],
            text=True, encoding='utf-8-sig', env=dict(env))
        candidates = json.loads(output)
        candidates.sort(key=lambda item: tuple(int(part) if part.isdigit() else 0
                                              for part in item.get('installationVersion', '0').split('.')),
                        reverse=True)
        for install in candidates:
            candidate = Path(install.get('installationPath', '')) / 'VC/Auxiliary/Build/vcvars64.bat'
            if candidate.is_file():
                return candidate
    # Unregistered/portable installations may be absent from vswhere, while
    # the IDE can still use them. Check existing standard installation paths.
    for key, default in [('PROGRAMFILES', 'C:/Program Files'),
                         ('PROGRAMFILES(X86)', 'C:/Program Files (x86)')]:
        base = Path(env.get(key, default)) / 'Microsoft Visual Studio'
        for year in ('2022', '18', '2026'):
            for edition in ('Community', 'Professional', 'Enterprise', 'BuildTools', 'Preview'):
                candidate = base / year / edition / 'VC/Auxiliary/Build/vcvars64.bat'
                if candidate.is_file():
                    return candidate
    raise RuntimeError('No Visual Studio x64 C++ tools were found. Open the Premake-generated solution '
                       'in your working Visual Studio installation, or install Desktop development '
                       'with C++ and the Windows SDK. Premake does not include a compiler.')


ENV_MARKER = '__LASTLIGHT_MSVC_ENV_BEGIN__'


def vcvars_command_line(script, comspec):
    # CreateProcess accepts a raw Windows command line. Passing /c's quoted
    # payload as a list item invokes Python's C-runtime escaping (\\\"),
    # which cmd.exe does not understand as quote escaping.
    script, comspec = str(script), str(comspec)
    if any(character in value for value in (script, comspec) for character in ('"', '\r', '\n')):
        raise RuntimeError('Invalid character in compiler-tools command path')
    return f'"{comspec}" /d /s /v:off /c "call "{script}" && echo {ENV_MARKER} && set"'


def vs_environment(vcvars=None, from_ide=False):
    original = normalized_environment(os.environ)
    if from_ide:
        if not configured_msvc(original):
            raise RuntimeError('The Visual Studio batch initializer did not provide x64 cl.exe, INCLUDE and LIB. '
                               'See the vcvars diagnostics above.')
        return original
    if not vcvars and configured_msvc(original):
        return original
    script = find_vcvars(vcvars, original)
    comspec = original.get('COMSPEC', 'cmd.exe')
    command_line = vcvars_command_line(script, comspec)
    try:
        output = subprocess.check_output(
            command_line, executable=comspec, shell=False, text=True,
            stderr=subprocess.STDOUT, env=original)
    except subprocess.CalledProcessError as error:
        details = (error.output or '').partition(ENV_MARKER)[0].strip()
        raise RuntimeError('Visual Studio tools initialization failed:\n' +
                           (details or f'cmd.exe exited with code {error.returncode}')) from error
    lines = output.splitlines()
    try:
        start = next(index for index, line in enumerate(lines) if line.strip() == ENV_MARKER) + 1
    except StopIteration:
        raise RuntimeError('Visual Studio tools did not return the expected environment marker') from None
    env = original.copy()
    for line in lines[start:]:
        if '=' in line:
            key, value = line.split('=', 1)
            if key:
                env[key.upper()] = value
    if not configured_msvc(env):
        raise RuntimeError('The selected Visual Studio installation did not initialize cl.exe, '
                           'INCLUDE and LIB for x64. Modify that installation to add its C++ tools/SDK.')
    return env



def preflight_msvc(env):
    """Compile/link/run x64 C++ + Windows SDK and compile a resource before Godot."""
    output = ROOT / 'reports'
    output.mkdir(exist_ok=True)
    log_path = output / 'msvc-preflight.log'
    tools = {name: shutil.which(name, path=env.get('PATH', ''))
             for name in ('cl.exe', 'link.exe', 'lib.exe', 'rc.exe')}
    missing = [name for name, location in tools.items() if not location]
    if missing:
        raise RuntimeError('Missing C++/Windows SDK tools: ' + ', '.join(missing))
    with tempfile.TemporaryDirectory(prefix='LastLight Toolchain ') as directory:
        directory = Path(directory)
        source = directory / 'probe.cpp'
        source.write_text('#include <windows.h>\n#include <string>\n'
                          'static_assert(sizeof(void*) == 8, "x64 required");\n'
                          'static_assert(_MSC_VER >= 1930, "VS2022 or newer required");\n'
                          'int main(){std::string s="sdk"; return s.size()==3 && GetCurrentProcessId()!=0 ? 0:1;}\n')
        resource = directory / 'probe.rc'
        resource.write_text('#include <windows.h>\n1 RCDATA { 1, 2, 3 }\n')
        commands = [
            [tools['rc.exe'], '/nologo', '/fo', str(directory / 'probe.res'), str(resource)],
            [tools['cl.exe'], '/nologo', '/EHsc', '/std:c++17', '/MT', str(source),
             str(directory / 'probe.res'), '/Fe' + str(directory / 'probe.exe')],
            [str(directory / 'probe.exe')],
        ]
        with log_path.open('w', encoding='utf-8') as log:
            for command in commands:
                result = subprocess.run(command, cwd=directory, env=env, stdout=log, stderr=subprocess.STDOUT)
                if result.returncode:
                    log.flush()
                    raise RuntimeError('MSVC/Windows SDK preflight failed:\n' +
                                       log_path.read_text(encoding='utf-8', errors='replace'))
    (output / 'msvc-toolchain.json').write_text(json.dumps({
        'tools': tools, 'toolset': env.get('VCTOOLSVERSION', ''),
        'sdk': env.get('WINDOWSSDKVERSION', ''), 'architecture': 'x64',
        'compile_link_run_resource_probe': 'passed'}, indent=2), encoding='utf-8')
    print('MSVC x64 + C++ standard library + Windows SDK + resource compiler: passed', flush=True)


def run_build(command, engine, env):
    """Keep the complete build output while streaming it into Visual Studio."""
    output = ROOT / 'reports'
    output.mkdir(exist_ok=True)
    log_path = output / 'godot-build.log'
    child_env = env.copy()
    child_env['PYTHONUNBUFFERED'] = '1'
    child_env['PYTHONUTF8'] = '1'
    print('Build log: ' + str(log_path), flush=True)
    with log_path.open('w', encoding='utf-8') as log:
        with subprocess.Popen(command, cwd=engine, env=child_env, stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT, text=True, encoding='utf-8', errors='replace') as process:
            for line in process.stdout:
                log.write(line)
                log.flush()
                sys.stdout.write(line)
                sys.stdout.flush()
            result = process.wait()
    if result:
        raise RuntimeError(f'Godot build exited with code {result}. Full compiler diagnostics: {log_path}')

def options(platform,target):
    flags=[f'platform={platform}',f'target={target}','arch=x86_64',f'custom_modules={ROOT / "modules"}',
           'modules_enabled_by_default=no','module_last_light_enabled=yes',
           'vulkan=no','opengl3=yes','lto=none','debug_symbols=no','optimize=size']
    for name in ('godot_physics_3d','navigation_3d','freetype','text_server_fb','svg','gltf','fbx','meshoptimizer','gdscript','regex','zip','jpg','webp'):
        flags.append(f'module_{name}_enabled=yes')
    if platform=='windows':flags+=['windows_subsystem=gui','use_static_cpp=yes','d3d12=no','use_mingw=no','use_llvm=no']
    return flags

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--configure-only',action='store_true',help='Validate toolchain and Godot build graph without compiling the engine');p.add_argument('--from-ide',action='store_true',help=argparse.SUPPRESS);p.add_argument('--vcvars',type=Path,help='x64 tools script supplied by the generated Visual Studio solution');p.add_argument('--clean',action='store_true');p.add_argument('--rebuild',action='store_true');p.add_argument('--jobs',type=int,default=1);p.add_argument('--debug',action='store_true');p.add_argument('--editor',action='store_true');p.add_argument('--embed',action='store_true');p.add_argument('--headless-linux',action='store_true');p.add_argument('--portable-python',type=Path,help='An extracted official Windows embeddable Python directory')
    a=p.parse_args()
    if not 1<=a.jobs<=8:p.error('--jobs must be 1..8; use 1 on an 8 GB machine')
    engine=ROOT/'vendor/godot'
    actual=subprocess.check_output(['git','rev-parse','HEAD'],cwd=engine,text=True).strip()
    if actual!=LOCK['commit']:raise RuntimeError('Godot revision differs from godot.lock.json; run Setup.bat')
    python=ROOT/'.tools/venv'/('Scripts/python.exe' if os.name=='nt' else 'bin/python')
    if not python.exists():raise RuntimeError('Run Setup.bat first to install project-local SCons')
    platform='windows' if os.name=='nt' else 'linuxbsd';target='editor' if a.editor else 'template_debug' if a.debug else 'template_release'
    flags=options(platform,target)
    if a.headless_linux:flags+=['x11=no','wayland=no','dbus=no','speechd=no','fontconfig=no']
    env=vs_environment(a.vcvars,a.from_ide) if os.name=='nt' else os.environ.copy()
    if os.name == 'nt':
        preflight_msvc(env)
        if a.vcvars:
            try:
                (ROOT/'.tools/msvc-tools.json').write_text(json.dumps({'vcvars':str(a.vcvars.resolve())}),encoding='utf-8')
            except OSError:
                pass  # Remembering the selection is optional; the tools were validated.
    command=[str(python),str(ROOT/'scripts/scons_entry.py'),*flags,f'-j{a.jobs}']
    if a.configure_only:
        output=ROOT/'reports';output.mkdir(exist_ok=True)
        with (output/'godot-configure.log').open('w',encoding='utf-8') as log:
            result=subprocess.run(command+['--dry-run','--quiet'],cwd=engine,env=env,stdout=log,stderr=subprocess.STDOUT)
        if result.returncode:
            details=(output/'godot-configure.log').read_text(encoding='utf-8',errors='replace')
            raise RuntimeError('Godot configuration failed:\n'+details[-12000:])
        print('Godot build graph validated; see reports/godot-configure.log');return
    if a.clean or a.rebuild:
        subprocess.run(command+['--clean'],cwd=engine,env=env,check=True)
        if a.clean:
            for name in ('LastLight.exe','LastLight','LastLight.pck'):
                (ROOT/'dist'/name).unlink(missing_ok=True)
            print('Build outputs cleaned.');return
    run_build(command,engine,env)
    suffix='.exe' if os.name=='nt' else ''
    binary=engine/'bin'/f'godot.{platform}.{target}.x86_64{suffix}'
    if not binary.exists():raise RuntimeError('Expected Godot binary not produced: '+str(binary))
    if a.editor:
        subprocess.run([str(binary),'--headless','--path',str(ROOT/'game'),'--editor','--import'],check=True)
        print('Custom editor and asset import complete:',binary);return
    output=ROOT/'dist';output.mkdir(exist_ok=True)
    destination=output/('LastLight.exe' if os.name=='nt' else 'LastLight')
    cache=ROOT/'game/.godot/global_script_class_cache.cfg'
    if not cache.exists():cache.parent.mkdir(exist_ok=True);cache.write_text('list=[]\n')
    pck=output/'LastLight.pck';count=pack(ROOT/'game',pck)
    if a.embed:
        embed(binary,pck,destination);pck.unlink()
    else:shutil.copy2(binary,destination)
    shutil.copytree(ROOT/'python',output/'python',ignore=shutil.ignore_patterns('__pycache__'),dirs_exist_ok=True)
    if a.portable_python:
        if not (a.portable_python/'python.exe').is_file():raise RuntimeError('Expected extracted official embeddable Python directory')
        shutil.copytree(a.portable_python,output/'python-runtime',dirs_exist_ok=True)
    for name in ('LICENSE.txt','COPYRIGHT.txt','AUTHORS.md'):
        if (engine/name).exists():shutil.copy2(engine/name,output/('Godot-'+name))
    print(f'Ready: {destination}; {count} PCK files. Python remains a separate optional process.')
if __name__=='__main__':
    try:
        main()
    except (RuntimeError,OSError,subprocess.CalledProcessError) as error:
        print('Build could not finish: '+str(error),file=sys.stderr)
        raise SystemExit(1)
