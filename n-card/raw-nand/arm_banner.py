"""Prepare a banner-only USB build with explicitly supplied NAND bytes."""

import argparse
import sys
from pathlib import Path

SIGNATURE = b"NRIO-BANNER-v1".ljust(16, b"\0")
ARMED = b"\x01\0\0\0"
EXPECTED_SIZE = 8 * 64
MAIN_SIZE = 2048
SPARE_SIZE = 64
BANNER_PAGE = 4


def arm(template, expected, main, spare):
    data = (expected, main, spare)
    sizes = (EXPECTED_SIZE, MAIN_SIZE, SPARE_SIZE)
    if any(len(part) != size for part, size in zip(data, sizes)):
        raise ValueError("Expected exactly 512 target spare bytes, 2048 new main bytes, and 64 new spare bytes")
    offset = template.find(SIGNATURE)
    if offset == -1 or template.find(SIGNATURE, offset + 1) != -1:
        raise ValueError("Expected exactly one banner plan in the template")
    start = offset + len(SIGNATURE)
    end = start + len(ARMED) + sum(sizes)
    if template[start:end] != bytes(end - start):
        raise ValueError("Use the original unarmed template")
    return template[:start] + ARMED + b"".join(data) + template[end:]


def read_range(path, offset, length):
    with path.open("rb") as source:
        source.seek(offset)
        data = source.read(length)
    if len(data) != length:
        raise ValueError(f"{path} is too short for {length} bytes at offset {offset:#x}")
    return data


def default_template():
    source_dir = Path(__file__).resolve().parent
    repo_template = source_dir / "compiled/write-enabled/banner-only/nrio-raw-banner.nds"
    if repo_template.is_file():
        return repo_template
    return source_dir / "nrio-raw-banner.nds"


def from_dumps(argv):
    parser = argparse.ArgumentParser(prog="arm_banner.py from-dumps",
                                     description="Prepare the banner app directly from separate NAND dumps.")
    parser.add_argument("target_spare", type=Path)
    parser.add_argument("donor_main", type=Path)
    parser.add_argument("donor_spare", type=Path)
    parser.add_argument("--template", type=Path, default=default_template())
    parser.add_argument("--output-dir", type=Path, default=Path("banner-ready"))
    args = parser.parse_args(argv)
    try:
        expected = read_range(args.target_spare, 0, EXPECTED_SIZE)
        main = read_range(args.donor_main, BANNER_PAGE * MAIN_SIZE, MAIN_SIZE)
        spare = read_range(args.donor_spare, BANNER_PAGE * SPARE_SIZE, SPARE_SIZE)
        image = arm(args.template.read_bytes(), expected, main, spare)
        args.output_dir.mkdir()
        (args.output_dir / "nrio-raw-banner-armed.nds").write_bytes(image)
        (args.output_dir / "banner.bin").write_bytes(main)
    except (OSError, ValueError) as error:
        parser.exit(1, f"{error}\n")
    print(f"Created {args.output_dir / 'nrio-raw-banner-armed.nds'}")
    print(f"Write {args.output_dir / 'banner.bin'} as 2048 bytes at 0x2000")


def main():
    if sys.argv[1:2] == ["from-dumps"]:
        return from_dumps(sys.argv[2:])
    parser = argparse.ArgumentParser(description=__doc__,
                                     epilog="For full dumps, run arm_banner.py from-dumps --help.")
    parser.add_argument("template", type=Path)
    parser.add_argument("--target-spare", type=Path, required=True, help="first 512 bytes of the target's spare dump")
    parser.add_argument("--banner-main", type=Path, required=True, help="exact 2048 replacement main bytes")
    parser.add_argument("--banner-spare", type=Path, required=True, help="exact 64 replacement spare bytes for page 4")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        image = arm(args.template.read_bytes(), args.target_spare.read_bytes(),
                    args.banner_main.read_bytes(), args.banner_spare.read_bytes())
        with args.output.open("xb") as output:
            output.write(image)
    except (OSError, ValueError) as error:
        parser.exit(1, f"{error}\n")
    print(f"Prepared {args.output} for a single 2048-byte write at 0x2000")


if __name__ == "__main__":
    main()
