"""Register pinned Godot safely, with Windows long-path checkout support."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import venv

ROOT = Path(__file__).resolve().parents[1]
LOCK = json.loads((ROOT / 'godot.lock.json').read_text())
GIT = ['git', '-c', 'core.longpaths=true']


def git(args, cwd=None, *, capture=False, check=True, input_bytes=None):
    return subprocess.run(
        GIT + list(map(str, args)), cwd=ROOT if cwd is None else cwd, check=check,
        input=input_bytes, capture_output=capture,
    )


def fs_path(path):
    """Python file checks must also handle long paths when Windows policy is off."""
    value = os.path.abspath(path)
    if os.name == 'nt' and not value.startswith('\\\\?\\'):
        return '\\\\?\\UNC\\' + value[2:] if value.startswith('\\\\') else '\\\\?\\' + value
    return value


def missing_tracked_files(engine):
    entries = git(['ls-tree', '-r', '--name-only', '-z', 'HEAD'], engine, capture=True).stdout
    return [entry for entry in entries.split(b'\0')
            if entry and not os.path.lexists(fs_path(engine / os.fsdecode(entry)))]


def repair_missing_files(engine):
    # Git clone can abort before writing any index entries. Reconstruct that
    # empty index from HEAD without touching existing working-tree contents.
    indexed = git(['ls-files', '-z'], engine, capture=True).stdout
    if not indexed:
        git(['restore', '--source=HEAD', '--staged', '--', '.'], engine)
        print('Reconstructed the empty checkout index; existing file contents were preserved.')
    missing = missing_tracked_files(engine)
    if missing:
        # Restore only absent paths, never existing edited files. NUL-delimited
        # stdin avoids command-length limits, spaces and wildcard expansion.
        git(['--literal-pathspecs', 'restore', '--source=HEAD', '--staged', '--worktree',
             '--pathspec-from-file=-', '--pathspec-file-nul'], engine,
            input_bytes=b'\0'.join(missing) + b'\0')
    return len(missing)


def engine_setup(repair_missing=False):
    # Git expands Windows 8.3 names (RUNNER~1); compare canonical roots.
    root = ROOT.resolve()
    result = git(['rev-parse', '--show-toplevel'], capture=True, check=False)
    if result.returncode:
        git(['init'])
        repo = root
    else:
        repo = Path(os.fsdecode(result.stdout).strip()).resolve()
    git(['config', '--local', 'core.longpaths', 'true'], repo)
    engine = root / 'vendor/godot'
    relative = engine.relative_to(repo).as_posix()
    recorded = git(['ls-files', '--stage', '--', relative], repo, capture=True).stdout
    initialized = os.path.lexists(fs_path(engine / '.git'))

    if not recorded.startswith(b'160000 '):
        if recorded.strip():
            raise RuntimeError('Godot is tracked as ordinary files; refusing to overwrite it.')
        # -c is inherited by Git's submodule helper and clone subprocess. This
        # must be set BEFORE checkout, not only after cloning has completed.
        git(['submodule', 'add', '--depth', '1', '--force', '--', LOCK['repository'], relative], repo)
    elif not initialized:
        git(['submodule', 'update', '--init', '--', relative], repo)

    git(['config', '--local', 'core.longpaths', 'true'], engine)
    if repair_missing:
        count = repair_missing_files(engine)
        print(f'Restored {count} missing tracked Godot files; existing files were preserved.')

    dirty = git(['status', '--porcelain', '-z'], engine, capture=True).stdout
    if dirty.strip(b'\0 \r\n'):
        missing = missing_tracked_files(engine)
        if missing:
            raise RuntimeError(
                f'Godot has {len(missing)} missing tracked files, possibly from an interrupted checkout. '
                'Retry Setup.bat --repair-missing to restore only absent files. '
                'Existing local edits will not be overwritten.'
            )
        raise RuntimeError(
            'Godot has local changes. Setup preserved them and did not switch revisions. '
            'Review git -C vendor/godot status before continuing.'
        )

    have = git(['cat-file', '-e', LOCK['commit'] + '^{commit}'], engine, capture=True, check=False)
    if have.returncode:
        git(['fetch', '--depth', '1', 'origin', LOCK['commit']], engine)
    git(['checkout', '--detach', LOCK['commit']], engine)
    git(['add', '--', relative], repo)
    if os.name == 'nt' and len(str(ROOT)) > 70:
        print('Recommendation: use a short project path such as C:\\Dev\\LastLightGodot. '
              'Git long paths are enabled, but compiler/SDK tools may still have path limits.')
    print('Godot gitlink registered at:', relative)
    print('Commit project sources, the repository-root .gitmodules and staged gitlink before pushing.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--engine-only', action='store_true')
    parser.add_argument('--repair-missing', action='store_true',
                        help='Restore missing tracked Godot files only; preserve existing files and edits.')
    args = parser.parse_args()
    if sys.version_info < (3, 10):
        parser.error('Python 3.10+ required')
    try:
        engine_setup(args.repair_missing)
        if not args.engine_only:
            directory = ROOT / '.tools/venv'
            venv.EnvBuilder(with_pip=True).create(directory)
            python = directory / ('Scripts/python.exe' if os.name == 'nt' else 'bin/python')
            subprocess.run([str(python), '-m', 'pip', 'install', '-r', str(ROOT / 'requirements-build.txt')], check=True)
            print('Build tools ready. Run BuildGame.bat --jobs 1 on Windows.')
    except (RuntimeError, OSError, subprocess.CalledProcessError) as error:
        print(f'Setup could not finish: {error}', file=sys.stderr)
        print('No automatic hard reset or deletion of your engine checkout was performed.', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
