#!/usr/bin/python3
"""
Combines the bootloader binary and kernel elf file. In particular,
writes the kernel to its own special partition so that the bootloader
knows where to find it.

See notes/boot.md for a more thorough explanation of this process.
"""
import argparse
import shutil
import typing

# Default sector size for MBR.
SECTOR_SZ = 512


def write_partition_entry(
    out_file: typing.BinaryIO, partition_idx: int, kernel_base: int, kernel_len: int
) -> None:
    """Write 16-byte MBR partition entry.

    I don't think modern BIOSes actually care about the CHS values,
    but I quickly copied some conversion logic from Wikipedia.
    """

    assert 1 <= partition_idx <= 4, "partition index must be in [1, 4]"
    assert kernel_base % SECTOR_SZ == 0, "kernel base must be sector aligned"
    assert kernel_len % SECTOR_SZ == 0, "kernel length must be sector aligned"

    def lba2chs(lba: int) -> tuple[int, int, int]:
        """Convert LBA to CHS coordinates. Default HPC and SPT values,
        and the conversion formulas taken from Wikipedia."""
        HPC = 16
        SPT = 63
        C = lba // (HPC * SPT)
        H = (lba // SPT) % HPC
        S = (lba % SPT) + 1
        return (C, H, S)

    def lba_as_chs(lba: int) -> int:
        """Transform LBA address to 3-byte CHS address."""
        C, H, S = lba2chs(lba)
        return (C & 0xFF) << 16 | (C & 0x300) << 6 | S << 8 | H

    def write(n: int, sz: int) -> None:
        out_file.write(n.to_bytes(sz, byteorder="little", signed=False))

    start_sector = kernel_base // SECTOR_SZ
    len_sectors = kernel_len // SECTOR_SZ

    out_file.seek(0x10 * (partition_idx - 1) + 0x01BE)
    # Boot indicator flag. (1)
    write(0x80, 1)
    # Starting CHS address. (3)
    write(lba_as_chs(start_sector), 3)
    # System ID (1).
    # https://en.wikipedia.org/wiki/Partition_type#PID_0Ch
    # TODO: assert that this is actually a FAT32 filesystem
    write(0x0C, 1)
    # Ending CHS address. (3)
    write(lba_as_chs(start_sector + len_sectors - 1), 3)
    # LBA start. (4)
    write(start_sector, 4)
    # Length in sectors. (4)
    write(len_sectors, 4)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-b", "--bootloader", help="path to bootloader binary", required=True
    )
    parser.add_argument(
        "-k", "--kernel", help="path to kernel binary file", required=True
    )
    parser.add_argument("-o", "--out", help="output file", required=True)
    args = parser.parse_args()

    shutil.copyfile(args.bootloader, args.out)
    print(f"Copied {args.bootloader} to {args.out}.")

    with open(args.out, "r+b") as out_file:
        # Start at 2048 sectors in.
        kernel_base = 2048 * SECTOR_SZ
        out_file.seek(kernel_base, 0)

        # Copy kernel file.
        with open(args.kernel, "rb") as kernel:
            out_file.write(kernel.read())

        # Pad to a full sector.
        kernel_end = out_file.tell()
        if kernel_end % SECTOR_SZ != 0:
            kernel_end += SECTOR_SZ - (kernel_end % SECTOR_SZ)
            out_file.seek(kernel_end)
        kernel_len = kernel_end - kernel_base
        print(
            f"Wrote kernel: {kernel_len} bytes ({kernel_len//SECTOR_SZ} "
            f"sectors) at {kernel_base}."
        )

        # For now, always write to the first partition (1-indexed).
        partition_idx = 1
        write_partition_entry(out_file, partition_idx, kernel_base, kernel_len)
        print(f"Wrote partition info at index {partition_idx} to MBR.")


if __name__ == "__main__":
    main()
