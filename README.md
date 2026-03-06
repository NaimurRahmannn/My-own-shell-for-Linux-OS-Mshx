<p align="center">
  <img src="https://img.shields.io/badge/Language-C-00599C?style=for-the-badge&logo=c&logoColor=white" alt="C" />
  <img src="https://img.shields.io/badge/Platform-Linux-FCC624?style=for-the-badge&logo=linux&logoColor=black" alt="Linux" />
  <img src="https://img.shields.io/badge/Build-Make-064F8C?style=for-the-badge&logo=gnu&logoColor=white" alt="Make" />
  <img src="https://img.shields.io/badge/License-MIT-green?style=for-the-badge" alt="License" />
</p>

<h1 align="center">🐚 MshX</h1>

<p align="center">
  <b>A lightweight, feature-rich Unix shell written in C from scratch.</b><br/>
  <i>Tokenizer → Parser → AST → Executor — the full pipeline, handcrafted.</i>
</p>

<p align="center">
  <a href="#-features">Features</a> •
  <a href="#-quick-start">Quick Start</a> •
  <a href="#-built-in-commands">Built-ins</a> •
  <a href="#-architecture">Architecture</a> •
  <a href="#-usage-examples">Examples</a>
</p>

---

## ✨ Features

| Feature | Description |
|---|---|
| 🔧 **Command Execution** | Run any external program with arguments, just like bash |
| 🔗 **Pipelines** | Chain commands with `\|` — up to 64 stages deep |
| ⚡ **Logical Operators** | `&&` (AND) and `\|\|` (OR) for conditional execution |
| 🔀 **I/O Redirection** | Input `<`, output `>`, and append `>>` redirection |
| 🌐 **Glob Expansion** | Wildcard pattern matching (`*`, `?`, `[...]`) |
| 💾 **Variable Expansion** | Shell variables with `$VAR` syntax and a full symbol table |
| 📜 **Command History** | Circular buffer history with `!!` and `!n` expansion |
| 🏃 **Background Jobs** | Run processes in the background with `&` |
| 🔍 **Dry-Run Mode** | Preview what a command *would* do without executing it |
| ⏱️ **Timeline Profiling** | Trace `fork`, `exec`, `exit`, `pipe`, and `redirect` events with ms-precision timestamps |
| 🏠 **Smart Prompt** | Displays `~/path:$` with home directory shortening |
| ♻️ **Multi-line Input** | Continue commands on the next line with `\` |

---

## 🚀 Quick Start

### Prerequisites

- **GCC** (or any C99 compiler)
- **GNU Make**
- A **Linux / POSIX** environment

### Build

```bash
git clone https://github.com/your-username/MshX.git
cd MshX
make
```

### Run

```bash
./mshX
```

You'll be greeted with the MshX prompt:

```
~_~:$
```

### Clean

```bash
make clean
```

---

## 🛠️ Built-in Commands

### `cd` — Change Directory

```bash
cd              # Go to $HOME
cd /tmp         # Go to /tmp
cd -            # Go to previous directory (OLDPWD)
```

### `history` — Command History

```bash
history         # Show all history with line numbers
!!              # Re-run the last command
!5              # Re-run command number 5
```

> History stores up to **1000** commands in a circular buffer and automatically deduplicates consecutive identical commands.

### `dry` — Dry-Run Mode 🔍

Preview exactly what the shell would do — without any side effects. No `fork`, no `exec`, no file I/O.

```bash
dry ls -l *.c
```

```
EXEC: /usr/bin/ls -l executor.c initsh.c main.c node.c parser.c ...
```

```bash
dry cat input.txt > output.txt
```

```
REDIRECT: output > output.txt
EXEC: /usr/bin/cat input.txt
```

```bash
dry ls | grep .c | wc -l
```

```
PIPE: 3 commands
EXEC: /usr/bin/ls
EXEC: /usr/bin/grep .c
EXEC: /usr/bin/wc -l
```

### `dump` — Dump Symbol Table

```bash
dump            # Print all shell variables and their values
```

### `timeline` — Execution Profiler ⏱️

Prefix any command with `timeline` to trace the kernel-level lifecycle of its execution:

```bash
timeline ls | grep .c
```

```
╔══════════════════════════════════════════════════╗
║              COMMAND TIMELINE                    ║
╠══════════════════════════════════════════════════╣
║  +0.000ms  [PID 12345] FORKED                   ║
║  +0.042ms  [PID 12345] EXECVE                   ║
║  +0.051ms  [PID 12346] FORKED                   ║
║  +0.089ms  [PID 12346] EXECVE                   ║
║  +1.204ms  [PID 12345] EXITED (code=0)          ║
║  +1.302ms  [PID 12346] EXITED (code=0)          ║
╚══════════════════════════════════════════════════╝
```

Tracks: `FORKED` · `EXECVE` · `EXITED` · `SIGNALED` · `STOPPED` · `CONTINUED` · `PIPED` · `REDIRECTED`

---

## 🏗️ Architecture

MshX follows a classic **compiler-style pipeline** architecture:

```
┌─────────┐    ┌───────────┐    ┌────────┐    ┌──────────┐
│  Input   │───▶│  Scanner  │───▶│ Parser │───▶│ Executor │
│ (prompt) │    │(tokenizer)│    │  (AST) │    │(fork/exec)│
└─────────┘    └───────────┘    └────────┘    └──────────┘
                                                   │
                                        ┌──────────┴──────────┐
                                        │                     │
                                   ┌────▼────┐         ┌──────▼──────┐
                                   │ Builtins│         │  External   │
                                   │ (cd,dry)│         │  Commands   │
                                   └─────────┘         └─────────────┘
```

### Source Tree

```
MshX/
├── main.c             # REPL loop, read-parse-execute cycle
├── prompt.c           # Smart prompt with ~ substitution
├── scanner.c          # Tokenizer / lexical analyzer
├── parser.c           # Recursive-descent parser → AST
├── node.c             # AST node creation & management
├── executor.c         # Command execution engine (fork/exec/pipe/redirect)
├── wordexp.c          # Word expansion ($VAR, globbing, splitting)
├── pattern.c          # Glob pattern matching
├── strings.c          # String utility functions
├── shunt.c            # Operator precedence parsing
├── source.c           # Input source abstraction
├── initsh.c           # Shell initialization
├── Makefile           # Build system
│
├── builtins/
│   ├── builtins.c     # Builtin command registry
│   ├── cd.c           # cd — change directory
│   ├── dry.c          # dry — dry-run execution mode
│   ├── dump.c         # dump — symbol table inspector
│   ├── history.c      # history — command history (circular buffer)
│   └── timeline.c     # timeline — execution profiler
│
└── symtab/
    └── symtab.c       # Symbol table (hash-based variable storage)
```

### Key Design Decisions

- **No external dependencies** — pure C with POSIX APIs only
- **Circular buffer history** — O(1) add, constant memory footprint (max 1000 entries)
- **AST-based execution** — commands are parsed into a tree before execution, enabling features like dry-run
- **Dual execution modes** — the same parser/AST drives both real and dry-run execution
- **Pipeline support** — multi-stage pipes implemented with `pipe()` + `fork()` + `dup2()`
- **Signal handling** — proper `SIGINT`/`SIGTSTP` handling; signals reset to default in child processes

---

## 💡 Usage Examples

### Pipes & Redirection

```bash
ls -la | grep ".c" | wc -l
cat file.txt > output.txt
sort < input.txt >> results.txt
```

### Logical Operators

```bash
mkdir test && cd test           # cd only if mkdir succeeds
gcc main.c || echo "Build failed"  # echo only if gcc fails
```

### Background Execution

```bash
sleep 60 &
find / -name "*.log" > logs.txt &
```

### Command Sequences

```bash
echo "Step 1" ; echo "Step 2" ; echo "Step 3"
```

### Variable Expansion

```bash
echo $HOME
echo $PATH
```

### Wildcards / Globbing

```bash
ls *.c
echo src/**/*.h
cat [Mm]akefile
```

---

## 🔧 Build Options

| Command | Description |
|---|---|
| `make` | Build the shell (debug mode with `-g -Wall -Wextra`) |
| `make clean` | Remove all build artifacts |

The binary is produced as `./mshX` in the project root.

---

## 📄 License

This project is open source. Feel free to use, modify, and distribute.

---

<p align="center">
  <b>Built with ❤️ and raw C</b><br/>
  <i>No readline. No ncurses. No shortcuts.</i>
</p>
