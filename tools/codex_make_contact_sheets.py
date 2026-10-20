from pathlib import Path
from PIL import Image, ImageDraw

root = Path(r"E:\Project\SVN\DFDL33-SH8619\.codex-doc-work")
pages = sorted((root / "rendered").glob("page-*.png"))
out = root / "contact-sheets"
out.mkdir(exist_ok=True)

for start in range(0, len(pages), 4):
    batch = pages[start:start + 4]
    images = [Image.open(p).convert("RGB") for p in batch]
    w = max(im.width for im in images)
    h = max(im.height for im in images)
    sheet = Image.new("RGB", (w * 2 + 60, h * 2 + 100), "#666666")
    draw = ImageDraw.Draw(sheet)
    for idx, (path, im) in enumerate(zip(batch, images)):
        x = 20 + (idx % 2) * (w + 20)
        y = 40 + (idx // 2) * (h + 40)
        sheet.paste(im, (x, y))
        draw.text((x, y - 24), path.stem, fill="white")
    sheet.save(out / f"contact-{start + 1:02d}-{start + len(batch):02d}.png")
