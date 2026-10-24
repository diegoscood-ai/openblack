# The Sculptor

The third step of finding the creature-gate stones on the first land: the last stone was destroyed long ago, so the
village sculptor offers to carve a new one if the player brings him the right rock from the old hermit's quarry. He
carves it for a minute beside his house and hands over the cow gate stone for the plinth.

**Land:** 1 · **Giver:** the sculptor, a villager working outside his house on the edge of the Norse village · **Script:** TheSculptor · **Reward:** the cow gate stone, the last of the three that open the creatures' gates · **Repeatable:** no

Sources: the land's challenge scripts (the original source text, checked against the PC game's compiled
`challenge.chl`), the land's map script (`Land1.txt`) and the game's text table. openblack is judged on this tree: none of
the commands the quest needs only logs "not implemented" in `src/CHLApi.cpp` (the doves' flock and the record are real);
the dialogue, advisor, scroll, villager-making, camera move, timer, effect, animation and property commands work, as do
making objects (`Create`), the pick-up, moveable, indestructible and fire flags, the camera cuts, fades, widescreen and
music. A run on 2026-10-22 (Land 1, SkipBox answer 0) reached Choose Your Creature's gate-stone loop and laid its first
stone, which starts The Lost Brother; this quest starts only with the second stone, so none of it has been played yet
(see [../../scripts/land1_script.md](../../scripts/land1_script.md)); rows are pending unless the notes say otherwise. The land as a whole is in [../land_1.md](../land_1.md); the quest that starts it is
[Choose Your Creature](choose_your_creature.md). An earlier, never compiled silver-scroll version with the same title is
in [../silver_scrolls/the_sculptor_cut_silver.md](../../../not-in-the-original.md#storysilver_scrollsthe_sculptor_cut_silver).

**Progress: 1/47 done, 0 chosen, 33 partial, 0 inferred — 25%**

## Where it sits in the story

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest is a gold (story) scroll, titled "The Sculptor" | partial 60% | `CreateHighlight` is real (`src/ECS/ScriptHighlight`); not checked in play: Choose Your Creature starts it once the second stone is on the plinth, and a run has so far laid only the first (2026-10-22) |
| It is started by the creature trainer once the tiger and ape stones are both on the plinth (the second from [The Lost Brother](the_lost_brother.md)): "Good. You have two of the Gate Stones. But the third could be a problem.", "The Villagers say the final one was destroyed aeons ago. All is not lost, though." and "The Village sculptor can carve a new Gate Stone for you. You should see him."; the camera rises to look down towards his house | pending | not checked in play: the plinth's stone count (`ObjectInfoBits`) is real, so Choose Your Creature starts this quest once two stones are in; a run has so far laid only the first (2026-10-22); see [choose_your_creature.md](choose_your_creature.md) |
| The uncarved rock stands in the old hermit's quarry from the start of the land (placed by the land's map) | done | our tree loads the land's mobile statics (`src/LHScriptX/FeatureScriptCommands.cpp`, `Land1.txt`) |
| A separate watcher keeps the rock in play from the end of the opening until it is carved: lost or left outside the player's influence it is made again at the quarry; left for a minute after being dropped more than 50 from the quarry and more than 20 from the sculptor's spot it is put back; once set down by the sculptor that becomes its home spot; it sparkles once this quest asks for it | partial | `ProtectQuarryRock` (started by `FollowUs`) calls only real natives (`GetInfluence`, `CREATE`, timers), and the held read (property 9) is real (`src/CHLApi.cpp:1679`); see [choose_your_creature.md](choose_your_creature.md) |
| Taking the uncarved rock to the plinth early brings out the trainer: "The stone isn't carved. Perhaps the sculptor can help?" (and again every three minutes it is left there) | partial | part of [choose_your_creature.md](choose_your_creature.md): reached only after the gate-stone loop starts, whose plinth reading (`OBJECT_INFO_BITS`, `src/CHLApi.cpp:7109`) is real; not checked in play |
| Placing the carved stone on the plinth with the other two opens the gates and starts the choice of creature (the creatures in the glade) | pending | the plinth's count (`OBJECT_INFO_BITS`, `src/CHLApi.cpp:7109`) and laying a stone in it (`src/ECS/HeldApply.cpp:249`) are real; not checked in play; see [choose_your_creature.md](choose_your_creature.md) |
| A new game that skips straight to choosing the creature (or keeps the old creature) deletes the rock and never starts this quest | partial | the skip questions are real (`CanSkipTutorial`, the SkipBox) and `LandControl1` deletes the rock (`ObjectDelete`) |
| The quest can't leave the story stuck: the rock is remade if lost, a dead or lost sculptor is replaced by his cousin, and the carved stone is remade if lost | partial | the same script; not checked in play: Choose Your Creature starts it once the second stone is on the plinth, and a run has so far laid only the first (2026-10-22) |

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The sculptor (a sculptor villager) is made beside his house; any loose rocks lying within 30 of him are held aside by the script until someone picks them up, so only rocks the player brings count | partial 60% | `CREATE` makes villagers and the rock checks use real distance natives; not checked in play: Choose Your Creature starts it once the second stone is on the plinth, and a run has so far laid only the first (2026-10-22) |
| Until his introduction is over he can't be hurt, burnt, moved or picked up | partial 60% | `SetIndestructable`, `SetHurtByFire`, `SetIdMoveable`, `SetIdPickupable` are real; not checked in play: Choose Your Creature starts it once the second stone is on the plinth, and a run has so far laid only the first (2026-10-22) |
| A half-finished sculpture is made in front of his house, fixed in place | partial 60% | `CREATE` makes mobile statics and the flags are real; not checked in play: Choose Your Creature starts it once the second stone is on the plinth, and a run has so far laid only the first (2026-10-22) |
| A gold scroll appears over his house, 5.5 up; he walks out to his sculpture and works at it in a loop | partial 60% | `CreateHighlight`, `MoveGameThing` on villagers and the script states are real; not checked in play: Choose Your Creature starts it once the second stone is on the plinth, and a run has so far laid only the first (2026-10-22) |
| Until the scroll or the house is clicked, whenever the camera is within 100 of the scroll and it is on screen, the good advisor pops out at most every 30 seconds, points at it and says "Hey! This dusty, chisel-wielding fellow must be the sculptor the Gatekeeper told us about." | partial 60% | the shared notify script calls only real natives (`SpiritEject`, `RunText`); not checked in play: Choose Your Creature starts it once the second stone is on the plinth, and a run has so far laid only the first (2026-10-22) |
| Clicking it puts the uncarved rock back at the quarry wherever the player had taken it, starts the rock's sparkle and gives the quarry a patch of the player's influence (radius 30) | partial 60% | `SetPosition`, `InfluencePosition` and `SpecialEffectObject` are real; not checked in play: Choose Your Creature starts it once the second stone is on the plinth, and a run has so far laid only the first (2026-10-22) |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A widescreen cut-scene with the happy generic script theme; the sculptor in high detail | partial 60% | widescreen, music and `SetHighGraphicsDetail` (`ecs::super_villager`) are real; not checked in play: Choose Your Creature starts it once the second stone is on the plinth, and a run has so far laid only the first (2026-10-22) |
| The camera comes down on him; he turns to face the camera and gossips | partial 60% | `MoveCameraPosition`, `SetFocus` on villagers and `PlayJcSpecial` (partly) are real; not checked in play: Choose Your Creature starts it once the second stone is on the plinth, and a run has so far laid only the first (2026-10-22) |
| The quest is recorded: "The Sculptor", 0 done, alignment 0, reminder (good advisor) "We need to get a carveable stone for the sculptor from the Hermit's Quarry over here." | pending | `Snapshot` is real; the challenge log is not drawn |
| "Holy one, I hear you're looking for a Gate Stone." Then, with a thank-you bow: "I am a sculptor. If you provide me with the right rock I'll carve one for you." | partial | `RunText` is real; not checked in play: Choose Your Creature starts it once the second stone is on the plinth, and a run has so far laid only the first (2026-10-22) |
| He turns and points towards the quarry while the camera looks that way: "You'll find the rock I need in the Old Hermit's Quarry. Please bring it here." | partial | real villager, camera and text natives; not checked in play: Choose Your Creature starts it once the second stone is on the plinth, and a run has so far laid only the first (2026-10-22) |
| He goes back to his sculpture; the screen fades to black and a flock of 20 doves is made at the quarry rock | pending | the fade and `FlockCreate` are real, and so is filling the flock with doves (`POPULATE_CONTAINER`, script ip 35819, `src/CHLApi.cpp:4127`); not checked in play |
| A cut to the rock, fade in, and the camera circles it for about six seconds; another fade | partial | cuts, fades and camera moves are real; not checked in play: Choose Your Creature starts it once the second stone is on the plinth, and a run has so far laid only the first (2026-10-22) |
| Back at the sculptor: "I'm definitely the best sculptor in the Village." The camera looks across towards the quarry and the music stops | partial | real text, camera and music natives; not checked in play: Choose Your Creature starts it once the second stone is on the plinth, and a run has so far laid only the first (2026-10-22) |
| From then on he can be hurt, burnt, moved and picked up | partial | the flags are real; not checked in play: Choose Your Creature starts it once the second stone is on the plinth, and a run has so far laid only the first (2026-10-22) |

## Bringing the rock

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Until carving starts, when no other dialogue is running and the camera is within 200 of the rock with it on screen and not held, the evil advisor points at it: "Here's the stone you need!" at most every 30 seconds | partial | real advisor and field-of-view natives; the held read (property 9) is real (`src/CHLApi.cpp:1679`); not checked in play: Choose Your Creature starts it once the second stone is on the plinth, and a run has so far laid only the first (2026-10-22) |
| The first time the rock is picked up, the record goes to 30% with the reminder "You need to drop the carveable stone next to the sculptor." | pending | the record (`UpdateSnapshot`) is real; the held read (property 9) is real (`src/CHLApi.cpp:1679`); not checked in play |
| Dropping the rock within 15 of the living sculptor (rock and sculptor both on the ground) starts the carving scene; the rock can't be picked up during it | partial | distance reads and the pick-up flag are real; the flying and held reads are real (`src/CHLApi.cpp:1671`, `src/CHLApi.cpp:1679`); not checked in play: Choose Your Creature starts it once the second stone is on the plinth, and a run has so far laid only the first (2026-10-22) |
| Widescreen: he walks up to stand beside the rock and the camera looks at it; he is impressed. The record goes to 80% with alignment +1 and the reminder "The sculptor's still carving the Gate Stone." | pending | the record is real now, not yet checked in this quest; the villager and camera natives are real |
| "Oh that's excellent. Exactly the right kind of rock to work with." Then, facing the camera: "Thank you. Come back later and I'll have it finished." | partial 60% | `RunText` is real; not checked in play: Choose Your Creature starts it once the second stone is on the plinth, and a run has so far laid only the first (2026-10-22) |
| The rock is set upright on the ground and he starts carving it for 60 seconds | pending | setting the rock upright uses the angle properties, which `SET_PROPERTY` handles (XANGLE and ZANGLE, `src/CHLApi.cpp:2239`); `CreateTimer` is real; not checked in play |
| Any other rock the player drops within 15 of him first (checked every 3 seconds) gets, once only, a cut-scene where he looks unimpressed: "Hmm. Actually this rock isn't right for carving. Try another." | partial 60% | real distance, villager and text natives; not checked in play: Choose Your Creature starts it once the second stone is on the plinth, and a run has so far laid only the first (2026-10-22) |

## While he carves

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Picking the rock up or moving it more than 3 away stops the carving: "Hey! I haven't finished working on carving the Gate Stone yet!"; the record drops back to 30% with the reminder "You need to drop the carveable stone next to the sculptor." and the time left is kept | pending | the held read (property 9) is real (`src/CHLApi.cpp:1679`) and the record is real now, not yet checked in this quest |
| Putting it back down within 15 of him: "Thank you. Now, if you don't mind. I've got carving to do." He walks to it, carves, the rock is set upright, the record returns to 80% (alignment +1) and the carving goes on for the time that was left | pending | the record is real now, not yet checked in this quest; setting the rock's leans to 0 is real (`SET_PROPERTY` XANGLE and ZANGLE, `src/CHLApi.cpp:2239`) |
| Picking the sculptor up stops him; when he is put down he walks back to the rock if it is within 30, otherwise to his sculpture, and goes on working | pending | the held read (property 9) is real (`src/CHLApi.cpp:1679`); not checked in play |
| If he is away from his sculpture while not carving a rock, he has 60 seconds to walk back to it; if he doesn't make it the script kills him as lost | partial | real timers and villager moves; not checked in play: Choose Your Creature starts it once the second stone is on the plinth, and a run has so far laid only the first (2026-10-22) |

## The sculptor's death

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| If he dies (or is lost), after 2 seconds his body becomes a skeleton and fades away once it is off screen and the camera is more than 100 away | pending | `SetSkeleton` is real; `ObjectDelete` is real |
| His cousin (a new sculptor, aged 33) comes out of the house in a cut-scene. The good advisor points at him: "What a senseless waste of human life!" and the evil advisor "Come on. Let's do another!"; if he was lost instead: "Oh dear. That's one very lost sculptor." and "Good riddance. Sculpting is for wusses." | partial 60% | `CREATE` of a villager and the advisor lines are real; not checked in play: Choose Your Creature starts it once the second stone is on the plinth, and a run has so far laid only the first (2026-10-22) |
| If the rock is within 15 the cousin says "Hey! That was my cousin! Now I've got to complete his sculpture." (lost: "Hmm. My sculpting cousin's vanished. I'd better finish his work.") and goes to the rock; otherwise "Oi! You killed my cousin! I suppose I've got to finish his work now." (lost: "My sculpting cousin has disappeared. I'd better finish his carving.") and goes to the sculpture | partial 60% | real villager and text natives; not checked in play: Choose Your Creature starts it once the second stone is on the plinth, and a run has so far laid only the first (2026-10-22) |
| The cousin carries on the quest exactly as the sculptor did; killing him brings another | partial 60% | the same script; not checked in play: Choose Your Creature starts it once the second stone is on the plinth, and a run has so far laid only the first (2026-10-22) |

## The reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| With a second left on the carving, a cut-scene: the cow gate stone appears where the rock was, turned half round from it, and the rock fades away | partial | `CREATE` makes the stone (a mobile static) and `ObjectDelete` removes the rock; not checked in play: Choose Your Creature starts it once the second stone is on the plinth, and a run has so far laid only the first (2026-10-22) |
| The new stone is handed to its own watcher: it sparkles, is remade where it was carved if lost, and is put back there a minute after being dropped anywhere but the plinth | partial | `ProtectGateKey3` calls only real natives (the held read is real, `src/CHLApi.cpp:1679`); see [choose_your_creature.md](choose_your_creature.md) |
| The camera looks at the stone; the sculptor faces the camera. The record closes fully done (alignment 0) with the reminder "The sculptor's finished the Gate Stone. We must return it to the Gate." | pending | the record is real now, not yet checked in this quest; the camera and villager natives are real |
| "I've finished! It's ready to be placed with the others by the Gate." The camera swings round to look past the stone at the plinth, and the sculptor walks off a little way | partial | real text, camera and villager natives; not checked in play: Choose Your Creature starts it once the second stone is on the plinth, and a run has so far laid only the first (2026-10-22) |
| The player takes the stone to the plinth; that is [Choose Your Creature](choose_your_creature.md)'s part | pending | the plinth's count (`OBJECT_INFO_BITS`, `src/CHLApi.cpp:7109`) and laying a stone in it (`src/ECS/HeldApply.cpp:249`) are real; not checked in play; see [Choose Your Creature](choose_your_creature.md) |

## After it ends

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The sculptor goes back to working on his sculpture; a minute later (if alive) he walks between two spots by it and is then left to live as an ordinary villager | partial 60% | `ReleaseFromScript` and the villager moves are real; not checked in play: Choose Your Creature starts it once the second stone is on the plinth, and a run has so far laid only the first (2026-10-22) |
| The half-finished sculpture stays in front of his house | partial 60% | made by `CREATE` if the script ran; not checked in play: Choose Your Creature starts it once the second stone is on the plinth, and a run has so far laid only the first (2026-10-22) |
| The script never removes its gold scroll (undetermined: whether the game hides a scroll whose record is fully done) | partial 60% | the same script; not checked in play: Choose Your Creature starts it once the second stone is on the plinth, and a run has so far laid only the first (2026-10-22) |
| Music: the happy generic script theme for the introduction | partial 60% | `StartMusic` plays it (`src/Audio/GameMusic`); not checked in play: Choose Your Creature starts it once the second stone is on the plinth, and a run has so far laid only the first (2026-10-22) |

