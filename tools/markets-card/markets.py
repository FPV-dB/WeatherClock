#!/usr/bin/env python3
"""Build a two-page Weather Clock Markets card from delayed public quotes."""

from __future__ import annotations

import argparse
import json
import math
import struct
import time
import urllib.parse
import urllib.request
from datetime import datetime
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent
ENDPOINT = "https://query1.finance.yahoo.com/v8/finance/chart/{}?interval=1d&range=5d"
ASSETS = (
    ("AAPL", "AAPL", "Apple", "share"),
    ("MSFT", "MSFT", "Microsoft", "share"),
    ("NVDA", "NVDA", "Nvidia", "share"),
    ("GC=F", "GOLD", "Gold", "oz"),
    ("SI=F", "SILVER", "Silver", "oz"),
    ("HG=F", "COPPER", "Copper", "lb"),
    ("BTC-USD", "BTC", "Bitcoin", "coin"),
)


def font(size: int, bold: bool = False):
    names = (
        "/System/Library/Fonts/Supplemental/Arial Bold.ttf" if bold else "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf" if bold else "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    )
    for name in names:
        if Path(name).exists():
            return ImageFont.truetype(name, size)
    return ImageFont.load_default()


def fetch(symbol: str) -> dict:
    url = ENDPOINT.format(urllib.parse.quote(symbol, safe=""))
    request = urllib.request.Request(url, headers={"User-Agent": "WeatherClock-Markets/1.0"})
    with urllib.request.urlopen(request, timeout=15) as response:
        result = json.load(response)["chart"]["result"][0]
    meta = result["meta"]
    price = meta.get("regularMarketPrice")
    previous = meta.get("chartPreviousClose") or meta.get("previousClose")
    if not all(isinstance(v, (int, float)) and math.isfinite(v) and v > 0 for v in (price, previous)):
        raise ValueError(f"{symbol}: quote or previous close missing")
    return {
        "price": float(price),
        "change": (float(price) / float(previous) - 1) * 100,
        "currency": meta.get("currency", "USD"),
        "market_time": int(meta.get("regularMarketTime") or time.time()),
    }


def load_quotes(sample: bool = False) -> list[dict]:
    examples = [198.75, 417.32, 142.61, 2386.40, 31.22, 4.57, 98420.0]
    changes = [1.24, -0.38, 2.71, 0.42, -0.63, 1.08, 3.16]
    quotes = []
    for index, (symbol, label, name, unit) in enumerate(ASSETS):
        quote = ({"price": examples[index], "change": changes[index], "currency": "USD", "market_time": int(time.time())}
                 if sample else fetch(symbol))
        quote.update(symbol=symbol, label=label, name=name, unit=unit)
        quotes.append(quote)
    return quotes


def money(value: float) -> str:
    if value >= 10000:
        return f"${value:,.0f}"
    if value >= 100:
        return f"${value:,.2f}"
    return f"${value:.3f}" if value < 10 else f"${value:.2f}"


def page(title: str, rows: list[dict], page_number: int) -> Image.Image:
    image = Image.new("RGB", (240, 240), (7, 12, 20))
    draw = ImageDraw.Draw(image)
    draw.rectangle((0, 0, 239, 34), fill=(20, 43, 66))
    draw.text((10, 7), title, font=font(18, True), fill=(244, 248, 252))
    draw.text((214, 10), f"{page_number}/2", font=font(11), fill=(145, 168, 187))
    row_height = 48 if len(rows) == 4 else 58
    y = 41
    for row in rows:
        positive = row["change"] >= 0
        colour = (83, 218, 151) if positive else (255, 105, 112)
        draw.rounded_rectangle((7, y, 232, y + row_height - 5), radius=7, fill=(13, 23, 35))
        draw.text((14, y + 6), row["label"], font=font(15, True), fill=(235, 241, 246))
        draw.text((14, y + 25), row["name"], font=font(10), fill=(132, 155, 174))
        price = money(row["price"])
        price_width = draw.textbbox((0, 0), price, font=font(17, True))[2]
        draw.text((225 - price_width, y + 5), price, font=font(17, True), fill=(245, 248, 250))
        change = f"{'+' if positive else ''}{row['change']:.2f}%"
        change_width = draw.textbbox((0, 0), change, font=font(12, True))[2]
        draw.text((225 - change_width, y + 27), change, font=font(12, True), fill=colour)
        y += row_height
    stamp = datetime.now().astimezone().strftime("%H:%M")
    draw.text((9, 226), f"USD · delayed · updated {stamp}", font=font(10), fill=(118, 140, 158))
    return image


def rgb565(pixel):
    red, green, blue = pixel
    return ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)


def write_wcr(path: Path, frames: list[Image.Image], delays: list[int]):
    output = bytearray(b"WCR2") + struct.pack("<HHBB", 240, 240, len(frames) + 1, 1)
    for delay in delays + [delays[0]]:
        output += struct.pack("<H", min(delay, 65535))
    previous = None
    for image in frames + [frames[0]]:
        pixels = [rgb565(pixel) for pixel in image.getdata()]
        records = bytearray()
        index = 0
        while index < len(pixels):
            if previous is not None and pixels[index] == previous[index]:
                length = 1
                while index + length < len(pixels) and length < 128 and pixels[index + length] == previous[index + length]:
                    length += 1
                records.append(0x80 | (length - 1))
            else:
                length = 1
                while (index + length < len(pixels) and length < 127 and pixels[index + length] == pixels[index]
                       and not (previous is not None and pixels[index + length] == previous[index + length])):
                    length += 1
                records += struct.pack("<BH", length, pixels[index])
            index += length
        output += struct.pack("<I", len(records)) + records
        previous = pixels
    path.write_bytes(output)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--sample", action="store_true", help="render stable example prices without the network")
    parser.add_argument("--output", type=Path, default=HERE / "markets.wcr")
    parser.add_argument("--preview", type=Path)
    args = parser.parse_args()
    quotes = load_quotes(args.sample)
    frames = [page("US STOCKS", quotes[:3], 1), page("METALS + BITCOIN", quotes[3:], 2)]
    write_wcr(args.output, frames, [10000, 10000])
    if args.preview:
        frames[0].save(args.preview)
        frames[1].save(args.preview.with_name(args.preview.stem + "-2" + args.preview.suffix))
    print("markets:", ", ".join(f"{q['label']} {money(q['price'])} {q['change']:+.2f}%" for q in quotes))


if __name__ == "__main__":
    main()
