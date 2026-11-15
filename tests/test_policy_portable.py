"""Independent policy scenarios compiled against the portable C++ core."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
@unittest.skipUnless(shutil.which('g++'),'portable compiler unavailable; native MSVC cases cover Windows')
class PortablePolicyTests(unittest.TestCase):
    def test_tactical_safety_scenarios(self):
        self.compile_and_run('hard_policy_tests.cpp')
    def test_planner_resource_and_uncertainty_scenarios(self):
        self.compile_and_run('planner_budget_tests.cpp')
    def compile_and_run(self,name):
        with tempfile.TemporaryDirectory(prefix='Last Light Policy ') as directory:
            executable=Path(directory)/'policy'
            built=subprocess.run(['g++','-std=c++17','-O2',str(ROOT/'tests'/name),'-o',str(executable)],text=True,capture_output=True)
            self.assertEqual(built.returncode,0,built.stdout+built.stderr)
            run=subprocess.run([str(executable)],text=True,capture_output=True)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)
