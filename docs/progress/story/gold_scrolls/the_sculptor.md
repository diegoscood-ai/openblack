# The Sculptor

The third step of finding the creature-gate stones on the first land: the last stone was destroyed long ago, so the
village sculptor offers to carve a new one if the player brings him the right rock from the old hermit's quarry. He
carves it for a minute beside his house and hands over the cow gate stone for the plinth.

**Land:** 1 · **Giver:** the sculptor, a villager working outside his house on the edge of the Norse village · **Script:** TheSculptor · **Reward:** the cow gate stone, the last of the three that open the creatures' gates · **Repeatable:** no

Sources: the land's challenge scripts (the original source text, checked against the PC game's compiled
`challenge.chl`), the land's map script (`Land1.txt`) and the game's text table. openblack is judged on this tree: of
the 67 commands the quest needs only 4 log "not implemented" in `src/CHLApi.cpp` (a flock and the record among them);
the dialogue, advisor, scroll, villager-making, camera move, timer, effect, animation and property commands work, as do
making objects (`Create`), the pick-up, moveable, indestructible and fire flags, the camera cuts, fades, widescreen and
music. The land's control script stops before this quest is reached, in Choose Your Creature's gate-stone loop (see
[../../scripts/land1_script.md](../../scripts/land1_script.md)), so none of it happens in game; rows are todo unless the
notes say otherwise. The land as a whole is in [../land_1.md](../land_1.md); the quest that starts it is
[Choose Your Creature](choose_your_creature.md). An earlier, never compiled silver-scroll version with the same title is
in [../silver_scrolls/the_sculptor_cut_silver.md](../silver_scrolls/the_sculptor_cut_silver.md).

**Progress: 1/47 done, 33 partial — 37%**

## Where it sits in the story

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest is a gold (story) scroll, titled "The Sculptor" | partial | `CreateHighlight` is real (`src/ECS/ScriptHighlight`); never started: Choose Your Creature stops before the second stone (`ObjectInfoBits` is a stub) |
| It is started by the creature trainer once the tiger and ape stones are both on the plinth (the second from [The Lost Brother](the_lost_brother.md)): "Good. You have two of the Gate Stones. But the third could be a problem.", "The Villagers say the final one was destroyed aeons ago. All is not lost, though." and "The Village sculptor can carve a new Gate Stone for you. You should see him."; the camera rises to look down towards his house | todo | the plinth's stone count `ObjectInfoBits` is a stub, so Choose Your Creature never starts this quest; see [choose_your_creature.md](choose_your_creature.md) |
| The uncarved rock stands in the old hermit's quarry from the start of the land (placed by the land's map) | done | our tree loads the land's mobile statics (`src/LHScriptX/FeatureScriptCommands.cpp`, `Land1.txt`) |
| A separate watcher keeps the rock in play from the end of the opening until it is carved: lost or left outside the player's influence it is made again at the quarry; left for a minute after being dropped more than 50 from the quarry and more than 20 from the sculptor's spot it is put back; once set down by the sculptor that becomes its home spot; it sparkles once this quest asks for it | partial | `ProtectQuarryRock` (started by `FollowUs`) calls only real natives (`GetInfluence`, `CREATE`, timers), but the held read (property 9) is not handled by `GetProperty`; see [choose_your_creature.md](choose_your_creature.md) |
| Taking the uncarved rock to the plinth early brings out the trainer: "The stone isn't carved. Perhaps the sculptor can help?" (and again every three minutes it is left there) | partial | part of [choose_your_creature.md](choose_your_creature.md): reached only after the gate-stone loop starts, which waits on a stubbed plinth reading |
| Placing the carved stone on the plinth with the other two opens the gates and starts the choice of creature (the creatures in the glade) | todo | the plinth reading is a stub; see [choose_your_creature.md](choose_your_creature.md) |
| A new game that skips straight to choosing the creature (or keeps the old creature) deletes the rock and never starts this quest | partial | the skip questions are real (`CanSkipTutorial`, the SkipBox) and `LandControl1` deletes the rock (`ObjectDelete`) |
| The quest can't leave the story stuck: the rock is remade if lost, a dead or lost sculptor is replaced by his cousin, and the carved stone is remade if lost | partial | the same script; never started: Choose Your Creature stops before the second stone (`ObjectInfoBits` is a stub) |

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The sculptor (a sculptor villager) is made beside his house; any loose rocks lying within 30 of him are held aside by the script until someone picks them up, so only rocks the player brings count | partial | `CREATE` makes villagers and the rock checks use real distance natives; never started: Choose Your Creature stops before the second stone (`ObjectInfoBits` is a stub) |
| Until his introduction is over he can't be hurt, burnt, moved or picked up | partial | `SetIndestructable`, `SetHurtByFire`, `SetIdMoveable`, `SetIdPickupable` are real; never started: Choose Your Creature stops before the second stone (`ObjectInfoBits` is a stub) |
| A half-finished sculpture is made in front of his house, fixed in place | partial | `CREATE` makes mobile statics and the flags are real; never started: Choose Your Creature stops before the second stone (`ObjectInfoBits` is a stub) |
| A gold scroll appears over his house, 5.5 up; he walks out to his sculpture and works at it in a loop | partial | `CreateHighlight`, `MoveGameThing` on villagers and the script states are real; never started: Choose Your Creature stops before the second stone (`ObjectInfoBits` is a stub) |
| Until the scroll or the house is clicked, whenever the camera is within 100 of the scroll and it is on screen, the good advisor pops out at most every 30 seconds, points at it and says "Hey! This dusty, chisel-wielding fellow must be the sculptor the Gatekeeper told us about." | partial | the shared notify script calls only real natives (`SpiritEject`, `RunText`); never started: Choose Your Creature stops before the second stone (`ObjectInfoBits` is a stub) |
| Clicking it puts the uncarved rock back at the quarry wherever the player had taken it, starts the rock's sparkle and gives the quarry a patch of the player's influence (radius 30) | partial | `SetPosition`, `InfluencePosition` and `SpecialEffectObject` are real; never started: Choose Your Creature stops before the second stone (`ObjectInfoBits` is a stub) |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A widescreen cut-scene with the happy generic script theme; the sculptor in high detail | partial | widescreen, music and `SetHighGraphicsDetail` (`ecs::super_villager`) are real; never started: Choose Your Creature stops before the second stone (`ObjectInfoBits` is a stub) |
| The camera comes down on him; he turns to face the camera and gossips | partial | `MoveCameraPosition`, `SetFocus` on villagers and `PlayJcSpecial` (partly) are real; never started: Choose Your Creature stops before the second stone (`ObjectInfoBits` is a stub) |
| The quest is recorded: "The Sculptor", 0 done, alignment 0, reminder (good advisor) "We need to get a carveable stone for the sculptor from the Hermit's Quarry over here." | todo | `Snapshot` is a stub; the challenge log is not drawn |
| "Holy one, I hear you're looking for a Gate Stone." Then, with a thank-you bow: "I am a sculptor. If you provide me with the right rock I'll carve one for you." | partial | `RunText` is real; never started: Choose Your Creature stops before the second stone (`ObjectInfoBits` is a stub) |
| He turns and points towards the quarry while the camera looks that way: "You'll find the rock I need in the Old Hermit's Quarry. Please bring it here." | partial | real villager, camera and text natives; never started: Choose Your Creature stops before the second stone (`ObjectInfoBits` is a stub) |
| He goes back to his sculpture; the screen fades to black and a flock of 20 doves is made at the quarry rock | todo | the fade and `FlockCreate` are real, but filling the flock with doves (`PopulateContainer`) is a stub |
| A cut to the rock, fade in, and the camera circles it for about six seconds; another fade | partial | cuts, fades and camera moves are real; never started: Choose Your Creature stops before the second stone (`ObjectInfoBits` is a stub) |
| Back at the sculptor: "I'm definitely the best sculptor in the Village." The camera looks across towards the quarry and the music stops | partial | real text, camera and music natives; never started: Choose Your Creature stops before the second stone (`ObjectInfoBits` is a stub) |
| From then on he can be hurt, burnt, moved and picked up | partial | the flags are real; never started: Choose Your Creature stops before the second stone (`ObjectInfoBits` is a stub) |

## Bringing the rock

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Until carving starts, when no other dialogue is running and the camera is within 200 of the rock with it on screen and not held, the evil advisor points at it: "Here's the stone you need!" at most every 30 seconds | partial | real advisor and field-of-view natives; the held read (property 9) is not handled; never started: Choose Your Creature stops before the second stone (`ObjectInfoBits` is a stub) |
| The first time the rock is picked up, the record goes to 30% with the reminder "You need to drop the carveable stone next to the sculptor." | todo | the record (`UpdateSnapshot`) is a stub and the held read (property 9) is not handled |
| Dropping the rock within 15 of the living sculptor (rock and sculptor both on the ground) starts the carving scene; the rock can't be picked up during it | partial | distance reads and the pick-up flag are real; flying and held reads are partly handled; never started: Choose Your Creature stops before the second stone (`ObjectInfoBits` is a stub) |
| Widescreen: he walks up to stand beside the rock and the camera looks at it; he is impressed. The record goes to 80% with alignment +1 and the reminder "The sculptor's still carving the Gate Stone." | todo | the record is a stub; the villager and camera natives are real |
| "Oh that's excellent. Exactly the right kind of rock to work with." Then, facing the camera: "Thank you. Come back later and I'll have it finished." | partial | `RunText` is real; never started: Choose Your Creature stops before the second stone (`ObjectInfoBits` is a stub) |
| The rock is set upright on the ground and he starts carving it for 60 seconds | todo | setting the rock upright needs the angle properties, which `SetProperty` does not handle; `CreateTimer` is real |
| Any other rock the player drops within 15 of him first (checked every 3 seconds) gets, once only, a cut-scene where he looks unimpressed: "Hmm. Actually this rock isn't right for carving. Try another." | partial | real distance, villager and text natives; never started: Choose Your Creature stops before the second stone (`ObjectInfoBits` is a stub) |

## While he carves

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Picking the rock up or moving it more than 3 away stops the carving: "Hey! I haven't finished working on carving the Gate Stone yet!"; the record drops back to 30% with the reminder "You need to drop the carveable stone next to the sculptor." and the time left is kept | todo | the held read (property 9) is not handled and the record is a stub |
| Putting it back down within 15 of him: "Thank you. Now, if you don't mind. I've got carving to do." He walks to it, carves, the rock is set upright, the record returns to 80% (alignment +1) and the carving goes on for the time that was left | todo | the record is a stub and the angles are not handled |
| Picking the sculptor up stops him; when he is put down he walks back to the rock if it is within 30, otherwise to his sculpture, and goes on working | todo | the held read (property 9) is not handled |
| If he is away from his sculpture while not carving a rock, he has 60 seconds to walk back to it; if he doesn't make it the script kills him as lost | partial | real timers and villager moves; never started: Choose Your Creature stops before the second stone (`ObjectInfoBits` is a stub) |

## The sculptor's death

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| If he dies (or is lost), after 2 seconds his body becomes a skeleton and fades away once it is off screen and the camera is more than 100 away | todo | `SetSkeleton` is a stub; `ObjectDelete` is real |
| His cousin (a new sculptor, aged 33) comes out of the house in a cut-scene. The good advisor points at him: "What a senseless waste of human life!" and the evil advisor "Come on. Let's do another!"; if he was lost instead: "Oh dear. That's one very lost sculptor." and "Good riddance. Sculpting is for wusses." | partial | `CREATE` of a villager and the advisor lines are real; never started: Choose Your Creature stops before the second stone (`ObjectInfoBits` is a stub) |
| If the rock is within 15 the cousin says "Hey! That was my cousin! Now I've got to complete his sculpture." (lost: "Hmm. My sculpting cousin's vanished. I'd better finish his work.") and goes to the rock; otherwise "Oi! You killed my cousin! I suppose I've got to finish his work now." (lost: "My sculpting cousin has disappeared. I'd better finish his carving.") and goes to the sculpture | partial | real villager and text natives; never started: Choose Your Creature stops before the second stone (`ObjectInfoBits` is a stub) |
| The cousin carries on the quest exactly as the sculptor did; killing him brings another | partial | the same script; never started: Choose Your Creature stops before the second stone (`ObjectInfoBits` is a stub) |

## The reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| With a second left on the carving, a cut-scene: the cow gate stone appears where the rock was, turned half round from it, and the rock fades away | partial | `CREATE` makes the stone (a mobile static) and `ObjectDelete` removes the rock; never started: Choose Your Creature stops before the second stone (`ObjectInfoBits` is a stub) |
| The new stone is handed to its own watcher: it sparkles, is remade where it was carved if lost, and is put back there a minute after being dropped anywhere but the plinth | partial | `ProtectGateKey3` calls only real natives (the held read is not handled); see [choose_your_creature.md](choose_your_creature.md) |
| The camera looks at the stone; the sculptor faces the camera. The record closes fully done (alignment 0) with the reminder "The sculptor's finished the Gate Stone. We must return it to the Gate." | todo | the record is a stub; the camera and villager natives are real |
| "I've finished! It's ready to be placed with the others by the Gate." The camera swings round to look past the stone at the plinth, and the sculptor walks off a little way | partial | real text, camera and villager natives; never started: Choose Your Creature stops before the second stone (`ObjectInfoBits` is a stub) |
| The player takes the stone to the plinth; that is [Choose Your Creature](choose_your_creature.md)'s part | todo | the plinth reading is a stub; see [Choose Your Creature](choose_your_creature.md) |

## After it ends

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The sculptor goes back to working on his sculpture; a minute later (if alive) he walks between two spots by it and is then left to live as an ordinary villager | partial | `ReleaseFromScript` and the villager moves are real; never started: Choose Your Creature stops before the second stone (`ObjectInfoBits` is a stub) |
| The half-finished sculpture stays in front of his house | partial | made by `CREATE` if the script ran; never started: Choose Your Creature stops before the second stone (`ObjectInfoBits` is a stub) |
| The script never removes its gold scroll (undetermined: whether the game hides a scroll whose record is fully done) | partial | the same script; never started: Choose Your Creature stops before the second stone (`ObjectInfoBits` is a stub) |
| Music: the happy generic script theme for the introduction | partial | `StartMusic` plays it (`src/Audio/Services/GameMusic`); never started: Choose Your Creature stops before the second stone (`ObjectInfoBits` is a stub) |

## Unused material

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A commented-out lightning-bolt reward from the sky for bringing the rock | n/a | commented out |
| A commented-out older carving scene with a sledgehammer and a 90-second timer, and older code for walking the sculptor back to the rock | n/a | commented out |
| Commented-out updates to the Choose Your Creature record at each step | n/a | commented out |
| An earlier silver-scroll version of the quest, never compiled | n/a | see [../silver_scrolls/the_sculptor_cut_silver.md](../silver_scrolls/the_sculptor_cut_silver.md) |
