# The Creature Cave

The temple's room for the player's creature. The creature stands in it by a fire, and around the cave are scrolls
telling of its attributes, its personality, the actions it has learnt and the miracles it knows, attack dummies hung
with the belts it has won in fights and plinths with medals for its miracles. Clicking the creature opens the tattoo
editor ([creature_tattoos.md](creature_tattoos.md)). The temple itself is in [../temple](../temple/).

**Progress: 17/31 done, 4 partial — 61%**

How the original does it, in our wiki: [The creature: groundwork, random streams and what is unknown](../../bw1-notes/creature.md).

## Getting there and moving about

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| F5 takes the player into the temple's Creature Cave | done | `CreatureCaveSystem::Update` and `Game.cpp` (F5, `ZOOM_TO_CREATURE_ROOM`, goes to the temple's creature room); test `CreatureCaveSystemTest.InTheTempleOpeningGoesToTheCreatureRoomAndClosingLeaves` |
| Without a temple, as on the testbed, F5 shows the cave's screen on its own, and F5 or Escape closes it | done | `CreatureCaveSystem`, its screen drawn by `src/Gui/CreatureCaveScreen.cpp` from the ImGui loop; test `CreatureCaveSystemTest.WithoutATempleF5ShowsTheCaveOnItsOwn` |
| The room's camera zooms to whatever is clicked (a scroll, the dummies, the plinths, the creature) and back out | partial | targets and zoom points in `src/3D/CreatureCaveTargets.*` and `src/Camera/TempleCameraModel.cpp` (tests `CreatureCaveTargets.*`); zooming onto the creature is not done (a TODO in `TempleCameraModel.cpp`) |
| A fourth target's tooltip says it zooms in, but clicking it does nothing | done | `CreatureCaveTargets` (kept as the game has it) |
| The cursor keys turn and tilt the zoomed view, with limits, slowing to a stop | todo | nothing in `TempleCameraModel.cpp` for the creature room's zoomed view |
| Tooltips say what clicking each target does | done | `src/3D/TempleToolTips.cpp` |
| The way out at the cave's far end leads out of the temple | done | `CreatureCaveTargets` exit target; test `CreatureCaveTargets.TheExitWithinItsReach` |
| The waterfall's water slides and sounds, and the fire crackles where the creature stands | done | `TempleInterior::UpdateCreatureCave` (the waterfall's texture slide, fire and water sounds every frame the room is drawn); test `CreatureCaveEffects.TheRoomsSoundsAtTheFireAndTheWaterfall` |
| Embers drift up from the fire | partial | `src/3D/CreatureCaveEffects.*` moves the fire's flames and smoke and the waterfall's spray and mist (tests `CreatureCaveEffects.*`); whether the room's sixteen drifting things are embers is unconfirmed |
| The scrolls and other room screens close when the player moves the view with the keys | todo | nothing in our tree |

## The creature in the cave

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player's creature stands in the cave, as it looks in the world (body, skin, tattoos) | todo | the temple's creature room draws no creature yet |
| It turns to face the hand as the hand moves about the cave | todo | nothing in our tree |
| It looks at the hand with a static pose, at random mirrored, when the hand is in front of it | todo | nothing in our tree |
| A click makes it look at where the hand clicked for a second and a half | todo | nothing in our tree |
| With no creature yet, the cave is empty and the scrolls say so | partial | without a creature the room's four scrolls are blank (`GatherScrollFacts` in `TempleInterior.cpp`) and no cave screen opens; what the original's scrolls say then is unchecked (our wiki's Pending) |
| The cave is always about the player's own creature, whichever the camera was following | done | `CreatureCaveSystem::GetCreature` |
| A picture of the creature can be taken from the cave and saved (unconfirmed what for) | todo | nothing in our tree |

## The scrolls

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The attributes scroll: name, species, age in years, alignment, strength, size, fatness, health and needs, as whole percentages cut short | done | `creature_cave::FactsOf`, drawn on the room's scrolls by `TempleScrolls` through `GatherScrollFacts`; test `CreatureCave.FactsCutPercentagesShort` |
| The personality scroll: each desire it has and how much it likes it, from extremely to not at all, its view of its god and of other creatures | done | tests `CreatureCave.FactsTellOfTheMind`, `TempleScrolls.TheCreaturesMindListsItsDesires` |
| The actions learnt scroll: what it has learnt to do and not to do, the strongest opinions each way | done | `creature_cave::LessonsOf`; test `CreatureCave.LessonsAreTheStrongestOpinionsEachWay` |
| The magic scroll: the skills it has learnt by watching and how far it has learnt each miracle | done | test `CreatureCave.FactsTellOfSkillsAndMiracles`, `CreatureCaveSystemTest.TheMindsTablesNameItsActionsSkillsAndMiracles` |
| The thing it likes most is named and shown | todo | nothing in our tree |
| A scroll's text unrolls when zoomed to and rolls up when left | done | the creature room's scrolls are the temple's `TempleScrolls`, which roll and unroll with the camera (test `TempleScrollTexture.TheParchmentRollsWithThePosition`); the ImGui cave screen (`src/Gui/CreatureCaveScreen.cpp`) is only for a land with no temple |
| The scrolls' pages go round from one to the next | done | test `CreatureCave.PagesGoRound` |
| A lesson browser steps through the actions it knows, telling for each what it has learnt about why, on what and how to do it | todo | nothing in our tree (unconfirmed how the player opens it) |
| The scrolls' texts come from the game's texts, in the player's language | done | `TempleScrolls::Write` |

## Trophies

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Belts hang on the attack dummies by how it fights: a row each way fills, white first, five a colour, as it leans to attack or defence | done | `src/3D/CreatureCaveTrophies.*`, drawn by `TempleInterior::UpdateCaveTrophies`; tests `CreatureCaveTrophies.AnEvenBalanceFillsBothRowsAlike`, `AFullBalanceFillsOneRow` |
| Medals stand on the magic plinths: one for all its miracles together, then its best four, wood to gem by how well learnt | done | `CreatureCaveTrophies`; tests `CreatureCaveTrophies.MedalsByLearning`, `LearningOfTheMiracles` |
| Every belt and every medal past wood is drawn shiny | done | `CreatureCaveTrophies` (environment map); test `CreatureCaveTrophies.BeltsAndMedalsPastWoodShine` |
| The miracles it knows hover as seeds in the room | todo | nothing in our tree (unconfirmed which four the room shows) |
| The fight scroll tells of its fights | partial | `TempleScrolls::CreatureFacts::tallies` has kills and battles, but they are made-up values until the game keeps its statistics (`TempleScrolls::Facts::Mock`) |
