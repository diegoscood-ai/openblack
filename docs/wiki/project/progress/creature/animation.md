# Creature animation

Every species shares one list of animations, from the creature spec file: moving, faces, sitting and sleeping, the
actions it plays once, looking, picking up, throwing, eating, fighting, dancing, rewards and punishments, recoils,
gestures on top of the body, pointing and catching. Each animation exists for the species' base mesh and may have its
own version for the evil, good, thin and fat meshes, blended as the body is.

**Progress: 36/57 done, 0 chosen, 10 partial, 6 inferred — 76%**

How the original does it, in our wiki: [Skeletal animation (villagers and animals)](../../../beings/animation.md), [The creature: groundwork, random streams and what is unknown](../../../creature/pose-and-animation.md).

## Playing animations on the body

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Animations are known by their place in the creature spec file, which every species shares | done | `src/Creature/CreatureLayers.h` (`creature_layers::animations`), `Data/ctrspec27.txt`; test `AnimationNames` |
| Each species' animations come from its body file, for its base, evil, good, thin and fat meshes | done | `CreatureRig` from `Data/CTR/*.cbn` (`src/Resources/Loaders.cpp`); test `CreatureRigLoaderTest.AnimationsFollowTheSpecFile` |
| An animation is the base's pulled towards the evil or good and the thin or fat versions, keyframe by keyframe | done | `creature_animation::Blend` (`src/Creature/CreatureAnimation.cpp`), called by `CreatureAnimationSystem`; tests `TranslationsBlendLinearlyOnBothAxes`, `RotationsBlendOnTheMatrices` |
| A variant without its own version moves as the base does, adjusted by how its stand differs | done | `creature_animation::AdjustFromStand` in `CreatureAnimationSystem`; test `AVariantWithoutItsOwnMovesByItsStand` |
| A bone a variant doesn't move keeps that variant's stand | done | test `BonesAVariantDoesNotMoveTakeItsStand` |
| The rest pose blends as the body does | done | `creature_animation::BlendRest` in `CreatureAnimationSystem`; test `TheRestPoseBlendsAsTheBody` |
| Weak and strong bodies move as the base does | inferred | `src/Creature/CreatureAnimation.h`: only the evil-good and thin-fat axes are blended, as the body file has only those meshes (row above); no test, and whether the original's strength changes how the body moves is not read |
| Bigger creatures play their animations more slowly | done | `creature_layers::PlaybackRate` in `CreatureAnimationSystem`; test `BiggerCreaturesPlayMoreSlowly` |
| Animations can be played mirrored, the left side's bones swapped with the right's | done | `skeletal_animation::MirrorJoints` (`src/3D/SkeletalAnimation.cpp`); tests `MirrorBonesPairAcrossTheBody`, `AMirroredAnimationMovesTheOtherSide` |
| Changing animation blends smoothly from the last pose into the new one (unconfirmed how the game does it) | inferred | each animation starts from its beginning without blending (`src/Creature/CreatureLayers.h`); whether the original blends from the last pose into a new animation is not read (its blended actions, `StartBlendedAction`, are the reach blends of picking up and knocking down) |

## Layers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Standing, the creature breathes: once every five seconds at size 1, more slowly the bigger it is | partial 85% | `creature_animation::BreathPeriod` in `CreatureAnimationSystem`: 5·√size, as the original (`SetSize` 0x480628..0x48063C); tests `BreathingTakesFiveSecondsAtSizeOne`, `TheBreathingPeriodEasesToItsTarget`, `CreatureAnimationSystemTest.ProcessTurnEasesTheBreathingButNotTheFatnessShown`. The original writes the period as both the current and the target each turn; ours eases to it (queued as a fix; [fights.md](../../../creature/fights.md#pending)) |
| Its breathing quickens fast and calms slowly | inferred | `creature_animation::EaseBreathPeriod`; test `BreathingCalmsSlowlyAndQuickensFast`. The original has a breath ease (`ProcessBreath`), but its rates are not read, and `SetSize` sets the current period each turn, so whether the breathing ever eases in the original is not settled |
| An action plays once, then the body stands again; nothing new starts while one plays | done | `creature_layers::AdvanceBody`; test `AnActionPlaysOnceThenTheBodyStands` |
| Sitting, sleeping, pooing, being sick and casting are a start, a loop for as long as wanted, and an end | done | test `ASitStartsLoopsUntilToldThenEnds`; the agendas in `src/Creature/CreatureIdleMind.cpp` |
| A loop can hold its last frame, as lying where it fell | done | `creature_layers` hold loop (`holdLoop`): a faint lies at the time its fall's last update stood, just short of its end, and that time never moves; a faint got up while it still falls gets up at once (`src/Creature/CreatureLayers.cpp`; tests in `test_creature_layers`) |
| What follows a part that ends plays in the same turn: a loop's end, the loop after its start, an end wanted during the start (skipping the loop), a dance's first move or finish, each moved on by that turn's step | done | the body step by the turn and by the frame (`creature_layers::AdvanceBodyByTurn`, `AdvanceBody`, with each part's own length); tests in `test_creature_layers`, `test_creature_animation_system`, `test_tattoo_editor` |
| The body's animation clock moves on once a turn by whole milliseconds, its step slowed by the creature's size | partial 70% | the turn's advance (`creature_layers::AdvanceBodyByTurn`, `creature_loop`), as the original's `UpdateTime`; tests in `test_creature_layers`, `test_creature_animation_system`. Still not as the original: the object actions step by a fixed 100 ms at the float rate, the fight's moves without `GetFightMul`'s casts, the walk's cycles and the stand slot as floats where the original cuts each to whole ms ([pose-and-animation.md](../../../creature/pose-and-animation.md#pending)) |
| A missing animation ends at once | done | test `AMissingAnimationEndsAtOnce` |
| The head turns on top of the body with the look animations | done | `creature_layers::TurnHead` in `CreatureAnimationSystem`; see [face_eyes_hair.md](face_eyes_hair.md) |
| The face plays on top of the body | done | `creature_layers::AdvanceFace` in `CreatureAnimationSystem`; see [face_eyes_hair.md](face_eyes_hair.md) |
| A gesture plays once on top of the body: nod, head shake, yawn, thirsty, squirting water, talking | partial 30% | the gesture layer is played and drawn (`CreatureAnimationSystem`, test `AGesturePlaysOnce`), but only the debug spawner starts one (`CreatureMindSystem::PlayGesture`): no ported action plays a nod, head shake, thirsty, squirt or talk gesture; the yawn before sleep is the tired action, not a gesture |
| Walking and running blend with the stand by speed, keeping the feet on the ground | done | see [locomotion.md](locomotion.md) |
| Reaching blends four animations by where the thing lies | done | `creature_reach` in `CreatureObjectActionSystem`; see [object_actions.md](object_actions.md) |
| Fights play their own states and moves at their own pace | done | `CreatureFightSystem`; see [fighting.md](fighting.md) |

## The animation set

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Moving: stand, walk, run, turning on the spot and stepping off at 0, 90 and 180 degrees each way | done | see [locomotion.md](locomotion.md) |
| A jump to the side (unconfirmed what plays it) | inferred | the spec file has the clip (`r_jump_l`); which of the original's actions plays it is not read |
| Sleep, poo, be sick, sit | done | the mind's agendas (`creature_mind::Sleep`, `SitDown` in `src/Creature/CreatureIdleMind.cpp`) |
| Cast and cast up (the powered-up casting pose) | partial 50% | the casting pose plays in fights (`fight::animations::k_Cast`) and in the creature's own casts (see [creature_casting.md](creature_casting.md)); which action plays the powered-up pose is not read |
| Scatter (unconfirmed what plays it) | inferred | the spec file has the start, loop and end of a scatter; which of the original's actions plays them is not read |
| The actions played once: summon, angry, hungry, happy, sad, tired, hot, cold, scratch, frightened, sneeze, confused, feeling nice, impress, need a poo, feel playful, play, look at me, taunt, drink, friendly wave, embarrassed, pick me | partial 90% | played by the mind's actions (`src/Creature/CreaturePlanActions.cpp`), embarrassed after a fizzled cast (`CreatureMindCasting.cpp`); play (`play_action`) is played by no ported action |
| Relaxed stand | inferred | the spec file has a relaxed stand; no action plays it in openblack, and which of the original's actions plays it is not read |
| Picking up front and back, left and right, holding, taking from the hand | partial 90% | reaching blends the four pick-ups and holding plays (`CreatureObjectActionSystem`); taking from the hand plays its clip (86, `CreatureObjectActionSystem::TakeFromHand`, tests in `test_creature_object_action_system`), unverified against v1.42 (see [feeding_and_thrown_things.md](feeding_and_thrown_things.md)) |
| Throwing flat and high, tossing away, eating, putting down, lobbing gently | done | `creature_throw` in `CreatureObjectActionSystem`; see [object_actions.md](object_actions.md) |
| Stroking, shaking, smelling and examining what it holds | done | `creature_object_actions::Kind::Keep` |
| Knocking things down front and back, left and right, and kicking low | done | `creature_reach::k_DestroyAnimations` in `CreatureObjectActionSystem` |
| Fainting, getting up, powering up a blow, the creation pose (unconfirmed) | done | the faint and get-up play in fights and in any faint (`CreatureFightSystem::KnockOut`, `ForceFaint`), the power-up while a charged blow waits in the stance (`CreatureFightSystem`), the creation pose (109) as a mind leaves for the other body in a swap (`SWAP_CREATURE`, `CreatureMindSwap.cpp`; test `test_creature_swap`); tests in `test_creature_fight_turns` |
| Fight start, stance, finish, blows at five heights and the special, twice over, steps, blocks | done | see [fighting.md](fighting.md) |
| Rewards for each part of the body stroked and punishments high, middle and low, hard and gentle | done | see [learning_from_feedback.md](learning_from_feedback.md) |
| Recoils from blows high, middle and low, to each side, top and bottom, and wobbles | partial 80% | see [fighting.md](fighting.md): which of the top or bottom reels a high or low blow plays is a guess |
| Pointing low and high, left and right | done | `creature_object_actions::k_PointAnimations` in `CreatureObjectActionSystem` |
| Kissing high and low, howling, laughing, crying, aha, praying, rude gesture | partial 90% | kissing high (`creature_town_help::KissVillager`), howling (`CreatureFriendAgendas.cpp`), laughing, praying and the rude sign (`CreaturePlayerAgendas.cpp`, `CreatureRestAndHome.cpp`; tests `test_creature_player_attention`, `test_creature_rest_and_home`), crying and aha (219, 220, the dazed looks in `CreatureKindAgendas.h`) are played; the low kiss (213) by no ported action |
| Dances: start, finish and five dance moves | partial 85% | ported from raffclar PR #520: the start, the first move drawn among seven as the original's `DO_DANCE` (GameRand(7) + 0x93 until the species has it), then of five, and the finish (`creature_layers::PlayDance`, `AdvanceBodyByTurn`; tests in `test_creature_layers`); each move's length by the dance's rate is not yet compared with `DO_DANCE` ([pose-and-animation.md](../../../creature/pose-and-animation.md)) |
| Catching: stepping right, back and front, catching high and low to each side | pending | no catching in our tree: no catch clips are blended and nothing plays them |
| Three animations reserved for scripts | pending | |

## Animation events

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Sounds are placed on moments of animations: footsteps as feet land, roars as the mouth opens, snores | done | `creature_audio` in `CreatureAudioSystem`; tests `EventsFireOnceAcrossFrames`, `LoopWrapsWithoutDoubleFiring` |
| Mirrored animations pass the same events | done | test `MirroredAnimationPassesTheSameEvents` |
| Sounds don't depend on the frame rate | partial 70% | an event fires once whatever the frames (tests `EventsFireOnceAcrossFrames`, `FractionWithinTheFrame`, `LoopWrapsWithoutDoubleFiring`); the sounds take their moments from the frames' layer times, which jump once a turn, so they bunch on the frame after a turn, where the original queues them by the turn and plays them the next (`PlayQueuedSoundEffects`; [pose-and-animation.md](../../../creature/pose-and-animation.md#pending)) |
| A sound is picked by the creature's size, species, the ground under it and the action | done | tests `SizeKey`, `KeysInBankOrder`, `TheKeysAreTheKeyTheBanksArePlayedBy`; the ground from `ecs::sea_cells::GetSurfaceType` |
| Voices come from the species' own bank; other players' creatures are heard in their voices only when a script allows | done | `CreatureAudioSystem`; tests `VoiceBankStem`, `CreatureAudioSystemTest.AnotherPlayersCreatureSpeaksOnlyWhenTheScriptLetsIt`; see [../audio](../audio/) |
| Footsteps, blows, snores, eating and drinking come from the bank all creatures share | done | `creature_audio::EventKind::Generic`; test `CreatureAudioRequest.TheSharedBankForFootstepsAndTheSpeciesOwnForItsVoice` |
| Things are taken hold of and let go at a moment of the animation particular to the species | done | `CreatureObjectActionSystem` (the action's event moment from the rig) |
| Footprints are laid as feet land | done | `FootprintSystem`; see [locomotion.md](locomotion.md) |
| Moments of an animation light and put out the creed glows on the hands and the belly | done | the creed glow actions 95..109 after the sound, heard or not (`CreatureAnimationSystem::PlayCreedEvent`, `creature_hand_glow::CreedEventOf`; tests `CreatureCreedPower.TheCreedMomentsSetTheLeftHandThenTheRightHandThenTheBelly`, `CreatureCreedPowerSystem.ACreedMomentSetsAGlowWithNoHumAndTheFirstMomentMakesTheGlows`); only the `script1` animation has them ([the Creed glows](../../../creature/drawing.md#the-creed-glows)) |
| Scripts can turn a creature's sounds on or off | done | `SET_CREATURE_SOUND` sets `audio::GetScriptAudioState().creatureSound` (`src/CHLApi.cpp`), which `CreatureAudioSystem` reads; test `TheScriptLetsOtherVoicesBeHeard` |

## Scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts can play a static or individual animation on a creature | pending | the creature parts of the script-state natives are stubs (`src/CHLApi.cpp`) |
| Scripts can override a thing's state animation | pending | `OVERRIDE_STATE_ANIMATION` works for villagers and animals but is a stub for creatures (`src/CHLApi.cpp`) |
| Scripts can turn the animation speed changes on or off | pending | `SET_ANIMATION_MODIFY` is a stub; the original's sets the creature's animation time modify (`Creature::SetAnimationTimeModify` 0x47C650) |
