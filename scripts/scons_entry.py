"""Launch pinned SCons using the already initialized native MSVC toolchain.

Godot 4.5.1 chooses MinGW when SCons cannot enumerate VS installations, even
when vcvars has supplied working MSVC tools. This process-local adapter bypasses
installation discovery only after the caller has compiled/linked an SDK probe.
No files in the Godot submodule or installed SCons package are modified.
"""
import json
import os
from pathlib import Path
import shutil
import sys

ROOT = Path(__file__).resolve().parents[1]


def configure_inherited_msvc():
    import SCons.Defaults
    import SCons.Tool.MSCommon

    from build import configured_msvc, normalized_environment
    env = normalized_environment(os.environ)
    if not configured_msvc(env):
        raise RuntimeError('SCons requires an initialized x64 MSVC environment; use BuildGame.bat.')
    for name in ('cl.exe', 'link.exe', 'lib.exe', 'rc.exe'):
        if not shutil.which(name, path=env.get('PATH', '')):
            raise RuntimeError('Required MSVC/Windows SDK tool is missing: ' + name)

    # SCons names the VS2022 tool family 14.3, including 14.4x updates.
    # VS2026 uses family 14.5. The compiler path/SDK remain the inherited ones.
    toolset = env.get('VCTOOLSVERSION', '14.30').split('.')
    family = '14.5' if len(toolset) > 1 and int(toolset[1]) >= 50 else '14.3'
    if not any(argument.startswith('msvc_version=') for argument in sys.argv[1:]):
        sys.argv.append('msvc_version=' + family)

    # ConstructionEnvironment is copied by each SCons Environment. Preserve
    # INCLUDE/LIB/PATH and the selected SDK instead of re-running vcvars and
    # possibly choosing a different VS installation. MSVC_SETUP_RUN prevents
    # SCons's redundant registry/vswhere setup; all tools were preflighted.
    SCons.Defaults.ConstructionEnvironment.update(
        ENV=env, MSVC_USE_SCRIPT=False, MSVC_SETUP_RUN=True,
        MSVC_VERSION=family, MSVS_VERSION=family,
        # Godot also consults this optional SCons variable for its informational
        # VS version check. Missing vswhere is handled by Godot's OSError path.
        VSWHERE=str(Path(env.get('PROGRAMFILES(X86)', 'C:/Program Files (x86)')) /
                    'Microsoft Visual Studio/Installer/vswhere.exe'))
    original_exists = SCons.Tool.MSCommon.msvc_exists

    def inherited_msvc_exists(env=None, version=None):
        # Godot's get_tools() probes without an environment/version. Explicit
        # version probes retain SCons's normal semantics.
        if version is None:
            return True
        return original_exists(env, version)

    SCons.Tool.MSCommon.msvc_exists = inherited_msvc_exists
    print('LastLight: using inherited MSVC x64 and Windows SDK (installation auto-detection bypassed).', flush=True)


def main():
    import SCons
    expected = json.loads((ROOT / 'godot.lock.json').read_text())['scons']
    if SCons.__version__ != expected:
        raise RuntimeError(f'Expected SCons {expected}, found {SCons.__version__}; run Setup.bat.')
    if os.name == 'nt':
        configure_inherited_msvc()
    import SCons.Script
    SCons.Script.main()


if __name__ == '__main__':
    try:
        main()
    except RuntimeError as error:
        print('Build configuration failed: ' + str(error), file=sys.stderr)
        raise SystemExit(1)
