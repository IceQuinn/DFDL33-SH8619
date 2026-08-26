from __future__ import annotations

import queue
import shutil
import sys
import threading
import time
import tkinter as tk
import webbrowser
from datetime import datetime
from pathlib import Path
from tkinter import messagebox, ttk

try:
    import serial
    from serial.tools import list_ports
except ImportError:  # Shown as a friendly error after the UI starts.
    serial = None
    list_ports = None

from modbus_rtu import hex_bytes, parse_hex, parse_master_request
from dlt645 import (
    DLT645StreamParser,
    DataIdentifierRegistry,
    build_read_address,
    build_read_data,
    build_write_data,
    looks_like_frame,
    parse_frame as parse_dlt645_frame,
)
from virtual_ports import COM0COM_DOWNLOAD_URL, create_pair_elevated, find_setupc, suggest_com_pair


CONFIG_NAME = "dlt645_data_identifiers.json"


def find_dlt_config() -> Path:
    source_dir = Path(__file__).resolve().parent
    if getattr(sys, "frozen", False):
        external = Path(sys.executable).resolve().parent / CONFIG_NAME
        if external.is_file():
            return external
        bundled = Path(getattr(sys, "_MEIPASS", source_dir)) / CONFIG_NAME
        if bundled.is_file():
            try:
                shutil.copyfile(bundled, external)
                return external
            except OSError:
                return bundled
    return source_dir / CONFIG_NAME


class SerialAssistant(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.title("串口代理监听与 Modbus RTU / DL/T 645-2007 协议工具")
        self.geometry("1380x860")
        self.minsize(1080, 700)
        self.serial_port = None
        self.proxy_port = None
        self.reader_threads: list[threading.Thread] = []
        self.stop_event = threading.Event()
        self.events: queue.Queue[tuple[str, object]] = queue.Queue()
        self.rx_count = 0
        self.tx_count = 0
        self.virtual_pair_deadline = 0.0
        self.dlt_config_path = find_dlt_config()
        self.dlt_registry = DataIdentifierRegistry([], {})
        self.dlt_config_error = ""
        try:
            self.dlt_registry = DataIdentifierRegistry.load(self.dlt_config_path)
        except ValueError as exc:
            self.dlt_config_error = str(exc)
        self.dlt_parsers = {
            "MASTER": DLT645StreamParser(), "SLAVE": DLT645StreamParser(),
            "RX": DLT645StreamParser(), "TX": DLT645StreamParser(),
        }
        self.dlt_field_vars: dict[str, tk.StringVar] = {}
        self.dlt_followup_payloads: dict[tuple[str, str], bytearray] = {}
        self.dlt_pending_writes: dict[str, str] = {}
        self._build_ui()
        self.refresh_ports()
        self._refresh_dlt_definitions()
        if self.dlt_config_error:
            self.after(100, messagebox.showerror, "645配置加载失败", self.dlt_config_error)
        self.after(30, self._drain_events)
        self.protocol("WM_DELETE_WINDOW", self.on_close)

    def _build_ui(self) -> None:
        style = ttk.Style(self)
        style.configure("Title.TLabel", font=("Microsoft YaHei UI", 13, "bold"))
        style.configure("Treeview", rowheight=27)

        conn = ttk.LabelFrame(self, text="串口及监听模式", padding=10)
        conn.pack(fill="x", padx=12, pady=(12, 6))
        self.mode_var = tk.StringVar(value="代理监听")
        self.proxy_var = tk.StringVar()
        self.port_var = tk.StringVar()
        self.baud_var = tk.StringVar(value="9600")
        self.data_var = tk.StringVar(value="8")
        self.stop_var = tk.StringVar(value="1")
        self.parity_var = tk.StringVar(value="无")
        self.protocol_var = tk.StringVar(value="DL/T 645-2007")
        ttk.Label(conn, text="模式").grid(row=0, column=0, padx=(4, 3), sticky="w")
        self.mode_combo = ttk.Combobox(conn, textvariable=self.mode_var,
                                       values=("代理监听", "直接收发"), width=11, state="readonly")
        self.mode_combo.grid(row=0, column=1, padx=(0, 10), sticky="w")
        self.mode_combo.bind("<<ComboboxSelected>>", self._mode_changed)
        ttk.Label(conn, text="代理端口（虚拟串口对的工具端）").grid(row=0, column=2, padx=(4, 3), sticky="w")
        self.proxy_combo = ttk.Combobox(conn, textvariable=self.proxy_var, width=12, state="normal")
        self.proxy_combo.grid(row=0, column=3, padx=(0, 10), sticky="w")
        self.proxy_hint = ttk.Label(conn, text="其他串口软件连接虚拟串口对的另一端")
        self.proxy_hint.grid(row=0, column=4, columnspan=2, padx=4, sticky="w")
        ttk.Label(conn, text="协议").grid(row=0, column=6, padx=(12, 3), sticky="e")
        self.protocol_combo = ttk.Combobox(
            conn, textvariable=self.protocol_var,
            values=("自动识别", "Modbus RTU", "DL/T 645-2007"), width=15, state="readonly",
        )
        self.protocol_combo.grid(row=0, column=7, padx=(0, 10), sticky="w")
        self.protocol_combo.bind("<<ComboboxSelected>>", self._protocol_changed)
        self.create_pair_button = ttk.Button(conn, text="自动创建虚拟串口对", command=self.create_virtual_pair)
        self.create_pair_button.grid(row=0, column=10, columnspan=2, padx=4, sticky="w")

        fields = [("设备端口（真实串口）", self.port_var, 11), ("波特率", self.baud_var, 10),
                  ("数据位", self.data_var, 6), ("停止位", self.stop_var, 6),
                  ("校验位", self.parity_var, 7)]
        self.combos = [self.mode_combo, self.proxy_combo, self.protocol_combo]
        choices = [[], ["1200", "2400", "4800", "9600", "19200", "38400", "57600", "115200"],
                   ["5", "6", "7", "8"], ["1", "1.5", "2"], ["无", "奇", "偶"]]
        for col, ((label, var, width), values) in enumerate(zip(fields, choices)):
            ttk.Label(conn, text=label).grid(row=1, column=col * 2, padx=(4, 3), pady=(8, 0))
            combo = ttk.Combobox(conn, textvariable=var, values=values, width=width, state="normal")
            combo.grid(row=1, column=col * 2 + 1, padx=(0, 10), pady=(8, 0))
            self.combos.append(combo)
            if col == 0:
                self.port_combo = combo
        ttk.Button(conn, text="刷新", command=self.refresh_ports).grid(row=1, column=10, padx=4, pady=(8, 0))
        self.open_button = ttk.Button(conn, text="打开串口", command=self.toggle_port)
        self.open_button.grid(row=1, column=11, padx=4, pady=(8, 0))
        self.status_var = tk.StringVar(value="串口已关闭")
        ttk.Label(conn, textvariable=self.status_var).grid(row=1, column=12, padx=10, pady=(8, 0))

        paned = ttk.Panedwindow(self, orient="vertical")
        paned.pack(fill="both", expand=True, padx=12, pady=6)
        upper = ttk.Frame(paned)
        lower = ttk.Frame(paned)
        paned.add(upper, weight=3)
        paned.add(lower, weight=2)

        rx_frame = ttk.LabelFrame(upper, text="收发记录（接收报文自动解析）", padding=8)
        rx_frame.pack(fill="both", expand=True)
        columns = ("time", "dir", "raw", "device", "action", "address", "count", "crc", "detail")
        self.tree = ttk.Treeview(rx_frame, columns=columns, show="headings")
        headings = ("时间", "方向", "原始报文 (HEX)", "设备/表地址", "操作/功能", "寄存器/数据标识", "数量/数据", "校验", "解析详情")
        widths = (85, 85, 255, 105, 135, 130, 100, 155, 340)
        for name, heading, width in zip(columns, headings, widths):
            self.tree.heading(name, text=heading)
            self.tree.column(name, width=width, minwidth=45, stretch=name in ("raw", "detail"))
        scroll_y = ttk.Scrollbar(rx_frame, orient="vertical", command=self.tree.yview)
        scroll_x = ttk.Scrollbar(rx_frame, orient="horizontal", command=self.tree.xview)
        self.tree.configure(yscrollcommand=scroll_y.set, xscrollcommand=scroll_x.set)
        self.tree.grid(row=0, column=0, sticky="nsew")
        scroll_y.grid(row=0, column=1, sticky="ns")
        scroll_x.grid(row=1, column=0, sticky="ew")
        rx_frame.rowconfigure(0, weight=1)
        rx_frame.columnconfigure(0, weight=1)
        self.tree.tag_configure("bad", foreground="#c62828")
        self.tree.tag_configure("tx", foreground="#1565c0")
        self.tree.tag_configure("slave", foreground="#2e7d32")

        toolbar = ttk.Frame(rx_frame)
        toolbar.grid(row=2, column=0, columnspan=2, sticky="ew", pady=(7, 0))
        ttk.Button(toolbar, text="清空记录", command=self.clear_log).pack(side="left")
        ttk.Button(toolbar, text="手动解析 HEX", command=self.manual_parse).pack(side="left", padx=6)
        self.counter_var = tk.StringVar(value="接收 0 字节 / 发送 0 字节")
        ttk.Label(toolbar, textvariable=self.counter_var).pack(side="right")

        notebook = ttk.Notebook(lower)
        notebook.pack(fill="both", expand=True)
        send = ttk.Frame(notebook, padding=10)
        dlt = ttk.Frame(notebook, padding=10)
        notebook.add(dlt, text="DL/T 645 操作")
        notebook.add(send, text="原始报文收发")
        self.send_text = tk.Text(send, height=6, font=("Consolas", 11), undo=True)
        self.send_text.insert("1.0", "01 03 00 00 00 02 C4 0B")
        self.send_text.pack(fill="both", expand=True)
        sendbar = ttk.Frame(send)
        sendbar.pack(fill="x", pady=(8, 0))
        self.hex_send_var = tk.BooleanVar(value=True)
        self.auto_crc_var = tk.BooleanVar(value=False)
        ttk.Checkbutton(sendbar, text="HEX 发送", variable=self.hex_send_var).pack(side="left")
        self.auto_crc_check = ttk.Checkbutton(sendbar, text="自动添加 Modbus CRC", variable=self.auto_crc_var)
        self.auto_crc_check.pack(side="left", padx=12)
        self.raw_hint = ttk.Label(sendbar, text="提示：勾选自动 CRC 时，输入内容不要包含 CRC 字节")
        self.raw_hint.pack(side="left")
        ttk.Button(sendbar, text="发送", command=self.send_data).pack(side="right")
        self._build_dlt_tab(dlt)
        self._protocol_changed()

    def _build_dlt_tab(self, parent: ttk.Frame) -> None:
        top = ttk.Frame(parent)
        top.pack(fill="x")
        self.dlt_address_var = tk.StringVar(value="000000000000")
        self.dlt_preamble_var = tk.IntVar(value=int(self.dlt_registry.defaults.get("preamble_count", 4)))
        ttk.Label(top, text="通信地址").grid(row=0, column=0, padx=(0, 4), sticky="w")
        ttk.Entry(top, textvariable=self.dlt_address_var, width=16).grid(row=0, column=1, padx=(0, 10))
        ttk.Label(top, text="前导FE").grid(row=0, column=2, padx=(0, 4))
        ttk.Spinbox(top, from_=0, to=16, textvariable=self.dlt_preamble_var, width=5).grid(row=0, column=3, padx=(0, 10))
        ttk.Button(top, text="生成读地址报文", command=self.generate_dlt_read_address).grid(row=0, column=4, padx=4)
        ttk.Button(top, text="读取通信地址", command=lambda: self.generate_dlt_read_address(send=True)).grid(row=0, column=5, padx=4)
        ttk.Button(top, text="重新加载配置", command=self.reload_dlt_config).grid(row=0, column=6, padx=4)
        self.dlt_config_var = tk.StringVar(value=f"配置：{self.dlt_config_path}")
        ttk.Label(top, textvariable=self.dlt_config_var).grid(row=0, column=7, padx=8, sticky="w")
        top.columnconfigure(7, weight=1)

        choose = ttk.Frame(parent)
        choose.pack(fill="x", pady=(9, 5))
        self.dlt_action_var = tk.StringVar(value="读")
        ttk.Label(choose, text="操作").pack(side="left")
        action = ttk.Combobox(choose, textvariable=self.dlt_action_var, values=("读", "写"), width=6, state="readonly")
        action.pack(side="left", padx=(4, 12))
        action.bind("<<ComboboxSelected>>", self._dlt_action_changed)
        ttk.Label(choose, text="数据标识").pack(side="left")
        self.dlt_di_var = tk.StringVar()
        self.dlt_di_combo = ttk.Combobox(choose, textvariable=self.dlt_di_var, width=40, state="normal")
        self.dlt_di_combo.pack(side="left", padx=(4, 12))
        self.dlt_di_combo.bind("<<ComboboxSelected>>", self._dlt_selection_changed)
        ttk.Button(choose, text="生成报文", command=self.generate_dlt_operation).pack(side="left", padx=4)
        ttk.Button(choose, text="生成并发送", command=lambda: self.generate_dlt_operation(send=True)).pack(side="left", padx=4)

        self.dlt_fields_frame = ttk.LabelFrame(parent, text="数据区结构", padding=7)
        self.dlt_fields_frame.pack(fill="x", pady=(3, 6))
        self.dlt_fields_hint = ttk.Label(self.dlt_fields_frame, text="选择数据标识后显示结构")
        self.dlt_fields_hint.grid(row=0, column=0, sticky="w")

        output = ttk.LabelFrame(parent, text="生成的645报文", padding=6)
        output.pack(fill="both", expand=True)
        self.dlt_frame_text = tk.Text(output, height=3, font=("Consolas", 11), undo=True)
        self.dlt_frame_text.pack(fill="both", expand=True)
        ttk.Button(output, text="发送此报文", command=self.send_dlt_generated).pack(side="right", pady=(5, 0))

    def refresh_ports(self) -> None:
        ports = [p.device for p in list_ports.comports()] if list_ports else []
        self.proxy_combo["values"] = ports
        self.port_combo["values"] = ports
        if ports and self.port_var.get() not in ports:
            self.port_var.set(ports[0])
        if ports and self.proxy_var.get() not in ports:
            self.proxy_var.set(ports[1] if len(ports) > 1 else "")

    def _mode_changed(self, _event=None) -> None:
        proxy_mode = self.mode_var.get() == "代理监听"
        self.proxy_combo.configure(state="normal" if proxy_mode else "disabled")
        self.proxy_hint.configure(foreground="" if proxy_mode else "gray")

    def _protocol_changed(self, _event=None) -> None:
        is_modbus = self.protocol_var.get() in ("自动识别", "Modbus RTU")
        self.auto_crc_check.configure(state="normal" if is_modbus else "disabled")
        if not is_modbus:
            self.auto_crc_var.set(False)
        self.raw_hint.configure(
            text="自动识别按帧头判断协议" if self.protocol_var.get() == "自动识别"
            else "645报文请使用DL/T 645操作页生成，或发送完整HEX帧"
            if not is_modbus else "提示：勾选自动 CRC 时，输入内容不要包含 CRC 字节"
        )

    def _refresh_dlt_definitions(self) -> None:
        if not hasattr(self, "dlt_di_combo"):
            return
        action = self.dlt_action_var.get()
        values = []
        for item in self.dlt_registry.definitions.values():
            allowed = action == "读" and item.access in ("read", "read_write")
            allowed = allowed or action == "写" and item.access in ("write", "read_write")
            if allowed:
                values.append(f"{item.di} | {item.description}")
        self.dlt_di_combo["values"] = values
        if values and (not self.dlt_di_var.get() or self._selected_di(silent=True) not in self.dlt_registry.definitions):
            self.dlt_di_var.set(values[0])
        self._dlt_selection_changed()

    def _selected_di(self, silent: bool = False) -> str:
        value = self.dlt_di_var.get().split("|", 1)[0].strip().replace(" ", "")
        if len(value) == 8 and all(char in "0123456789abcdefABCDEF" for char in value):
            return value.upper()
        if silent:
            return ""
        raise ValueError("请输入8位十六进制数据标识")

    def _dlt_action_changed(self, _event=None) -> None:
        self._refresh_dlt_definitions()

    def _dlt_selection_changed(self, _event=None) -> None:
        for child in self.dlt_fields_frame.winfo_children():
            child.destroy()
        self.dlt_field_vars.clear()
        data_identifier = self._selected_di(silent=True)
        definition = self.dlt_registry.get(data_identifier) if data_identifier else None
        if not definition:
            if self.dlt_action_var.get() == "写":
                variable = tk.StringVar()
                self.dlt_field_vars["__raw__"] = variable
                ttk.Label(self.dlt_fields_frame, text="未配置的数据区HEX").grid(row=0, column=0, padx=4, sticky="w")
                ttk.Entry(self.dlt_fields_frame, textvariable=variable, width=60).grid(row=0, column=1, padx=4, sticky="ew")
            else:
                ttk.Label(self.dlt_fields_frame, text="未配置的数据标识仍可生成读取报文，回复数据将以HEX显示").grid(row=0, column=0, sticky="w")
            return
        section = definition.write_request if self.dlt_action_var.get() == "写" else definition.read_response
        fields = section.get("fields", [])
        if not fields:
            ttk.Label(self.dlt_fields_frame, text=f"{definition.description}：没有配置数据字段").grid(row=0, column=0, sticky="w")
            return
        for row, field_def in enumerate(fields):
            name = str(field_def["name"])
            label = str(field_def.get("description", name))
            unit = str(field_def.get("unit", ""))
            kind = str(field_def.get("type", "hex"))
            ttk.Label(self.dlt_fields_frame, text=label).grid(row=row, column=0, padx=4, pady=2, sticky="w")
            if self.dlt_action_var.get() == "写":
                variable = tk.StringVar(value=str(field_def.get("default", "")))
                self.dlt_field_vars[name] = variable
                if kind == "enum" and field_def.get("values"):
                    widget = ttk.Combobox(self.dlt_fields_frame, textvariable=variable,
                                          values=tuple(field_def["values"].values()), state="readonly", width=22)
                    if not variable.get():
                        variable.set(next(iter(field_def["values"].values())))
                else:
                    widget = ttk.Entry(self.dlt_fields_frame, textvariable=variable, width=24)
                widget.grid(row=row, column=1, padx=4, pady=2, sticky="w")
            else:
                ttk.Label(self.dlt_fields_frame, text=f"{kind} / {field_def.get('length')}字节").grid(row=row, column=1, padx=4, pady=2, sticky="w")
            ttk.Label(self.dlt_fields_frame, text=unit).grid(row=row, column=2, padx=4, pady=2, sticky="w")

    def reload_dlt_config(self) -> None:
        try:
            self.dlt_registry = DataIdentifierRegistry.load(self.dlt_config_path)
            self.dlt_preamble_var.set(int(self.dlt_registry.defaults.get("preamble_count", 4)))
            self._refresh_dlt_definitions()
            self.dlt_config_var.set(f"配置：{self.dlt_config_path}")
            messagebox.showinfo("配置已加载", f"已加载{len(self.dlt_registry.definitions)}个数据标识")
        except ValueError as exc:
            messagebox.showerror("配置加载失败", str(exc))

    def _set_dlt_generated(self, data: bytes) -> None:
        if self.protocol_var.get() == "Modbus RTU":
            self.protocol_var.set("DL/T 645-2007")
            self._protocol_changed()
        text = hex_bytes(data)
        self.dlt_frame_text.delete("1.0", "end")
        self.dlt_frame_text.insert("1.0", text)
        self.send_text.delete("1.0", "end")
        self.send_text.insert("1.0", text)

    def generate_dlt_read_address(self, send: bool = False) -> None:
        try:
            frame = build_read_address(int(self.dlt_preamble_var.get()))
            self._set_dlt_generated(frame)
            if send:
                self._send_bytes(frame)
        except Exception as exc:
            messagebox.showerror("生成失败", str(exc))

    def generate_dlt_operation(self, send: bool = False) -> None:
        try:
            address = self.dlt_address_var.get()
            data_identifier = self._selected_di()
            preamble = int(self.dlt_preamble_var.get())
            if self.dlt_action_var.get() == "读":
                frame = build_read_data(address, data_identifier, preamble)
            else:
                if "__raw__" in self.dlt_field_vars:
                    payload = parse_hex(self.dlt_field_vars["__raw__"].get())
                else:
                    payload = self.dlt_registry.encode(
                        data_identifier, {name: variable.get() for name, variable in self.dlt_field_vars.items()}
                    )
                defaults = self.dlt_registry.defaults
                frame = build_write_data(
                    address, data_identifier, payload,
                    str(defaults.get("password_permission", "00")),
                    str(defaults.get("password", "000000")),
                    str(defaults.get("operator_code", "00000000")),
                    preamble,
                    bool(defaults.get("write_include_security", True)),
                )
            self._set_dlt_generated(frame)
            if send:
                self._send_bytes(frame)
        except Exception as exc:
            messagebox.showerror("生成645报文失败", str(exc))

    def send_dlt_generated(self) -> None:
        try:
            data = parse_hex(self.dlt_frame_text.get("1.0", "end").strip())
            self._send_bytes(data)
        except Exception as exc:
            messagebox.showerror("发送失败", str(exc))

    def create_virtual_pair(self) -> None:
        if self.serial_port and self.serial_port.is_open:
            messagebox.showwarning("请先关闭串口", "创建虚拟串口前请先关闭当前连接")
            return
        setupc = find_setupc()
        if not setupc:
            if messagebox.askyesno(
                "未检测到 com0com",
                "自动创建需要先安装 com0com 虚拟串口驱动。\n\n是否打开官方项目下载页面？",
            ):
                webbrowser.open(COM0COM_DOWNLOAD_URL)
            return
        current_ports = [p.device for p in list_ports.comports()] if list_ports else []
        try:
            first, second = suggest_com_pair(current_ports)
            if not messagebox.askyesno(
                "创建虚拟串口对",
                f"将使用管理员权限创建 {first} ↔ {second}。\n\n"
                f"其他串口软件使用 {first}，本工具使用 {second}。是否继续？",
            ):
                return
            create_pair_elevated(setupc, first, second)
        except Exception as exc:
            messagebox.showerror("创建失败", str(exc))
            return
        self.status_var.set(f"正在等待系统创建 {first} ↔ {second}…")
        self.create_pair_button.configure(state="disabled")
        self.virtual_pair_deadline = time.monotonic() + 20.0
        self._wait_for_virtual_pair(first, second)

    def _wait_for_virtual_pair(self, first: str, second: str) -> None:
        ports = [p.device.upper() for p in list_ports.comports()] if list_ports else []
        if first.upper() in ports and second.upper() in ports:
            self.refresh_ports()
            self.mode_var.set("代理监听")
            self.proxy_var.set(second)
            self._mode_changed()
            self.create_pair_button.configure(state="normal")
            self.status_var.set(f"已创建 {first} ↔ {second}；代理端口已选择 {second}")
            messagebox.showinfo(
                "创建成功",
                f"虚拟串口对 {first} ↔ {second} 已创建。\n\n"
                f"其他串口软件请选择 {first}，本工具代理端口已设为 {second}。",
            )
            return
        if time.monotonic() >= self.virtual_pair_deadline:
            self.create_pair_button.configure(state="normal")
            self.status_var.set("未检测到新虚拟串口")
            messagebox.showerror(
                "未检测到虚拟串口",
                "com0com 命令已启动，但系统未出现目标端口。请确认已允许管理员授权，"
                "并检查驱动是否正确安装和签名。",
            )
            return
        self.after(500, self._wait_for_virtual_pair, first, second)

    def toggle_port(self) -> None:
        if self.serial_port and self.serial_port.is_open:
            self.close_port()
            return
        if serial is None:
            messagebox.showerror("缺少依赖", "未安装 pyserial，请先执行：pip install -r requirements.txt")
            return
        try:
            parity = {"无": serial.PARITY_NONE, "奇": serial.PARITY_ODD, "偶": serial.PARITY_EVEN}[self.parity_var.get()]
            stopbits = {"1": serial.STOPBITS_ONE, "1.5": serial.STOPBITS_ONE_POINT_FIVE, "2": serial.STOPBITS_TWO}[self.stop_var.get()]
            if self.mode_var.get() == "代理监听" and self.proxy_var.get().upper() == self.port_var.get().upper():
                raise ValueError("代理端口和设备端口不能相同")
            self.serial_port = serial.Serial(self.port_var.get(), int(self.baud_var.get()),
                                             bytesize=int(self.data_var.get()), parity=parity,
                                             stopbits=stopbits, timeout=0.03)
            if self.mode_var.get() == "代理监听":
                if not self.proxy_var.get():
                    raise ValueError("请选择虚拟串口对的工具端口")
                self.proxy_port = serial.Serial(self.proxy_var.get(), int(self.baud_var.get()),
                                                bytesize=int(self.data_var.get()), parity=parity,
                                                stopbits=stopbits, timeout=0.03)
        except Exception as exc:
            self._close_serial_objects()
            messagebox.showerror("打开串口失败", str(exc))
            return
        self.stop_event.clear()
        self.dlt_parsers = {
            "MASTER": DLT645StreamParser(), "SLAVE": DLT645StreamParser(),
            "RX": DLT645StreamParser(), "TX": DLT645StreamParser(),
        }
        self.dlt_followup_payloads.clear()
        if self.proxy_port:
            self._start_reader(self.proxy_port, "MASTER", self.serial_port)
            self._start_reader(self.serial_port, "SLAVE", self.proxy_port)
        else:
            self._start_reader(self.serial_port, "RX", None)
        self.open_button.configure(text="关闭串口")
        if self.proxy_port:
            self.status_var.set(f"代理中：{self.proxy_var.get()} ↔ {self.port_var.get()} @ {self.baud_var.get()}")
        else:
            self.status_var.set(f"已连接 {self.port_var.get()} @ {self.baud_var.get()}")
        for combo in self.combos:
            combo.configure(state="disabled")
        self.create_pair_button.configure(state="disabled")

    def close_port(self) -> None:
        self.stop_event.set()
        self._close_serial_objects()
        self.reader_threads.clear()
        self.open_button.configure(text="打开串口")
        self.status_var.set("串口已关闭")
        for combo in self.combos:
            combo.configure(state="normal")
        self.mode_combo.configure(state="readonly")
        self.protocol_combo.configure(state="readonly")
        self.create_pair_button.configure(state="normal")
        self._mode_changed()

    def _close_serial_objects(self) -> None:
        ports = (self.serial_port, self.proxy_port)
        self.serial_port = self.proxy_port = None
        for port in ports:
            if not port:
                continue
            try:
                port.close()
            except Exception:
                pass

    def _start_reader(self, port, direction: str, forward_port) -> None:
        thread = threading.Thread(target=self._reader, args=(port, direction, forward_port), daemon=True)
        self.reader_threads.append(thread)
        thread.start()

    def _reader(self, port, direction: str, forward_port) -> None:
        """Read one complete RTU frame from the driver buffer at a time.

        Do not submit a one-byte read while the buffer is empty. Doing so makes
        Windows complete the first IRP with the slave address and a second IRP
        with the rest of the frame. Instead, observe ``in_waiting`` until its
        value remains stable for an RTU frame gap, then issue one exact-sized
        read. Serial monitoring tools will therefore also see a complete frame.
        """
        while not self.stop_event.is_set():
            try:
                waiting = port.in_waiting
                if waiting <= 0:
                    self.stop_event.wait(0.001)
                    continue

                # 1 start bit + data bits + optional parity + stop bits.
                bits_per_char = 1 + port.bytesize + (0 if port.parity == serial.PARITY_NONE else 1) + float(port.stopbits)
                frame_gap = max(0.006, 3.5 * bits_per_char / max(port.baudrate, 1))
                stable_count = waiting
                last_change = time.monotonic()

                while not self.stop_event.is_set():
                    self.stop_event.wait(min(0.002, frame_gap / 3))
                    current_count = port.in_waiting
                    now = time.monotonic()
                    if current_count != stable_count:
                        stable_count = current_count
                        last_change = now
                    if stable_count >= 256 or (stable_count > 0 and now - last_change >= frame_gap):
                        # Bytes are already buffered, so this produces one
                        # IRP_MJ_READ containing the complete RTU request.
                        data = port.read(stable_count)
                        if data:
                            if forward_port:
                                forward_port.write(data)
                            self.events.put(("frame", (direction, data)))
                        break
            except Exception as exc:
                if not self.stop_event.is_set():
                    self.events.put(("error", f"{direction}: {exc}"))
                break

    def _drain_events(self) -> None:
        try:
            while True:
                kind, payload = self.events.get_nowait()
                if kind == "frame":
                    direction, data = payload  # type: ignore[misc]
                    if direction in ("SLAVE", "RX"):
                        self.rx_count += len(data)
                    else:
                        self.tx_count += len(data)
                    self._update_counter()
                    self._process_received_chunk(direction, data)
                elif kind == "error":
                    messagebox.showerror("串口读取错误", str(payload))
                    self.close_port()
        except queue.Empty:
            pass
        self.after(15, self._drain_events)

    def _process_received_chunk(self, direction: str, data: bytes) -> None:
        protocol = self.protocol_var.get()
        if protocol == "DL/T 645-2007":
            frames = self.dlt_parsers.setdefault(direction, DLT645StreamParser()).feed(data)
            for frame in frames:
                self.add_frame(direction, frame, "DL/T 645-2007")
            return
        if protocol == "自动识别" and looks_like_frame(data):
            frames = self.dlt_parsers.setdefault(direction, DLT645StreamParser()).feed(data)
            for frame in frames:
                self.add_frame(direction, frame, "DL/T 645-2007")
            return
        self.add_frame(direction, data, "Modbus RTU")

    def add_frame(self, direction: str, frame: bytes, protocol: str | None = None) -> None:
        selected = protocol or self.protocol_var.get()
        if selected == "DL/T 645-2007" or selected == "自动识别" and looks_like_frame(frame):
            self._add_dlt_frame(direction, frame)
        else:
            self._add_modbus_frame(direction, frame)

    def _add_modbus_frame(self, direction: str, frame: bytes) -> None:
        now = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        if direction in ("RX", "MASTER", "MANUAL"):
            result = parse_master_request(frame)
            device = "—" if result.device_address is None else f"{result.device_address} (0x{result.device_address:02X})"
            action = f"{result.operation} / {result.function_name} (0x{result.function_code:02X})" if result.function_code is not None else "未知"
            if result.operation == "读/写":
                address = f"读 0x{result.read_address:04X} / 写 0x{result.register_address:04X}"
                count = f"读 {result.read_count} / 写 {result.register_count}"
            else:
                address = "—" if result.register_address is None else f"0x{result.register_address:04X} ({result.register_address})"
                count = "—" if result.register_count is None else str(result.register_count)
            crc_text = "正确" if result.crc_ok else "错误"
            if result.received_crc is not None and result.calculated_crc is not None:
                crc_text += f" (接收 {result.received_crc:04X} / 计算 {result.calculated_crc:04X})"
            direction_text = "主机→设备" if direction == "MASTER" else "接收"
            values = (now, direction_text, hex_bytes(frame), device, action, address, count, crc_text, result.detail)
            tag = () if result.crc_ok and result.valid else ("bad",)
            self.tree.insert("", "end", values=values, tags=tag)
        else:
            direction_text = "设备→主机" if direction == "SLAVE" else "发送"
            tag = ("slave",) if direction == "SLAVE" else ("tx",)
            values = (now, direction_text, hex_bytes(frame), "—", "—", "—", "—", "—", "透明转发" if direction == "SLAVE" else "")
            self.tree.insert("", "end", values=values, tags=tag)
        self._scroll_to_last()

    def _add_dlt_frame(self, direction: str, frame: bytes) -> None:
        result = parse_dlt645_frame(frame, self.dlt_registry)
        now = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        decoded_values = list(result.decoded_values)
        combined_payload: bytes | None = None
        result_detail = ""
        function = result.control & 0x1F if result.control is not None else -1
        if (result.valid and result.direction == "从机→主机" and result.address
                and result.data_identifier and function in (0x11, 0x12)):
            key = (result.address, result.data_identifier)
            if result.follow_up:
                accumulated = self.dlt_followup_payloads.setdefault(key, bytearray())
                accumulated.extend(result.payload)
                combined_payload = bytes(accumulated)
                if direction == "RX":
                    self.after(30, self._send_dlt_follow_up, result.address, result.data_identifier,
                               result.frame_sequence or 0)
            elif key in self.dlt_followup_payloads:
                accumulated = self.dlt_followup_payloads.pop(key)
                accumulated.extend(result.payload)
                combined_payload = bytes(accumulated)
                try:
                    decoded_values = self.dlt_registry.decode(result.data_identifier, combined_payload)
                except ValueError as exc:
                    decoded_values = []
                    result_detail = str(exc)
        if direction == "MASTER":
            direction_text = "主机→设备"
        elif direction == "SLAVE":
            direction_text = "设备→主机"
        elif direction == "TX":
            direction_text = "发送"
        else:
            direction_text = result.direction
        address = f"表号 {result.address}" if result.address else "—"
        action = result.operation
        if result.control is not None:
            action += f" (0x{result.control:02X})"
        if result.address and result.data_identifier and result.direction == "主机→从机" and function == 0x14:
            self.dlt_pending_writes[result.address] = result.data_identifier
        pending_di = self.dlt_pending_writes.pop(result.address, None) if (
            result.address and result.direction == "从机→主机" and function == 0x14
        ) else None
        target_di = result.data_identifier or pending_di
        target = f"DI {target_di}" if target_di else ("通信地址" if result.operation == "读通信地址" else "—")
        data_summary = f"{len(result.payload)}字节" if result.payload else "—"
        check = "CS正确" if result.checksum_ok else "CS错误"
        if result.received_checksum is not None and result.calculated_checksum is not None:
            check += f" ({result.received_checksum:02X}/{result.calculated_checksum:02X})"
        details: list[str] = []
        definition = self.dlt_registry.get(result.data_identifier) if result.data_identifier else None
        if definition:
            details.append(definition.description)
        if decoded_values:
            details.extend(f"{name}={value}{unit}" for name, value, unit in decoded_values)
            data_summary = ", ".join(f"{name}={value}{unit}" for name, value, unit in decoded_values)
        elif combined_payload is not None:
            data_summary = f"累计{len(combined_payload)}字节"
            details.append(f"累计数据={hex_bytes(combined_payload)}")
        elif result.payload:
            details.append(f"数据={hex_bytes(result.payload)}")
        if result.follow_up:
            details.append("有后续数据")
        if result.frame_sequence is not None:
            details.append(f"帧序号={result.frame_sequence}")
        if result_detail:
            details.append(result_detail)
        if result.detail:
            details.append(result.detail)
        values = (now, direction_text, hex_bytes(frame), address, action, target, data_summary, check, "；".join(details))
        if not result.valid:
            tags = ("bad",)
        elif result.direction == "从机→主机":
            tags = ("slave",)
        else:
            tags = ("tx",)
        self.tree.insert("", "end", values=values, tags=tags)
        if result.valid and result.operation == "读通信地址" and result.direction == "从机→主机" and result.address:
            self.dlt_address_var.set(result.address)
        self._scroll_to_last()

    def _send_dlt_follow_up(self, address: str, data_identifier: str, sequence: int) -> None:
        try:
            frame = build_read_data(address, data_identifier, 0, follow_up=True, sequence=sequence)
            self._set_dlt_generated(frame)
            self._send_bytes(frame)
        except Exception as exc:
            messagebox.showerror("读取后续数据失败", str(exc))

    def _scroll_to_last(self) -> None:
        children = self.tree.get_children()
        if children:
            self.tree.see(children[-1])

    def send_data(self) -> None:
        try:
            text = self.send_text.get("1.0", "end").strip()
            data = parse_hex(text) if self.hex_send_var.get() else text.encode("utf-8")
            if self.auto_crc_var.get():
                from modbus_rtu import crc16
                checksum = crc16(data)
                data += bytes((checksum & 0xFF, checksum >> 8))
            if not data:
                raise ValueError("发送内容为空")
            self._send_bytes(data)
        except Exception as exc:
            messagebox.showerror("发送失败", str(exc))

    def _send_bytes(self, data: bytes) -> None:
        port = self.serial_port
        if not port or not port.is_open:
            raise RuntimeError("请先选择并打开设备串口")
        if not data:
            raise ValueError("发送内容为空")
        port.write(data)
        self.tx_count += len(data)
        self._update_counter()
        self.add_frame("TX", data)

    def manual_parse(self) -> None:
        try:
            data = parse_hex(self.send_text.get("1.0", "end").strip())
            self.add_frame("MANUAL", data)
        except ValueError as exc:
            messagebox.showerror("格式错误", str(exc))

    def clear_log(self) -> None:
        self.tree.delete(*self.tree.get_children())
        self.rx_count = self.tx_count = 0
        self._update_counter()

    def _update_counter(self) -> None:
        if self.mode_var.get() == "代理监听":
            self.counter_var.set(f"主机→设备 {self.tx_count} 字节 / 设备→主机 {self.rx_count} 字节")
        else:
            self.counter_var.set(f"接收 {self.rx_count} 字节 / 发送 {self.tx_count} 字节")

    def on_close(self) -> None:
        self.close_port()
        self.destroy()


if __name__ == "__main__":
    SerialAssistant().mainloop()
