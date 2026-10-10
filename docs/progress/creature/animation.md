# Creature animation

Every species shares one list of animations, from the creature spec file: moving, faces, sitting and sleeping, the
actions it plays once, looking, picking up, throwing, eating, fighting, dancing, rewards and punishments, recoils,
gestures on top of the body, pointing and catching. Each animation exists for the species' base mesh and may have its
own version for the evil, good, thin and fat meshes, blended as the body is.

**Progress: 39/56 done, 5 partial — 74%**

How the original does it, in our wiki: [Skeletal animation (villagers and animals)](../../bw1-notes/animation.md), [The creature: groundwork, random streams and what is unknown](../../bw1-notes/creature.md).

## Playing animations on the body

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Animations are known by their place in the creature spec file, which every species shares | done | `src/Creature/CreatureLayers.h` (`creature_layers::animations`), `Data/ctrspec27.txt`; test `AnimationNames` |
| Each species' animations come from its body file, for its base, evil, good, thin and fat meshes | done | `CreatureRig` from `Data/CTR/*.cbn` (`src/Resources/Loaders.cpp`); test `CreatureRigLoaderTest.AnimationsFollowTheSpecFile` |
| An animation is the base's pulled towards the evil or good and the thin or fat versions, keyframe by keyframe | done | `creature_animation::Blend` (`src/Creature/CreatureAnimation.cpp`), called by `CreatureAnimationSystem`; tests `TranslationsBlendLinearlyOnBothAxes`, `RotationsBlendOnTheMatrices` |
| A variant without its own version moves as the base does, adjusted by how its stand differs | done | `creature_animation::AdjustFromStand` in `CreatureAnimationSystem`; test `AVariantWithoutItsOwnMovesByItsStand` |
| A bone a variant doesn't move keeps that variant's stand | done | test `BonesAVariantDoesNotMoveTakeItsStand` |
| The rest pose blends as the body does | done | `creature_animation::BlendRest` in `CreatureAnimationSystem`; test `TheRestPoseBlendsAsTheBody` |
| Weak and strong bodies move as the base does | done | `src/Creature/CreatureAnimation.h`: only the evil-good and thin-fat axes are blended |
| Bigger creatures play their animations more slowly | done | `creature_layers::PlaybackRate` in `CreatureAnimationSystem`; test `BiggerCreaturesPlayMoreSlowly` |
| Animations can be played mirrored, the left side's bones swapped with the right's | done | `skeletal_animation::MirrorJoints` (`src/3D/SkeletalAnimation.cpp`); tests `MirrorBonesPairAcrossTheBody`, `AMirroredAnimationMovesTheOtherSide` |
| Changing animation blends smoothly from the last pose into the new one (unconfirmed how the game does it) | todo | each animation starts from its beginning without blending (`src/Creature/CreatureLayers.h`) |

## Layers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Standing, the creature breathes: once every five seconds at size 1, more slowly the bigger it is | done | `creature_animation::BreathPeriod` in `CreatureAnimationSystem`; tests `BreathingTakesFiveSecondsAtSizeOne`, `TheBreathingPeriodEasesToItsTarget`, `CreatureAnimationSystemTest.ProcessTurnEasesTheBreathingButNotTheFatnessShown` |
| Its breathing quickens fast and calms slowly | done | `creature_animation::EaseBreathPeriod`; test `BreathingCalmsSlowlyAndQuickensFast` |
| An action plays once, then the body stands again; nothing new starts while one plays | done | `creature_layers::AdvanceBody`; test `AnActionPlaysOnceThenTheBodyStands` |
| Sitting, sleeping, pooing, being sick and casting are a start, a loop for as long as wanted, and an end | done | test `ASitStartsLoopsUntilToldThenEnds`; the agendas in `src/Creature/CreatureIdleMind.cpp` |
| A loop can hold its last frame, as lying where it fell | done | `creature_layers` hold loop (`holdLoop`) |
| A missing animation ends at once | done | test `AMissingAnimationEndsAtOnce` |
| The head turns on top of the body with the look animations | done | `creature_layers::TurnHead` in `CreatureAnimationSystem`; see [face_eyes_hair.md](face_eyes_hair.md) |
| The face plays on top of the body | done | `creature_layers::AdvanceFace` in `CreatureAnimationSystem`; see [face_eyes_hair.md](face_eyes_hair.md) |
| A gesture plays once on top of the body: nod, head shake, yawn, thirsty, squirting water, talking | partial | the gesture layer is played and drawn (`CreatureAnimationSystem`, test `AGesturePlaysOnce`), but only the debug spawner starts one (`CreatureMindSystem::PlayGesture`); the yawn before sleep is the tired action, not a gesture |
| Walking and running blend with the stand by speed, keeping the feet on the ground | done | see [locomotion.md](locomotion.md) |
| Reaching blends four animations by where the thing lies | done | `creature_reach` in `CreatureObjectActionSystem`; see [object_actions.md](object_actions.md) |
| Fights play their own states and moves at their own pace | done | `CreatureFightSystem`; see [fighting.md](fighting.md) |

## The animation set

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Moving: stand, walk, run, turning on the spot and stepping off at 0, 90 and 180 degrees each way | done | see [locomotion.md](locomotion.md) |
| A jump to the side (unconfirmed what plays it) | todo | |
| Sleep, poo, be sick, sit | done | the mind's agendas (`creature_mind::Sleep`, `SitDown` in `src/Creature/CreatureIdleMind.cpp`) |
| Cast and cast up (the powered-up casting pose) | partial | the casting pose plays in fights (`fight::animations::k_Cast`, see [creature_casting.md](creature_casting.md)); the powered-up pose is unconfirmed |
| Scatter (unconfirmed what plays it) | todo | |
| The actions played once: summon, angry, hungry, happy, sad, tired, hot, cold, scratch, frightened, sneeze, confused, feeling nice, impress, need a poo, feel playful, play, look at me, taunt, drink, friendly wave, embarrassed, pick me | partial | most are played by the mind's actions (`src/Creature/CreaturePlanActions.cpp`); embarrassed and play have no named animation or action yet (`creature_layers::animations`) |
| Relaxed stand | todo | no relaxed stand animation is named or played |
| Picking up front and back, left and right, holding, taking from the hand | partial | reaching and holding play (`CreatureObjectActionSystem`); taking from the hand is todo (see [object_actions.md](object_actions.md)) |
| Throwing flat and high, tossing away, eating, putting down, lobbing gently | done | `creature_throw` in `CreatureObjectActionSystem`; see [object_actions.md](object_actions.md) |
| Stroking, shaking, smelling and examining what it holds | done | `creature_object_actions::Kind::Keep` |
| Knocking things down front and back, left and right, and kicking low | done | `creature_reach::k_DestroyAnimations` in `CreatureObjectActionSystem` |
| Fainting, getting up, powering up a blow, the creation pose (unconfirmed) | partial | faint and get up play in fights, power-up in the fight's stance (`CreatureFightSystem`); the creation pose doesn't |
| Fight start, stance, finish, blows at five heights and the special, twice over, steps, blocks | done | see [fighting.md](fighting.md) |
| Rewards for each part of the body stroked and punishments high, middle and low, hard and gentle | done | see [learning_from_feedback.md](learning_from_feedback.md) |
| Recoils from blows high, middle and low, to each side, top and bottom, and wobbles | done | see [fighting.md](fighting.md) |
| Pointing low and high, left and right | done | `creature_object_actions::k_PointAnimations` in `CreatureObjectActionSystem` |
| Kissing high and low, howling, laughing, crying, aha, praying, rude gesture | todo | the actions that play them (see [friends_and_other_creatures.md](friends_and_other_creatures.md)) are todo |
| Dances: start, finish and five dance moves | todo | see [town_actions.md](town_actions.md) |
| Catching: stepping right, back and front, catching high and low to each side | todo | no catching in our tree: no catch clips are blended and nothing plays them |
| Three animations reserved for scripts | todo | |

## Animation events

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Sounds are placed on moments of animations: footsteps as feet land, roars as the mouth opens, snores | done | `creature_audio` in `CreatureAudioSystem`; tests `EventsFireOnceAcrossFrames`, `LoopWrapsWithoutDoubleFiring` |
| Mirrored animations pass the same events | done | test `MirroredAnimationPassesTheSameEvents` |
| Sounds don't depend on the frame rate | done | test `SoundsDoNotDependOnTheFrameRate` |
| A sound is picked by the creature's size, species, the ground under it and the action | done | tests `SizeKey`, `KeysInBankOrder`, `TheKeysAreTheKeyTheBanksArePlayedBy`; the ground from `ecs::sea_cells::GetSurfaceType` |
| Voices come from the species' own bank; other players' creatures are heard in their voices only when a script allows | done | `CreatureAudioSystem`; tests `VoiceBankStem`, `CreatureAudioSystemTest.AnotherPlayersCreatureSpeaksOnlyWhenTheScriptLetsIt`; see [../audio](../audio/) |
| Footsteps, blows, snores, eating and drinking come from the bank all creatures share | done | `creature_audio::EventKind::Generic`; test `CreatureAudioRequest.TheSharedBankForFootstepsAndTheSpeciesOwnForItsVoice` |
| Things are taken hold of and let go at a moment of the animation particular to the species | done | `CreatureObjectActionSystem` (the action's event moment from the rig) |
| Footprints are laid as feet land | done | `FootprintSystem`; see [locomotion.md](locomotion.md) |
| Scripts can turn a creature's sounds on or off | done | `SET_CREATURE_SOUND` sets `audio::GetScriptAudioState().creatureSound` (`src/CHLApi.cpp`), which `CreatureAudioSystem` reads; test `TheScriptLetsOtherVoicesBeHeard` |

## Scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts can play a static or individual animation on a creature | todo | the creature parts of the script-state natives are stubs (`src/CHLApi.cpp`) |
| Scripts can override a thing's state animation | todo | `OVERRIDE_STATE_ANIMATION` works for villagers and animals but is a stub for creatures (`src/CHLApi.cpp`) |
| Scripts can turn the animation speed changes on or off | todo | `SET_ANIMATION_MODIFY` is a stub |
| Scripts can play a gesture | todo | `PLAY_GESTURE` is a stub |
| The debug spawner plays any animation, face or gesture | done | `src/Debug/CreatureSpawner.cpp` (openblack only) |
