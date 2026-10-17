# One-shot and special features

The special objects the lands hide: singing stones, weeping stones, idols, fireflies and the like, most tied to a
challenge or giving a reward once. The miracle bubbles and dispensers are covered in `../miracles/`.

**Progress: 2/8 done, 0 partial — 25%**

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land scripts place singing stones and their base | done | `MobileStaticArchetype` places them (`src/ECS/Archetypes/MobileStaticArchetype.cpp`; the base also through CHL CREATE in `src/CHLApi.cpp`) |
| Singing stones sing when lit or set right, with their own music (unconfirmed exact rule) | todo | Nothing sets them off in our tree; see ../story/ and the quests: [the_singing_stones.md](../story/silver_scrolls/the_singing_stones.md), [the_singing_stones_land_2.md](../story/silver_scrolls/the_singing_stones_land_2.md), [the_miracle_stones.md](../story/silver_scrolls/the_miracle_stones.md) |
| The weeping stone and its reward | todo | Placed as scenery only; see ../story/ |
| Idols and their rewards | todo | Placed as scenery only; see ../story/ and [the_idol.md](../story/silver_scrolls/the_idol.md) |
| Fireflies come out at nightfall to hover by houses and street lights and hide in trees and rocks by day; lifting a tree or rock one hides in gives a one-shot miracle by the land's odds | done | `src/ECS/FireFlies.cpp` and `src/Worship/FireFlyReward.cpp`; placing them from the land script is still a stub; owned by [fireflies.md](fireflies.md) |
| The standalone altar | todo | Placed as scenery only |
| The meteor | todo | Not in our tree |
| The creature cage | todo | see ../story/ |

Toys and how the creature plays with them are in [toys.md](toys.md).
