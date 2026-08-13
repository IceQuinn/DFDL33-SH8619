"""Helpers for creating a com0com virtual serial-port pair on Windows."""

from __future__ import annotations

import ctypes
import os
import re
import shutil
import sys
from pathlib import Path
from typing import Iterable, Optional


COM0COM_DOWNLOAD_URL = "https://sourceforge.net/projects/com0com/files/com0com/"


def suggest_com_pair(port_names: Iterable[str], start: int = 10) -> tuple[str, str]:
    """Return the first unused consecutive COM-number pair."""
    used = set()
    for name in port_names:
        match = re.fullmatch(r"COM(\d+)", name.strip(), re.IGNORECASE)
        if match:
            used.add(int(match.group(1)))
    for number in range(max(1, start), 255):
        if number not in used and number + 1 not in used:
            return f"COM{number}", f"COM{number + 1}"
    raise RuntimeError("没有找到可用的连续 COM 端口号")


def find_setupc() -> Optional[Path]:
    """Locate com0com's command-line configuration utility."""
    candidates: list[Path] = []
    executable_dir = Path(sys.executable).resolve().parent
    source_dir = Path(__file__).resolve().parent
    candidates.extend((
        executable_dir / "com0com" / "setupc.exe",
        executable_dir / "setupc.exe",
        source_dir / "com0com" / "setupc.exe",
        source_dir / "setupc.exe",
    ))
    for variable in ("ProgramW6432", "ProgramFiles", "ProgramFiles(x86)"):
        root = os.environ.get(variable)
        if root:
            candidates.append(Path(root) / "com0com" / "setupc.exe")
    on_path = shutil.which("setupc.exe")
    if on_path:
        candidates.append(Path(on_path))
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    return None


def create_pair_elevated(setupc: Path, first: str, second: str) -> None:
    """Launch setupc with UAC elevation to create a named port pair."""
    if os.name != "nt":
        raise OSError("自动创建虚拟串口仅支持 Windows")
    if not setupc.is_file():
        raise FileNotFoundError(str(setupc))
    for name in (first, second):
        if not re.fullmatch(r"COM\d+", name, re.IGNORECASE):
            raise ValueError(f"无效串口名称：{name}")
    parameters = f'install PortName={first.upper()} PortName={second.upper()}'
    result = ctypes.windll.shell32.ShellExecuteW(
        None, "runas", str(setupc), parameters, str(setupc.parent), 1
    )
    if result <= 32:
        if result == 5:
            raise PermissionError("管理员授权被取消")
        raise OSError(f"无法启动 com0com 配置程序，ShellExecute 错误码：{result}")
