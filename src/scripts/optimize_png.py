#!/usr/bin/env python3
"""Quantize a source PNG to an indexed (<=256 color) PNG for PROGMEM embedding.

Strips ICC/XMP/EXIF metadata, reduces the color palette (exactly, if the
source already fits; adaptively via median-cut otherwise), and reports how
far the quantized output drifts from the source so a bad conversion shows up
as a printed warning/non-zero exit instead of a silent tint change on real
hardware.

Usage:
    python optimize_png.py <input.png> -o <output.png> [--max-colors 256]
                            [--width 480] [--height 320]
                            [--max-avg-delta 4] [--max-channel-delta 24]
"""
import argparse
import sys

from PIL import Image


def per_channel_deltas(source: Image.Image, quantized: Image.Image):
    src_rgb = source.convert("RGB")
    quant_rgb = quantized.convert("RGB")
    src_px = src_rgb.load()
    quant_px = quant_rgb.load()
    width, height = src_rgb.size

    channel_sums = [0, 0, 0]
    channel_max = [0, 0, 0]
    pixel_count = width * height

    for y in range(height):
        for x in range(width):
            sr, sg, sb = src_px[x, y]
            qr, qg, qb = quant_px[x, y]
            deltas = (abs(sr - qr), abs(sg - qg), abs(sb - qb))
            for i, d in enumerate(deltas):
                channel_sums[i] += d
                if d > channel_max[i]:
                    channel_max[i] = d

    avg_deltas = [s / pixel_count for s in channel_sums]
    return avg_deltas, channel_max


def count_colors(image: Image.Image) -> int:
    colors = image.convert("RGB").getcolors(maxcolors=1 << 24)
    return len(colors) if colors is not None else -1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", help="Source PNG path")
    parser.add_argument("-o", "--output", required=True, help="Optimized PNG output path")
    parser.add_argument("--max-colors", type=int, default=256, help="Palette size ceiling (default: 256)")
    parser.add_argument("--width", type=int, default=480, help="Required source width (default: 480)")
    parser.add_argument("--height", type=int, default=320, help="Required source height (default: 320)")
    parser.add_argument("--max-avg-delta", type=float, default=4.0, help="Max allowed average per-channel delta")
    parser.add_argument("--max-channel-delta", type=int, default=24, help="Max allowed per-pixel per-channel delta")
    args = parser.parse_args()

    source = Image.open(args.input)
    source.load()

    if source.size != (args.width, args.height):
        print(
            f"error: {args.input} is {source.size[0]}x{source.size[1]}, "
            f"expected {args.width}x{args.height}. Rotate/resize the source "
            f"asset before embedding rather than at runtime.",
            file=sys.stderr,
        )
        return 1

    source_rgb = source.convert("RGB")
    source_colors = count_colors(source_rgb)

    if source_colors != -1 and source_colors <= args.max_colors:
        quantized = source_rgb.convert("P", palette=Image.ADAPTIVE, colors=source_colors)
    else:
        quantized = source_rgb.quantize(colors=args.max_colors, method=Image.MEDIANCUT, dither=Image.NONE)

    quantized.info.clear()
    quantized.save(args.output, format="PNG", optimize=True, icc_profile=None)

    reloaded = Image.open(args.output)
    reloaded.load()
    avg_deltas, max_deltas = per_channel_deltas(source_rgb, reloaded)
    quantized_colors = count_colors(reloaded)

    import os

    source_size = os.path.getsize(args.input)
    output_size = os.path.getsize(args.output)

    print(f"source:    {args.input} ({source_size} bytes, {source_colors} colors)")
    print(f"output:    {args.output} ({output_size} bytes, {quantized_colors} colors)")
    print(f"avg delta (R,G,B):  {avg_deltas[0]:.2f}, {avg_deltas[1]:.2f}, {avg_deltas[2]:.2f}")
    print(f"max delta (R,G,B):  {max_deltas[0]}, {max_deltas[1]}, {max_deltas[2]}")

    failed = False
    if max(avg_deltas) > args.max_avg_delta:
        print(
            f"error: average per-channel delta {max(avg_deltas):.2f} exceeds "
            f"--max-avg-delta {args.max_avg_delta}",
            file=sys.stderr,
        )
        failed = True
    if max(max_deltas) > args.max_channel_delta:
        print(
            f"error: max per-channel delta {max(max_deltas)} exceeds "
            f"--max-channel-delta {args.max_channel_delta}",
            file=sys.stderr,
        )
        failed = True

    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
