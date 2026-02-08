# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

HLL is a Python C extension implementing a 64-bit HyperLogLog algorithm for cardinality estimation of very large datasets. It uses a Murmur64A hash and supports both sparse and dense register representations for memory efficiency.

## Build and Development Commands

```bash
# Install locally for development
make install

# Run tests
python test.py

# Build distributions
make wheel_manylinux  # Recommended: builds in Docker manylinux container
make wheel_musllinux  # For Alpine/musl
make sdist            # Source distribution only

# Check and upload
make check            # Run twine check on dist artifacts
make upload           # Upload to PyPI
make upload_test      # Upload to TestPyPI

# Cleanup
make clean            # Remove build artifacts
```

## Architecture

### Core Components

- **`src/hll.c`** - Main C implementation (~1200 lines). Contains Python C API bindings, all HyperLogLog methods, and dual register storage logic.
- **`src/hll.h`** - Header with forward declarations.
- **`lib/murmur2.c`** - Murmur64A hash function implementation.
- **`lib/murmur2.h`** - Hash function header.

### Register Storage System

The library uses a hybrid sparse/dense approach:

1. **Sparse mode** (default): Stores only non-zero registers in a sorted linked list. Memory efficient for small datasets.
2. **Dense mode**: Stores all registers using 6 bits each. Automatic switch when sparse list exceeds `max_list_size`.
3. **Temporary buffer**: Defers register updates in sparse mode for performance. Configurable via `max_buffer_size`.

### Key API

```python
from HLL import HyperLogLog

hll = HyperLogLog(p=12)  # 2^p registers (default p=12 = 4096 registers)
hll.add('data')          # Add item
hll.cardinality()        # Get estimate (returns int)
hll.merge(other_hll)     # Merge another HyperLogLog (must have same p)
hll.hash('data')         # Get Murmur64A hash value
hll.get_register(i)      # Get register value (0-63)
hll.size()               # Number of registers
```

Constructor parameters: `p`, `seed`, `sparse`, `max_list_size`, `max_buffer_size`

### Algorithm Reference

Uses the 64-bit HyperLogLog algorithm from Ertl (2017) "New Cardinality Estimation Methods for HyperLogLog Sketches" (arXiv:1706.07290).
