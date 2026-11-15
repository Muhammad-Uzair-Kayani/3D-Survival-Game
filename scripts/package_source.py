"""Package only client source/configuration and the engine submodule declaration."""
import argparse
import hashlib
from pathlib import Path
import zipfile
ROOT=Path(__file__).resolve().parents[1]
EXCLUDE={'.git','.tools','vendor','dist','build','reports','__pycache__','incoming','imported','.godot'}
def package(destination):
    from pack import pack
    if (ROOT/'game/assets').is_dir():pack(ROOT/'game/assets',ROOT/'game/game-content.pck',prefix='assets/')
    destination=Path(destination).resolve();destination.parent.mkdir(parents=True,exist_ok=True);manifest=[]
    with zipfile.ZipFile(destination,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for file in sorted(ROOT.rglob('*')):
            if not file.is_file():continue
            relative=file.relative_to(ROOT)
            if any(part in EXCLUDE for part in relative.parts):continue
            if relative.parts[:2]==('game','assets'):continue
            if file.suffix.lower() in ('.exe','.dll','.so','.o','.a','.obj','.pdb','.pyc','.zip','.log'):continue
            data=file.read_bytes()
            if file.suffix.lower()=='.bat':data=data.replace(b'\r\n',b'\n').replace(b'\n',b'\r\n')
            manifest.append(hashlib.sha256(data).hexdigest()+'  '+relative.as_posix())
            z.writestr('LastLightGodot/'+relative.as_posix(),data)
        z.writestr('LastLightGodot/MANIFEST.sha256','\n'.join(manifest)+'\n')
    with zipfile.ZipFile(destination) as z:
        if z.testzip():raise RuntimeError('archive CRC failure')
    print(destination, len(manifest),'files',destination.stat().st_size,'bytes')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('destination',type=Path);package(p.parse_args().destination)
