import unittest
import tempfile
from pathlib import Path
from unittest.mock import patch

from virtual_ports import find_setupc, suggest_com_pair


class VirtualPortTests(unittest.TestCase):
    def test_suggest_pair_skips_existing_ports_case_insensitively(self):
        self.assertEqual(suggest_com_pair(["COM3", "com10", "COM11"]), ("COM12", "COM13"))

    def test_suggest_pair_ignores_non_com_names(self):
        self.assertEqual(suggest_com_pair(["CNCA0", "ttyS0"]), ("COM10", "COM11"))

    def test_find_setupc_returns_bundled_copy(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            expected = root / "com0com" / "setupc.exe"
            expected.parent.mkdir()
            expected.touch()
            with patch("virtual_ports.__file__", str(root / "virtual_ports.py")), \
                    patch("virtual_ports.shutil.which", return_value=None):
                self.assertEqual(find_setupc(), expected)


if __name__ == "__main__":
    unittest.main()
