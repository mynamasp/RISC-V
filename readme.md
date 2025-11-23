# RISC-V Pipeline Simulator

5-stage pipelined RISC-V processor simulator. Handles data and control hazards.

## Quick Start

```bash
# Compile
g++ -std=c++11 -o simulator main.cpp

# Run
./simulator
```

## Requirements

- g++ with C++11
- Linux/macOS/Windows

## Input Format

Machine code in hex format (one instruction per line):

```
00500093    # addi x1, x0, 5
00A00113    # addi x2, x0, 10
002081B3    # add x3, x1, x2
```

## Usage

1. Run `./simulator`
2. Enter filename (e.g., `test.hex`)
3. Choose mode:
   - Mode 1: Step by instruction
   - Mode 2: Step by cycle
4. Enter number of steps
5. Use menu:
   - `c` - continue
   - `v` - view pipeline
   - `m` - view memory
   - `s` - statistics
   - `q` - quit

## Instructions Supported

**Arithmetic:** add, sub, addi, subi, mul, div, rem  
**Logical:** and, or, andi, ori, sll, srl  
**Comparison:** slti, sltiu  
**Memory:** lw, sw  
**Control:** beq, jal, jalr, lui

## Test Files Included

- `test.hex` - basic instructions
- `fibonacci.hex` - Fibonacci sequence
- `gcd.hex` - GCD algorithm
- `binary_search.hex` - binary search
