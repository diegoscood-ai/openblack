# Face, eyes and hair

A creature's face shows what it is doing and feeling: it smiles, grimaces, growls, looks scared, sad, amazed or puzzled,
laughs, and says ooh and aah. Its eyes look about and blink, its head turns towards what interests it, and some species
have tufts of hair that swing as they move and change with their alignment.

**Progress: 32/45 done, 6 partial — 78%**

How the original does it, in our wiki: [The creature: groundwork, random streams and what is unknown](../../bw1-notes/creature.md).

## Faces

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Twelve faces from the creature spec: smile, grimace, growl, scared, sad, amazed, puzzled, laugh, ooh, aah and two spare | done | `creature_face::Face` (`src/Creature/CreatureFace.h`); test `FacesAreTheSpecFilesFaceAnimations` |
| A face plays through once, holds the full expression as long as it is pulled, then relaxes back a quarter as fast | done | `creature_layers::AdvanceFace` in `CreatureAnimationSystem`; tests `AFaceHoldsItsExpressionRatherThanLooping`, `AFaceRelaxesWhenItsTimeIsUp` |
| A new face first runs the current one back at full speed | done | test `AFaceRunsBackBeforeTheNextStarts` |
| Pulling the held face again keeps holding it | done | test `PullingTheHeldFaceAgainKeepsHoldingIt` |
| A face the species lacks stays at its start | done | test `AFaceTheSpeciesLacksStaysAtItsStart` |
| Idling, it pulls any of ten faces at random for three seconds, and again every few seconds | done | the idle cue of the agendas (`src/Creature/CreatureIdleMind.cpp`); test `IdlingPullsAnyOfTenFacesForThreeSeconds`; see [idle_behaviour.md](idle_behaviour.md) |
| A few feelings take turns between two or three faces, by a count that moves on by a random step each face | done | test `SomeFeelingsPickBetweenFacesByTheVariety` |
| Showing how it feels about the player: a smile when it likes them, sad when it doesn't | done | `Cue::AttitudeToPlayer` in the agendas; test `TheAttitudeToThePlayerShowsAsASmileOrSadness` |
| Faces for actions done out of anger (growl), compassion, curiosity, fear, playfulness | done | `creature_face` cues set by the agendas (`src/Creature/CreatureIdleMind.cpp`) and the fights' anger (`CreatureFightSystem`), pulled by `CreatureMindSystem` |
| Smiling at a friend or having a poo; grimacing asleep or being sick; amazed at what it watches; puzzled at something strange | done | the action cues in `src/Creature/CreatureIdleMind.cpp` |
| Puzzled at finding no way to where it is going | done | `Cue::Lost` pulled by `CreatureLocomotionSystem` |
| Pleased at being stroked, the face by the body part (aah for head and belly, ooh for the groin, a smile elsewhere) | done | `creature_feedback::RewardFace` pulled by `CreatureHandSystem`; see [learning_from_feedback.md](learning_from_feedback.md) |
| Pleased or saddened when its tattoo is drawn | partial | `Cue::Tattooed` exists; nothing pulls it yet (see [creature_tattoos.md](creature_tattoos.md)) |
| Mood faces: frightened, sad, exhausted | partial | the faces for these moods exist (test `MoodsHaveTheirFaces`); only the debug spawner pulls them |
| Mood faces: happy, in pain, irritable, lonely | todo | |
| A face is pulled with each idle step as it starts and again every few seconds | done | `creature_mind::k_FaceRepeatSeconds` |
| Faces play their sounds only the first time through | done | test `FaceSoundsOnlyItsFirstTimeThrough` |

## Eyes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each eye is an eyeball and an eyelid set into the body at a point on a triangle of the mesh | done | `creature_eyes`, placed by `CreatureAnimationSystem`; test `ThePointIsOnTheTriangleAndItsNormalPointsIn` |
| Small creatures have bigger eyes for their size | done | test `SmallCreaturesHaveBiggerEyesForTheirSize` |
| The eyes look where the creature looks, or ahead, never back into the head | done | tests `EyesNeverLookBackIntoTheHead`, `TheEyeballFacesAwayFromWhereItLooks` |
| Open eyes turn faster | done | test `OpenEyesTurnFaster` |
| The lids blink every few seconds, closing and opening over 200 milliseconds each | done | `creature_eyes::AdvanceBlink` in `CreatureAnimationSystem`; tests `BlinksCloseAndOpenOverTwoHundredMillisecondsEach`, `CreatureAnimationSystemTest.TheEyesBlinkByTheTurnOnTheRuntimesNumbers` |
| While calm the lids follow the pupils up and down | done | tests `CalmLidsSwingShutOverABlink`, `LidAnglesByMode` |
| Closed while asleep or knocked out | done | `CreatureMindSystem` (sleep) and `CreatureFightSystem` (knocked out) set `creature_eyes::Mode::Closed` |
| Wide open when frightened | partial | `creature_eyes::Mode::Wide` exists (test `LidAnglesByMode`); nothing sets it |
| Stoned, wide and darting, when high | partial | `creature_eyes::Mode::Stoned` exists; nothing sets it |
| Fluttering eyelids when looking at something it loves (unconfirmed what) | todo | |
| Just woken up, it looks about blearily | todo | |
| Looking down at its feet, or at its dependants, or at a partner | todo | |
| Eye colours or sizes change with alignment | done | eye size follows the evil to good axis (`creature_eyes::EyeSize`) |

## The head and looking

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The head turns towards what the creature looks at, right and left up to half a turn, down and up a quarter turn | done | `creature_layers::TurnHead`, `AnglesTowards` in `CreatureAnimationSystem`; tests `TheHeadNeverTurnsPastItsLimit`, `AnglesTowardsATarget` |
| It speeds up steadily and slows so as to stop exactly there, never overshooting | done | tests `TheHeadSpeedsUpSteadily`, `TheHeadSettlesWithoutOvershooting` |
| Sitting, it turns its head with the sitting look animations | done | `k_SitLookRightLeft`, `k_SitLookDownUp` in `CreatureAnimationSystem` |
| Idle, it looks at the most interesting thing in sight, growing bored of it after a while | done | `creature_look`, candidates gathered by `CreatureMindSystem`; see [idle_behaviour.md](idle_behaviour.md) |
| It looks at the hand, the camera, the player | partial | looking at the hand and camera is part of some actions; looking at the player when the hand comes near is todo |
| Walking or running somewhere, it looks where it is going; running from something, back at it | todo | |
| It looks up at flying things | todo | |
| It turns its head to show love or hate (the hate to love look animation) | todo | |

## Hair

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Some species have tufts of hair: groups of strands rooted on triangles of the body | done | `creature_hair`, `CreatureHairSystem`, drawn by `src/Graphics/CreatureDraw.cpp`; tests `StrandsGrowStraightOutOfTheSurface`, `RootFollowsTheBody` |
| Each strand springs back towards growing straight on, sags under gravity, loses speed and never stretches | done | tests `StiffStrandHoldsItsShape`, `LimpStrandSagsUnderGravity`, `StrandNeverStretches`, `SpeedIsCapped` |
| A group's colour, length, stiffness, damping and thickness move from neutral towards evil or good with the alignment | done | tests `ValuesMoveFromNeutralTowardsEvilOrGood`, `LookFollowsAlignment` |
| Strands are drawn as ribbons facing the camera, lit like the land | done | tests `RibbonFacesTheEye`, `StrandsTakeTheLandsLightAndColour`, `CreatureDrawHair.EachStrandIsARibbonInItsTuftsLitColour` |
| Animations can show or hide a tuft at a moment | partial | the event kind is read (`creature_audio::EventKind::HairGroup`) and skipped by `CreatureAudioSystem`; nothing hides a tuft |
| Hair grows with the creature's size | done | test `ScaleGrowsWithSizeButHeadsShrink` |
| The hair can be hidden altogether | done | `CreatureHairSystem::SetShown` (debug spawner only) |
