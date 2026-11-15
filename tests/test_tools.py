import hashlib
import importlib.util
import io
import json
from pathlib import Path
import socket
import struct
import subprocess
import sys
import tempfile
import unittest
import zipfile
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'));sys.path.insert(0,str(ROOT/'python'))
from pack import pack,embed,MAGIC
from prepare_assets import extract_zip,safe_relative
from neural import Policy

class ToolTests(unittest.TestCase):
    def test_pack_directory_hashes_and_embedding(self):
        with tempfile.TemporaryDirectory(prefix='Last Light ') as folder:
            root=Path(folder);source=root/'game';source.mkdir();(source/'project.godot').write_text('config_version=5\n');(source/'main.tscn').write_text('[gd_scene format=3]\n')
            output=root/'game.pck';self.assertEqual(pack(source,output),2)
            data=output.read_bytes();header=struct.unpack_from('<6I2Q',data)
            self.assertEqual(header[:6],(MAGIC,3,4,5,1,2));base,directory=header[6:]
            count=struct.unpack_from('<I',data,directory)[0];offset=directory+4
            for _ in range(count):
                length=struct.unpack_from('<I',data,offset)[0];offset+=4;name=data[offset:offset+length].rstrip(b'\0').decode();offset+=length
                position,size=struct.unpack_from('<QQ',data,offset);offset+=16;md5=data[offset:offset+16];offset+=20
                self.assertEqual(hashlib.md5(data[base+position:base+position+size]).digest(),md5)
                self.assertEqual(data[base+position:base+position+size],(source/name).read_bytes())
            executable=root/'original';executable.write_bytes(b'MZ test binary');combined=root/'combined';embed(executable,output,combined)
            embedded=combined.read_bytes();length,magic=struct.unpack_from('<QI',embedded,len(embedded)-12)
            self.assertEqual(magic,MAGIC);self.assertEqual(embedded[-12-length:-12],data)

    def test_archive_traversal_rejected(self):
        for name in ('../evil','/tmp/evil','C:/evil','a/../../evil','..\\evil'):
            with self.assertRaises(ValueError):safe_relative(name)
        with tempfile.TemporaryDirectory() as folder:
            bad=Path(folder)/'asset.zip'
            with zipfile.ZipFile(bad,'w') as z:z.writestr('../escape.txt','bad')
            with self.assertRaises(ValueError):extract_zip(bad,Path(folder)/'out')

    def test_neural_masks_and_roundtrip(self):
        p=Policy(seed=4)
        self.assertEqual(p.choose([.5]*16,[0,0,0,0,0,1]),'Progress')
        with self.assertRaises(ValueError):p.choose([float('nan')]*16,[1]*6)
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/'model.json';p.save(path);self.assertEqual(p.weights,Policy.load(path).weights)

    def test_personal_learning_changes_intention_but_respects_masks(self):
        policy=Policy([0.0]*374)
        before=policy.weights[:]
        self.assertEqual(policy.choose([.5]*16,[1]*6,[0,0,0,1,0,0]),'Avoid')
        self.assertEqual(policy.choose([.5]*16,[1]*6,[0,0,1,0,0,0]),'Cooperate')
        self.assertEqual(policy.choose([.5]*16,[0,0,0,0,0,1],[0,0,2,0,0,0]),'Progress')
        self.assertEqual(policy.weights,before)
        for invalid in ([0]*5,[True]*6,[float('nan')]*6,[3]*6):
            with self.assertRaises(ValueError):policy.choose([.5]*16,[1]*6,invalid)

    def test_loopback_service_batches_and_errors(self):
        server=socket.socket();server.bind(('127.0.0.1',0));server.listen();server.settimeout(5)
        process=subprocess.Popen([sys.executable,str(ROOT/'python/service.py'),'--port',str(server.getsockname()[1]),'--token','test-secret'])
        try:
            peer,_=server.accept();peer.settimeout(5)
            with peer,peer.makefile('rwb') as stream:
                self.assertEqual(json.loads(stream.readline())['token'],'test-secret')
                def request(q):
                    stream.write(json.dumps(q).encode()+b'\n');stream.flush();return json.loads(stream.readline())
                self.assertEqual(request(dict(v=1,type='hello',request_id=10,episode=0))['architecture'],[16,16,6])
                self.assertEqual(request(dict(v=1,type='reset',request_id=11,episode=42))['type'],'reset')
                self.assertEqual(request(dict(v=1,type='episode_result',request_id=12,episode=42,metrics={'arrival':1}))['type'],'recorded')
                q=dict(v=1,type='load_policy',request_id=0,episode=0,weights=Policy().weights)
                self.assertEqual(request(q)['type'],'loaded')
                q=dict(v=1,type='infer_batch',request_id=1,episode=42,agents=[dict(id=i,features=[.5]*16,valid_actions=[0,0,0,0,0,1]) for i in (1,2)])
                reply=request(q);self.assertEqual([a['intent'] for a in reply['actions']],['Progress','Progress'])
                q['agents'][1]['id']=1;self.assertEqual(request(q)['type'],'error')
                for invalid in ([float('nan')]*16,[True]*16,[.5]*15):
                    q['agents']=[dict(id=1,features=invalid,valid_actions=[1]*6)]
                    self.assertEqual(request(q)['type'],'error')
                with tempfile.TemporaryDirectory() as directory:
                    saved=Path(directory)/'checkpoint.json'
                    self.assertEqual(request(dict(v=1,type='save_checkpoint',request_id=13,episode=42,path=str(saved)))['type'],'saved')
                    self.assertEqual(Policy.load(saved).weights,Policy().weights)
                stream.write(json.dumps(dict(v=1,type='shutdown',request_id=2,episode=0)).encode()+b'\n');stream.flush()
            process.wait(timeout=5);self.assertEqual(process.returncode,0)
        finally:
            server.close()
            if process.poll() is None:process.kill();process.wait()

if __name__=='__main__':unittest.main()
