# Operating Systems Projects

Three small C projects covering Unix processes, thread synchronization, and RAID 5-style storage. Originally developed as Operating Systems coursework, then cleaned up and tested.

## Projects

| Project | What it does | Main concepts |
| --- | --- | --- |
| **[Shelf Steam](shell/shelf-steam.c)** | Launches executables from a chosen directory, lists programs with short `--help` descriptions, and supports input redirection. | `fork`, `exec`, `waitpid`, `dup2`, ELF, directories |
| **[Parallel Hashing](parallel-hashing/hash.c)** | Matches a small list of candidate strings against hash values using worker threads. | pthreads, mutexes, atomic compare-and-swap, hash table |
| **[RAID 5](raid5/raid5.c)** | Distributes hex-encoded input into simulated disks with rotating XOR parity. | block striping, parity, file I/O |

## Build and test

Linux or another POSIX environment with a C compiler, `make`, OpenSSL development headers (`libssl-dev` on Debian/Ubuntu), and Python 3 for tests.

```sh
make
make test
make demo
```

Builds `bin/shelf-steam`, `bin/parallel-hashing`, and `bin/raid5`. `make clean` removes the binaries. The demo uses only small synthetic inputs; its captured output is in [demo/example-output.txt](demo/example-output.txt).

## Usage

```sh
./bin/shelf-steam /path/to/executables
./bin/parallel-hashing demo/passwords.txt demo/hashes.txt results.txt 4
./bin/raid5 2 17 demo/input.hex 8 disk1.hex disk2.hex disk3.hex disk4.hex
```

Shelf Steam supports `ls`, `path DIRECTORY`, `exit`, program arguments, and `program < input.txt` (spaces around `<` are required). `ls` runs `--help` on ELF executables, so use a directory of programs you trust.

The parallel hashing input contains one candidate per line and one 32-digit hex hash per line. The original project compares the **first 16 bytes** of MD5, SHA1, SHA256, or SHA512 digests; the missing hashing header was restored with a small OpenSSL wrapper. This is a concurrency exercise, not a security-grade password tool.

RAID 5 accepts a hex file, logical file length in bytes, block size in bytes, per-disk capacity in bytes, and at least three output disk names. Unused space and the final incomplete block are zero-padded. Disk files are written as hex text.

## Background

These projects came from university coursework, including collaborative course work. Original per-person contributions cannot be determined from the surviving source files. The modern build, compatibility layer, tests, and demos were added during restoration.
