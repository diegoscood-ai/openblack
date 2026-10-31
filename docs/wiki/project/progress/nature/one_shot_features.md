# One-shot and special features

The special objects the lands hide: singing stones, weeping stones, idols, fireflies and the like, most tied to a
challenge or giving a reward once. The miracle bubbles and dispensers are covered in `../miracles/`.

**Progress: 1/7 done, 0 chosen, 0 partial, 0 inferred — 14%**

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land scripts place singing stones and their base | done | `MobileStaticArchetype` places them as any mobile static (`src/ECS/Archetypes/MobileStaticArchetype.cpp`; the base also through CHL CREATE in `src/CHLApi.cpp`); tests `LandScriptObjectCommandsTest.CreateMobileStaticTakesItsYAngleAndScaleAndMakesABonfireForTheBonfireInfo`, `LandScriptObjectCommandsTest.CreateMobileStaticWithAnglesIsRaisedAndTurnedButMakesNoLantern` (the placement, for any row), `ParticleCarriedObjectsTest.AScriptsNewMobileStaticIsARockByItsRowsKind` (a script's CREATE) |
| Singing stones sing when lit or set right, with their own music (unconfirmed exact rule) | pending | The singing stones are the quests' own scripts: Land 1's start in the original's order (test `ChallengeVmWorld.ANewGameStartsTheStoryAndLand1sFirstScriptsInTheOriginalsOrder`, `CheckStonePositionsAndPlayNotes` among them), but no run shows them playing their notes, and Land 2's rely on a stub native (GET_DEAD_LIVING); see ../story/ and the quests: [the_singing_stones.md](../story/silver_scrolls/the_singing_stones.md), [the_singing_stones_land_2.md](../story/silver_scrolls/the_singing_stones_land_2.md), [the_miracle_stones.md](../../not-in-the-original.md#storysilver_scrollsthe_miracle_stones) |
| The weeping stone and its reward | pending | Placed as scenery only; see ../story/ |
| Idols and their rewards | pending | Placed as scenery only; see ../story/ and [the_idol.md](../story/silver_scrolls/the_idol.md) |
| The standalone altar | pending | Placed as scenery only |
| The meteor | pending | The meteor is a mobile static row (`MobileStaticInfo::Meteor`, `src/Enums.h`), placed as any mobile static (`MobileStaticArchetype`); a challenge script's meteor falls from the script's height ([rocks_and_features.md](rocks_and_features.md), test `ParticleCarriedObjectsTest.AScriptsNewRockMadeAsAMobileStaticGoesIntoThePhysicsWhereItIs`); what more Land 4's meteors do in their quest is not checked |
| The creature cage | pending | see ../story/ |

Toys and how the creature plays with them are in [toys.md](toys.md); the fireflies, which hide in trees and rocks and
give a one-shot miracle when caught, are in [fireflies.md](fireflies.md).
