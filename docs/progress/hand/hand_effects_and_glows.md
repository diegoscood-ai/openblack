# Hand effects and glows

The effects the god hand carries: the bands and bracelets and the flowing glow of a miracle held in it, the miracle's own
effect riding in the hand, the pulse of a miracle charging, the tribal power ring, the flash of a recognised gesture,
and the glows scripts put on the creature's hands. How the hand's own skin and shape show alignment, its light and its
shadow are in [look_and_morph.md](look_and_morph.md).

**Progress: 34/45 done, 0 chosen, 6 partial, 1 inferred — 83%**

How the original does it, in our wiki: [The hand and the interface](../../../hand/states.md).

## Bands and bracelets of a held miracle

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Taking a miracle into the hand, five bands fly from in front of the camera onto the hand, a tenth of a second apart | done | `magic::hand_fx::AddSpellToHandVisuals` (`src/Magic/Hand/HandMagicFX.cpp`): five temporary bands 0.1 s apart, flying from 4 m in front of the camera to the hand's root bone, as [the wiki's bands](../../../magic/hand-casting.md#the-hand); seen in the scenarios with a miracle in the hand ("A seed from worship, and a creature spell by hand", "Lightning, food and wood held by hand"); no unit test of the flight |
| The bands that come with a miracle still settling wait before flying in | done | `hand_fx::AddSpellToHandVisuals(delayed)` and `SetPowerUpLevel(level, delayed)`: delayed bands start 2.4 s later |
| Each fly-in plays the power-up band sound | done | `hand_fx::AddSpellToHandVisuals` plays G_SpellPowerUpBand (InGame) once with the five bands; tests `MiracleFxSystemTest.APowerUpPlaysTheBandsSoundThenNamesTheLevel`, `MiracleFxSystemTest.NoPowerUpHasNoVoice` |
| The hand wears one bracelet for the plain miracle and one more for each power-up, five at most | done | `hand_fx::SetPowerUpLevel(powerUp + 1)` from `seed::SetPowerUp` (`src/Magic/Core/SpellSeed.cpp`), five at most; test `MiracleFxSystemTest.AMiracleInTheHandWearsABraceletForEachPowerUpAndTheyGoWhenItLeaves` |
| Powering up adds the newest bracelets; powering down takes the newest off | done | `hand_fx::SetPowerUpLevel` adds bands, or takes the newest off |
| The bracelets sit along the hand and arm, each further one further up the arm and spinning a little faster | done | `HandMagicFX.cpp` `DrawBand`: scale 10 on the root bone at 10 + 40 per index along the forearm, spinning at (1 + 0.2 index) x 12 rad/s |
| Letting the miracle go takes the bracelets off | done | `HandSpellSeed.cpp`: the seed leaving the hand sets the level to 0 and releases the in-hand effect |
| Shaking the miracle off sends one band flying off the hand back to the camera over a second | done | `hand_fx::RemoveHandSpellVisuals`: one temporary band flying back, alpha reversed, over 1 s |
| Shaking a miracle off plays the shake-off sound | done | `hand_fx::RemoveHandSpellVisuals`: the InGame shake-hand sample, not 3D; test `MiracleFxSystemTest.AMiracleShakenOffPlaysTheShakeSound` |
| The announcer names the first, second or third power-up as it comes to the hand | done | `seed::SetPowerUp`: SpellDialogue samples 10, 11 or 12 for power-up 0, 1 or 2; tests `MiracleFxSystemTest.APowerUpPlaysTheBandsSoundThenNamesTheLevel`, `MiracleFxSystemTest.ALevelThatFellIsNamedWithoutTheBandsSound` |
| Bands and bracelets stand still while the game is paused | done | `hand_fx::Update` runs with the game time step, 0 while paused (`hand_casting::Update`) |
| A land change keeps the hand's bracelets, bands, charge and tribes' names, and takes only the miracle's effect out of the hand; a new game clears them all | done | `hand_fx::OnClearMap` from the land load, `hand_fx::Reset` from `magic::OnNewGame`, and `hand_fx::OnNewLand` gives the kept bands their mesh again in the new registry (2026-10-22, #206); a held seed takes its bracelets and ring off through the map clear's status pass; test `MiracleFxSystemTest.AMapClearKeepsTheRingTheColumnsAndTheBraceletsAndDeletesOnlyTheEffectInTheHand` ([what a land change keeps](../../../scripts/land-change-and-saving.md#what-a-land-change-keeps)) |

## The glow of a held miracle

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Holding a miracle, a flowing glow in the player's colour runs over the hand | partial 40% | `hand_fx::GetGlow` computes it and `graphics::HandGlowOf` (`src/Graphics/HandGlow.h`) turns it into a draw (tests `HandGlow.*`, `test/test_hand_glow.cpp`); missing: the draw itself, the hand drawn a second time, additive, with the flowing texture: `Renderer::DrawHandGlow` is called with nothing (issue #122) |
| The glow is drawn at 0.8 strength, and not at all below a hundredth | partial 50% | `hand_fx::Update`: alpha 0.8, its frame not stepped below 0.01, as the wiki's glow pass; test `HandGlow.fourFifthsInThePlayersColour`; missing: it is not drawn, so the 0.01 gate on the draw is not there (issue #122) |
| The glow's texture steps through a sheet of cells an eighth of the sheet across and down | partial 60% | `graphics::frame_anim::HandFlowFrame`: the cell of an 8 x 4 atlas at -20 frames a second, the frame rounded and wrapped as the original (tests in `test/test_frame_anim.cpp`, `HandGlow.theFlowingCellOfTheFrame`); missing: the cell is not applied to a drawn texture |
| The glow also shows while the hand holds a miracle's living creation, or anything else that counts as magic | pending | TODO in `hand_fx::Update`: only a spell seed in the hand turns the glow on; the original also turns it on for the magic and magic-living objects |
| The glow takes the player's colour, with its own strength as alpha | partial 50% | `graphics::HandGlowOf`: the player's colour with the glow's alpha, the colour's own alpha not read (tests `HandGlow.fourFifthsInThePlayersColour`, `HandGlow.theColoursAlphaIsNotRead`); missing: not drawn |

## A miracle charging in the hand

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| While a miracle charges from worship into the hand, the hand pulses, more often as the charge fills | done | `hand_fx::Update`: charge bands while one of the player's icons charges for this hand, one every lerp(6, 0.3, charge) s, each shorter and brighter as the charge fills, the wiki's lerp(3.5, 1, c) and lerp(6, 0.3, c); seen in the scenario "A seed from worship, and a creature spell by hand"; no unit test |
| The first pulse comes as soon as charging starts | done | `hand_fx::Update`: the first charge band comes on the frame charging starts |
| With a force-feedback mouse the charge is also felt, its beat quickening with the charge (charge less a tenth) | pending | No force feedback (noted in `hand_fx::Update`); the original starts and stops its immersion effect here |
| The charging feel stops when the charging stops | pending | No force feedback |

## The miracle's own effect in the hand

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A held miracle's effect (flames, water, sparks, the storm's cloud …) rides with the hand | done | `hand_fx::CreateInHandEffect` (the level's in-hand particle type), carried by the hand-follow particle rule `FollowLocalHand` (`src/Particles/Rules/HandFollow.cpp`); test `MiracleFxSystemTest.AMiracleInTheHandStartsItsLevelsEffectAtTheHandAndItGoesWhenTheMiracleLeaves` |
| The effect sits at the hand, or at a point of the hand's mesh where the miracle asks for one | partial 50% | `hand_fx::UpdateInHandEffect` puts it at the hand's origin (the plain case); missing: the attach-to-bone flag is read but not applied, and which point of the mesh the original uses is not yet known |
| The effect is scaled with the hand's size, so it keeps its size on screen | done | `hand_fx::UpdateInHandEffect`: magnitude = the hand's scale |
| The effect steps by the game's time, at least a millisecond a frame, and ends when it says it has finished | done | `hand_fx::UpdateInHandEffect`: a step of max(1, the game time step) ms, strength = the seed's PSys power; a finished effect is dropped; test `MiracleFxSystemTest.TheHandsStepRunsTheInHandEffectByTheFramesGameTime`. Not stepping it before the seed is ready is inferred |
| The fireball held in the hand rolls its flames with the hand's smoothed movement on screen | inferred | The in-hand fireball follows the hand (`FollowLocalHand`); the roll itself is in neither our tree nor the wiki, and was not found in the original: unverified that the original does it |

## Tribal power ring

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Taking a tribal power miracle into the hand starts a ring of the tribe's name round it, in the player's colour, fully opaque | done | `hand_fx::StartTribalPowerRing` (`src/Magic/Hand/HandMagicFX.cpp`) when the seed comes into the local hand (`src/Magic/Core/SpellSeed.cpp`), in the local player's colour at full alpha; `magic::tribal_spin` (`src/Magic/TribalPowerSpin.cpp`), run by `hand_fx::Update` and drawn by `Renderer::DrawTribalPower`; test `MiracleFxSystemTest.ARingStartsHeldAtTheCameraInTheLocalPlayersColour`. Only for a tribe whose tribal power is above 1, so it never shows in the vanilla game; not drawn inside the temple ([the tribal power column](../../../magic/hand-casting.md#the-tribal-power-column)) |
| The ring flies in from the camera and settles round the hand, above it | done | `tribal_spin::Runner` held: its first sample at the camera's position, flying in over the first second to round the hand, above it; tests `TribalPowerSpin.ItFliesInFromTheCamera`, `TribalPowerSpin.TheRingSettlesRoundTheHandAboveIt` |
| Nothing shows until the ring has two samples of the spin | done | `tribal_spin::Spin` draws no letters before its second sample; test `TribalPowerSpin.NothingShowsUntilTheRingHasTwoSamples` |
| Letting go leaves the ring rising as a column where the hand was | done | `hand_fx::ReleaseOrCreateTribalPowerRing` from the cast's post-cast step (`hand_fx::CastTribalPower`, `src/Magic/Core/SpellSeed.cpp`) lets the ring go at the hand; test `MiracleFxSystemTest.ReleasingTheRingLetsItGoWhereTheHandIs` |
| Letting go with no ring started raises the column straight away, in the player's colour | done | `hand_fx::CreateTribalPowerColumn` at the hand in the local player's colour; another player's cast raises one at the cast's position in its colour; tests `MiracleFxSystemTest.ReleasingWithoutARingRaisesAColumnAtTheHandInTheLocalPlayersColour`, `MiracleFxSystemTest.AnotherPlayersCastRaisesAColumnWhereItWasCastInTheCastersColour` |
| The column fades after three seconds and is gone after 4.8 seconds | done | `tribal_spin::Runner` released: full alpha up to 3 s, then fading, gone after 4.8 s; tests `TribalPowerSpin.TheColumnFadesAfterThreeSeconds`, `TribalPowerSpin.LetGoItRisesAsAColumnAndIsGoneAfterFourSecondsAndAHalf` (the test's name is older; it checks 4.8 s, the original's, [page](../../../magic/hand-casting.md#the-tribal-power-column)) |

## Gesture flash

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A recognised gesture makes the hand flash in its player's colour | pending | Not found in our tree: a recognised gesture (`gestures::Success`) makes only the sparkles and the light sheet. The original pulses the hand's colour from the gesture's effect, through a call not yet identified (issue #122, [page](../../../magic/hand-casting.md#the-hand)) |
| The sparkles of a recognised gesture settle on the gesture's shape | done | `psys::utility::GestureRecognised` (`src/Particles/Utility.cpp`) from `gestures::Success` (`src/Magic/Gestures/PowerUpSystem.cpp`): sparkles along the matched stroke |

## Glows on the creature's hands

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts light a glow on the creature's left hand, right hand, or above its hands (the creed on land 4) | done | `SET_CREATURE_CREED_PROPERTIES` (`src/CHLApi.cpp`) keeps the glow on the creature (`components::CreatureHandGlows`), placed on the body at each of its draws, as the object draw list draws it (`CreatureAnimationSystem::PlaceHandGlows`) ([the Creed glows](../../../creature/drawing.md#the-creed-glows-in-openblack)) |
| Each glow has a scale and a power, set at once (the script's time is not read); a power of 0 puts the glow out | done | `creature_hand_glow::Set` (`src/Creature/CreatureHandGlow.cpp`) |
| The glow is a glowing shell over the creed model at the hand's fingertips, scaled by the body and the glow, reaching out twice its power, its texture sliding in real time | done | `creature_hand_glow::PlaceOn`, `Renderer::DrawCreedGlow` over the artefact aura's shell; tests `CreatureCreedGlowDraw.*` |
| While any of its glows is lit the creature carries a looping glow sound, which stops when the last goes out | done | the script's powers start and stop it (`CreatureAnimationSystem::SetCreedPower`, sample 166); the animation's creed events give no hum in the game either |
| In the opening film the creature's hand glows rise and fall with the film | done | the falling film's copy lights its two hand glows at the start and gives them their power of the film time as it is drawn (`FallingSpell` hooks `glowCreature` and `placeCreatureGlows`, `Renderer::DrawFallingCreatureGlows`; test `FallingSpell.TheCreaturesHandGlowsAreLitAtInitPlacedAsItIsDrawnThenGivenTheirPowers`) ([the copy of the creature](../../../magic/falling-spell.md#the-copy-of-the-creature)) |

## The hand's effect on the world

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Trees bend away from the physics objects moving past them, from below their tops (the empty hand itself bends nothing) | done | `ecs::UpdateTrees` (`UpdateTreeBends`, `src/ECS/Trees.cpp`) takes the awake physics objects at their drawn position as sources, and `RenderingSystem.cpp` bends the crown; the original never takes the hand itself ([page](../../../world/trees.md#drawing)); tests `TreeBends.*` (`test/test_tree_system.cpp`) |
| Moving the hand quickly blows chimney smoke about | done | `chimney_smoke::UpdateHandWind` (`src/ECS/ChimneySmoke.cpp`): the hand's velocity as a wind on the smoke, scaled by the turn length as the original ([game clock](../../../engine/game-clock.md)); test `ChimneySmoke.HandStirsTheSmokeNearby` |
| Gripping the land throws up a puff of dust | partial 80% | `HandSystem::UpdateGripDust` (the grip dust spot visual, `HandEffects.cpp`), the 100-turn spot visual at the press's action point ([the grip dust](../../../hand/picking.md#the-grip-dust)); missing: the original sends it as a packet and so shows it a turn later; no unit test (seen in the hand demos) |
| Gripping the sea splashes a ring on the water | done | `HandSystem::SplashHand` (`HandFish.cpp`): a splash at the point, the next of ten hand-in-water samples, and a water ring (`ECS/WaterRings.h`); tests in `test/test_water_rings.cpp` |
| Crossing an influence border ripples the border | done | `src/ECS/Influence/InfluenceCircles.cpp`: the ripple and the sound of a hand crossing a circle, drawn by `RendererInfluence.cpp`; tests in `test/test_influence.cpp` |
| A held object carried through trees bends them too | done | `UpdateTreeBends` takes the object the hand carries as a source at its drawn position in the hand, and nothing while the hand is put away (checked with `OPENBLACK_HAND_TEST_HOLD` and the scenario `hand.put_away_over_trees`, [page](../../../world/trees.md#drawing)); tests `TreeBends.WhatTheDrawnHandHoldsBendsTrees`, `TreeBends.AHandPutAwayBendsNoTreesWithWhatItHolds` |
| Objects whizzing past the camera after a throw make a whoosh (rocks, fireballs) | done | `PhysicsObjects.cpp`: a body entering 10 m of the camera faster than 20 m/s plays G_RockPast_01..05 (picked by the tick count); the fireball's own whoosh in `src/Particles/Rules/Fireball.cpp`; test `PhysicsTurn.FastThingsWhooshPastTheCamera` |
