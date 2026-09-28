import argparse
import subprocess
from pathlib import Path


KATSU_INTRODUCTION = "a" * 30
# z0N47h%KZ7(L83#oHWI<j&No*gG"l= << output from 30 a's


def main() -> int:
    project_root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(
        description="Encrypt a message with Maplex and print lossless ciphertext."
    )
    parser.add_argument(
        "--config",
        type=Path,
        default=project_root / "examples" / "generated" / "generated-config.json",
        help="Path to the Maplex JSON configuration.",
    )
    parser.add_argument(
        "--executable",
        type=Path,
        default=project_root / "out" / "build-vs18" / "Debug" / "Maplex.exe",
        help="Path to the built Maplex executable.",
    )
    parser.add_argument(
        "--ciphertext-out",
        type=Path,
        default=project_root / "ciphertext.bin",
        help="Path to write the raw ciphertext bytes to.",
    )
    parser.add_argument(
        "message",
        nargs="?",
        default=KATSU_INTRODUCTION,
        help="Message to encrypt; defaults to 30 'a' characters.",
    )
    arguments = parser.parse_args()

    if not arguments.config.is_file():
        parser.error(f"configuration file not found: {arguments.config}")
    if not arguments.executable.is_file():
        parser.error(f"Maplex executable not found: {arguments.executable}")

    input_bytes = arguments.message.encode("utf-8")
    encryption = subprocess.run(
        [str(arguments.executable), "encrypt", str(arguments.config)],
        input=input_bytes,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if encryption.returncode != 0:
        error_text = encryption.stderr.decode("utf-8", errors="replace")
        parser.exit(encryption.returncode, error_text)

    decryption = subprocess.run(
        [str(arguments.executable), "decrypt", str(arguments.config)],
        input=encryption.stdout,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if decryption.returncode != 0:
        error_text = decryption.stderr.decode("utf-8", errors="replace")
        parser.exit(decryption.returncode, error_text)

        # generally prefer the console output, but you can use the file too
    arguments.ciphertext_out.write_bytes(encryption.stdout)

    matches_input = decryption.stdout == input_bytes
    print(f"Input: {arguments.message}")
    print(f"Raw ciphertext (latin-1 decoded): {encryption.stdout.decode('latin-1')}")
    print(f"Raw ciphertext written to: {arguments.ciphertext_out}")
    print(f"Decrypted output matches input: {'PASS' if matches_input else 'FAIL'}")
    if not matches_input:
        print(f"Decrypted output: {decryption.stdout.decode('latin-1')}")
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())