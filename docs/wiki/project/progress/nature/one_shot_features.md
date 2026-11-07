# One-shot and special features

The special objects the lands hide: singing stones, weeping stones, idols, fireflies and the like, most tied to a
challenge or giving a reward once. The miracle bubbles and dispensers are covered in `../miracles/`.

**Progress: 2/7 done, 0 chosen, 1 partial, 0 inferred — 37%**

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land scripts place singing stones and their base | done | `MobileStaticArchetype` places them as any mobile static (`src/ECS/Archetypes/MobileStaticArchetype.cpp`; the base also through CHL CREATE in `src/CHLApi.cpp`); tests `LandScriptObjectCommandsTest.CreateMobileStaticTakesItsYAngleAndScaleAndMakesABonfireForTheBonfireInfo`, `LandScriptObjectCommandsTest.CreateMobileStaticWithAnglesIsRaisedAndTurnedButMakesNoLantern` (the placement, for any row), `ParticleCarriedObjectsTest.AScriptsNewMobileStaticIsARockByItsRowsKind` (a script's CREATE) |
| Singing stones sing when lit or set right, with their own music | done | The singing stones are the quests' own scripts, on ported natives: Land 1's circle plays each holder's stone note with a sparkle (`PLAY_SOUND_EFFECT` and `SPECIAL_EFFECT_OBJECT(10, ...)` of task PlaySingingCircleNote, refs/story/evidence/l1op_ogre.log lines 4713-4715), and a tapped stone plays its own note (task CheckSingingCircleClicked in the user's natural play, refs/story/played/u_20261106_141006.log line 1064254); Land 2's stones play their day and night notes when tapped (its finding-the-dead native is ported since). The rules are the quests' rows: [the_singing_stones.md](../story/silver_scrolls/the_singing_stones.md), [the_singing_stones_land_2.md](../story/silver_scrolls/the_singing_stones_land_2.md), [the_miracle_stones.md](../../not-in-the-original.md#storysilver_scrollsthe_miracle_stones) |
| The weeping stone and its reward | pending | Placed as scenery only; see ../story/ |
| Idols and their rewards | partial 60% | The Idol is a quest's own script: its reward, the fireball's first and second power levels at the town, is done ([the_idol.md](../story/silver_scrolls/the_idol.md) "The town is given the fireball miracle's first and second power levels"), and its start rows are done from the user's natural play; missing: the rest of that quest's rows, at its own page |
| The standalone altar | pending | Placed as scenery only |
| The meteor | pending | The meteor is a mobile static row (`MobileStaticInfo::Meteor`, `src/Enums.h`), placed as any mobile static (`MobileStaticArchetype`); a challenge script's meteor falls from the script's height ([rocks_and_features.md](rocks_and_features.md), test `ParticleCarriedObjectsTest.AScriptsNewRockMadeAsAMobileStaticGoesIntoThePhysicsWhereItIs`); what more Land 4's meteors do in their quest is not checked |
| The creature cage | pending | see ../story/ |

Toys and how the creature plays with them are in [toys.md](toys.md); the fireflies, which hide in trees and rocks and
give a one-shot miracle when caught, are in [fireflies.md](fireflies.md).
