#!/usr/bin/env python3
"""Small integration checks for all three C programs."""
import hashlib
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BIN = ROOT / "bin"


def run(command, *, input=None, good=True):
    result = subprocess.run(command, input=input, text=True, capture_output=True,
                            cwd=ROOT, timeout=12, check=False)
    assert (result.returncode == 0) == good, (command, result.stderr, result.stdout)
    return result


def test_shell(directory):
    game_dir = directory / "games"
    game_dir.mkdir()
    run(["cc", "-o", str(game_dir / "sample"), str(ROOT / "demo/sample.c")])
    for i in range(505):
        (game_dir / f"file{i:03d}.txt").write_text("not executable\n")
    (directory / "input.txt").write_text("hello from a file\n")
    assert not run([BIN / "shelf-steam", directory / "missing"], good=False).returncode == 0
    result = run([BIN / "shelf-steam", game_dir], input=(
        f"ls\npath {game_dir}\nsample alpha beta\n"
        f"sample stdin < {directory / 'input.txt'}\n"
        "sample <\nsample < missing\ninvalid\nexit\n"
    ))
    assert "sample: Small sample program for Shelf Steam" in result.stdout
    assert "file504.txt: (empty)" in result.stdout
    assert "Argument: alpha" in result.stdout and "Argument: beta" in result.stdout
    assert "Input: hello from a file" in result.stdout
    assert result.stderr.count("An error has occurred") == 3
    assert run([BIN / "shelf-steam", game_dir], input="path not-real\nls extra\nexit extra\nexit\n").stderr.count("An error has occurred") == 3
    print("PASS  Shelf Steam: commands, 500+ entries, arguments, redirection, errors")


def test_hash(directory):
    words = directory / "passwords.txt"
    words.write_text("alpha\nbeta\ngamma\n")
    hashes = directory / "hashes.txt"
    lines = [hashlib.new(name, text.encode()).digest()[:16].hex()
             for name, text in [("md5", "alpha"), ("sha1", "beta"),
                                ("sha256", "gamma"), ("sha512", "alpha")]]
    lines += [lines[0], "00" * 16]
    hashes.write_text("\n".join(lines) + "\n")
    expected = "alpha:MD5\nbeta:SHA1\ngamma:SHA256\nalpha:SHA512\nalpha:MD5\nnot found\n"
    for threads in (1, 4, 11):
        output = directory / "results.txt"
        run([BIN / "parallel-hashing", words, hashes, output, str(threads)])
        assert output.read_text() == expected
    hashes.write_text("not a hex digest\n")
    run([BIN / "parallel-hashing", words, hashes, directory / "results.txt"], good=False)
    run([BIN / "parallel-hashing", words, hashes, directory / "results.txt", "0"], good=False)
    run([BIN / "parallel-hashing", directory / "missing", hashes, directory / "results.txt"], good=False)
    print("PASS  Parallel Hashing: four digests, duplicates, missing, 1/4/11 threads, errors")


def test_raid(directory):
    for length, block, count, capacity in [(17, 2, 4, 8), (20, 2, 3, 10), (3, 4, 3, 4)]:
        data = bytes(range(1, length + 1))
        source = directory / "source.hex"
        source.write_text(data.hex() + "\n")
        paths = [directory / f"disk{i}.hex" for i in range(count)]
        args = [BIN / "raid5", str(block), str(length), source, str(capacity), *paths]
        run(args)
        disks = [bytes.fromhex(p.read_text()) for p in paths]
        assert all(len(d) == capacity for d in disks)
        stripes = (length + block * (count - 1) - 1) // (block * (count - 1))
        restored = bytearray()
        for stripe in range(stripes):
            parity = count - 1 - stripe % count
            start = stripe * block
            calculated = bytearray(block)
            for i in range(1, count):
                d = (parity + i) % count
                piece = disks[d][start:start + block]
                restored.extend(piece)
                for j, byte in enumerate(piece):
                    calculated[j] ^= byte
            assert disks[parity][start:start + block] == calculated
        assert restored[:length] == data
    run([BIN / "raid5", "0", "5", source, "10", *paths], good=False)
    run([BIN / "raid5", "2", "20", source, "2", *paths], good=False)
    source.write_text("fg0\n")
    run([BIN / "raid5", "2", "3", source, "4", *paths], good=False)
    print("PASS  RAID 5: rotation, XOR parity, partial blocks, capacities, bad input")


if __name__ == "__main__":
    with tempfile.TemporaryDirectory(prefix="os-projects-") as temporary:
        directory = Path(temporary)
        test_shell(directory)
        test_hash(directory)
        test_raid(directory)
    print("All tests passed")
