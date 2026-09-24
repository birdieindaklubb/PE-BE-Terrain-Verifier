# PE-BE-Worldgen-Lib terrain snapshot

This directory is the minimal source closure needed to build the standalone
terrain verifier. It is intentionally not a second general-purpose copy of
the world-generation library.

The verifier includes these files directly so a clean clone builds without a
network connection or a sibling checkout. The root CMake option
`MCPE_WORLDGEN_SOURCE_DIR` can instead point at a complete library checkout.
When `MCPE_WORLDGEN_REFERENCE_DIR` is set, the
`mcpe_worldgen_snapshot_parity` target compares every path listed in
`SNAPSHOT-FILES.txt` byte-for-byte with that checkout.

The snapshot retains the upstream MIT license in `LICENSE`.
