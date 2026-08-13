from __future__ import annotations

import queue
import threading
import time
import tkinter as tk
import webbrowser
from datetime import datetime
from tkinter import messagebox, ttk

try:
    import serial
    from serial.tools import list_ports
except ImportError:  # Shown as a friendly error after the UI starts.
    serial = None
    list_ports = None

from modbus_rtu import hex_bytes, parse_hex, parse_master_request
from virtual_ports import COM0COM_DOWNLOAD_URL, create_pair_elevated, find_setupc, suggest_com_pair


class SerialAssistant(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.title("串口代理监听与 Modbus RTU 主机报文解析工具")
        self.geometry("1180x760")
        self.minsize(960, 650)
        self.serial_port = None
        self.proxy_port = None
        self.reader_threads: list[threading.Thread] = []
        self.stop_event = threading.Event()
        self.events: queue.Queue[tuple[str, object]] = queue.Queue()
        self.rx_count = 0
        self.tx_count = 0
        self.virtual_pair_deadline = 0.0
        self._build_ui()
        self.refresh_ports()
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
        ttk.Label(conn, text="模式").grid(row=0, column=0, padx=(4, 3), sticky="w")
        self.mode_combo = ttk.Combobox(conn, textvariable=self.mode_var,
                                       values=("代理监听", "直接收发"), width=11, state="readonly")
        self.mode_combo.grid(row=0, column=1, padx=(0, 10), sticky="w")
        self.mode_combo.bind("<<ComboboxSelected>>", self._mode_changed)
        ttk.Label(conn, text="代理端口（虚拟串口对的工具端）").grid(row=0, column=2, padx=(4, 3), sticky="w")
        self.proxy_combo = ttk.Combobox(conn, textvariable=self.proxy_var, width=12, state="normal")
        self.proxy_combo.grid(row=0, column=3, padx=(0, 10), sticky="w")
        self.proxy_hint = ttk.Label(conn, text="其他串口软件连接虚拟串口对的另一端")
        self.proxy_hint.grid(row=0, column=4, columnspan=5, padx=4, sticky="w")
        self.create_pair_button = ttk.Button(conn, text="自动创建虚拟串口对", command=self.create_virtual_pair)
        self.create_pair_button.grid(row=0, column=10, columnspan=2, padx=4, sticky="w")

        fields = [("设备端口（真实串口）", self.port_var, 11), ("波特率", self.baud_var, 10),
                  ("数据位", self.data_var, 6), ("停止位", self.stop_var, 6),
                  ("校验位", self.parity_var, 7)]
        self.combos = [self.mode_combo, self.proxy_combo]
        choices = [[], ["1200", "2400", "4800", "9600", "19200", "38400", "57600", "115200"],
                   ["5", "6", "7", "8"], ["1", "1.5", "2"], ["无", "奇", "偶"]]
        for col, ((label, var, width), values) in enumerate(zip(fields, choices)):
            ttk.Label(conn, text=label).grid(row=1, column=col * 2, padx=(4, 3), pady=(8, 0))
            combo = ttk.Combobox(conn, textvariable=var, values=values, width=width, state="normal")
            combo.grid(row=1, column=col * 2 + 1, padx=(0, 10), pady=(8, 0))
            self.combos.append(combo)
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
        headings = ("时间", "方向", "原始报文 (HEX)", "设备地址", "功能", "寄存器地址", "数量", "CRC", "解析详情")
        widths = (85, 85, 245, 75, 125, 95, 60, 75, 300)
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

        send = ttk.LabelFrame(lower, text="数据发送", padding=10)
        send.pack(fill="both", expand=True)
        self.send_text = tk.Text(send, height=6, font=("Consolas", 11), undo=True)
        self.send_text.insert("1.0", "01 03 00 00 00 02 C4 0B")
        self.send_text.pack(fill="both", expand=True)
        sendbar = ttk.Frame(send)
        sendbar.pack(fill="x", pady=(8, 0))
        self.hex_send_var = tk.BooleanVar(value=True)
        self.auto_crc_var = tk.BooleanVar(value=False)
        ttk.Checkbutton(sendbar, text="HEX 发送", variable=self.hex_send_var).pack(side="left")
        ttk.Checkbutton(sendbar, text="自动添加 Modbus CRC", variable=self.auto_crc_var).pack(side="left", padx=12)
        ttk.Label(sendbar, text="提示：勾选自动 CRC 时，输入内容不要包含 CRC 字节").pack(side="left")
        ttk.Button(sendbar, text="发送", command=self.send_data).pack(side="right")

    def refresh_ports(self) -> None:
        ports = [p.device for p in list_ports.comports()] if list_ports else []
        self.proxy_combo["values"] = ports
        self.combos[2]["values"] = ports
        if ports and self.port_var.get() not in ports:
            self.port_var.set(ports[0])
        if ports and self.proxy_var.get() not in ports:
            self.proxy_var.set(ports[1] if len(ports) > 1 else "")

    def _mode_changed(self, _event=None) -> None:
        proxy_mode = self.mode_var.get() == "代理监听"
        self.proxy_combo.configure(state="normal" if proxy_mode else "disabled")
        self.proxy_hint.configure(foreground="" if proxy_mode else "gray")

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
                    self.add_frame(direction, data)
                elif kind == "error":
                    messagebox.showerror("串口读取错误", str(payload))
                    self.close_port()
        except queue.Empty:
            pass
        self.after(15, self._drain_events)

    def add_frame(self, direction: str, frame: bytes) -> None:
        now = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        if direction in ("RX", "MASTER"):
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
        children = self.tree.get_children()
        if children:
            self.tree.see(children[-1])

    def send_data(self) -> None:
        port = self.serial_port
        if not port or not port.is_open:
            messagebox.showwarning("串口未打开", "请先选择并打开串口")
            return
        try:
            text = self.send_text.get("1.0", "end").strip()
            data = parse_hex(text) if self.hex_send_var.get() else text.encode("utf-8")
            if self.auto_crc_var.get():
                from modbus_rtu import crc16
                checksum = crc16(data)
                data += bytes((checksum & 0xFF, checksum >> 8))
            if not data:
                raise ValueError("发送内容为空")
            port.write(data)
            self.tx_count += len(data)
            self._update_counter()
            self.add_frame("TX", data)
        except Exception as exc:
            messagebox.showerror("发送失败", str(exc))

    def manual_parse(self) -> None:
        try:
            data = parse_hex(self.send_text.get("1.0", "end").strip())
            self.add_frame("MASTER", data)
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
