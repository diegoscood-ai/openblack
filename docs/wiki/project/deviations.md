# Deliberate deviations taken from raffclar's tree

## Summary

raffclar's tree keeps a list of where his openblack deliberately differs from vanilla (his `docs/deviations.md`). When
one of his lanes is ported here, its deviations are taken by the rule in the repository's CLAUDE.md ("Importing his
newer master"): a presentation-only one is taken as he wrote it; one that changes game logic keeps vanilla's behaviour by
default, with his fix beside it for a later "fix vanilla bugs" switch. Our own chosen differences (🎯) are listed on
[A reconstruction, not a decompilation](reconstruction.md#what-is-deliberately-different--chosen-differences).

## Taken

| His row | What | Kind | Ported with |
|---|---|---|---|
| dev#60 (his numbering at #528) | The library's three buttons drawn in the 4:3 front-end space | Presentation | PR #528, the temple library |
| — | The light column's tint drawn unlit | Presentation | PR #250, light columns |

## Kept as vanilla, his fix beside it

| His row | What | Ported with |
|---|---|---|
| vb#63 | Villagers who hide when a shield falls pop out at once (vanilla's state table); his tree lets them hide 15-30 s | PR #410 |

## Pending

- Every row is unverified against the frozen v1.42 exe until the fidelity audit of the imported code.
