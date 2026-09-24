MCPE terrain verifier

WIP, not finished, and not guaranteed to support all versions pre 1.18 (to account for structure changes etc)

It verifies or filters the 32-bit Pocket Edition/Bedrock Edition pre 1.18 world-seed space using only
observed, seed-generated terrain.  Each observation is air or non-air; the
tool never tries to match a terrain shape approximately.

## Included terrain

| version | dimension | included |
| --- | --- | --- |
| PE 0.6.1 | Overworld | finite-world density terrain and biome surface replacement |
| PE 0.9.0 | Overworld | density terrain, biome surfaces, and the native cave-carver pass |
| PE 1.1.5.0 | End | base islands and the End-stone outer-island feature |
| PE 1.1.5.0 | Nether | base terrain and the native Nether cave-carver pass; optional canonical population pass |

Population is not run by default. Ores, trees, plants, loot, mobs,
structures, pillars, End cities, gateways, chorus, portal/podium geometry,
and player edits are excluded from the default base-terrain predicate. The
one exception is PE 1.1.5.0's small outer End-island feature: it writes End
stone, so omitting it would make the End terrain predicate wrong. Its chance,
position, and shape use the per-chunk MT19937 stream directly; the End biome
decorator does not run the ordinary ore pass first.

For PE 1.1.5.0 Nether observations that deliberately include population
writes, use `--nether-stage populated`. It runs the source's fortress, lava,
fire, glowstone, mushroom, quartz, and magma passes for every observed chunk
and its immediate source-chunk halo in stable `(chunk X, chunk Z)` order. The
halo is required because decorators and flowing lava cross chunk edges. A
particular saved game's chunk request history is runtime state rather than a
seed property, so this mode is reproducible but does not pretend its order is
the original client's loading order. It is substantially slower and
unsuitable as the first pass of a large exhaustive scan.

`1` means any non-air block from the included terrain stage.  This includes
seed-generated water and lava in the Overworld/Nether.  `0` means literal air.
Do not record blocks altered by excluded population or runtime systems.

## Build

The release executable is statically linked and has no game-file, companion
DLL, network, or third-party dependency. This checkout compiles only the
needed observable terrain translation units from the adjacent
`../mcpe-worldgen` source tree directly into the executable; unrelated loot,
village, mineshaft, and monster-room implementations are not part of the
verifier target. `MCPE_WORLDGEN_SOURCE_DIR` may select the same source tree at
a different location.

The PE 0.6.1 verifier uses a terrain-only backend, so its population, falling
blocks, plants, drops, mobs, ores, trees, springs, and `Level` implementation
are not compiled. PE 0.9.0 Overworld dispatches directly to its terrain
generator rather than constructing the stateful `World`. End dispatch omits
pillars, portals, chorus, gateways, and chunk-population caches. The remaining
`World`, feature, and fortress sources in a full build serve only the optional
PE 1.1.5.0 `--dimension nether --nether-stage populated` mode.

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

## Observation files

Each non-comment line is:

```text
x y z value
```

Coordinates are normal world block coordinates.  `y` must be in `0..127`.
Commas may replace spaces.  Blank lines and `#` comments are accepted.
Duplicate lines are accepted when they agree; conflicting duplicates are an
error.  PE 0.6.1 observations must lie in its finite `x/z = 0..255` world.

Examples are in [`examples`](examples).

## Use

Verify one known seed:

```powershell
.\build\Release\mcpe_terrain_verifier.exe `
  --verify 0.9.0 12345 .\examples\overworld-observations.txt
```

Filter candidate seeds.  Candidate files accept one signed/unsigned decimal
or `0x` 32-bit seed per line, including `unsigned=...` output produced by this
tool or the PE 1.1.5 pillar cracker:

```powershell
.\build\Release\mcpe_terrain_verifier.exe `
  --filter 1.1.5.0 .\candidates.txt .\examples\end-observations.txt `
  --dimension end --threads 8
```

Scan a bounded seed range:

```powershell
.\build\Release\mcpe_terrain_verifier.exe `
  --scan 0.6.1 0 100000 .\examples\overworld-observations.txt `
  --threads 8
```

Range scans and full `--all` scans show a live percentage and exact
`completed/total` counter. The display updates four times per second and
finishes at `100.00%`.

The scanner compiles immutable observations into exact early-rejection checks
before starting its workers. PE 0.6.1 can test the first native surface column
without materializing a whole chunk; PE 0.9.0 uses cave geometry as a necessary
precondition for below-sea-level air; and PE 1.1.5.0 End evaluates a rare solid
point before generating all observed chunks. Every survivor still runs through
the complete terrain backend, so these checks change performance rather than
the accepted seed set.

For PE 1.1.5.0 choose the dimension explicitly:

```text
--dimension end
--dimension nether
```

For the Nether's canonical populated stage:

```text
--dimension nether --nether-stage populated
```

For PE 0.9.0, the default is the Infinite generator.  Use
`--pe090-source legacy` for a save whose persisted generator selector is
Legacy; it is a distinct random-terrain/biome configuration, not a substitute
PRNG or an approximate compatibility mode.

Every filter or scan prints survivors and also saves them under
`results\terrain-VERSION-YYMMDD-HHMMSS.txt`.  A collision adds a numeric
suffix rather than replacing an older result.  `--output PATH` overrides that
location.

`--all VERSION OBSERVATIONS.txt` is available for an exact full 2^32 scan,
but is deliberately explicit: terrain generation is substantially more work
per seed than a pillar-only test and an exhaustive scan can take a long time.

## Exactness boundary

This is a version-pinned observable reimplementation, not a heuristic and
not a Java Edition generator.  It preserves the selected version's PRNG,
noise construction/order, chunk coordinates, surface/cave order, and strict
floating-point path used by the terrain core.  An observation can only prove
or reject a seed relative to the included terrain stage; it cannot validate
trees, structures, population, player edits, or runtime changes that are
explicitly outside that stage.
