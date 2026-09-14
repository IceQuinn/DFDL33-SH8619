"""验证地址记忆、统一版本名称及用户图标的打包转换，测试不启动窗口或访问实际串口。"""

import hashlib
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import Mock, patch
from PIL import Image

from app import SerialAssistant, load_settings, save_settings, saved_dlt_address, resource_path
from app_version import APP_NAME, APP_VERSION, EXE_NAME
from build_assets import prepare_icon


class AppReleaseTests(unittest.TestCase):
    def test_address_restore_and_legacy_settings(self):
        """合法地址保留前导零及广播格式，旧配置或非法记录兼容回退默认地址。"""
        for value, expected in (("000102030405", "000102030405"), ("00 01 02 03 04 05", "000102030405"),
                                ("aaaaaaaaaaaa", "AAAAAAAAAAAA"), ("123", "000000000000"), ("00010203040F", "000000000000")):
            with self.subTest(value=value):
                self.assertEqual(saved_dlt_address({"dlt645_address": value}), expected)
        self.assertEqual(saved_dlt_address({}), "000000000000")

    def test_address_settings_round_trip(self):
        """设置文件中的地址始终作为字符串保存，版本exe改名不改变设置文件内容。"""
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "serial_assistant_settings.json"  # 测试只在临时目录保存，禁止覆盖用户实际设置。
            save_settings({"dlt645_address": "000102030405"}, path)
            self.assertEqual(saved_dlt_address(load_settings(path)), "000102030405")

    def test_close_saves_current_address(self):
        """正常关闭必须保存当前地址，并保持现有关闭串口及销毁窗口流程。"""
        variables = {name: Mock() for name in ("mode_var", "protocol_var", "port_var", "proxy_var", "baud_var",
                                               "data_var", "stop_var", "parity_var", "dlt_address_var")}  # 使用无窗口替身验证关闭流程。
        for variable in variables.values():
            variable.get.return_value = "test"
        variables["dlt_address_var"].get.return_value = "000102030405"
        window = SimpleNamespace(**variables, settings_path=Path("unused.json"), close_port=Mock(), destroy=Mock())
        with patch("app.save_settings") as save:
            SerialAssistant.on_close(window)
            self.assertEqual(save.call_args.args[0]["dlt645_address"], "000102030405")
            self.assertEqual(save.call_args.args[1], window.settings_path)
        window.close_port.assert_called_once_with()
        window.destroy.assert_called_once_with()

    def test_name_and_version_are_consistent(self):
        """窗口和打包名称来自同一份独立上位机版本，版本格式固定为三段数字。"""
        self.assertEqual(APP_NAME, "DFDL33_SH8619协议转换单元上位机")
        self.assertRegex(APP_VERSION, r"^\d+\.\d+\.\d+$")
        self.assertEqual(EXE_NAME, f"{APP_NAME}_V{APP_VERSION}")

    def test_packaged_resource_uses_bundle_not_external_icon(self):
        """打包图标只从内置资源目录加载，不能被exe旁边替换的PNG实时改变。"""
        with patch("app.sys._MEIPASS", "bundled", create=True):
            self.assertEqual(resource_path("assets/app_icon.ico"), Path("bundled/assets/app_icon.ico"))

    def test_icon_conversion_preserves_source_and_embeds_multiple_sizes(self):
        """ICO应含全部目标尺寸和透明通道，转换不得修改用户PNG源文件。"""
        source = Path(__file__).resolve().parent / "assets/app_icon.png"  # 使用用户提供的图标验证真实资源转换。
        original_hash = hashlib.sha256(source.read_bytes()).hexdigest()
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "app_icon.ico"  # 生成资源仅放入测试临时目录。
            prepare_icon(source, target)
            with Image.open(target) as icon:
                self.assertEqual(icon.format, "ICO")
                self.assertEqual(icon.mode, "RGBA")
                self.assertEqual(icon.ico.sizes(), {(size, size) for size in (16, 24, 32, 48, 64, 128, 256)})
        self.assertEqual(hashlib.sha256(source.read_bytes()).hexdigest(), original_hash)


if __name__ == "__main__":
    unittest.main()
