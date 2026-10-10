# Home and pen

A creature has a home: the pen by its player's citadel (temple), where it is kept while young, where it is carried
when it passes out, and which it goes back to when it wants to rest or feels safe. The game also has a creature building
a home of its own and bringing things back to it.

**Progress: 8/36 done, 8 partial — 33%**

How the original does it, in our wiki: [The creature: groundwork, random streams and what is unknown](../../bw1-notes/creature.md).

## The pen at the citadel

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each citadel has a pen for its player's creature, made by the land's script | partial | the pen is the temple mesh's pen point, kept as the creature's home each turn (`player_creature::FollowTemplePens`, `TemplePenPoint` in `src/ECS/PlayerCreature.cpp`; tests `PlayerCreature.ATemplesPenPointTurnsAndMovesWithIt`, `PlayerCreatureNativesTest.TheHomeFollowsTheTemplesPenOnTheGround`); the land script's `CREATE_CREATURE_PEN` does nothing (`FeatureScriptCommands::CreateCreaturePen`) |
| A planned pen is shown as a plan before the citadel is built | todo |  |
| The pen's size and look come from the game's pen tables | todo |  |
| The creature's home is where a script sets it | done | `SET_CREATURE_HOME` writes the leash's home (`player_creature::SetHome`, `src/CHLApi.cpp`); a built temple's pen overrides it each turn, as in the original; tests `PlayerCreatureNativesTest.SetCreatureHomeHousesOnlyACreature`, `TheLeashSystemKeepsTheHomeItIsGiven` |
| With no temple, the creature's home is where it stands (as on the testbed) | done | `LeashSystem::ConfineToHome` and `creature_mode::PenOf` fall back to where it stands; test `CreatureMode.PenIsTheHomeThenTheTempleThenTheFallback` |
| Scripts can put the creature inside the temple or bring it out | todo | `SET_CREATURE_IN_TEMPLE` is a stub in `src/CHLApi.cpp` |
| The creature shrinks to fit as it nears the citadel, and grows back as it leaves | done | `player_creature::ShrinkInPens` each turn (`BetweenPenWalls`, `PenDrawnSize`) sets the drawn scale; tests `PlayerCreature.InThePenACreatureIsDrawnDownToANewbornsSize`, `OutsideThePenACreatureIsDrawnAtItsOwnSize`, `Land1sHomeIsBetweenThePensWalls`; see [growth_and_size.md](growth_and_size.md) |
| Enter the citadel | todo |  |
| Pray at the citadel | todo |  |
| A creature without a citadel behaves as if it had no home | partial | `LeashSystem::FreeOfHome` needs the player to have a temple, but only the debug spawner asks it; other effects todo |

## Kept at home

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| While it starts to grow up the creature is kept within a short distance of its home | partial | `LeashSystem::ConfineToHome` (within 12, `k_HomeConfinement`; test `LeashRules.Confinement`) exists, but only the debug spawner calls it: `DEV_FUNCTION` 1 (start development scripts) is not ported |
| Straying out of the area it is kept in, it walks back | done | `LeashSystem::ProcessTurn` walks it back to the confinement's centre (once something confines it) |
| On a working leash it may go further than the area | done | `creature_leash::IsConfined`; test `LeashRules.Confinement` |
| It is free to roam only within reach of home while its player has a temple | partial | `creature_leash::FreeOfHome` (test `LeashRules.Confinement`); `LeashSystem::FreeOfHome` is read only by the debug spawner |
| Once mature enough it may leave home | partial | `CREATURE_IN_DEV_SCRIPT` and `SET_CREATURE_DEV_STAGE` work (`ECS/PlayerCreature.cpp`); nothing confines it or frees it by stage ([development_phases.md](development_phases.md)) |
| Its plans weigh being too far from home, or on the leash | todo | see [decision_making.md](decision_making.md) |
| Scripts can confine a creature to an area | todo | no script native confines a creature; the confinement exists only for the debug spawner |
| Tying the leash to a house teaches it to stay there | partial | see [leash.md](leash.md) |

## Going home

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Hang around at home: walks somewhere nearby and sits | partial | plan action `HangAroundAtHome` walks somewhere near where it is, not near home (`creature_mind::HangAround` in `src/Creature/CreatureIdleMind.cpp`); test `CreatureIdleMind.HangingAroundWalksSomewhereNearbyThenSits` |
| Go home | todo | the wish to go home is a desire with no action ([desires.md](desires.md)) |
| Run home when frightened | todo |  |
| Go home to recover when hurt | todo |  |
| Rest at home | todo |  |
| Nothing scary near home makes home a safe place to go | todo |  |
| Is it near home, is it not | partial | distance to home is measured for roaming (`LeashSystem::FreeOfHome`) and for the pen's drawn size (`ShrinkInPens`); not used by plans |

## Carried home

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature that passes out (from damage, hunger or tiredness) is carried to its pen to come round | done | `CreatureMindSystem::ProcessTurn` knocks out a fainting creature (`physiology::ShouldFaint`) through `CreatureFightSystem::KnockOut`, which carries it to `HomeOf` (the temple's pen point); test `CreatureMode.PassesOutWhenAStatusReachesAHundredPercent` |
| A creature knocked out in a fight lies out cold, then fades out and in at home | done | `CreatureFightSystem::KnockOut`, `HomeOf`, the `CreatureKnockedOut` stages (lying, fading out, fading in, waiting); test `CreatureFightSystemTest.KnockedOutItIsTakenHomeAtOnce` |
| It rests at home until healthy enough, then gets up | done | `CreatureKnockedOut` resting and getting up (`CreatureFightSystem`) |
| The player is told their creature was carried home | todo | see [lessons_and_help.md](lessons_and_help.md) |
| Scripts can call the player's creature home | todo | `CALL_PLAYER_CREATURE` only hands the creature to the script (`src/CHLApi.cpp`); nothing calls it home |

## Its own home (unconfirmed whether used in the five lands)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature wants to build a home of its own | todo | the desire exists with nothing to act on ([desires.md](desires.md)) |
| It creates the home where it chooses | todo |  |
| It builds it from rocks, and is told when more rocks are needed | todo |  |
| It knows whether its home is built, being built, or missing | todo |  |
| Bring things home, bring food home, take food or fish home, take a toy home | todo |  |
| Go out to look for food and bring it back | todo |  |
