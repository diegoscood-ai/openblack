# Hand animations

Every animation of the god hand and when the game plays it. The animations are in the hand's animation file, named by the
hand spec: each cycle is followed by two lean ranges (sideways, and back and forth) that the trailing cursor blends in.
Some slots in the spec are spare and never used.

**Progress: 19/35 done, 5 partial — 61%**

How the original does it, in our wiki: [The hand and the interface](../../bw1-notes/hand-and-interface.md).

## How they play

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The animations are loaded from the hand's animation file, laid out by the hand spec | done | `HandSystem::LoadAnimations` -> `HandAnimator::Load` (`src/3D/HandAnimator.cpp`): `Data/CTR/hh.HBN` laid out by `hndspec5.txt`, through the blob cache; no test in our tree |
| Looping cycles run on by the frame's milliseconds, wrapping at their length | done | `HandAnimator::Sample`: a looping clip wraps at its length; `HandAnimator::Update` advances by the frame's ms |
| Each state keeps its own cycle time; the camera state's restarts whenever it is entered | partial | `HandAnimator` has one clip time, restarted at every clip change (`HandAnimator::Play`), not one per state |
| A cycle's sideways lean range is blended in by the trailing cursor's sideways gap, clamped to 80 pixels | done | `HandSystem::Update`: lag = clamp(smoothed x - mouse x, -80, 80) / 80, sampled from the `L*_lr` layer (`HandAnimator::ApplyMotion`) |
| Its back-and-forth lean range is blended in by the vertical gap | done | `HandSystem::Update`: clamp(mouse y - smoothed y, -80, 80) for the `L*_fb` layer |
| A lean smaller than a ten-thousandth is left out | done | `HandSystem::Update`: a lag of 0.0001 or less (or a smoothed y that small) is left out |
| Moving the camera, the sideways lean goes the other way | done | `HandSystem::Update`: while gripping the land the sideways lag is mouse x - smoothed x |
| Changing animation blends from the last pose over 0.13 seconds | done | `HandCrossFade` (`src/3D/HandCrossFade.h`, the hand system's `_stateBlend`): 0.13 s, linear, in world space, only at a change of the hand's state; test `HandCrossFade.FadesAtAnEvenPaceOverATenthAndAThirdOfASecond`. Our wiki differs: the original blends only when the hand's state changes; a change of clip inside a state is not blended ([page](../../bw1-notes/hand-and-interface.md#the-hands-clip-handstatenormal)) |
| Holding something takes a still frame of a hold, picked by how big the thing is for the hand, without leaning | partial | `HandSystem::Update`: `Chold_above` / `Chold_side` held at a frame set by the hold radius against the hand's size, for every held object; the hold's lean layers are still applied when the file has them |
| A right hand plays the same animations on the mirrored mesh | todo | Our hand is drawn unmirrored and has no left-handed mode (`HandSystem::Initialize` notes it as pending) |
| The bones keep their lengths, and the palm stays at rest in the standing pose | done | `HandAnimator` applies rotations on the bind skeleton, the layers rotation-only (translation only for `Cgrip`); no test in our tree |

## The standard set

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Wiggle: the idle fingers, hovering over land, sea or sky | done | `HandSystem::Update`: `Cwiggle` by default, with nothing under the cursor |
| Wiggle in the temple, all the time | done | `HandSystem::Update` runs inside the temple too (the CITADEL state); see [temple_hand.md](temple_hand.md) |
| Wiggle while holding a miracle whose hold is the idle hand | done | `HandSystem::Update`: a spell seed before it is ready (MAGIC hold) holds `Cwiggle` at half its length |
| Wiggle while held to a creature but not touching its body | partial | Our hand keeps the normal state's rule over a creature: `Cstroke` while the creature is under the cursor, not the wiggle |
| Point | todo | Loaded but never chosen (unconfirmed when the game plays it) |
| Hold from above: holding things and miracles of that hold | done | `HandSystem::Update`: `Chold_above` for the ABOVE hold type (`HandHolding.cpp` picks the hold type per class) |
| Hold from the side: holding things and miracles of the three side holds, and holding a totem | partial | `Chold_side` for the SIDE, TREE and VILLAGER holds of held objects and seeds; no totem (see [totem.md](totem.md)) |
| Can pick up, fingers open: over a miracle's worship icon | done | `HandSystem::Update`: `Ccan_pickup` over a spell icon, and over anything when the interface hand state is 9 (can pick up) |
| Hold fingers: over a leash, held still at its first frame, or half way through while the leash is being taken | todo | Not ported (the leash pick) |
| The two spare slots of the standard set | n/a | empty in the game's file |

## On a creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Stroke: the hand's feeling cycle, played hovering over any object | done | `HandSystem::Update`: `Cstroke` over any object with a mesh (or a temple's entrance) when the hand state is not 9 |
| Tickle: the caress played while the hand rests on a creature's body, the cycle the creature state starts in | partial | Our hand plays `Cstroke` on the creature's body, not the tickle cycle; the creature state's own clip is not traced |
| The caress only plays once the hand has rested on the body a second | todo | The hand has no caress clip; the 1 s rest only times the creature's stroke reaction (`creature_feedback::k_StrokeHoldMs`) |
| Slap: played once the hand sweeps across the creature faster than 3.25 times the creature's size a second | todo | The slap is classed (`creature_feedback::ClassifySlap`) but the hand never plays a slap clip: nothing reads the pose's slapping flag |
| Punch | todo | Loaded but never chosen (unconfirmed when the game plays it) |
| The spare slots of the reward and punishment sets | n/a | empty in the game's file |

## Moving the camera

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Grip: gripping and panning the land | done | `HandSystem::Update`: `Cgrip` while gripping the land, with the layers' translation; see [navigation.md](navigation.md) |
| Rotate: turning by the edge, and offered at the sides and bottom | todo | Not ported: `HandSystem::Update` notes the camera's rotate and pitch clips as not ported |
| Pitch: tilting, and offered at the top | todo | Not ported, as above |
| Zoom: only reachable through a camera hint the game never sets, so in effect unused | done | Never chosen in our tree either, as in the game |
| The spare slot of the camera set | n/a | empty in the game's file |

## Set animations

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Tap house: played once where the hand is when it knocks on a house's roof, then the hand goes back to its state | done | `abodes::InterfaceTap` -> `HandSystem::StartFixedPosAnimation("Ctap_house")`: the play-anim state plays it once, then back to NORMAL |
| Knocking also plays one of nine knocking sounds in turn | done | `abodes::InterfaceTap`: InGame sample 110 plus the knock counter (0..8); see [hand_sounds.md](hand_sounds.md) |
| While a set animation plays, the hand stays upright at the place it started | done | `HandPlacement.cpp`: in the play-anim state the hand is pinned at the animation's point with the world up |
| Beckon | todo | Loaded but never chosen (unconfirmed when the game plays it) |

## Special holds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Horn: the horn-shaped hold of some miracles | todo | `Chorn` is loaded but never played: `HandSystem::Update` always passes no special hold |
| The hold the spec lists after the horn | todo | Loaded but never chosen (unconfirmed what the game uses it for) |
| The spare slot of the special holds | n/a | empty in the game's file |
