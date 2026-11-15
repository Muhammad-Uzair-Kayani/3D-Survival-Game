"""Validate the delivered binary and bundled Python, without build tools."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parent


def main():
    if os.name != 'nt':
        raise RuntimeError('This package is built for 64-bit Windows')
    for line in (ROOT / 'SHA256SUMS.txt').read_text(encoding='utf-8').splitlines():
        expected, relative = line.split('  ', 1)
        if hashlib.sha256((ROOT / relative).read_bytes()).hexdigest() != expected:
            raise RuntimeError('Missing or damaged package file: ' + relative)
    print('Package integrity passed.', flush=True)
    output = ROOT / 'diagnostics'
    output.mkdir(exist_ok=True)
    env = os.environ.copy()
    env['APPDATA'] = str(output / 'app-data')
    env['LOCALAPPDATA'] = str(output / 'local-data')
    for case in ('physics', 'combat', 'gate', 'gate_save', 'gate_negative', 'save', 'assets', 'social', 'melee', 'lifecycle', 'python', 'fallback', 'python_exit', 'python_fault_malformed', 'python_fault_stale', 'python_fault_timeout', 'python_fault_duplicate', 'peaceful', 'solo', 'autoplay', 'arctic_crossfire', 'arctic_equipment', 'arctic_social', 'arctic_interactions', 'arctic_animation', 'profiles', 'arctic', 'arctic_hostile','compound_animation','compound_security','compound_routes','compound_inventory','compound_engagement','compound_nav','compound_policy','compound_save','compound_learning','compound_orders','compound_beacon','compound','compound_hostile'):
        report = output / (case + '.json')
        report.unlink(missing_ok=True)
        command = [str(ROOT / 'LastLight.exe'), '--headless', '--fixed-fps', '60', '--max-fps', '0', '--',
                   '--smoke', '--case', case, '--frames', '30000' if case in ('arctic','compound','compound_hostile') else '900' if case=='compound_engagement' else '3600' if case=='compound_nav' else '18000' if case in ('peaceful', 'solo', 'autoplay', 'arctic_hostile') else '500' if case in ('arctic_animation','arctic_interactions') else '360',
                   '--bots', '1' if case == 'combat' else '2' if case == 'autoplay' else '5' if case in ('arctic_hostile','compound_hostile') else '0',
                   '--report', str(report)]
        if case.startswith('python_fault_'):command+=['--autoplay']
        if case in ('python', 'python_exit') or case.startswith('python_fault_'):
            command += ['--python', str(ROOT / 'python-runtime/python.exe')]
        if case == 'fallback':
            command += ['--python', str(output / 'missing-python.exe')]
        with (output / (case + '.log')).open('w', encoding='utf-8') as log:
            result = subprocess.run(command, cwd=ROOT, env=env, stdout=log,
                                    stderr=subprocess.STDOUT, timeout=300 if case in ("compound","compound_hostile") else 120)
        if result.returncode or not report.is_file():
            raise RuntimeError(case + ' did not finish successfully; inspect diagnostics/' + case + '.log')
        data = json.loads(report.read_text(encoding='utf-8'))
        if not data.get('test_passed') or data.get('navigation_polygons', 0) < 1:
            raise RuntimeError(case + ' reported a gameplay failure')
        if case == 'python' and data['python_failures']:
            raise RuntimeError('Bundled Python failed to provide the policy service')
        if (case in ('fallback', 'python_exit') or case.startswith('python_fault_')) and data['python_failures'] < 1:
            raise RuntimeError('Expected Python fallback was not exercised')
        if case in ('peaceful', 'solo', 'autoplay','arctic','compound') and data['phase'] != 'Victory':
            raise RuntimeError(case + ' did not complete the mission')
        if case in ('peaceful','arctic','compound') and data['arrived']!=5:raise RuntimeError('Peaceful survivors did not all arrive')
        if case=='assets' and (not data.get('skeletal_actors') or not data.get('environment_art')):raise RuntimeError('Integrated art/rig check failed')
        if case=='arctic_hostile' and (data['shots_hit']<1 or sum(actor['travelled']>5 for actor in data['actors'] if actor['id']>=10)<3):raise RuntimeError('Arctic distractors did not interact')
        print(case + ': passed', flush=True)
    print('All 41 portable Windows game checks passed.', flush=True)


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        print('Verification failed: ' + str(error), file=sys.stderr)
        raise SystemExit(1)
