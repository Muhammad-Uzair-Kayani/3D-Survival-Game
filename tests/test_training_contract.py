"""Reject evolution resumes from a different executable before starting episodes."""
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'python'))
spec=importlib.util.spec_from_file_location('frostworks_training',ROOT/'python/train.py')
training=importlib.util.module_from_spec(spec);spec.loader.exec_module(training)
class TrainingContractTests(unittest.TestCase):
    def test_different_engine_checkpoint_is_rejected_before_an_episode(self):
        with tempfile.TemporaryDirectory(prefix='Last Light Contract ') as directory:
            root=Path(directory);engine=root/'engine with spaces.exe';engine.write_bytes(b'new engine')
            checkpoint=root/'checkpoint.json';checkpoint.write_text(json.dumps({'version':3,'engine_sha256':'0'*64}))
            args=SimpleNamespace(engine=engine,output=root/'out',seed=42,resume=checkpoint)
            with self.assertRaisesRegex(ValueError,'incompatible checkpoint'):
                training.train(args)
