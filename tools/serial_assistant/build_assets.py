"""打包时将用户PNG转换为固定嵌入的多尺寸Windows图标，不修改原始PNG。"""

from pathlib import Path
from PIL import Image


def prepare_icon(source: Path, target: Path) -> None:
    """保留PNG透明度和长宽比例，以透明边缘补成正方形并生成多尺寸ICO供exe及窗口共用。"""
    with Image.open(source) as original:
        image = original.convert("RGBA")  # PNG统一转换为透明RGBA图像，原始文件保持不变。
    edge = max(image.size)  # 用较长边作为正方形尺寸，避免非正方形图标被拉伸。
    square = Image.new("RGBA", (edge, edge), (0, 0, 0, 0))  # 仅增加透明边缘，不裁剪用户图标。
    square.alpha_composite(image, ((edge - image.width) // 2, (edge - image.height) // 2))  # 原图居中放置并保留透明通道。
    square.save(target, format="ICO", sizes=[(size, size) for size in (16, 24, 32, 48, 64, 128, 256)])  # 多尺寸覆盖标题栏、任务栏及资源管理器图标。
