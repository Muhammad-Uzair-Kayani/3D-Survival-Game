"""Check the actual C++ scorer against the trainer on the full feature layout."""
import importlib.util
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


@unittest.skipUnless(shutil.which("g++") and importlib.util.find_spec("numpy"),
                     "Native parity check requires g++ and NumPy")
class NativePolicyTests(unittest.TestCase):
    def test_checkpoint_parity_and_validation(self):
        sys.path.insert(0, str(ROOT / "python"))
        try:
            import frostworks_train as trainer
        finally:
            sys.path.remove(str(ROOT / "python"))
        import numpy as np

        with tempfile.TemporaryDirectory() as directory:
            exe = Path(directory) / "score"
            model = Path(directory) / "model.llnp"
            subprocess.run(["g++", "-std=c++17", "-O2", "-I", str(ROOT / "modules/last_light"),
                            str(ROOT / "tests/native_policy_probe.cpp"), "-o", str(exe)], check=True)
            rng = np.random.default_rng(404)
            policy = trainer.Policy()
            for weight in policy.parameters():
                weight[:] = rng.normal(0, .12, weight.shape)
            policy.save(model)
            size = len(trainer.FEATURES)
            fields = np.eye(size, dtype=np.float32)
            inputs = np.concatenate([fields, fields * -4, fields * 4, np.zeros((1, size)),
                                     rng.uniform(-4, 4, (200, size))]).astype(np.float32)
            stimulus = "\n".join(" ".join(map(str, row)) for row in inputs) + "\n"
            raw = subprocess.check_output([str(exe), str(model)], input=stimulus, text=True)
            native = np.fromstring(raw, sep="\n")
            expected, _ = trainer.Policy.load(model).score(inputs)
            np.testing.assert_allclose(native, expected, rtol=3e-5, atol=3e-6)
            self.assertEqual(np.argmax(native), np.argmax(expected))

            valid = model.read_bytes()
            for corrupted in (b"", valid[:-4], b"FAKE" + valid[4:], valid[:4] + b"\x02" + valid[5:],
                              valid[:16] + struct.pack("<f", float("nan")) + valid[20:]):
                model.write_bytes(corrupted)
                self.assertEqual(subprocess.run([str(exe), str(model)], input="", text=True).returncode, 3)
                with self.assertRaises(ValueError):
                    trainer.Policy.load(model)


if __name__ == "__main__":
    unittest.main()
