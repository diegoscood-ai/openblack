# hashdiff: which entity moved first

openblack can write a per-turn hash of the game state (`src/Debug/StateHash.cpp`). Two runs of the same scenario that
should behave the same give the same hashes; when a change makes them differ, the hash only says that a part (for
example `transform`) differs from some turn on. `hashdiff.py` compares two traced runs and names the first entity that
changed and what changed in it.

## Install

Python 3.8 or newer, standard library only.

## Making the traces

The state hash is on when `OPENBLACK_STATE_HASH=<dir>/<label>_hash.txt` is set for the game. Set, next to it:

    OPENBLACK_STATE_HASH_ENTITIES=transform,creature     # or "all" for every part

and each turn also writes `<dir>/<label>_hash_entities.txt` (the hash file's name without `.txt`, plus
`_entities.txt`). The `_hash.txt` file is byte-identical with or without it; without `OPENBLACK_STATE_HASH` the second
variable does nothing.

Run the base build and the changed build once each, the same way (a fixed time step and scripted input, so that both
runs are deterministic), then:

    python hashdiff.py <base>_hash_entities.txt <run>_hash_entities.txt [--part transform] [--max 20]

Exit code 0 means the compared parts are identical, 1 that they differ. The output gives the first turn that differs,
the entities only one run has, and per entity the values that changed (old -> new), by turn then entity.

The traced files are for one investigation: do not commit them or keep them as references (with "all" they are large:
every footpath node and every transform, each turn).

## File format

    turn <n>
    <n> <part> <key> <value> <value> ...

`key` is `e<entity id>` (the raw entt id, with its version) for values folded after `Hasher::Entity`, or `-` for a
part that names no entity (the record then holds every value the part folded that turn). Values: `u<decimal>` (U32),
`x<hex>` (U64), `f<8 hex bits>/<shortest decimal>` (Float), `b<hex>` (raw bytes). A turn number going back (a land
load in a run that changes lands) starts a new segment.

hashdiff names the fields of `transform` (pos, rot<column><row>, scale) and `creature`; other parts show `#<index>`.

## How it stays in step with the hash

The trace is written by the same `Hasher` calls that make the hash (a `Hasher` built with a `Trace*`): there is no
second extraction that could drift. `test/test_state_hash_trace.cpp` checks that the hash is the same traced and not.

## Self-test

    python hashdiff.py --self-test

## Limits

- Entities are named by id only, not by archetype or type.
- Parts whose owners do not call `Hasher::Entity` per entity (`pools`, `footpaths`, `town_owner_list`,
  `wonder_power`, `player_alignment`, `random`, `crt`, `clock`) are traced as one `-` record per turn.
- Field names are known only for `transform` and `creature`.
