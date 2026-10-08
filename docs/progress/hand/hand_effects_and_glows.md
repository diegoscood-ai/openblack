# Hand effects and glows

The effects the god hand carries: the bands and bracelets and the flowing glow of a miracle held in it, the miracle's own
effect riding in the hand, the pulse of a miracle charging, the tribal power ring, the flash of a recognised gesture,
and the glows scripts put on the creature's hands. How the hand's own skin and shape show alignment, its light and its
shadow are in [look_and_morph.md](look_and_morph.md).

**Progress: 30/45 done, 6 partial — 73%**

How the original does it, in our wiki: [The hand and the interface](../../bw1-notes/hand-and-interface.md).

## Bands and bracelets of a held miracle

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Taking a miracle into the hand, five bands fly from in front of the camera onto the hand, a tenth of a second apart | done | `magic::hand_fx::AddSpellToHandVisuals` (`src/Magic/Hand/HandMagicFX.cpp`): five temporary bands 0.1 s apart, flying from 4 m in front of the camera to the hand's root bone |
| The bands that come with a miracle still settling wait before flying in | done | `hand_fx::AddSpellToHandVisuals(delayed)` and `SetPowerUpLevel(level, delayed)`: delayed bands start 2.4 s later |
| Each fly-in plays the power-up band sound | done | `hand_fx::AddSpellToHandVisuals` plays G_SpellPowerUpBand (InGame) once with the five bands |
| The hand wears one bracelet for the plain miracle and one more for each power-up, five at most | done | `hand_fx::SetPowerUpLevel(powerUp + 1)` from `seed::SetPowerUp` (`src/Magic/Core/SpellSeed.cpp`), five at most |
| Powering up adds the newest bracelets; powering down takes the newest off | done | `hand_fx::SetPowerUpLevel` adds bands, or takes the newest off |
| The bracelets sit along the hand and arm, each further one further up the arm and spinning a little faster | done | `HandMagicFX.cpp` `DrawBand`: scale 10 on the root bone at 10 + 40 per index along the forearm, spinning at (1 + 0.2 index) x 12 rad/s |
| Letting the miracle go takes the bracelets off | done | `HandSpellSeed.cpp`: the seed leaving the hand sets the level to 0 and releases the in-hand effect |
| Shaking the miracle off sends one band flying off the hand back to the camera over a second | done | `hand_fx::RemoveHandSpellVisuals`: one temporary band flying back, alpha reversed, over 1 s |
| Shaking a miracle off plays the shake-off sound | done | `hand_fx::RemoveHandSpellVisuals`: the InGame shake-hand sample, not 3D |
| The announcer names the first, second or third power-up as it comes to the hand | done | `seed::SetPowerUp`: SpellDialogue samples 10, 11 or 12 for power-up 0, 1 or 2 |
| Bands and bracelets stand still while the game is paused | done | `hand_fx::Update` runs with the game time step, 0 while paused (`hand_casting::Update`) |

## The glow of a held miracle

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Holding a miracle, a flowing glow in the player's colour runs over the hand | partial | `hand_fx::GetGlow` computes it (player colour, alpha 0.8) but it is not drawn: it needs a skinned mesh pass with two textures |
| The glow is drawn at 0.8 strength, and not at all below a hundredth | partial | Computed, not drawn (see above): alpha 0.8, and not stepped below 0.01 |
| The glow's texture steps through a sheet of cells an eighth of the sheet across and down | partial | `graphics::frame_anim::HandFlowFrame`: the cell of an 8 x 4 atlas at -20 frames a second; computed, not drawn |
| The glow also shows while the hand holds a miracle's living creation, or anything else that counts as magic | todo | TODO in `hand_fx::Update`: only a spell seed in the hand turns the glow on, not the magic objects |
| The glow takes the player's colour, with its own strength as alpha | partial | Computed with the local player's colour and the 0.8 alpha, but not drawn |

## A miracle charging in the hand

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| While a miracle charges from worship into the hand, the hand pulses, more often as the charge fills | done | `hand_fx::Update`: charge bands while one of the player's icons charges for this hand, one every lerp(6, 0.3, charge) s, each shorter and brighter as the charge fills |
| The first pulse comes as soon as charging starts | done | `hand_fx::Update`: the first charge band comes on the frame charging starts |
| With a force-feedback mouse the charge is also felt, its beat quickening with the charge (charge less a tenth) | todo | No force feedback (noted in `hand_fx::Update`) |
| The charging feel stops when the charging stops | todo | No force feedback |

## The miracle's own effect in the hand

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A held miracle's effect (flames, water, sparks, the storm's cloud …) rides with the hand | done | `hand_fx::CreateInHandEffect` (the level's in-hand particle type), carried by the hand-follow particle rule `FollowLocalHand` (`src/Particles/Rules/HandFollow.cpp`) |
| The effect sits at the hand, or at a point of the hand's mesh where the miracle asks for one | partial | `hand_fx::UpdateInHandEffect` always puts it at the hand's origin; the attach-to-bone flag is read but not applied |
| The effect is scaled with the hand's size, so it keeps its size on screen | done | `hand_fx::UpdateInHandEffect`: magnitude = the hand's scale |
| The effect steps by the game's time, at least a millisecond a frame, and ends when it says it has finished | done | `hand_fx::UpdateInHandEffect`: a step of max(1, the game time step) ms, strength = the seed's PSys power; a finished effect is dropped |
| The fireball held in the hand rolls its flames with the hand's smoothed movement on screen | partial | The in-hand fireball follows the hand (`FollowLocalHand`); a roll of its flames by the hand's smoothed screen movement is not found in our tree |

## Tribal power ring

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Spinning the hand with a tribal power miracle starts a ring in the player's colour, fully opaque | done | `hand_fx::StartTribalPowerRing` (`src/Magic/Hand/HandMagicFX.cpp`) when the seed comes into the local hand (`src/Magic/Core/SpellSeed.cpp`), in the local player's colour at full alpha; `magic::tribal_spin` (`src/Magic/TribalPowerSpin.cpp`), run by `hand_fx::Update` and drawn by `Renderer::DrawTribalPower`; test `HandFxTribalPowerTest.ARingStartsHeldAtTheCameraInTheLocalPlayersColour`. Only for a tribe whose tribal power is above 1, so it never shows in the vanilla game; not drawn inside the temple. Our wiki differs: the ring is the tribe's name spinning round the hand, started when the seed comes into the hand rather than by spinning the hand ([page](../../bw1-notes/magic.md#the-hand-handmagicfxcpp-phandfx-and-the-effect-in-the-hand)) |
| The ring flies in from the camera and settles round the hand, above it | done | `tribal_spin::Runner` held: its first sample at the camera's position, flying in over the first second to round the hand, above it; tests `TribalPowerSpin.ItFliesInFromTheCamera`, `TribalPowerSpin.TheRingSettlesRoundTheHandAboveIt` |
| Nothing shows until the ring has two samples of the spin | done | `tribal_spin::Spin` draws no letters before its second sample; test `TribalPowerSpin.NothingShowsUntilTheRingHasTwoSamples` |
| Letting go leaves the ring rising as a column where the hand was | done | `hand_fx::ReleaseOrCreateTribalPowerRing` from the cast's post-cast step (`hand_fx::CastTribalPower`, `src/Magic/Core/SpellSeed.cpp`) lets the ring go at the hand; test `HandFxTribalPowerTest.MyCastLetsTheRingGoWhereTheHandIs` |
| Letting go with no ring started raises the column straight away, in the player's colour | done | `hand_fx::CreateTribalPowerColumn` at the hand in the local player's colour; another player's cast raises one at the cast's position in its colour; tests `HandFxTribalPowerTest.MyCastWithoutARingRaisesAColumnAtTheHandInTheLocalPlayersColour`, `HandFxTribalPowerTest.AnotherInterfacesCastRaisesAColumnWhereItWasCastInTheCastersColour` |
| The column fades after three seconds and is gone after four and a half | done | `tribal_spin::Runner` released: full alpha up to 3 s, then fading, gone after 4.8 s; tests `TribalPowerSpin.TheColumnFadesAfterThreeSeconds`, `TribalPowerSpin.LetGoItRisesAsAColumnAndIsGoneAfterFourSecondsAndAHalf`. Our wiki differs: the column is gone after 4.8 seconds, not 4.5 ([page](../../bw1-notes/magic.md#the-hand-handmagicfxcpp-phandfx-and-the-effect-in-the-hand)) |

## Gesture flash

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A recognised gesture makes the hand flash in its player's colour | todo | Not found in our tree: a recognised gesture (`gestures::Success`) makes only the sparkles |
| The sparkles of a recognised gesture settle on the gesture's shape | done | `psys::utility::GestureRecognised` (`src/Particles/Utility.cpp`) from `gestures::Success` (`src/Magic/Gestures/PowerUpSystem.cpp`): sparkles along the matched stroke |

## Glows on the creature's hands

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts light a glow on the creature's left hand, right hand, or above its hands (the creed on land 4) | todo | `SET_CREATURE_CREED_PROPERTIES` is a stub in `src/CHLApi.cpp` |
| Each glow has a scale, a power and a time to reach it; a power of 0 puts the glow out | todo | Not ported |
| The glow is a sprite on the hand's point, scaled by the glow's scale, its brightness twice its power | todo | Not ported |
| While any of its glows is lit the creature carries a looping glow sound, which stops when the last goes out | todo | The creed glow sound actions are named in `src/Creature/CreatureAudio.cpp` but never played |
| In the opening film the creature's hand glows rise and fall with the film | todo | Not ported; see [../story/](../story/) |

## The hand's effect on the world

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Trees within the hand's reach bend away from it, from below their tops | done | `ecs::UpdateTrees` (`UpdateTreeBends`, `src/ECS/Trees.cpp`) and `RenderingSystem.cpp`, the crown bending away. Our wiki differs: the trees bend away from what the hand carries, the physics objects in flight and the creature, not from the empty hand ([page](../../bw1-notes/trees.md#drawing)) |
| Moving the hand quickly blows chimney smoke about | done | `chimney_smoke::UpdateHandWind` (`src/ECS/ChimneySmoke.cpp`): the hand's velocity as a wind on the smoke |
| Gripping the land throws up a puff of dust | done | `HandSystem::UpdateGripDust` (the grip dust spot visual, `HandEffects.cpp`) |
| Gripping the sea splashes a ring on the water | done | `HandSystem::SplashHand` (`HandFish.cpp`): a splash at the point, the next of ten hand-in-water samples, and a water ring (`ECS/WaterRings.h`) |
| Crossing an influence border ripples the border | done | `src/ECS/Influence/InfluenceCircles.cpp`: the ripple and the sound of a hand crossing a circle, drawn by `RendererInfluence.cpp` |
| A held object carried through trees bends them too | done | `UpdateTreeBends` takes the object the hand carries as a source (checked with `OPENBLACK_HAND_TEST_HOLD`, see [trees.md](../../bw1-notes/trees.md)) |
| Objects whizzing past the camera after a throw make a whoosh (rocks, fireballs) | done | `PhysicsObjects.cpp`: a body entering 10 m of the camera faster than 20 m/s plays G_RockPast_01..05; the fireball's own whoosh in `src/Particles/Rules/Fireball.cpp` |
