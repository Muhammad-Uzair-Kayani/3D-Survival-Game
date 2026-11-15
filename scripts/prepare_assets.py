"""Safely inspect/extract local asset ZIPs and prepare low-resolution textures.

This never executes instructions/scripts from an archive. Native skeleton and
animation mappings require the import report and the custom Godot editor.
"""
import argparse
import json
from pathlib import Path,PurePosixPath
import shutil
import stat
import zipfile
ROOT=Path(__file__).resolve().parents[1]
ALLOW={'.fbx','.glb','.gltf','.obj','.mtl','.bin','.png','.jpg','.jpeg','.tga','.webp','.wav','.ogg','.txt','.md','.pdf','.rar'}

def safe_relative(name):
    normalized=name.replace('\\','/')
    path=PurePosixPath(normalized)
    if not normalized or path.is_absolute() or '..' in path.parts or ':' in normalized or normalized.startswith('/'):
        raise ValueError('Unsafe archive path: '+name)
    return Path(*path.parts)

def extract_zip(archive,destination):
    report=[];destination=Path(destination)
    with zipfile.ZipFile(archive) as z:
        if sum(i.file_size for i in z.infolist())>6*1024**3:raise ValueError('Uncompressed archive exceeds 6 GiB budget')
        for item in z.infolist():
            relative=safe_relative(item.filename)
            if item.is_dir():continue
            if stat.S_ISLNK(item.external_attr>>16):raise ValueError('Archive links are not accepted')
            if item.file_size>2*1024**3:raise ValueError('Single asset exceeds 2 GiB')
            if relative.suffix.lower() not in ALLOW:continue
            target=destination/relative
            target.parent.mkdir(parents=True,exist_ok=True)
            with z.open(item) as source,target.open('wb') as out:shutil.copyfileobj(source,out)
            report.append(relative.as_posix())
    return report

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('ual','sword','pistol','map'):p.add_argument('--'+name,type=Path)
    p.add_argument('--texture-size',type=int,choices=[512,1024],default=1024)
    a=p.parse_args();from PIL import Image,ImageOps
    report={};destination=ROOT/'game/assets/imported'
    for name in ('ual','sword','pistol','map'):
        source=getattr(a,name)
        if source is None:
            report[name]={'status':'not supplied locally'};continue
        folder=destination/name
        files=extract_zip(source,folder)
        warnings=[];models=[]
        for relative in files:
            path=folder/relative
            if path.suffix.lower()=='.rar':warnings.append('Nested RAR requires extraction with a local archive tool before Godot import: '+relative)
            if path.suffix.lower() in ('.fbx','.glb','.gltf','.obj'):models.append(relative)
            if path.suffix.lower() in ('.png','.jpg','.jpeg','.tga','.webp'):
                with Image.open(path) as image:
                    image.load();image.thumbnail((a.texture_size,a.texture_size),Image.Resampling.LANCZOS)
                    if 'normal' in path.name.lower() and 'directx' in path.name.lower():
                        rgb=image.convert('RGB');r,g,b=rgb.split();image=Image.merge('RGB',(r,ImageOps.invert(g),b))
                    image.save(path)
        report[name]={'status':'extracted; editor import and rig inspection required','files':len(files),'models':models,'warnings':warnings,'texture_cap':a.texture_size}
    output=ROOT/'reports/asset-preparation.json';output.parent.mkdir(exist_ok=True);output.write_text(json.dumps(report,indent=2))
    print(json.dumps(report,indent=2))
    print('Next: BuildGame.bat --editor; inspect imported skeletons and configure game/content/assets.json. No unverified model is auto-bound.')
if __name__=='__main__':main()
