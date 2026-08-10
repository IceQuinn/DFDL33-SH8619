# 串口收发与 Modbus RTU 解析工具

Windows 桌面串口工具。它会将一段串口静默间隔前收到的数据视为一帧，并自动按照 **Modbus RTU 主机请求报文**解析。

## 直接使用

双击 `release/串口Modbus解析工具_v1.1.exe` 即可运行。该文件已经包含运行环境，目标电脑不需要安装 Python。

## 功能

- 串口参数：端口、波特率、数据位、停止位、奇偶校验
- HEX 或 UTF-8 文本发送，可自动追加 Modbus CRC
- 接收报文以 HEX 显示，并解析设备地址、读/写操作、功能码、寄存器/线圈起始地址、数量
- 接收线程先等待 RTU 帧间隔，再对驱动缓冲区执行一次整帧读取，串口监听器不会显示为首字节和后续字节两次读取
- CRC-16/Modbus 校验，并同时显示计算值
- 支持功能码 `01`、`02`、`03`、`04`、`05`、`06`、`08`、`0F`、`10`、`16`、`17`
- 无串口设备时也可用“手动解析 HEX”验证报文

## 运行

```powershell
cd tools\serial_assistant
python -m pip install -r requirements.txt
python app.py
```

重新生成 EXE：

```powershell
python -m pip install pyinstaller
python -m PyInstaller --noconfirm --clean --onefile --windowed --name "串口Modbus解析工具" --distpath release --workpath build app.py
```

运行测试：

```powershell
python -m unittest -v test_modbus_rtu.py
```

示例：`01 03 00 00 00 02 C4 0B` 将解析为设备地址 1、读保持寄存器、起始地址 0、数量 2、CRC 正确。
