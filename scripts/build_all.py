#!/usr/bin/python3
"""
Builds every known/supported build variant.
"""
import subprocess
import shlex
import argparse

# TODO: also ensure that the current toolchain is supported.

DEBUG_OPTS = ("", "DEBUG=1")
GNU_OPTS = ("", "GNU=1")
OPT_OPTS = ("", "OPT=1", "OPT=2", "OPT=3")

OPT_TUPLES = (
    (debug, gnu, opt) for debug in DEBUG_OPTS for gnu in GNU_OPTS for opt in OPT_OPTS
)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-c", "--clean", help="clean before building", action="store_true"
    )
    parser.add_argument(
        "-j",
        "--threads",
        help="number of threads to run each make command with",
        metavar="N",
        type=int,
        default=16,
    )
    args = parser.parse_args()

    if args.clean:
        subprocess.run(["make", "cleanall"], check=True)

    for opt_tuple in OPT_TUPLES:
        cmd = f"make -j{args.threads} {' '.join(opt_tuple)}"
        # split + join to remove extra spaces
        print(f"Building: `{' '.join(cmd.split())}`")
        subprocess.run(shlex.split(cmd), check=True)

    print("All done!")


if __name__ == "__main__":
    main()
