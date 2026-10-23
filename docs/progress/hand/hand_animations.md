# Hand animations

Every animation of the god hand and when the game plays it. The animations are in the hand's animation file, named by the
hand spec: each cycle is followed by two lean ranges (sideways, and back and forth) that the trailing cursor blends in.
Some slots in the spec are spare and never used.

**Progress: 24/35 done, 0 chosen, 5 partial, 5 inferred — 80%**

How the original does it, in our wiki: [The hand and the interface](../../../hand/animation-and-placement.md).

## How they play

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The animations are loaded from the hand's animation file, laid out by the hand spec | done | `HandSystem::LoadAnimations` -> `HandAnimator::Load` (`src/3D/HandAnimator.cpp`): `Data/CTR/hh.HBN` laid out by `hndspec5.txt`, through the blob cache; checked by the scripted hand demos (no unit test) |
| Looping cycles run on by the frame's milliseconds, wrapping at their length | done | `HandAnimator::Sample`: a looping clip wraps at its length; `HandAnimator::Update` advances by the frame's ms |
| Each state keeps its own cycle time: the normal state's clock runs on through a change of its pose, the camera state's restarts whenever it is entered and runs on through its pose changes | partial 60% | Of the five states with a clock of their own in the original (camera, citadel, creature, normal, play-anim), three have one in openblack: the camera state's (`CameraHand::animationMs`, restarted on entering, kept through its poses), the play-anim state's (`_playAnimMs`) and the creature state's (`HandStateClock`, zeroed at its Enter, on by the frame's real ms and wrapped at the length of the cycle shown, through its changes of cycle; tests `HandStateClock.*`). Missing: the normal and citadel states', where `HandAnimator` still restarts its one clip time at every change of clip (`HandAnimator::Play`) and the original's normal state runs on modulo the new pose's length ([page](../../../hand/animation-and-placement.md#the-navigation-poses)) |
| A cycle's sideways lean range is blended in by the trailing cursor's sideways gap, clamped to 80 pixels | done | `HandSystem::Update`: lag = clamp(smoothed x - mouse x, -80, 80) / 80, sampled from the `L*_lr` layer (`HandAnimator::ApplyMotion`) |
| Its back-and-forth lean range is blended in by the vertical gap | done | `HandSystem::Update`: clamp(mouse y - smoothed y, -80, 80) for the `L*_fb` layer |
| A lean smaller than a ten-thousandth is left out | done | `HandSystem::Update`: a lag of 0.0001 or less (or a smoothed y that small) is left out |
| Moving the camera, the sideways lean goes the other way | done | `HandSystem::Update`: while gripping the land the sideways lag is mouse x - smoothed x |
| Changing animation blends from the last pose over 0.13 seconds: at a change of the hand's state and at the normal state's change of pose, and in the camera and creature states at their own changes | partial 90% | `HandCrossFade` (`src/3D/HandCrossFade.h`, the hand system's `_stateBlend`): 0.13 s, linear, at a change of the hand's state, at the normal state's change of pose (`_normalPose.Change`, swapping the two pose buffers) and at the creature state's change of cycle to the slap or the caress (not back to the wiggle), as the original; tests `HandCrossFade.FadesAtAnEvenPaceOverATenthAndAThirdOfASecond`, `CreatureTouch.ChangingToTheSlapOrTheCaressStartsTheHandsBlendAndBackToTheWiggleDoesNot`. Missing: the camera state's own blends at its pose changes ([page](../../../hand/animation-and-placement.md#the-hands-clip)) |
| Holding something takes a still frame of a hold, picked by how big the thing is for the hand, leaning as the empty hand does | done | `HandSystem::Update`: `Chold_above` / `Chold_side` held at the frame `magic::hand_hold::HoldTimeMs` gives from the hold radius against the hand's size; the lean layers apply, as the original's holding state's lean clamped to 80 pixels |
| A right hand plays the same animations on the mirrored mesh | partial 70% | `hand_detail::HandModelScale` mirrors the mesh's x for a right hand (the profile's left-handed setting, the Hand menu's "Right Handed"); tests `HandHandedness.*`. Not checked against the original's mirror of its hand at load and its left-handed toggle (wiki Pending, fork issue #28) |
| The bones keep their lengths, and the palm stays at rest in the standing pose | done | `HandAnimator` applies rotations on the bind skeleton, the layers rotation-only (translation only for `Cgrip`); checked by the scripted hand demos (no unit test) |

## The standard set

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Wiggle: the idle fingers, hovering over land, sea or sky | done | `HandSystem::Update`: `Cwiggle` by default, with nothing under the cursor |
| Wiggle in the temple, all the time | done | `HandSystem::Update` runs inside the temple too (the CITADEL state); see [temple_hand.md](temple_hand.md) |
| Wiggle while holding a miracle whose hold is the idle hand | done | `HandSystem::Update`: a spell seed before it is ready (MAGIC hold) holds `Cwiggle` at half its length |
| Wiggle while held to a creature but not touching its body | done | the creature state's cycle (`creature_feedback::NextCycle`, `HandSystem::Update` in hand state 7): `Cwiggle` off the body; test `CreatureTouch.TheHandCaressesWhileStrokingAndKeepsSlappingUntilItStrokesAgain` |
| Point | inferred | Loaded but never chosen; no hand state the wiki reads picks it, and when the game plays it is not found |
| Hold from above: holding things and miracles of that hold | done | `HandSystem::Update`: `Chold_above` for the ABOVE hold type (`HandHolding.cpp` picks the hold type per class) |
| Hold from the side: holding things and miracles of the three side holds, and holding a totem | partial 90% | `Chold_side` for the SIDE, TREE and VILLAGER holds of held objects and seeds, and in the TOTEM state closed by the icon's hold radius (`village_totem::GripTimeMs`, `VillageTotemHandHold.TheSideHoldClosesWithTheIconsHoldRadius`); missing: what the clip's first field is (its length in ms or its frame count) is not read (see [totem.md](totem.md)) |
| Can pick up, fingers open: over a miracle's worship icon | done | `HandSystem::Update`: `Ccan_pickup` over a spell icon, and over anything when the interface hand state is 9 (can pick up) |
| Hold fingers: over a leash rope, or while the leash is being pulled | pending | Not ported: it needs a pick of the leash rope ([page](../../../hand/animation-and-placement.md#the-hands-clip)) |

## On a creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Stroke: the hand's feeling cycle, played hovering over any object | done | `HandSystem::Update`: `Cstroke` over any object with a mesh (or a temple's entrance) when the hand state is not 9 |
| Tickle: the caress played while the hand rests on a creature's body, the cycle the creature state starts in | done | `HandOnCreature::cycle` starts at `TouchCycle::Tickle` and `HandSystem::Update` plays `Ctickle` for it in the CREATURE state; off the body it turns to `Cwiggle` (`creature_feedback::NextCycle`), as the original's state; tests `CreatureTouch.TheHandCaressesWhileStrokingAndKeepsSlappingUntilItStrokesAgain`, `CreatureHandSystemTest.Resting_TheHandStrokes_SweptFastItSlaps` |
| The caress only plays once the hand has rested on the body a second | done | `CreatureHandSystem::Step`: the caress comes back only on the body once the time on it in all is past `creature_feedback::k_StrokeHoldMs` (1000 ms) with the cursor moving across, the time kept off the body; test `CreatureHandSystemTest.TheCaressWaitsForASecondOnTheBodyInAllWithTheCursorMoving` |
| Slap: played once the hand sweeps across the creature faster than five times the creature's height a second, until it strokes again | done | `creature_feedback::NextCycle` -> `Cslap` (`HandSystem::Update`), the 5H of the original's creature update ([page](../../../hand/states.md#the-creature-states-update)); test as above |
| Punch | inferred | Loaded but never chosen; when the game plays it is not found |

## Moving the camera

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Grip: gripping and panning the land | done | `HandSystem::Update`: `Cgrip` while gripping the land, with the layers' translation; see [navigation.md](navigation.md) |
| Rotate: turning the camera, and offered at the sides and bottom by the camera's hints | partial 80% | `hand_navigation_pose::WhileDragging` / `WhileHovering` (`src/3D/HandNavigationPose.h`): `Crotate` from the camera's hints, with its up towards the camera's focus; tests `test/hand/test_hand_navigation_pose.cpp`. Missing: the turning pose the camera's edge turn asks for with its ring cursor |
| Pitch: tilting, and offered at the top | done | `hand_navigation_pose`: `Cpitch` from the camera's hints; tests as above |
| Zoom: only reachable through a camera hint the game never sets, so in effect unused | done | `hand_navigation_pose::Pose::Zoom` (`Czoom`) on the zoom hint, as the original |

## Set animations

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Tap house: played once where the hand is when it knocks on a house's roof, then the hand goes back to its state | done | `AbodeKnockSystem::Tap` -> `HandSystem::StartFixedPosAnimation("Ctap_house")`: the play-anim state plays it once, then back to NORMAL; testbed knock scenes (`TestbedScenarioKnock.cpp`) |
| Knocking also plays one of nine knocking sounds in turn | done | `AbodeKnockSystem::Tap`: InGame sample 110 plus the knock counter (0..8); see [hand_sounds.md](hand_sounds.md) |
| While a set animation plays, the hand stays upright at the place it started | done | `HandPlacement.cpp`: in the play-anim state the hand is pinned at the animation's point with the world up |
| Beckon | inferred | Loaded but never chosen; when the game plays it is not found |

## Special holds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Horn: the horn-shaped hold of some miracles | inferred | `Chorn` / `Cphile` are loaded and `HandAnimator` can blend them (`SetSpecialHold`), but `HandSystem::Update` never passes a special hold; which miracles the original holds so is not read |
| The hold the spec lists after the horn | inferred | Loaded but never chosen; what the game uses it for is not found |
