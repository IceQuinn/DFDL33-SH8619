# -*- mode: python ; coding: utf-8 -*-

from pathlib import Path
import runpy

project_dir = Path(SPECPATH)  # 以spec所在目录定位源码和资源，避免命令执行目录影响发布内容。
version = runpy.run_path(str(project_dir / 'app_version.py'))  # exe文件名与窗口标题从同一份版本定义取值。
assets = runpy.run_path(str(project_dir / 'build_assets.py'))  # 仅在打包时执行资源转换，不引入运行时Pillow依赖。
icon_path = project_dir / 'assets' / 'app_icon.ico'  # exe和窗口使用同一份多尺寸图标。
assets['prepare_icon'](project_dir / 'assets' / 'app_icon.png', icon_path)  # 每次打包重新转换当前PNG，已生成的exe图标保持固定。

a = Analysis(
    [str(project_dir / 'app.py')],
    pathex=[str(project_dir)],
    binaries=[],
    datas=[(str(project_dir / 'dlt645_data_identifiers.json'), '.'), (str(icon_path), 'assets')],
    hiddenimports=[],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    noarchive=False,
    optimize=0,
)
pyz = PYZ(a.pure)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.datas,
    [],
    name=version['EXE_NAME'],
    icon=str(icon_path),
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    upx_exclude=[],
    runtime_tmpdir=None,
    console=False,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
)
