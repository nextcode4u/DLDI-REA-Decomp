#!/usr/bin/env python3
"""Compare a DSFlash2-style image with independent NRIO NAND dumps. No writes."""

import argparse
import struct
from pathlib import Path

from patch_safe_mode import crc16

SECURE_END = 0x8000
WRAPPED_ROM = 0x8000
VIEW_BYTES = WRAPPED_ROM + 0x200
LOGO_CRC = 0xCF56
SECURE_SENTINEL = bytes.fromhex("ff de ff e7") * 2
DSFLASH2_PAIR = ((0x1000, 0xC0, 8), (0x1008, 0x80, 64))


def parse_header(data):
    if len(data) < 0x200:
        return None
    logo, header = struct.unpack_from("<HH", data, 0x15C)
    if (logo != LOGO_CRC or crc16(data[0xC0:0x15C]) != logo or
            crc16(data[:0x15E]) != header):
        return None
    return {
        "title": data[:12].rstrip(b"\x00 ").decode("ascii", "replace"),
        "code": data[0x0C:0x10].decode("ascii", "replace"),
        "size": struct.unpack_from("<I", data, 0x80)[0],
        "secure": struct.unpack_from("<H", data, 0x6C)[0],
    }


def inspect_image(data, file_size=None):
    if file_size is None:
        file_size = len(data)
    header = parse_header(data)
    if not header:
        return ["No standard DS header with valid logo/header CRC at file offset 0."], False
    lines = [f"DS header: {header['title']!r}, {header['code']!r}.",
             f"Declared size field: 0x{header['size']:X}."]
    if header["code"] == "YJYJ" and header["title"].upper() in ("YJLOADER", "YJMENU"):
        return lines + ["YJLoader-family header. NRIO layout and write support are not established."], False
    if header["code"] == "DSGB" and header["title"].upper().startswith("NINJAPASS"):
        lines.append("NinjaPass-branded DSGB header. This does not identify a NAND layout.")
    if len(data) < SECURE_END:
        return lines + ["Image does not reach the end of the secure-area window."], False

    clear = data[0x4000:0x4008] == SECURE_SENTINEL
    secure = crc16(data[0x4000:0x8000])
    keys = data[0x1000:0x1048]
    table = data[0x2000:0x3000]
    populated = len(set(keys)) > 8 and len(set(table)) > 64
    generated = not any(data[0x200:0x1000]) and populated and not clear and secure == header["secure"]
    if clear:
        lines.append("Secure-area start has the decrypted/dump sentinel.")
    lines.append(f"Secure-area CRC over file bytes: 0x{secure:04X} (header 0x{header['secure']:04X}).")
    if generated:
        lines.append("Matches the DSFlash2 0.3Z generated-image layout (not proof of origin).")
    elif not populated:
        lines.append("No DSFlash2-style tables detected at file offsets 0x1000 and 0x2000.")
    else:
        lines.append("Tables are populated, but the complete DSFlash2 0.3Z pattern did not match.")
    inner_data = data[WRAPPED_ROM:WRAPPED_ROM + 0x200]
    inner = parse_header(inner_data)
    if inner and inner["size"] and inner["code"] == header["code"]:
        arm_sections = (struct.unpack_from("<4I", inner_data, field) for field in (0x20, 0x30))
        structurally_valid = all(0x200 <= start < inner["size"] and
                                 0 < length <= inner["size"] - start
                                 for start, _, _, length in arm_sections)
        if structurally_valid:
            lines.append(f"Nested DS ROM at file offset 0x{WRAPPED_ROM:X}, "
                         f"declared size 0x{inner['size']:X}.")
            if WRAPPED_ROM + inner["size"] > file_size:
                lines.append("File does not contain the full nested ROM.")
            else:
                lines.append("Looks like a wrapped image, not a raw NAND placement.")
    return lines, generated


def compare_spare(image, spare):
    if len(image) < 0x1048 or len(spare) < 0xC8:
        return "Too short to compare first boot spare records."
    window = image[0x1000:0x1048]
    if len(set(window)) <= 8:
        return "Image does not contain a populated 0x48-byte table to compare."
    matched = sum(x == y
                  for source, target, length in DSFLASH2_PAIR
                  for x, y in zip(image[source:source+length], spare[target:target+length]))
    return f"DSFlash2 table vs. packed boot spare: {matched}/72 bytes match in the observed DS Linker order."


def read_at(stream, offset, length):
    stream.seek(offset)
    return stream.read(length)


def inspect_main(path, image):
    lines = []
    size = path.stat().st_size
    with path.open("rb") as stream:
        first = stream.read(0x200)
        if first[3:11] == b"DSGBAFLS":
            return ["NAND input looks like a filesystem volume, not a physical main dump."]
        if parse_header(first):
            return ["NAND input has a standard DS header at file offset 0. "
                    "Use a physical main dump rather than a logical .nds or writer image."]
        header = read_at(stream, 0x400, 16)
        if header[:4] == b"DSGB" and header[12:16] == b"DSGB":
            lines.append("Physical main dump has the DSGB boot header at 0x400.")
        for offset in (0x8000, 0x40000):
            word = read_at(stream, offset, 4)
            label = f"Table 0x{offset:X}"
            if len(word) < 4:
                lines.append(f"{label}: outside supplied dump.")
                continue
            if read_at(stream, offset - 4, 4) == b"QAFE":
                lines.append(f"{label}: skipped (QAFE).")
                continue
            pointer, = struct.unpack("<I", word)
            # A bootleg ROM can be at the nominal secondary table address.
            if offset == 0x40000 and read_at(stream, offset, 32) == b"<<GAME>>" * 4:
                lines.append(f"{label}: single-game marker occupies this location.")
                continue
            if offset == 0x40000 and parse_header(read_at(stream, offset, 0x200)):
                lines.append(f"{label}: DS ROM header occupies this location.")
                continue
            if pointer == 0xFFFFFFFF:
                lines.append(f"{label}: first entry erased.")
            elif not pointer or pointer % 0x4000:
                lines.append(f"{label}: first word 0x{pointer:X} is not a normal map entry.")
            elif pointer + 0x200 > size:
                lines.append(f"{label}: first entry 0x{pointer:X}; header outside supplied dump.")
            else:
                mapped = read_at(stream, pointer, 0x200)
                relation = ("candidate header matches byte for byte" if mapped == image[:0x200] else
                            "valid DS header" if parse_header(mapped) else
                            "compact NRIO header" if mapped[:4] == b"\x2e\x00\x00\xea" and
                            mapped[12:16] == b"####" and crc16(mapped[:0x15E]) ==
                            struct.unpack_from("<H", mapped, 0x15E)[0] else "unrecognized header")
                lines.append(f"{label}: first entry 0x{pointer:X}; {relation}.")
    return lines


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path, help="candidate DS ROM or generated writer image")
    parser.add_argument("--main", type=Path, help="physical NAND main-only dump")
    parser.add_argument("--spare", type=Path, help="packed NAND spare-only dump")
    args = parser.parse_args()

    try:
        with args.image.open("rb") as stream:
            image = stream.read(VIEW_BYTES)
        lines, _ = inspect_image(image, args.image.stat().st_size)
        if args.main:
            lines.extend(inspect_main(args.main, image))
        if args.spare:
            with args.spare.open("rb") as stream:
                lines.append(compare_spare(image, stream.read(0x200)))
        for line in lines:
            print(line)
        print("Read only. A match does not establish a flash offset or cart compatibility.")
    except OSError as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
