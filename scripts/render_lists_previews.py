#!/usr/bin/env python3
"""Make review sheets from the actual C presenter's PPM captures (requires Pillow)."""
import argparse
from pathlib import Path
from PIL import Image, ImageDraw

def render(frames, output):
    output.mkdir(parents=True, exist_ok=True)
    names = ['home', 'list', 'task', 'time', 'keyboard', 'confirm', 'alert', 'new-list']
    for profile, name, height in [('watch', 'watch', 240), ('paper', 'x4', 400)]:
        sheet = Image.new('RGB', (1056, 2 * (height + 28) + 32), '#d7dce0')
        draw = ImageDraw.Draw(sheet)
        for index, screen in enumerate(names):
            x, y = 16 + index % 4 * 260, 16 + index // 4 * (height + 28)
            draw.text((x, y), screen.upper(), fill='#202b30')
            frame = Image.open(frames / f'{profile}-{screen}.ppm')
            sheet.paste(frame.resize((240, height), Image.Resampling.NEAREST), (x, y + 20))
        sheet.save(output / f'{name}.png', optimize=True)
    sheet = Image.new('RGB', (752, 844), '#d7dce0')
    draw = ImageDraw.Draw(sheet)
    for profile, x, title in [('watch', 8, 'Watch'), ('paper', 264, 'X4')]:
        draw.text((x, 8), title, fill='#202b30')
        sheet.paste(Image.open(frames / f'{profile}-completed.ppm'), (x, 28))
    sheet.save(output / 'completed.png', optimize=True)
    sheet = Image.new('RGB', (1024, 444), '#d7dce0')
    draw = ImageDraw.Draw(sheet)
    for index, (profile, screen, title) in enumerate([
        ('watch', 'list', 'Watch / OPEN'), ('watch', 'list-all', 'Watch / ALL'),
        ('paper', 'list', 'X4 / OPEN'), ('paper', 'list-all', 'X4 / ALL'),
    ]):
        draw.text((16 + index * 252, 8), title, fill='#202b30')
        frame = Image.open(frames / f'{profile}-{screen}.ppm')
        height = 240 if profile == 'watch' else 400
        sheet.paste(frame.resize((240, height), Image.Resampling.NEAREST), (16 + index * 252, 28))
    sheet.save(output / 'tabs.png', optimize=True)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--frames', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=Path(__file__).resolve().parents[1] / 'docs/lists')
    args = parser.parse_args()
    render(args.frames, args.output)
