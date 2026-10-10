# Influence

The area where a player's hand can act and their miracles can be cast: round their temple and round each town that
believes in them, shown by a glowing border on the land. Khazar teaches it on the second land:
[Impress Village](../story/gold_scrolls/impress_village.md#khazars-lesson-on-influence).

**Progress: 21/22 done, 0 partial — 95%**

How the original does it, in our wiki: [Magic: the core of the miracles](../../bw1-notes/magic.md).

## How far it reaches

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The temple's reach is set per land of the story, fixed the first time, times the land's player multiplier | done | `influence::CitadelInfluenceAt` (`src/ECS/Influence/InfluenceSources.cpp`): the heart's story influence, set once, times the player multiplier; test `InfluenceTest.citadelStoryInfluence` |
| A town's reach is a base for the land plus each building's share by its size and its people, times the land's multiplier | done | `influence::ProcessTowns` (`src/ECS/Influence/InfluenceSources.cpp`): the land's base plus each abode's influence by its build, scale, life and people, times the town multiplier; test `InfluenceTest.townRadiusAndPlayer` |
| Only a player's own towns give them influence; the neutral player has none | done | `influence::CalculatePlayerRawInfluence` (`src/ECS/Influence/Influence.cpp`) counts only the player's own towns. Our wiki differs: a neutral town counts for the neutral player, which only gets no border ([page](../../bw1-notes/magic.md#influence-srcecsinfluence)) |
| Influence is full near the middle and falls away to nothing at the edge | done | rings fall off by `CalculateInfluenceOnRange` (test `Influence.rangeGradient`); the citadel and towns give full influence inside their radius. Our wiki differs: the citadel and towns are all or nothing, only the influence rings fall away towards their edge ([page](../../bw1-notes/magic.md#influence-srcecsinfluence)) |
| A town's reach grows as it builds and fills, and shrinks as it loses buildings | done | the radius is worked out every turn from the town's abodes (`influence::ProcessTowns`), and towns build (`src/ECS/Systems/Implementations/VillagerBuildingSites.cpp`); see `../town/` |
| Land scripts set influence multipliers for one town, all towns and the players | done | `SET_A_TOWNS_INFLUENCE_MULTIPLIER`, `SET_TOWN_INFLUENCE_MULTIPLIER`, `SET_PLAYER_INFLUENCE_MULTIPLIER` in `src/LHScriptX/FeatureScriptCommands.cpp` |
| Under another player's shield a player has no influence | done | a shield makes an anti ring for every other player (`src/Magic/Spells/SpellShield.cpp`), and an anti ring gives that player none (`influence::CalculatePlayerRawInfluence`) |
| Scripts make influence rings round places and objects, which follow the objects they are on | done | `CREATE_INFLUENCE_RING` (`src/LHScriptX/FeatureScriptCommands.cpp`), `INFLUENCE_OBJECT` and `INFLUENCE_POSITION` (`src/Magic/Script/CHLInfluence.cpp`); a ring follows its object and goes with it (`influence::ProcessRings`); test `InfluenceTest.ringFollowsItsObject` |
| Anti-influence rings shut a player out of an area | done | `influence::CalculatePlayerRawInfluence`, `IsInAntiInfluence`; test `InfluenceTest.ringsAndAntiRings` |
| The most influential player at a place decides the land's alignment there | done | the land's alignment sums each player's influence times alignment (`alignment::LandAlignmentAt`, `src/ECS/Effects/Alignment.cpp`) for fields and trees; the sky takes the most influential player's (`alignment::MostInfluentialPlayer`). Our wiki differs: the land's alignment is that weighted sum over every player, not the most influential player's; only the sky uses the most influential player ([page](../../bw1-notes/miracles.md#forest-magicspellsspellforest-magicobjectsmagictree-ecstrees)) |
| Scripts read a player's influence at a place | done | `GET_INFLUENCE` (`src/Magic/Script/CHLInfluence.cpp`) |
| Prayer power can buy influence elsewhere (virtual influence; unconfirmed what it costs) | todo | `SET_VIRTUAL_INFLUENCE` is a stub in `src/CHLApi.cpp` and the virtual influence is not read (`influence::CalculatePlayerInfluence`); reaching past the border draws motes of light from it to the hand, see [../rendering/light_beams.md](../rendering/light_beams.md) |
| Allies share influence | n/a | multiplayer only, see `../multiplayer/`; the ally fallback in `influence::CalculatePlayerInfluence` gives 0 |

## The border

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A curtain of light round the temple and each town, in the player's colour, following the land | done | `src/ECS/Influence/InfluenceCircles.cpp`, `src/Graphics/RendererInfluence.cpp` (`land_morph::InfluenceCurtain`) |
| It is drawn again every ten turns, and only once a reach has moved | done | `influence::Update3DInfluence` (turn % 10 and a radius moved); test `InfluenceTest.circlesRebuildEveryTenTurns` |
| Circles inside others and the insides of overlapping ones are hidden | done | tests `InfluenceTest.circlesOverlapOfOnePlayer`, `circlesHideTheirContact` |
| It shows only from high enough above the land | done | the curtain's alpha by the camera's height (`influence::SetCurtainAlpha`); test `InfluenceTest.curtainAlphaAndLatch` |
| Its texture scrolls with game time | done | `frame_anim::InfluenceScroll` |
| A player's border shows once their temple stands | done | the player's border latch (`influence::ProcessCitadels`), set as soon as the player has a temple (openblack has no temple fade) |
| The hand crossing a border sends out a ripple and makes a sound | done | `influence::ProcessHandCrossing`: the ripple and sound 52; test `InfluenceTest.handCrossingMakesARipple` |

## What it allows

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Miracles land only in the caster's influence; a press outside does nothing | done | the hand's in-influence flag (`src/ECS/Systems/Implementations/HandSpellSeed.cpp`) and the cast rule (`src/Magic/CastRules.cpp`) |
| Shaking a miracle off the hand needs influence (except while powering up) | done | the scribble shakes off only in influence (`src/Magic/Gestures/PowerUpSystem.cpp`) |
| The hand may pick up and use things only inside influence | done | taps, locked selects and pick-ups need it (`HandSystem::InInfluence`, `src/ECS/Systems/Implementations/HandSystem.cpp`) |
