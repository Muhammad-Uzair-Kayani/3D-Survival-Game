"""Unencrypted Godot 4.5 PCK v3 writer, matching the pinned PCKPacker format."""
import argparse
import hashlib
from pathlib import Path
import struct
MAGIC=0x43504447

def pack(source,destination,prefix=""):
    source=Path(source);destination=Path(destination);destination.parent.mkdir(parents=True,exist_ok=True)
    files=[]
    for path in sorted(source.rglob('*')):
        if not path.is_file():continue
        rel=path.relative_to(source)
        if any(p in ('.git','.editor','editor') for p in rel.parts):continue
        if rel.parts[0]=='assets' and (source/'game-content.pck').exists():continue
        if rel.parts[0]=='.godot' and (len(rel.parts)<2 or rel.parts[1] not in ('imported','global_script_class_cache.cfg','uid_cache.bin')):continue
        if path.suffix.lower() in ('.zip','.rar','.blend','.tmp','.log'):continue
        if not path.name.endswith('.import') and Path(str(path)+'.import').exists():continue
        files.append((rel.as_posix(),path))
    entries=[]
    temporary=destination.with_name(destination.name+'.writing')
    with temporary.open('w+b') as f:
        f.write(struct.pack('<6I2Q16I',MAGIC,3,4,5,1,2,128,0,*([0]*16)))
        f.write(b'\0'*(128-f.tell()))
        for relative,path in files:
            data=path.read_bytes();offset=f.tell()-128;f.write(data);f.write(b'\0'*(-f.tell()%32))
            entries.append((prefix+relative,offset,len(data),hashlib.md5(data).digest()))
        directory=f.tell();f.write(struct.pack('<I',len(entries)))
        for name,offset,size,md5 in entries:
            name=name.encode();name+=b'\0'*(-len(name)%4)
            f.write(struct.pack('<I',len(name)));f.write(name);f.write(struct.pack('<QQ',offset,size));f.write(md5);f.write(struct.pack('<I',0))
        f.seek(32);f.write(struct.pack('<Q',directory))
    temporary.replace(destination)
    return len(entries)

def embed(executable,pck,destination):
    destination=Path(destination)
    with destination.open('wb') as f:
        f.write(Path(executable).read_bytes());f.write(b'\0'*(-f.tell()%8))
        data=Path(pck).read_bytes();f.write(data);f.write(struct.pack('<QI',len(data),MAGIC))
    destination.chmod(Path(executable).stat().st_mode)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('source');p.add_argument('destination');a=p.parse_args()
    print('Packed',pack(a.source,a.destination),'files')
