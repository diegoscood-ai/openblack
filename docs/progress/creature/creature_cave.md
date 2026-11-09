# The Creature Cave

The temple's room for the player's creature. The creature stands in it by a fire, and around the cave are scrolls
telling of its attributes, its personality, the actions it has learnt and the miracles it knows, attack dummies hung
with the belts it has won in fights and plinths with medals for its miracles. Clicking the creature opens the tattoo
editor ([creature_tattoos.md](creature_tattoos.md)). The temple itself is in [../temple](../temple/).

**Progress: 20/36 done, 0 chosen, 7 partial, 8 inferred — 67%**

How the original does it, in our wiki: [The creature: groundwork, random streams and what is unknown](../../../creature/overview.md).

## Getting there and moving about

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| F5 takes the player into the temple's Creature Cave | done | `CreatureCaveSystem::Update` and `Game.cpp` (F5, `ZOOM_TO_CREATURE_ROOM`, goes to the temple's creature room); test `CreatureCaveSystemTest.InTheTempleOpeningGoesToTheCreatureRoomAndClosingLeaves` |
| The cave can be entered only when the player has a creature, and outside a skirmish once the main room has been drawn | done | `temple::IsRoomAvailable`, as the exe's `CreatureRoom::IsAvailable` 0x789B30 ([temple-interior.md](../../../interface/temple-interior.md)); tests `TempleRoomAccess.TheCreaturesRoomNeedsACreature`, `OnlyTheMainRoomIsOpenUntilItHasBeenDrawn` |
| Without a temple, as on the testbed, F5 shows the cave's screen on its own, and F5 or Escape closes it | done | `CreatureCaveSystem`, its screen drawn by `src/Gui/CreatureCaveScreen.cpp` from the ImGui loop; test `CreatureCaveSystemTest.WithoutATempleF5ShowsTheCaveOnItsOwn` |
| The room's camera zooms to whatever is clicked (a scroll, the dummies, the plinths, the creature) and back out | partial 80% | targets and zoom points in `src/3D/CreatureCaveTargets.cpp` and `src/Camera/TempleCameraModel.cpp` (tests `CreatureCaveTargets.*`, `TempleCamera.APressLeavesTheCreatureZoomedTo`); zoomed onto the creature the camera takes the tattoo editor's view and zooms back out once it closes, as the exe ([tattoos.md#the-zoom-the-opening-and-the-closing](../../../creature/tattoos.md#the-zoom-the-opening-and-the-closing)); the 20% left: the creature is found by its body under the cursor through `cave_copy_pick` (tests `CaveCopyPick.*`), imported from raffclar #349 and not yet read against the exe's mesh pick |
| A fourth target's tooltip says it zooms in, but clicking it does nothing | inferred | `CreatureCaveTargets` (`Target::Fourth`); no test, and not read in the exe |
| The cursor keys turn and tilt the zoomed view, with limits, slowing to a stop | inferred | nothing in `TempleCameraModel.cpp`; in the exe the movement keys end the creature's zoom (`CreatureRoomCamera::UpdateMain` 0x78AC2C..0x78ACFD, [tattoos.md](../../../creature/tattoos.md#the-zoom-the-opening-and-the-closing)); whether they turn any other zoomed view is not read |
| Tooltips say what clicking each target does | done | `src/3D/TempleToolTips.cpp`; test `TempleToolTips.CreatureRoomTargets` |
| Entering the cave starts its help; each scroll looked at starts its own, the dummies and the plinths theirs, and the player's input stops the scrolls' | done | `src/Temple/TempleHelp.cpp`, as the exe's `TempleRoom::OnEnterRoom`, the scrolls' Set callbacks and the room's zoom ([temple-interior.md](../../../interface/temple-interior.md)); tests `TempleHelp.EnteringARoomStopsTheHelpThenStartsTheRoomsOwn`, `LookingAtAScrollStartsItsHelpWhetherOrNotTheHelpSystemIsOn`, `OnlyTheBeltsAndMedalsOfTheCreaturesRoomStartHelp`, `LeavingAScrollStopsTheScrollHelpOfTheRoomsCamera` |
| The way out at the cave's far end leads out of the temple | done | `CreatureCaveTargets` exit target; test `CreatureCaveTargets.TheExitWithinItsReach` |
| The waterfall's water slides and sounds, and the fire crackles where the creature stands | done | `TempleInterior::UpdateCreatureCave` (the waterfall's texture slide, fire and water sounds every frame the room is drawn); test `CreatureCaveEffects.TheRoomsSoundsAtTheFireAndTheWaterfall` |
| Embers drift up from the fire | inferred | `src/3D/CreatureCaveEffects.cpp` moves the fire's flames and smoke and the waterfall's spray and mist (tests `CreatureCaveEffects.*`); whether the room's sixteen drifting things are embers is unconfirmed |
| The scrolls and other room screens close when the player moves the view with the keys | partial 50% | the movement keys zoom back from every target (`TempleCameraModel::UpdateCaveTargets`), as the exe does for the creature's zoom; no test covers the keys, and the exe's handling of them for the scrolls is not read |

## The creature in the cave

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player's creature stands in the cave, as it looks in the world (body, skin, tattoos) | partial 85% | the room's copy of the creature is made, posed and drawn by the fire with its body, skins and tattoos, as the exe's `InitEngine` and `DrawAdditional` (`CreatureCopyArchetype`, `CreatureAnimationSystem::UpdateCopies`, `RenderingSystemTemple`; tests `CreatureCopyArchetypeTest.*`, `CreatureDrawBodies.TheCreatureRoomsCopyIsDrawnAsItsCreaturesSpeciesWithItsCreaturesSkins`; [tattoos.md](../../../creature/tattoos.md#the-copy-in-openblack)); its hair and the sleep's snore are not ported |
| It rests by the fire, asleep or sitting by a coin toss (a creature of size 1.25 or more never sleeps), looking into the fire when sitting | done | `creature_tattoo_editor::DrawCopyRest`, `CopyRestAnimations`, as the exe's `InitEngine` ([tattoos.md#where-the-creature-stands](../../../creature/tattoos.md#where-the-creature-stands)); tests `TattooEditor.TheCopysRestTossesTheCoinThenTheRestsFlag`, `TheCopysRestIsTheSleepOrTheSitAroundItsLoop`, `CreatureCopyArchetypeTest.ACopyOfACreatureOf125OrMoreNeverSleepsButStillTossesTwice` |
| As the editor opens it gets up and turns to face the editor's eye; closed, it turns back to the fire and rests again | done | `creature_tattoo_editor::CopyStep`, `StartCopyTurn`, as the exe's `CreatureRoom::Draw` ([tattoos.md#the-copys-turns-and-looks](../../../creature/tattoos.md#the-copys-turns-and-looks)); tests `TattooEditor.TheEditorsFirstFrameGetsTheCopyUpToTurnToTheEditorsEye`, `ClosedAndNotFacingTheFireTheCopyGetsUpAndTurnsToIt`, `ClosedAndFacingTheFireTheCopyRestsAgainDrawingItsFlagEveryFrame`, `TheCopysTurnPlaysTheSpinsOfItsSideBlendedByTheAngle` |
| Sitting, it yawns now and then | done | `creature_tattoo_editor::CopyStartsYawn` (one drawn frame in a thousand), as the exe ([tattoos.md](../../../creature/tattoos.md#how-the-copy-is-posed-and-drawn)); test `TattooEditor.TheCopyYawnsOneDrawnFrameInAThousandOnlyWhileSittingAndNotInTheReflection` |
| A good or evil creature sparkles in the cave | partial 40% | the sparkles' draws are made on the local stream as the exe's (`DrawCopyMoment`; tests `TattooEditor.TheCopysMomentsDrawTheirSparklesAroundTheRestsFlag`, `EachMomentOfTheCopyDrawsItsSparklesAsTheRoomsStartDoes`), but the sparkles are not shown yet |
| A scripted switch keeps the creature out of the cave | partial 80% | `SET_CREATURE_IN_TEMPLE` sets `CreatureCaveSystem::IsCreatureShown`, read as the room starts, as the exe's [0xD96270] (test `CreatureCaveSystemTest.TheCreatureRoomShowsTheCreatureUntilAScriptSaysNot`); imported from raffclar #349, not saved with the game yet |
| With the editor open, a press makes it look at the pointer for the first second and a half | done | `creature_tattoo_editor::CopyStep`, as the exe's `CreatureRoom::Draw` ([tattoos.md#the-copys-turns-and-looks](../../../creature/tattoos.md#the-copys-turns-and-looks)); test `TattooEditor.WithTheEditorOpenTheCopyLooksAtThePointerForTheFirstSecondAndAHalfOfAPress` |
| With no creature yet, the cave is empty and the scrolls say so | partial 50% | without a creature no copy is made (test `TattooEditor.TheCopyIsMadeOnlyAsTheRoomStartsAndOnlyWhenTheCreatureIsShown`), the room's four scrolls are blank (`GatherScrollFacts` in `TempleInterior.cpp`) and no cave screen opens; what the exe's scrolls say then is not read (our wiki's Pending) |
| The cave is always about the player's own creature, whichever the camera was following | done | `CreatureCaveSystem::GetCreature`; the exe's room copies the local player's creature ([tattoos.md#where-the-creature-stands](../../../creature/tattoos.md#where-the-creature-stands)); tests `CreatureCaveSystemTest.InTheTempleTheScreenFollowsTheCreatureRoom` |

## The scrolls

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The attributes scroll: name, species, age in years, alignment, strength, size, fatness, health and needs, as whole percentages cut short | inferred | `creature_cave::FactsOf`, drawn on the room's scrolls by `TempleScrolls` through `GatherScrollFacts`; test `CreatureCave.FactsCutPercentagesShort`; the exe's `CreatureRoom::MakeCreatureScrollText` 0x788A10 is not read |
| The personality scroll: each desire it has and how much it likes it, from extremely to not at all, its view of its god and of other creatures | partial 40% | tests `CreatureCave.FactsTellOfTheMind`, `TempleScrolls.TheCreaturesMindListsItsDesires`; the exe's `MakePersonalityScrollText` 0x788D20 writes the attention line, a perceived desire's line end, the creatures it knows and four beliefs ([mimicry.md](../../../creature/mimicry.md)); the list of desires by liking is not in it (perhaps the likes scroll, not read) |
| The personality scroll's attention its god has paid it is the share of its life the player's camera looked at it, in five words; what it thinks of its god shows only once it has been looked at | done | `creature_attention::Share` / `HowMuch` at float precision, counted in `CreatureMindSystem::ProcessTurn` (`turnsLookedAt`, kept in the mind file); tests `CreatureAttention.*`, `CreatureCave.FactsHideTheOpinionUntilLookedAt`, `TempleScrolls.TheMindScrollsAttentionIsOneOfFiveWords`; see [../../../creature/mind.md](../../../creature/mind.md#the-attention-its-god-pays-it) |
| The actions learnt scroll: what it has learnt to do and not to do, the strongest opinions each way | inferred | `creature_cave::LessonsOf`; test `CreatureCave.LessonsAreTheStrongestOpinionsEachWay`; the exe's `MakeActionsLearntScrollText` 0x788C10 is not read |
| The magic scroll: the skills it has learnt by watching and how far it has learnt each miracle it knows about, even at 0 % | done | as the exe's `MakeMagicScrollText` 0x789070 ([mind.md#the-magic-scroll](../../../creature/mind.md#the-magic-scroll)); tests `CreatureCave.FactsTellOfSkillsAndMiracles`, `CreatureCaveSystemTest.TheMindsTablesNameItsActionsSkillsAndMiracles`, `CreatureCaveSystemTest.TheMagicScrollListsTheMiraclesItKnowsAboutHoweverLittleLearnt` |
| The thing it likes most is named and shown | pending | nothing in our tree; the exe has a likes scroll (`CreatureScrollLikesSet`, its help "CitadelCreatureRoomLikesScroll"), not read |
| A scroll's text unrolls when zoomed to and rolls up when left | done | the creature room's scrolls are the temple's `TempleScrolls`, which roll and unroll with the camera (test `TempleScrollTexture.TheParchmentRollsWithThePosition`); the ImGui cave screen (`src/Gui/CreatureCaveScreen.cpp`) is only for a land with no temple |
| The scrolls' pages go round from one to the next | done | test `CreatureCave.PagesGoRound` |
| A lesson browser steps through the actions it knows, telling for each what it has learnt about why, on what and how to do it | inferred | nothing in our tree; no such browser is found among the exe's creature room functions; unconfirmed |
| The scrolls' texts come from the game's texts, in the player's language | done | `TempleScrolls::Write`; test `TempleScrolls.TheMindScrollsAttentionIsOneOfFiveWords` |

## Trophies

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Belts hang on the attack dummies by how it fights: a row each way fills, white first, five a colour, as it leans to attack or defence | done | `src/3D/CreatureCaveTrophies.*`, drawn by `TempleInterior::UpdateCaveTrophies`; tests `CreatureCaveTrophies.AnEvenBalanceFillsBothRowsAlike`, `AFullBalanceFillsOneRow` |
| Medals stand on the magic plinths: one for all its miracles together, then its best four, wood to gem by how well learnt | done | `CreatureCaveTrophies`; tests `CreatureCaveTrophies.MedalsByLearning`, `LearningOfTheMiracles` |
| Every belt and every medal past wood is drawn shiny | done | `CreatureCaveTrophies` (environment map); test `CreatureCaveTrophies.BeltsAndMedalsPastWoodShine` |
| The miracles it knows hover as seeds in the room | inferred | nothing in our tree; not read in the exe (unconfirmed which four the room shows) |
| The fight scroll tells of its fights | inferred | `TempleScrolls::CreatureFacts::tallies` has kills and battles, made-up values (`TempleScrolls::Facts::Mock`); the exe's room makes four scroll texts (creature, actions learnt, personality, magic), none a fight scroll; whether the attributes scroll tells of fights is not read |
