"""打包时将用户PNG转换为固定嵌入的多尺寸Windows图标，不修改原始PNG。"""

from pathlib import Path
from PIL import Image

ICON_SIZES = (16, 20, 24, 28, 30, 32, 36, 40, 48, 60, 64, 72, 80, 96, 128, 256)  # 覆盖常见DPI下的窗口及任务栏尺寸，减少二次缩放。


def prepare_icon(source: Path, target: Path) -> None:
    """保留PNG透明度和长宽比例，以透明边缘补成正方形并生成多尺寸ICO供exe及窗口共用。"""
    with Image.open(source) as original:
        image = original.convert("RGBA")  # PNG统一转换为透明RGBA图像，原始文件保持不变。
    bounds = image.getchannel("A").getbbox()  # 只移除完全透明的外围留白，不裁剪任何可见图案。
    if bounds:
        image = image.crop(bounds)
    edge = max(image.size)  # 用较长边作为正方形尺寸，避免非正方形图标被拉伸。
    square = Image.new("RGBA", (edge, edge), (0, 0, 0, 0))  # 仅增加透明边缘，不裁剪用户图标。
    square.alpha_composite(image, ((edge - image.width) // 2, (edge - image.height) // 2))  # 原图居中放置并保留透明通道。
    frames = []  # 每个尺寸独立渲染，避免从已缩小的图标继续缩放。
    for size in ICON_SIZES:
        margin = max(1, round(size * 0.04))  # 保留约4%安全边距，图案比原先更饱满且不贴边。
        content = square.resize((size - margin * 2, size - margin * 2), Image.Resampling.LANCZOS)  # 高质量抗锯齿缩放，保持透明边缘。
        frame = Image.new("RGBA", (size, size), (0, 0, 0, 0))
        frame.alpha_composite(content, (margin, margin))
        frames.append(frame)
    frames[-1].save(target, format="ICO", sizes=[(size, size) for size in ICON_SIZES], append_images=frames[:-1])  # 嵌入独立尺寸图像供Windows按DPI选取。
