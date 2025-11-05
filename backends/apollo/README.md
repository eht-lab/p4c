<!--!
\page apollo_backend Apollo Backend
-->
<!--
Documentation Inclusion:
This README is integrated as a standalone page in the P4 compiler documentation.

Refer to the full page here: https://p4lang.github.io/p4c/apollo_backend.html
-->
<!--!
\internal
-->
# Apollo Backend
<!--!
\endinternal
-->

<!--!
[TOC]
-->
This is a back-end which generates code for the Apollo multi-architecture platform.

The Apollo backend supports multiple P4 architectures:
- **Tuna NIC**: Network Interface Card architecture for high-performance packet processing

# Dependencies

To run and test this back-end you need:

- The Python scapy library: `sudo pip3 install scapy`
- Standard C++17 compiler (GCC 9.1+ or Clang 6.0+)
- CMake 3.16.3 or higher
- P4C core dependencies

# Architecture Support


## Tuna NIC Architecture
The Tuna architecture targets Network Interface Card deployments with optimizations for packet processing efficiency.

**Output Binary**: `p4c-apollo-tuna`

### Usage:
```bash
p4c-apollo-tuna program.p4 -o program.json
```

### Features:
- Ingress and egress packet processing pipelines
- Built-in packet recirculation support
- Optimized metadata handling
- Support for Tuna-specific extern functions

### Compatible with:
- P4_16 programs written for `tuna.p4` architecture

# Unified Driver Support

The Apollo backend integrates with the P4C unified driver:

```bash
# Compile for Tuna NIC architecture
p4c --target apollo --arch tuna program.p4
```


# Testing

## Basic Compilation Tests

```bash
# Test Tuna compilation
p4c-apollo-tuna testdata/tuna_basic.p4 -o tuna_test.json

# Test unified driver
p4c --target apollo --arch tuna testdata/tuna_basic.p4
```

## Build Configuration

Apollo is integrated into the P4C build system:

```bash
mkdir build && cd build
cmake .. -DENABLE_APOLLO=ON
make -j$(nproc)
```

# Contributing

Contributions to Apollo are welcome! Please:

1. Follow existing code style and patterns
2. Add appropriate unit tests for new features
3. Update documentation as needed
4. Ensure compatibility with existing architectures
5. Test with Tuna architectures

# License

Apollo Backend is licensed under the Apache License 2.0. See the LICENSE file for full details.

## Copyright Notice

- **Apollo-specific components**: Copyright 2025-2026 EHTech
