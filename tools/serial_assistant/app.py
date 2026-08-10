from __future__ import annotations

import queue
import threading
import time
import tkinter as tk
from datetime import datetime
from tkinter import messagebox, ttk

try:
    import serial
    from serial.tools import list_ports
except ImportError:  # Shown as a friendly error after the UI starts.
    serial = None
    list_ports = None

from modbus_rtu import hex_bytes, parse_hex, parse_master_request


class SerialAssistant(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.title("串口收发与 Modbus RTU 主机报文解析工具")
        self.geometry("1180x760")
        self.minsize(960, 650)
        self.serial_port = None
        self.reader_thread = None
        self.stop_event = threading.Event()
        self.events: queue.Queue[tuple[str, object]] = queue.Queue()
        self.rx_buffer = bytearray()
        self.last_rx_time = 0.0
        self.rx_count = 0
        self.tx_count = 0
        self._build_ui()
        self.refresh_ports()
        self.after(30, self._drain_events)
        self.protocol("WM_DELETE_WINDOW", self.on_close)

    def _build_ui(self) -> None:
        style = ttk.Style(self)
        style.configure("Title.TLabel", font=("Microsoft YaHei UI", 13, "bold"))
        style.configure("Treeview", rowheight=27)

        conn = ttk.LabelFrame(self, text="串口设置", padding=10)
        conn.pack(fill="x", padx=12, pady=(12, 6))
        self.port_var = tk.StringVar()
        self.baud_var = tk.StringVar(value="9600")
        self.data_var = tk.StringVar(value="8")
        self.stop_var = tk.StringVar(value="1")
        self.parity_var = tk.StringVar(value="无")
        fields = [("端口", self.port_var, 11), ("波特率", self.baud_var, 10),
                  ("数据位", self.data_var, 6), ("停止位", self.stop_var, 6),
                  ("校验位", self.parity_var, 7)]
        self.combos = []
        choices = [[], ["1200", "2400", "4800", "9600", "19200", "38400", "57600", "115200"],
                   ["5", "6", "7", "8"], ["1", "1.5", "2"], ["无", "奇", "偶"]]
        for col, ((label, var, width), values) in enumerate(zip(fields, choices)):
            ttk.Label(conn, text=label).grid(row=0, column=col * 2, padx=(4, 3))
            combo = ttk.Combobox(conn, textvariable=var, values=values, width=width, state="normal")
            combo.grid(row=0, column=col * 2 + 1, padx=(0, 10))
            self.combos.append(combo)
        ttk.Button(conn, text="刷新", command=self.refresh_ports).grid(row=0, column=10, padx=4)
        self.open_button = ttk.Button(conn, text="打开串口", command=self.toggle_port)
        self.open_button.grid(row=0, column=11, padx=4)
        self.status_var = tk.StringVar(value="串口已关闭")
        ttk.Label(conn, textvariable=self.status_var).grid(row=0, column=12, padx=10)

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
        widths = (85, 48, 245, 75, 125, 95, 60, 75, 300)
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
        self.combos[0]["values"] = ports
        if ports and self.port_var.get() not in ports:
            self.port_var.set(ports[0])

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
            self.serial_port = serial.Serial(self.port_var.get(), int(self.baud_var.get()),
                                             bytesize=int(self.data_var.get()), parity=parity,
                                             stopbits=stopbits, timeout=0.03)
        except Exception as exc:
            messagebox.showerror("打开串口失败", str(exc))
            return
        self.stop_event.clear()
        self.reader_thread = threading.Thread(target=self._reader, daemon=True)
        self.reader_thread.start()
        self.open_button.configure(text="关闭串口")
        self.status_var.set(f"已连接 {self.port_var.get()} @ {self.baud_var.get()}")
        for combo in self.combos:
            combo.configure(state="disabled")

    def close_port(self) -> None:
        self.stop_event.set()
        port, self.serial_port = self.serial_port, None
        if port:
            try:
                port.close()
            except Exception:
                pass
        self.open_button.configure(text="打开串口")
        self.status_var.set("串口已关闭")
        for combo in self.combos:
            combo.configure(state="normal")

    def _reader(self) -> None:
        while not self.stop_event.is_set():
            port = self.serial_port
            if not port:
                break
            try:
                data = port.read(port.in_waiting or 1)
                if data:
                    self.events.put(("rx", data))
            except Exception as exc:
                self.events.put(("error", str(exc)))
                break

    def _drain_events(self) -> None:
        try:
            while True:
                kind, payload = self.events.get_nowait()
                if kind == "rx":
                    self.rx_buffer.extend(payload)  # type: ignore[arg-type]
                    self.rx_count += len(payload)  # type: ignore[arg-type]
                    self.last_rx_time = time.monotonic()
                    self._update_counter()
                elif kind == "error":
                    messagebox.showerror("串口读取错误", str(payload))
                    self.close_port()
        except queue.Empty:
            pass
        # A silence of roughly 3.5 characters delimits RTU frames. Use a safe
        # lower bound so Tk scheduling and USB serial latency do not split frames.
        baud = max(int(self.baud_var.get() or 9600), 1)
        gap = max(0.006, 3.5 * 11 / baud)
        if self.rx_buffer and time.monotonic() - self.last_rx_time >= gap:
            frame = bytes(self.rx_buffer)
            self.rx_buffer.clear()
            self.add_frame("RX", frame)
        self.after(15, self._drain_events)

    def add_frame(self, direction: str, frame: bytes) -> None:
        now = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        if direction == "TX":
            values = (now, "发送", hex_bytes(frame), "—", "—", "—", "—", "—", "")
            self.tree.insert("", "end", values=values, tags=("tx",))
        else:
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
            if result.calculated_crc is not None:
                crc_text += f" (计算 0x{result.calculated_crc:04X})"
            values = (now, "接收", hex_bytes(frame), device, action, address, count, crc_text, result.detail)
            tag = () if result.crc_ok and result.valid else ("bad",)
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
            self.add_frame("RX", data)
        except ValueError as exc:
            messagebox.showerror("格式错误", str(exc))

    def clear_log(self) -> None:
        self.tree.delete(*self.tree.get_children())
        self.rx_count = self.tx_count = 0
        self._update_counter()

    def _update_counter(self) -> None:
        self.counter_var.set(f"接收 {self.rx_count} 字节 / 发送 {self.tx_count} 字节")

    def on_close(self) -> None:
        self.close_port()
        self.destroy()


if __name__ == "__main__":
    SerialAssistant().mainloop()
