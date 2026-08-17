# IPC Prime vs Abundant Number Race

A C program that demonstrates multi-directional inter-process communication
(IPC) using the POSIX `fork()` and `pipe()` system calls. A parent process
spawns two child processes that race each other: one counts prime numbers,
the other counts abundant numbers, and each announces the winner based on
data relayed back and forth through pipes.

## How it works

1. **Parent process (P1)** reads an integer `N` from the user and generates
   two files, `File1` and `File2`, each containing `N` random non-negative
   integers.
2. P1 forks two child processes, **P2** and **P3**, and sets up four pipes
   (two full-duplex channels) so it can talk to each child independently.
3. P1 sends `N` to both children.
4. **P2** reads `File1` and counts how many of the `N` integers are
   **prime**. **P3** reads `File2` and counts how many are **abundant**
   (the sum of a number's proper divisors exceeds the number itself).
5. Each child reports its count back to P1, which cross-relays the counts:
   P2 receives P3's count and vice versa.
6. Each child independently compares the two counts and prints:
   `I am Child process Px: The winner is child process Py` (or a tie
   message if the counts are equal).
7. P1 waits for both children to finish and prints a summary of the total
   count of integers, primes found in `File1`, and abundant numbers found
   in `File2`.

## Project structure

```
ipc-prime-abundant-race/
├── src/
│   └── main.c   # Program source
├── hw1/         # Original coursework submission (unmodified, kept for reference)
└── README.md
```

## Technologies used

- **Language:** C (C11)
- **APIs:** POSIX process control and IPC — `fork()`, `pipe()`, `read()`,
  `write()`, `wait()` (`<unistd.h>`, `<sys/wait.h>`)
- **Platform:** Linux / any POSIX-compliant system (requires `fork()` and
  `pipe()`, so it will not build with MSVC or plain MinGW on Windows — use
  WSL, Linux, or macOS)

## Building and running

```bash
gcc -Wall -Wextra -std=c11 -o ipc_race src/main.c
./ipc_race
```

Example session:

```
Enter the number of integers (N): 50
I am Child process P2: The winner is child process P2
I am Child process P3: The winner is child process P2
The number of positive integers in each file: 50
The number of prime numbers in File1: 12
The number of abundant numbers in File2: 10
```

Running the program creates (or overwrites) `File1` and `File2` in the
current working directory.

## Notes

- The random number range is `[0, X]`, where `X` is a fixed offset defined
  in `src/main.c` (`RANDOM_RANGE_MAX`).
- The `hw1/` folder preserves the original assignment materials and initial
  submission as-is, for reference.
