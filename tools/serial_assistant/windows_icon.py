"""为Tk窗口加载匹配当前DPI的原生图标，避免任务栏放大小尺寸窗口图标。"""

import ctypes
import sys
from ctypes import wintypes


def install_window_icons(window, icon_path):
    """窗口映射及移动到不同DPI屏幕后重新选取图标，窗口销毁时释放原生句柄。"""
    if sys.platform != "win32":  # 非Windows保持Tk默认图标行为。
        return
    user32 = ctypes.WinDLL("user32", use_last_error=True)
    user32.GetAncestor.argtypes = [wintypes.HWND, wintypes.UINT]
    user32.GetAncestor.restype = wintypes.HWND
    user32.LoadImageW.argtypes = [wintypes.HINSTANCE, wintypes.LPCWSTR, wintypes.UINT, ctypes.c_int, ctypes.c_int, wintypes.UINT]
    user32.LoadImageW.restype = wintypes.HANDLE
    user32.SendMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
    user32.SendMessageW.restype = ctypes.c_ssize_t
    user32.DestroyIcon.argtypes = [wintypes.HANDLE]
    handles = []  # 保存本模块拥有的两个图标句柄，不能销毁Tk拥有的旧图标。
    last_dpi = [0]  # 相同DPI下不重复加载，避免窗口布局事件产生资源抖动。

    def update(event=None):
        """窗口显示后按实际窗口DPI加载大小图标，失败时保留已有Tk图标。"""
        if event is not None and event.widget is not window:  # 忽略子控件事件。
            return
        hwnd = user32.GetAncestor(window.winfo_id(), 2)  # GA_ROOT获取Tk外层原生窗口。
        dpi = 96  # 旧版Windows没有窗口DPI接口时使用100%缩放。
        if hasattr(user32, "GetDpiForWindow"):
            user32.GetDpiForWindow.argtypes = [wintypes.HWND]
            user32.GetDpiForWindow.restype = wintypes.UINT
            dpi = user32.GetDpiForWindow(hwnd) or 96
        if dpi == last_dpi[0]:  # 只有DPI改变或首次映射才重新设置。
            return
        sizes = [round(16 * dpi / 96), round(32 * dpi / 96)]  # 标题栏和任务栏分别使用对应物理像素尺寸。
        new_handles = [user32.LoadImageW(None, str(icon_path), 1, size, size, 0x10) for size in sizes]  # IMAGE_ICON、LR_LOADFROMFILE。
        if not all(new_handles):  # 任一加载失败不替换，释放本次成功分配的资源。
            for handle in new_handles:
                if handle:
                    user32.DestroyIcon(handle)
            return
        for kind, handle in enumerate(new_handles):
            user32.SendMessageW(hwnd, 0x80, kind, handle)  # WM_SETICON分别设置ICON_SMALL和ICON_BIG。
        for handle in handles:
            user32.DestroyIcon(handle)
        handles[:] = new_handles
        last_dpi[0] = dpi

    def release(event):
        """主窗口销毁后释放自建图标，不处理子窗口的销毁事件。"""
        if event.widget is window:
            for handle in handles:
                user32.DestroyIcon(handle)
            handles.clear()

    window.bind("<Map>", update, add="+")
    window.bind("<Configure>", update, add="+")
    window.bind("<Destroy>", release, add="+")
