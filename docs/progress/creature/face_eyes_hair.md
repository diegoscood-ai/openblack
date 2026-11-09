# Face, eyes and hair

A creature's face shows what it is doing and feeling: it smiles, grimaces, growls, looks scared, sad, amazed or puzzled,
laughs, and says ooh and aah. Its eyes look about and blink, its head turns towards what interests it, and some species
have tufts of hair that swing as they move and change with their alignment.

**Progress: 32/44 done, 0 chosen, 9 partial, 2 inferred — 88%**

How the original does it, in our wiki: [The faces a creature pulls](../../../creature/pose-and-animation.md#the-faces-a-creature-pulls) and [the eyes' size](../../../creature/drawing.md#the-eyes-size-and-a-young-creatures-bigger-parts).

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
| Faces for actions done out of anger (growl), compassion, curiosity, fear, playfulness | done | `creature_face` cues set by the agendas (`src/Creature/CreatureIdleMind.cpp`) and the fights' anger (`CreatureFightSystem`), pulled by `CreatureMindSystem`; only the idle face draws its random pick, the others draw only the variety's step, as the original; tests `CreatureFace.SomeFeelingsPickBetweenFacesByTheVariety`, `CuesWithFacesOfTheirOwnPullNoneByThemselves` |
| Smiling at a friend or having a poo; grimacing asleep or being sick; amazed at what it watches; puzzled at something strange | done | the action cues in `src/Creature/CreatureIdleMind.cpp`; tests `CreatureFriendship.SmilingAtAFriendGoesNearFacesItAndHalfTheTimeShowsItIsHappy`, `CreatureFace.CuesWithFacesOfTheirOwnPullNoneByThemselves` |
| Puzzled at finding no way to where it is going | partial 80% | `Cue::Lost` pulled by `CreatureLocomotionSystem` (the puzzled face, 3 s); no test pins it |
| Pleased at being stroked, the face by the body part (aah for head and belly, ooh for the groin, a smile elsewhere) | partial 80% | `creature_feedback::RewardFace` pulled by `CreatureHandSystem`, as the hand's creature state pulls a face in the original; no test pins the face by the part; see [learning_from_feedback.md](learning_from_feedback.md) |
| Pleased or saddened when its tattoo is drawn | pending | the original's tattoo editor pulls faces on its creature; ours has only the cue's name (`Cue::Tattooed`, no face of its own) and nothing pulls it (see [creature_tattoos.md](creature_tattoos.md)) |
| Mood faces: frightened, sad, exhausted | partial 90% | pulled by the agendas the original gives them: frightened by running home and being frightened on the spot, sad by pining for and looking for a friend and being sad, exhausted by resting on the spot (`src/Creature/CreatureRestAndHome.cpp`, `CreatureFriendAgendas.cpp`, `CreatureKindAgendas.cpp`); tests `CreatureFace.MoodsHaveTheirFaces`, `CreatureRestAndHome.RunningHomeRunsToTwoHeightsAfterAFrightHalfTheTime`; one more caller of the sad face in the exe (fn_0047EC10) is not read |
| A face is pulled with each idle step as it starts and again every few seconds | done | `creature_mind::k_FaceRepeatSeconds` (4.2 s, the original's 42 turns); test `CreatureIdleMind.IdleStepsPullAFaceAsTheyStartAndEveryFewSeconds` |
| Faces play their sounds only the first time through | partial 80% | the face plays once and holds (`creature_layers::AdvanceFace`), so its moments pass once (`creature_audio::QueueTurn`); the test the row named (`FaceSoundsOnlyItsFirstTimeThrough`) does not exist and none pins it |

## Eyes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each eye is an eyeball and an eyelid set into the body at a point on a triangle of the mesh | done | `creature_eyes`, placed by `CreatureAnimationSystem`; test `ThePointIsOnTheTriangleAndItsNormalPointsIn` |
| Small creatures have bigger eyes for their size | done | test `SmallCreaturesHaveBiggerEyesForTheirSize` |
| The eyes look where the creature looks, or ahead, never back into the head | done | tests `EyesNeverLookBackIntoTheHead`, `TheEyeballFacesAwayFromWhereItLooks` |
| Open eyes turn faster | done | test `OpenEyesTurnFaster` |
| The lids blink every few seconds, closing and opening over 200 milliseconds each | done | `creature_eyes::AdvanceBlink` in `CreatureAnimationSystem`; tests `BlinksCloseAndOpenOverTwoHundredMillisecondsEach`, `EachBlinkDrawsItsWaitOnceAsTheEyesOpenAgain`, `CreatureAnimationSystemTest.TheEyesBlinkAtTheTurnsEndOnTheLocalStream`, `AFrozenCreatureBlinksByTheTimeItLives`. The original's numbers, stream and clock: once a creature turn at its end, by the time the creature lives, the wait drawn on the local random stream. The fluttering eyelids' quicker blink (1250 ms) is `k_FlutterBlinkIntervalMs`, test `CreatureKindActions.TalkingWhenHighFluttersTheEyelidsUntilItTakesUpSomethingElse` |
| While calm the lids follow the pupils up and down | done | tests `CalmLidsSwingShutOverABlink`, `LidAnglesByMode` |
| Closed while asleep or knocked out | done | `CreatureMindSystem` (sleep) and `CreatureFightSystem` (knocked out) set `creature_eyes::Mode::Closed`; tests `CreatureNeedsMind.ItSleepsWithItsEyesClosedUntilRested`, `CreatureEyes.LidAnglesByMode` |
| Wide open when frightened | done | held while the fright plays before running home, frightening villagers in play or for a town, and frightened while high (`Step::heldEyes`, `Eyes::Wide`, applied by `CreatureMindSystem`); tests `CreatureRestAndHome.RunningHomeRunsToTwoHeightsAfterAFrightHalfTheTime`, `CreatureKindActions.HeldEyesLastWhileTheActionPlays`, `CreatureEyes.LidAnglesByMode` |
| Stoned, wide and darting, when high | done | the high agendas hold `Eyes::Stoned` while each action plays (`src/Creature/CreatureKindAgendas.cpp`, `CreatureOddAgendas.cpp`), bigger by 1.1 and slower to look (`creature_eyes`); tests `CreatureKindActions.BeingHighWithoutSittingDown`, `HeldEyesLastWhileTheActionPlays`, `CreatureEyes.SmallCreaturesHaveBiggerEyesForTheirSize` |
| Fluttering eyelids when smiling at a friend, being silly or talking while high | done | `Step::flutterEyelids` (`src/Creature/CreatureFriendAgendas.cpp`, the silly and high agendas), a 1250 ms blink until the next plan; tests `CreatureFriendship.SmilingAtAFriendGoesNearFacesItAndHalfTheTimeShowsItIsHappy`, `CreatureKindActions.TalkingWhenHighFluttersTheEyelidsUntilItTakesUpSomethingElse`, `CreaturePlayerAgendas.BeingSillyDoesFourRoundsOfTalkPlayLaughAndAFace` |
| Yawning or tired, its eyes go sleepy | done | `Eyes::Sleepy` with the yawn and tired actions (`src/Creature/CreatureIdleMind.cpp`), half shut with slower blinks (`ApplyEyes` in `CreatureMindSystem.cpp`); test `CreatureIdleMind.BeingIdleIsWaitingThenYawningTwice` |
| Looking down at its feet, or at its dependants, or at a partner | inferred | resting on the spot it looks at its own feet (`creature_mind::RestOnTheSpot`'s gaze); looks at dependants or a partner are not found in the exe |
| Eye colours or sizes change with alignment | done | eye size follows the evil to good axis, 1 + 0.05 of it, as the original (`creature_eyes::EyeSize`); test `CreatureEyes.SmallCreaturesHaveBiggerEyesForTheirSize` |

## The head and looking

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The head turns towards what the creature looks at, right and left up to half a turn, down and up a quarter turn | done | `creature_layers::TurnHead`, `AnglesTowards` in `CreatureAnimationSystem`; tests `TheHeadNeverTurnsPastItsLimit`, `AnglesTowardsATarget` |
| It speeds up steadily and slows so as to stop exactly there, never overshooting | done | tests `TheHeadSpeedsUpSteadily`, `TheHeadSettlesWithoutOvershooting` |
| Sitting, it turns its head with the sitting look animations | partial 80% | `k_SitLookRightLeft`, `k_SitLookDownUp` in `CreatureAnimationSystem`; no test pins the sitting animations |
| Idle, it looks at the most interesting thing in sight, growing bored of it after a while | done | `creature_look`, candidates gathered by `CreatureMindSystem`; tests `CreatureMindSystemTest.ACreatureLooksAtADoveOverACowAndACowOverAVillager`, `AStepWithSomethingToLookAtHasTheHeadOnIt`; see [idle_behaviour.md](idle_behaviour.md) |
| It looks at the hand, the camera, the player | partial 70% | the hand's pick-up reaction and some actions watch the hand (`Step::Kind::WatchHand`), the silly, high and showing actions look at the camera (`Step::Kind::LookAtCamera`); the hand's pick-up reaction is never made by our hand ([reactions](reactions.md)), and looking at the player when the hand comes near is not found in the exe |
| Walking or running somewhere, it looks where it is going; running from something, back at it | partial 50% | on its way to a point it looks at the point, what interests it and about in turn (`creature_mind::LookWhileGoing`; test `CreatureScriptAgendas.OnTheWayToAPointItLooksAtThePointAtWhatInterestsItAndAboutInTurn`); looking back at what it runs from is not read |
| On its way to a far point, with nothing to watch, it looks for two seconds at the last moving thing it came across | partial 90% | kept as it comes across things (`creature_object_beliefs::MovingSeen`, `KeepsMovingThing`, in `CreatureMindSystem::ComeAcross`) and taken by the walk's second look (`creature_mind::LookWhileGoing`, `MovingSeenRecently`); what moves is asked as the game asks it, the other things by where they were last put (`components::PlacedAt`, `ecs::MovedSincePlaced`) ([mind.md](../../../creature/mind.md#the-last-moving-thing-seen)); tests `CreatureObjectBeliefs.TheLastMovingThingSeenIsKeptAndLookedAtForTwoSeconds`, `CreatureScriptAgendas.WithNothingToWatchTheSecondLookTakesTheLastMovingThingSeen`; two of the original's writers of a thing's place and its perception flag are not matched ([mind.md](../../../creature/mind.md) Pending) |
| It looks up at flying things | done | a thing thrown from a hand, still in the air and seen, makes it watch, amazed, its head on the thing (`creature_reaction_rules::Priority`, `creature_mind::LookAtFlyingObject`), as the original's flying-thing reaction ([reactions](reactions.md)); test `CreatureReactionRules.AThrownThingInTheAirCatchesItsEyeAfterTheWait` |
| It turns its head to show love or hate (the hate to love look animation) | inferred | the creature spec has the animation (79); whether and when the game plays it is not read |

## Hair

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Some species have tufts of hair: groups of strands rooted on triangles of the body | done | `creature_hair`, `CreatureHairSystem`, drawn by `src/Graphics/CreatureDraw.cpp`; tests `StrandsGrowStraightOutOfTheSurface`, `RootFollowsTheBody` |
| Each strand springs back towards growing straight on, sags under gravity, loses speed and never stretches | done | tests `StiffStrandHoldsItsShape`, `LimpStrandSagsUnderGravity`, `StrandNeverStretches`, `SpeedIsCapped` |
| A group's colour, length, stiffness, damping and thickness move from neutral towards evil or good with the alignment | done | tests `ValuesMoveFromNeutralTowardsEvilOrGood`, `LookFollowsAlignment` |
| Strands are drawn as ribbons facing the camera, lit like the land | done | tests `RibbonFacesTheEye`, `StrandsTakeTheLandsLightAndColour`, `CreatureDrawHair.EachStrandIsARibbonInItsTuftsLitColour` |
| Animations can show or hide a tuft at a moment | partial 40% | the moments are read and passed on as a group shown or hidden (`creature_audio::QueueTurn`; test `CreatureSoundMoments.AHairMomentShowsItsGroupOnlyWithModeZero`), but `CreatureAudioSystem` passes no handler, so no tuft is hidden |
| Hair grows with the creature's size | done | test `ScaleGrowsWithSizeButHeadsShrink` |
