# Influence

The area where a player's hand can act and their miracles can be cast: round their temple and round each town that
believes in them, shown by a glowing border on the land. Khazar teaches it on the second land:
[Impress Village](../story/gold_scrolls/impress_village.md#khazars-lesson-on-influence).

**Progress: 23/27 done, 0 chosen, 2 partial, 0 inferred — 91%**

How the original does it, in our wiki: [Magic: the core of the miracles](../../../magic/influence-and-alignment.md).

## How far it reaches

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The temple's reach is set per land of the story, fixed the first time, times the land's player multiplier | done | `influence::CitadelInfluenceAt` (`src/ECS/Influence/InfluenceSources.cpp`): the heart's story influence, set once, times the player multiplier; test `InfluenceTest.citadelStoryInfluence` |
| A player's whole influence (their temple's, their towns' and their rings') is worked out as soon as the land's script has run, so the land's first turn already knows it | done | `worship::citadel::PostLoadCleanup` (`src/Worship/Citadel.cpp`), from `Game::LoadMap`, for every player with a temple: their towns without a worship site join it, then `influence::NoteInfluencePowerAfterLoad` (`src/ECS/Influence/InfluenceSources.cpp`); test `InfluenceTest.theLandsScriptLeavesThePowerOfEveryPlayerWithACitadel` ([page](../../../magic/influence-and-alignment.md#more-addresses-from-the-code)) |
| A player's whole influence lasts from one land to the next until it is worked out again, so a player without a temple on a new land is first seen with what they had on the last | done | the player service keeps it (`PlayerSystemInterface::InfluencePower`, `src/ECS/Systems/Implementations/PlayerSystem.cpp`); test `InfluenceTest.thePowerLastsUntilItIsWorkedOutAgainOnAnotherLand` |
| A town's reach is a base for the land plus each building's share by its size and its people, times the land's multiplier | done | `influence::ProcessTowns` (`src/ECS/Influence/InfluenceSources.cpp`): the land's base plus each abode's influence by its build, scale, life and people, times the town multiplier; test `InfluenceTest.townRadiusAndPlayer` |
| Only a player's own towns give them influence; a neutral town counts for the neutral player, which gets no border | done | `influence::CalculatePlayerRawInfluence` and `TownsInfluenceAt` (`src/ECS/Influence/Influence.cpp`) count the towns whose owner is the player, the neutral player's too ([page](../../../magic/influence-and-alignment.md#influence)); test `InfluenceTest.townRadiusAndPlayer` |
| Influence rings fall away to nothing at their edge; the temple and the towns give all or nothing inside their reach | done | rings fall off by `CalculateInfluenceOnRange` (test `Influence.rangeGradient`); the citadel and towns give full influence inside their radius, as the original ([page](../../../magic/influence-and-alignment.md#influence)) |
| A town's reach grows as it builds and fills, and shrinks as it loses buildings | done | the radius is worked out every turn from the town's abodes (`influence::ProcessTowns`), and towns build (`src/ECS/Systems/Implementations/VillagerBuildingSites.cpp`); see `../town/` |
| Land scripts set influence multipliers for one town, all towns and the players | done | `SET_A_TOWNS_INFLUENCE_MULTIPLIER`, `SET_TOWN_INFLUENCE_MULTIPLIER`, `SET_PLAYER_INFLUENCE_MULTIPLIER` in `src/LHScriptX/FeatureScriptCommands.cpp` |
| Under another player's shield a player has no influence | done | a shield makes an anti ring for every other player (`src/Magic/Spells/SpellShield.cpp`), and an anti ring gives that player none (`influence::CalculatePlayerRawInfluence`) |
| Scripts make influence rings round places and objects, which follow the objects they are on | done | `CREATE_INFLUENCE_RING` (`src/LHScriptX/FeatureScriptCommands.cpp`), `INFLUENCE_OBJECT` and `INFLUENCE_POSITION` (`src/Magic/Script/CHLInfluence.cpp`); a ring follows its object and goes with it (`influence::ProcessRings`); test `InfluenceTest.ringFollowsItsObject` |
| Anti-influence rings shut a player out of an area | done | `influence::CalculatePlayerRawInfluence`, `IsInAntiInfluence`; test `InfluenceTest.ringsAndAntiRings` |
| The land's alignment at a place is every player's influence there times their alignment; the sky takes the most influential player's | done | `alignment::LandAlignmentAt` (`src/ECS/Effects/Alignment.cpp`) for fields and trees; `alignment::MostInfluentialPlayer`, the first player whose influence beats the ones before (the original's 0x5CD630); tests in `test/test_storm.cpp` and `test/test_fields.cpp` ([page](../../../magic/influence-and-alignment.md#the-sky-alignment)) |
| Scripts read a player's influence at a place | partial 90% | `GET_INFLUENCE` (`src/Magic/Script/CHLInfluence.cpp`); no test pins the native |
| Past the border the hand keeps a waning share of its player's influence (virtual influence), which scripts switch on and off | partial 60% | the strength, its waning and its grant (`src/Common/VirtualInfluence.*`, asked by `influence::CalculatePlayerInfluence`) and `SET_VIRTUAL_INFLUENCE` in `src/CHLApi.cpp`; tests `VirtualInfluence.*`, `VirtualInfluenceGrant.*`; the worship sites' spare chants it reads and uses are passed as 0, and its hum (sound 129) and the motes of light to the hand are not heard or drawn ([page](../../../magic/influence-and-alignment.md#virtual-influence), [../rendering/light_beams.md](../rendering/light_beams.md)) |
| Allies share influence | done | Land 2 allies the player with Khazar; outside their own influence a player has the first ally's above a strength of 0.1 (`influence::CalculatePlayerInfluence`, `ecs::alliances::AlliesInfluence`); test `InfluenceTest.outsideTheirInfluenceAPlayerHasTheirAllysWhenTheAllianceIsAboveATenth` |

## The border

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A curtain of light round the temple and each town, in the player's colour, following the land | done | `src/ECS/Influence/InfluenceCircles.cpp`, `src/Graphics/RendererInfluence.cpp` (`land_morph::InfluenceCurtain`) |
| It is drawn again every ten turns, and only once a reach has moved | done | `influence::Update3DInfluence` (turn % 10 and a radius moved); test `InfluenceTest.circlesRebuildEveryTenTurns` |
| Circles inside others and the insides of overlapping ones are hidden | done | tests `InfluenceTest.circlesOverlapOfOnePlayer`, `circlesHideTheirContact` |
| It shows only from high enough above the land | done | the curtain's alpha by the camera's height (`influence::SetCurtainAlpha`); test `InfluenceTest.curtainAlphaAndLatch` |
| Its texture scrolls with game time | done | `frame_anim::InfluenceScroll`; `test/test_frame_anim.cpp` |
| A player's border shows once their temple stands | done | the player's border latch (`influence::ProcessCitadels`), set as soon as the player has a temple (openblack has no temple fade) |
| Each land remaps the players' border colours | pending | the per-land colour remap (`GetRemapedPlayer` 0x64D790) is not ported; openblack takes the identity remap ([page](../../../magic/influence-and-alignment.md#pending)) |
| The border also shows in the temple's view of the world | pending | the temple's world room border is not ported ([page](../../../magic/influence-and-alignment.md#pending)) |
| The hand crossing a border sends out a ripple and makes a sound | done | `influence::ProcessHandCrossing`: the ripple and sound 52; test `InfluenceTest.handCrossingMakesARipple` |

## What it allows

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Miracles land only in the caster's influence; a press outside does nothing | done | the hand's in-influence flag (`src/ECS/Systems/Implementations/HandSpellSeed.cpp`) and the cast rule (`src/Magic/CastRules.cpp`) |
| Shaking a miracle off the hand needs influence (except while powering up) | done | the scribble shakes off only in influence (`src/Magic/Gestures/PowerUpSystem.cpp`) |
| The hand may pick up and use things only inside influence | done | taps, locked selects and pick-ups need it (`HandSystem::InInfluence`, `src/ECS/Systems/Implementations/HandSystem.cpp`) |
