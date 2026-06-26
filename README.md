A quick msgpack-c wrapper for MATLAB & Octave.

[![build](https://github.com/WTI-BrainWorks/mex-msgpack/actions/workflows/ci.yml/badge.svg)](https://github.com/WTI-BrainWorks/mex-msgpack/actions/workflows/ci.yml)

~two orders of magnitude faster than https://github.com/bastibe/matlab-msgpack

Generally follows jsondecode convention for deserializing (heterogeneous) arrays.

## Building

Needs CMake and a C compiler; the bundled `msgpack-c` submodule is built automatically.

    git clone --recurse-submodules https://github.com/WTI-BrainWorks/mex-msgpack
    matlab -batch build       % or:  octave --eval build

`run_tests` exercises the result. CI (`.github/workflows/ci.yml`) builds and tests the
MEX for MATLAB and Octave on Linux and Windows.

Known issues:

 - Strong assumption that maps have string keys; non-string keys are rejected rather than read unsafely (a scalar map errors, a non-string key inside an array of maps is skipped). We could do containers.Map to be more flexible, but they're more uncommon?
 - Round-trip is not identity: array shape isn't preserved (everything deserializes to row vectors) and numeric types may change (integer arrays come back as double, scalar integers as int64/uint64, single as double).
