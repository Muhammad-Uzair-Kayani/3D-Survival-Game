"""Package a built Windows game with an official embeddable Python runtime."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import tempfile
import zipfile
ROOT=Path(__file__).resolve().parents[1]

def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def package(dist,runtime,destination,build_run=''):
    dist=Path(dist);runtime=Path(runtime);destination=Path(destination).resolve()
    for path in (dist/'LastLight.exe',dist/'LastLight.pck',runtime/'python.exe',runtime/'python312.dll'):
        if not path.is_file():raise ValueError('Required package file missing: '+str(path))
    destination.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='last-light-package-') as directory:
        output=Path(directory)/'LastLight-Windows-x64';output.mkdir()
        for name in ('LastLight.exe','LastLight.pck','Godot-LICENSE.txt','Godot-COPYRIGHT.txt','Godot-AUTHORS.md'):shutil.copy2(dist/name,output/name)
        shutil.copytree(runtime,output/'python-runtime',ignore=shutil.ignore_patterns('__pycache__','*.pyc'))
        shutil.copytree(ROOT/'distribution',output,dirs_exist_ok=True)
        (output/'python').mkdir()
        for name in ('service.py','neural.py'):shutil.copy2(ROOT/'python'/name,output/'python'/name)
        shutil.copy2(ROOT/'game/content/ASSET-CREDITS.txt',output/'ASSET-CREDITS.txt')
        info={'name':'Last Light Windows x64 survival MVP','godot':json.loads((ROOT/'godot.lock.json').read_text()),'windows_build_and_tests':build_run,'executable_sha256':digest(output/'LastLight.exe'),'resources_sha256':digest(output/'LastLight.pck'),'policy_sha256':digest(ROOT/'game/content/champion.json'),'python_runtime':'Official CPython 3.12.10 Windows x64 embeddable package','python_runtime_source':'https://www.python.org/ftp/python/3.12.10/python-3.12.10-embed-amd64.zip','python_runtime_archive_sha256':'4acbed6dd1c744b0376e3b1cf57ce906f9dc9e95e68824584c8099a63025a3c3','portable_validation_workflow':'https://github.com/Muhammad-Uzair-Kayani/PlagC-Public/actions/workflows/lastlight-binary.yml'}
        (output/'BUILD-INFO.json').write_text(json.dumps(info,indent=2))
        manifest=[]
        for path in sorted(output.rglob('*')):
            if not path.is_file():continue
            if path.suffix.lower()=='.bat':path.write_bytes(path.read_bytes().replace(b'\r\n',b'\n').replace(b'\n',b'\r\n'))
            manifest.append(digest(path)+'  '+path.relative_to(output).as_posix())
        (output/'SHA256SUMS.txt').write_text('\n'.join(manifest)+'\n')
        temporary=destination.with_suffix('.writing')
        with zipfile.ZipFile(temporary,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as archive:
            for path in sorted(output.rglob('*')):
                if path.is_file():archive.write(path,'LastLight-Windows-x64/'+path.relative_to(output).as_posix())
        with zipfile.ZipFile(temporary) as archive:
            if archive.testzip():raise RuntimeError('ZIP integrity check failed')
        temporary.replace(destination)
    print(str(destination),destination.stat().st_size,'bytes; SHA256',digest(destination))

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--dist',type=Path,default=ROOT/'dist');parser.add_argument('--python-runtime',type=Path,required=True);parser.add_argument('--output',type=Path,required=True);parser.add_argument('--build-run',default='');args=parser.parse_args();package(args.dist,args.python_runtime,args.output,args.build_run)
