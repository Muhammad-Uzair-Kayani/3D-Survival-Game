import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch
spec=importlib.util.spec_from_file_location('safe_push',Path(__file__).with_name('git_push.py'))
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class PushTests(unittest.TestCase):
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory(prefix='Git Push space ');self.base=Path(self.temp.name);self.repo=self.base/'client with spaces';self.remote=self.base/'remote.git'
  self.cmd(self.base,'init','--bare','--initial-branch=main',str(self.remote));self.cmd(self.base,'init','-b','main',str(self.repo))
  for key,value in [('user.name','Test User'),('user.email','test@example.invalid'),('core.autocrlf','false')]:self.cmd(self.repo,'config',key,value)
  (self.repo/'file.txt').write_text('initial');self.cmd(self.repo,'add','.');self.cmd(self.repo,'commit','-m','initial');self.cmd(self.repo,'remote','add','origin',str(self.remote));self.cmd(self.repo,'push','origin','main')
 def tearDown(self):self.temp.cleanup()
 def cmd(self,root,*args):
  p=subprocess.run(['git','-c','core.longpaths=true',*args],cwd=root,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
  self.assertEqual(p.returncode,0,p.stderr.decode(errors='replace'));return p.stdout.decode().strip()
 def job(self,answers):
  answers=iter(answers);return m.Push(self.repo,ask=lambda _:next(answers),allow_local=True)
 def other(self):
  p=self.base/'other';self.cmd(self.base,'clone',str(self.remote),str(p));self.cmd(p,'config','user.name','Other');self.cmd(p,'config','user.email','other@example.invalid');return p
 def test_message_is_data_deletions_and_dotfiles_are_included(self):
  (self.repo/'file.txt').unlink();(self.repo/'.hidden').write_text('shared')
  message='Fix & whoami > injected.txt | $(touch pwned) %PATH% ! Unicode café'
  self.job(['','',message]).run()
  self.assertEqual(self.cmd(self.repo,'log','-1','--format=%B'),message)
  self.assertEqual(self.cmd(self.base,'--git-dir='+str(self.remote),'ls-tree','--name-only','main'),'.hidden')
  self.assertFalse((self.repo/'injected.txt').exists());self.assertFalse((self.repo/'pwned').exists())
 def test_divergent_remote_publishes_separate_branch_without_overwrite(self):
  other=self.other();(other/'remote.txt').write_text('preserve');self.cmd(other,'add','.');self.cmd(other,'commit','-m','remote');self.cmd(other,'push','origin','main');old=self.cmd(other,'rev-parse','HEAD')
  (self.repo/'new.txt').write_text('local');self.job(['','','Game-Prototype','upload separately']).run()
  self.assertEqual(self.cmd(self.base,'--git-dir='+str(self.remote),'rev-parse','main'),old)
  self.assertEqual(self.cmd(self.repo,'rev-parse','HEAD'),self.cmd(self.base,'--git-dir='+str(self.remote),'rev-parse','Game-Prototype'))
 def test_unrelated_repository_uses_new_branch(self):
  remote=self.base/'different.git';self.cmd(self.base,'init','--bare',str(remote));other=self.other();self.cmd(other,'checkout','--orphan','foreign');self.cmd(other,'commit','--allow-empty','-m','foreign');self.cmd(other,'push',str(remote),'HEAD:main')
  (self.repo/'new.txt').write_text('local');self.job([str(remote),'main','Game-Prototype','new repo upload']).run()
  self.assertEqual(self.cmd(self.base,'--git-dir='+str(remote),'rev-parse','main'),self.cmd(other,'rev-parse','HEAD'))
 def test_cancel_keeps_existing_staging_backup_and_files(self):
  (self.repo/'new.txt').write_text('new');old=self.cmd(self.repo,'rev-parse','HEAD');job=self.job(['','',''])
  with self.assertRaises(m.Stop):job.run()
  self.assertEqual(self.cmd(self.repo,'rev-parse','HEAD'),old);self.assertTrue((job.backup/'index.before').exists());self.assertEqual((self.repo/'new.txt').read_text(),'new')
 def test_detached_head_stops(self):
  self.cmd(self.repo,'checkout','--detach');job=self.job([])
  with self.assertRaisesRegex(m.Stop,'Detached'):job.preflight()
 def test_oversized_blob_stops_before_commit(self):
  with (self.repo/'large.bin').open('wb')as f:f.truncate(100*1024*1024)
  head=self.cmd(self.repo,'rev-parse','HEAD')
  with self.assertRaisesRegex(m.Stop,'100 MiB'):self.job(['','']).run()
  self.assertEqual(self.cmd(self.repo,'rev-parse','HEAD'),head)
 def test_loose_engine_index_repaired_without_touching_engine_files(self):
  engine=self.base/'upstream';self.cmd(self.base,'init','-b','main',str(engine));self.cmd(engine,'config','user.name','Upstream');self.cmd(engine,'config','user.email','up@example.invalid');(engine/'engine.cpp').write_text('upstream');self.cmd(engine,'add','.');self.cmd(engine,'commit','-m','engine');self.cmd(engine,'tag','stable-test');pin=self.cmd(engine,'rev-parse','HEAD')
  import json
  (self.repo/'godot.lock.json').write_text(json.dumps(dict(repository=str(engine),commit=pin,tag='stable-test')))
  target=self.repo/'vendor/godot';target.mkdir(parents=True);file=target/'engine.cpp';file.write_text('local engine preserved');self.cmd(self.repo,'add','vendor/godot/engine.cpp')
  (self.repo/'game').mkdir();(self.repo/'game/game-content.pck').write_bytes(b'asset pack')
  self.job(['','','y','repair copied engine']).run()
  entry=self.cmd(self.repo,'ls-files','--stage','--','vendor/godot');self.assertTrue(entry.startswith('160000 '+pin+' '));self.assertEqual(file.read_text(),'local engine preserved')
  self.assertTrue((self.repo/'.gitmodules').exists());self.assertNotIn('engine.cpp',entry)
 def test_push_race_rejects_without_overwriting_remote(self):
  other=self.other();(self.repo/'mine.txt').write_text('mine');job=self.job(['','','mine']);original=job.git;published=[]
  def race(*args,**kw):
   if 'push' in args and args[0]=='-c':
    (other/'theirs.txt').write_text('theirs');self.cmd(other,'add','.');self.cmd(other,'commit','-m','theirs');self.cmd(other,'push','origin','main');published.append(self.cmd(other,'rev-parse','HEAD'))
   return original(*args,**kw)
  job.git=race
  with self.assertRaisesRegex(m.Stop,'Push failed'):job.run()
  self.assertEqual(self.cmd(self.base,'--git-dir='+str(self.remote),'rev-parse','main'),published[0]);self.assertEqual(self.cmd(self.repo,'log','-1','--format=%s'),'mine')
 def test_no_changes_can_push_without_duplicate_commit(self):
  head=self.cmd(self.repo,'rev-parse','HEAD');self.job(['','','y']).run();self.assertEqual(self.cmd(self.repo,'rev-parse','HEAD'),head)
 def test_missing_origin_can_be_configured(self):
  self.cmd(self.repo,'remote','remove','origin');(self.repo/'new.txt').write_text('new')
  self.job([str(self.remote),'main','configure origin']).run()
  self.assertEqual(self.cmd(self.repo,'remote','get-url','origin'),str(self.remote))
 def test_empty_local_repository_and_empty_remote(self):
  empty=self.base/'empty';remote=self.base/'empty.git';self.cmd(self.base,'init','--bare','--initial-branch=main',str(remote));self.cmd(self.base,'init','-b','main',str(empty));self.cmd(empty,'config','user.name','New');self.cmd(empty,'config','user.email','new@example.invalid');(empty/'new.txt').write_text('first')
  answers=iter([str(remote),'main','first commit']);job=m.Push(empty,ask=lambda _:next(answers),allow_local=True);job.run()
  self.assertEqual(self.cmd(self.base,'--git-dir='+str(remote),'rev-parse','main'),self.cmd(empty,'rev-parse','HEAD'))
 def test_only_clean_github_https_or_ssh_urls_are_accepted(self):
  job=m.Push(self.repo)
  for value in ['https://github.com/owner/repo.git','git@github.com:owner/repo.git']:self.assertEqual(job.url(value),value)
  for value in ['https://secret@github.com/owner/repo.git','file:///tmp/repo','-x','https://github.com/owner/repo.git?token=secret','https://evil.example/owner/repo']:
   with self.assertRaises(m.Stop):job.url(value)
 def test_git_environment_override_stops(self):
  job=self.job([]);job.env['GIT_INDEX_FILE']='another-index'
  with self.assertRaisesRegex(m.Stop,'override variables'):job.preflight()
 def test_lock_is_never_deleted(self):
  lock=self.repo/'.git/index.lock';lock.write_text('busy')
  with self.assertRaisesRegex(m.Stop,'index.lock'):self.job([]).preflight()
  self.assertTrue(lock.exists())
if __name__=='__main__':unittest.main()
