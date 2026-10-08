# Leash

The leash is how the player leads their creature. Held in the hand it makes the creature follow; tied to something it
keeps the creature there and has it act on that thing. There are three leashes: the learning leash (a plain rope), and
the aggression and compassion leashes, which make the creature angry or kind for as long as it wears them.

**Progress: 30/51 done, 8 partial — 67%**

How the original does it, in our wiki: [The creature: groundwork, random streams and what is unknown](../../bw1-notes/creature.md).

## Which creature and which leash

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each player can lead exactly one creature, their own; other players' creatures and creatures of nobody can't be leashed | done | `src/Creature/LeashOwnership`, `LeashSystem::WhyNot` (`src/ECS/Systems/Implementations/LeashSystem.cpp`); tests `LeashOwnership.*`, `LeashSystemTest.WhoMayLeadWhichCreature` |
| A player's first creature becomes the one they lead; giving a creature away takes its leash off | done | `LeashSystem::ClaimOnArrival` (from `CreatureArchetype::Create`), `SetOwner`; tests `LeashOwnership.APlayersFirstCreatureClaimsTheLeash`, `MakingOneLeashableDisplacesTheOwnersOther` |
| The creature must know a leash before it can wear it, the learning leash first | done | `LeashSystem::Knows` and `SetKnown`; tests `LeashOwnership.ItMustKnowTheLeashes`, `LeashKeys.TheLeashKeyNeedsTheLearningLeash` |
| The creature is given the learning leash as it starts to grow up, and the aggression and compassion leashes at the stage of growing up that teaches good and evil | partial | `DEV_FUNCTION` 2 and 3 set them known (`ecs::player_creature::DevFunction` in `src/ECS/PlayerCreature.cpp`; tests `PlayerCreatureNativesTest.DevFunctionTwoTeachesTheRopeLeash`, `DevFunctionThreeTeachesTheOtherLeashesAndHandsTheCreatureOver`); the stages themselves are in [development_phases.md](development_phases.md); the shipped lessons: the Leash of Learning in its own lesson, the other two shown in the tying lesson ([../story/gold_scrolls/the_creatures_learning.md](../story/gold_scrolls/the_creatures_learning.md#lesson-4-the-leash-of-learning)) |
| The leash can only be tied to things once the creature has grown past its early stages | todo | `LeashSystem::TieTo` checks no stage of growing up |
| The citadel hangs a post for each leash the player has; tapping a post picks that leash, tapping it again unpicks it | todo | no leash posts in our tree (test `LeashSystemTest.ALandWithATempleGetsNoLeashPosts`) |
| The temple shows the collar of the leash worn | todo | see [../temple](../temple/) |
| The hand's tooltip over a post or the leashed creature says what a tap does | todo |  |

## Putting it on and taking it off

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Clicking the player's own creature with the right button puts the picked leash on | done | a let-go within 450 ms of camera time is a click (`creature_hand::IsClick`), sent as the leash key (`ECS/CreatureHandPackets.cpp`); tests `CreatureHand.LetGoWithinAClicksCameraTimeIsAClick`, `CreatureHandPacketsTest.TheClickIsThePlayersLeashKey` |
| L toggles the leash: puts the picked leash on, unties a tied leash back to the hand, or takes it off | done | `src/Creature/LeashKeys`, read each frame by `ecs::creature_loop::ProcessLeashKeys`; tests `LeashKeys.TheLeashKeyPutsOnUntiesAndTakesOff`, `TheGameBindsLVAndB`, `LeashKeysInputTest.*` |
| V and B step the picked leash through the leashes the creature knows, swapping the one worn | done | `LeashKeys`; tests `LeashKeys.VStepsUpAndBDownThroughTheLeashNumbers`, `AStepOntoAnUnknownLeashDoesNothing` |
| Drawing the leash gesture opens a picker of the known leashes, each drawn as a gesture to choose | todo | the leash gesture is not recognised and there is no picker (`src/Creature/LeashKeys.h`); see [../gesture](../gesture/) |
| A scribble drawn with the empty hand shakes off a leash held in the hand; a tied leash stays | todo | no shake in our tree (raffclar's shake tracker was left out); a hand demo takes the held leash off (`ecs::creature_loop::ReleaseLeashHeldInHand`) |
| After putting a leash on there is a short delay before the hand can hand out another | todo |  |
| The leash goes on with its own sounds, and the two tying sounds play in turn | partial | the two tying sounds play in turn when it is tied (`LeashSystem::TieTo`); no sound found for putting it on |
| Leashes can't be put on a creature in a fight or knocked out, and don't pull it about | partial | `LeashSystem::ProcessTurn` skips fighting or knocked-out creatures, so they are not pulled; putting a leash on them is not refused |

## The rope

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The rope is a chain of masses on springs from the hand (or the thing tied to) to the creature's collar, swinging and sagging | done | `src/Creature/LeashRope`, stepped by `LeashSystem::Update` (`ecs::creature_loop::UpdateLeash`); tests `LeashRope.*` |
| It is stepped 200 times a second however long the frame, with a cap on catching up | done | `leash_rope::k_StepSeconds`, `k_MaxSteps`; test `LeashRope.NoTimeMeansOnlyTheEndsMove` |
| It is kept off the ground and inside the world | done | tests `LeashRope.StaysOffTheGround`, `EndsAreKeptWithinTheWorld` |
| It is drawn as a ribbon facing the camera, with a shadow strip on the land below | done | `src/Graphics/RendererLeash.cpp`, `src/Graphics/LeashDraw`; tests `LeashRope.RibbonFacesTheEyeAndShadowLiesOnTheLand`, `LeashDraw.*` |
| Each leash looks its own (its band of the leash texture), the compassion leash thicker | done | `creature_leash::LookFor`; test `LeashRules.EachLeashLooksItsOwn` |
| Held in the hand, the leash is as long as the creature is big | done | `creature_leash::InHand`; test `LeashRules.HandLengthsGrowWithTheCreature` |
| Tied to something still, it reaches one and a half times the distance to it, within limits; tied to something moving, seven times the creature's height | done | `TiedToStatic`, `TiedToMobile` in `src/Creature/LeashRules`; test `LeashRules.TiedLengths` |
| Its tension, slack to taut, is what pulls the creature | done | tests `LeashRope.ASlackRopeHasNoTension`, `StretchedToItsFullLengthIsTaut`, `LeashSystemTest.ATautRopePullsTheCreatureToTheHand` |
| Scripts can hide all leashes | done | `SET_DRAW_LEASH` in `src/CHLApi.cpp` sets `drawLeash`, read by `RendererLeash.cpp`; test `LeashDraw.TheRopesAreThoseDrawnAndNoneWhileHidden` |
| In a network game the leash's pull is sent to the other players | todo | see [../multiplayer](../multiplayer/) |

## Leading the creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Pulled taut, the creature stops what it is doing and walks to the hand | done | `ShouldPull`, `DecideLead`, `CreatureLocomotionSystem::LeadTo`; tests `LeashRules.PullsOnlyWhenTaut`, `LeadsToTheHand` |
| The harder the pull the faster it goes, up to twice its running pace; the pull fades once it is under way | done | `FadePull`; tests `LeashRules.PullFades`, `CreatureLocomotion.PullingTheLeashSpeedsItUp` |
| Close to the hand it stays where it is | done | `k_CloseToHand` in `src/Creature/LeashRules` |
| Pulled away from acting on the same desire twice, it goes off that desire for a while | done | `RecordPull`; test `LeashRules.SecondPullHoldsTheDesireBack` |
| While leashed it can be dragged over land it could not walk | todo | (unconfirmed) |
| A leashed creature that strays out of reach of the hand or tied thing is brought back | done | `LeashSystem::ProcessTurn` walks it back within the rope's length |
| The leash overrides what a script has the creature doing | todo | (unconfirmed which scripts it overrides) |
| Scripts can make the leash do nothing while it is worn | done | `SET_LEASH_WORKS`, `creature_leash::script::SetWorks`, `LeashSystem::SetWorks`; tests `LeashScriptTest.SetWorksTakesAnyValueButZeroAsSet`, `LeashSystemTest.ALeashThatDoesNotWorkPullsNothing` |
| The creature is kept near home while it starts to grow up, unless led on a working leash | partial | `LeashSystem::ConfineToHome`, `leash::IsConfined` (test `LeashRules.Confinement`, `LeashSystemTest.KeptNearHomeItWalksBack`), but only the debug spawner confines a creature |
| Away from home and its player has no temple, it isn't free to roam | done | `leash::FreeOfHome`, `LeashSystem::FreeOfHome` |

## Feelings and lessons

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The aggression leash makes the creature angry, the compassion leash kind, above every other desire, while worn | done | `leash::ForcedDesireFor` set each turn by `LeashSystem::ProcessTurn`, read by the mind; test `LeashRules.LeashesForceTheirFeelings` |
| The learning leash makes the creature watch the player more keenly: each miracle seen counts three times | partial | `leash::MiracleSightingWeight`; sightings only come from the debug spawner; see [learning_by_observation.md](learning_by_observation.md) |
| On the learning leash in the hand, the creature copies what the player does | partial | `learningInHand` reaches the copying rules; only four deeds are reported; see [learning_by_observation.md](learning_by_observation.md) |
| Tied to something, the creature acts on it, choosing an action for the desire the leash gives it | done | `actOn`, set by `LeashSystem::TieTo`, read in `CreatureMindLearning.cpp`; tying is only done by scripts |
| Tying the leash to something teaches the creature which desire to act on it with: anger over compassion on the aggression leash, the reverse on the compassion leash | done | `leash::LessonsFor`; test `LeashRules.LessonsOfWhatThePlayerShows` |
| Tied to a village centre on any leash but aggression, the creature wants to impress that village | partial | `LeashSystem::ProcessTurn` forces the impress desire (`k_ImpressTownValue`); impressing towns is mostly todo in [town_actions.md](town_actions.md) |
| Holding something in the hand while the creature is on the leash has it act on that thing | todo |  |
| Tied to another creature on the aggression leash, the two fight | done | `CreatureFightSystem` takes the tied creature through `actOn` (see [fighting.md](fighting.md)) |
| Tied to another creature, the other gets angry too if near enough, and they warm (compassion) or cool (aggression) to each other over time | done | `LeashSystem::ProcessTurn`, `leash::AttitudeChange`; test `LeashRules.LeashedCreaturesWarmOrCoolToEachOther` |
| At the tying stage of growing up the creature is taught to stay where its leash is tied; the stage was planned around a house, but the shipped lesson ties it to a palm tree by the pen | partial | tying works through the scripts; the stage's lesson and help text are in [development_phases.md](development_phases.md); the shipped lesson: [../story/gold_scrolls/the_creatures_learning.md](../story/gold_scrolls/the_creatures_learning.md#lesson-5-tying-the-leash) |

## Scripts and computer players

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts can tie a creature's leash to an object, back to the hand, or take it off | done | `ATTACH_OBJECT_LEASH_TO_OBJECT`, `ATTACH_OBJECT_LEASH_TO_HAND`, `DETACH_OBJECT_LEASH` in `src/CHLApi.cpp` through `src/Creature/LeashScript`; tests `LeashScriptTest.*`; the leash service may refuse where the original always unties and turns the leash on ([creature.md](../../bw1-notes/creature.md#what-openblack-does-with-the-leash-natives)) |
| Scripts can ask whether a creature is leashed, to what, and on which leash | done | `IS_LEASHED`, `IS_LEASHED_TO_OBJECT`, `GET_OBJECT_LEASH_TYPE` (`src/Creature/LeashScript`); tests `LeashScriptTest.IsLeashedAsksTheLeash`, `IsLeashedToThingComparesTheTie`, `TypeOfNumbersThePickedLeashAsTheScriptsDo`; the Pied Piper is caught by tying the creature's leash to him, a person, and dragging him from his cave: [the_pied_piper.md](../story/silver_scrolls/the_pied_piper.md) |
| Scripts can toggle a player's leash as the key does | done | `TOGGLE_LEASH`; test `LeashScriptTest.TogglePressesTheLeashKeyForTheScriptsPlayer` |
| Computer players leash their creatures to things and to other creatures to teach or fight | todo | see [../multiplayer](../multiplayer/) and [learning_by_observation.md](learning_by_observation.md) |
| Leash state is saved and loaded with the game | todo | no saved games; see [../engine](../engine/) |
