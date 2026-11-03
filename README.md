# athrill-arm-common

What Athrill's Arm targets share: the meaning of each Arm instruction as Arm's
pseudo-code gives it, the pseudo-code's helper functions, and the generator of
the instruction decoders from Arm's machine-readable architecture data.

A target, such as
[athrill-target-stm32](https://github.com/hakoniwalab/athrill-target-stm32),
combines these with its decoders, the glue from each encoding to the
pseudo-code, its profile (A or M) and its SoC's devices. See
[docs/design.md](docs/design.md).

## Build and test

The Athrill core (`toppers/athrill`) is expected next to this checkout as
`../athrill`, for its `std_types.h`.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
(cd build && ctest --output-on-failure)
tools/coverage.sh
```

`tools/coverage.sh` (clang) reports the code coverage of the headers by the
tests.

## Licence

MIT (see `LICENSE`). Code taken from
[athrill-target-ARMv7-A](https://github.com/toppers/athrill-target-ARMv7-A)
is used under this licence with the permission of its author.
