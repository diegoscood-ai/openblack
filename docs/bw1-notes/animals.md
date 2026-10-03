# Animals: AI, states and clips

All the animal AI ported from runblack.exe (session "animales", 2026-09-30; commits 06fec160 herbivores, hand and
death · 2d629816 predators and fleeing · e3a9d81f birds · 333ad5ae API for the spells · 2894cfd9 villagers as prey,
flocks, age, smoke, food reactions and flying object · f992a01a full audit against the original; 2026-10-01: a8e3313d detour, lairs and script animals · 3f548aec
common clock · 19d165f3 death notices · 0b8471b2 water from sea_cells · d207fdd4 villager eaten via VillagerDead ·
b7f48053 MOVE_GAME_THING). Handover for whoever continues: `dev\documentacion\animals\HANDOVER.md`. bw1-decomp
only has empty stubs of Animal*.cpp: everything comes from the executable.

Research with addresses, in `C:\Users\diewgarc\dev\documentacion\animals\`: `grazing_ai.md` (herbivores),
`hand_death.md` (hand, flight, landing, death), `predator_ai.md` (predators), `hunting.md` (hunting), `flee.md`
(fleeing), `birds_ai.md` / `birds_draw.md` (birds), `misc.md` (flocks, age, smoke), `villager_prey.md` (hunted
villagers), `reactions.md` (all the reactions), `wallhug.md` (moving), `old_age.md` (old age), `audit.md` and
`audit_r1..r4.md` (the audit), `openblack_plumbing.md`. Scripts in the same folder (`dumpanimals.py` info.dat
values, `vtd.py` vtables, `a.txt` disassembly of Animal.cpp).

In openblack: `ECS/AnimalAI.*` (turn, herbivores, hand, death, movement states), `ECS/AnimalPredators.cpp`
(predators and hunting), `ECS/AnimalLairs.cpp` (lairs), `ECS/AnimalBirds.cpp` (birds), `ECS/AnimalFlee.cpp` (the
reactions), `ECS/AnimalAIDetail.h` (what
they share), `ECS/AnimalAnimations.*` (clip per state and species), `ECS/AnimalApi.cpp` (functions for spells and
scripts, in `AnimalAI.h`), `ECS/AnimalDebugHooks.cpp` (test hooks), `ECS/SmokyStuff.*` (smoke from the carcass),
`components::AnimalBrain` (the Living / MobileWallHug / Animal fields that the AI uses), `components::Flock`,
`ECS/ScriptHeld.*` + `components::ScriptHeld` / `CannotBeEaten` (what the scripts hold and their flags) and
`ECS/AnimalScript.cpp` (the animal that a script releases).

**Land1 sheep:** `Land1.txt` creates none. The 9 sheep are created by the challenge "The Lost Flock" (`challenge.chl`,
script `TheLostFlock`), which `LandControl1` launches only after the `FollowUs` and `CitadelGuide` intro; in openblack the intro
does not finish yet, so they do not appear. Land2 does create them in the map.

## Turn (`Animal::ProcessState` 0x417EE0)

Every turn, after the villagers: TurnsSinceStateChange + 1; if the state has the needs flag
(info.dat `animalStateTable.field0xa4`) `ProcessNeeds` (hunger, sleep, breeding +1 up to their info.dat maximum; the leader
counts `leaderTurns`); then the state's function (`g_AnimalStateTable` 0xD12108). There are no entry / exit clips
nor speed changes per state (`Animal::SetStateSpeed` empty). `SetTopState`: exit filter (in the hand only
FLYING, LANDED or death; flying only IN_HAND, LANDED or death), state, counter to 0 and the state's clip.
`PlayAnimThenSetState`: WAIT_FOR_ANIMATION with the clip unchanged until turns × 100 ms ≥ its duration.

## Herbivores (sheep, tortoise, cow, horse, pig)

A single class (constructor 0x41D0B0, vtable "Cow"); only the clips change. Cycle:

- **DECIDE_WHAT_TO_DO** (`Cow::DecideWhatToDo` 0x41D1B0): breed if it is time; the leader (first member of the flock) takes
  the herd to a random point of the domain every `stayTime` turns (cow 200, the rest 1000) or if it left the domain; a
  member more than `flockDistance` from the leader goes back to its side; otherwise, the needs; otherwise, START_WANDER.
- **WANDER** (`Cow::Wander` 0x41D280): in a straight line at `step` per turn and it only changes heading on entering another
  10 m cell (no wall-hug nor water). The new heading (`SetNewWander` 0x41A3F0) adds, with a speed budget
  along the major axis (`fn_0041A5B0`), 0.9 × speed towards the leader if it is far (the distance in whole metres,
`__ftol` 0x41A421, compared as int with the int radii), the flock (`fn_0041AD70`: 1/5 towards the
  centre of the others, the nearest neighbour by axes, the cohesion vector **again**, 3/5 of the neighbour's step; the
  distance is compared with raw MapCoords, so it almost always attracts) and a random turn of ±turnAngle/2.
- **Hunger** (50 turns, horse 100): `LookForGrazePos` walks in a spiral (domain/10)² cells from its own, in front
  (±viewAngle/2 = ±90°), neither its own nor that of another member (or its destination), without water nor a fixed object;
  MOVE_TO_POS there → **START_TO_EAT** (head-lowering clip once) → **EAT** 20..34 times (GameRand(15) + 20), each time one of
  the two eating clips at random → **FINISH_EATING** (raise the head) → DECIDE.
- **Sleep**: a pure counter, without day / night. Full (1000) → SEEK_SLEEP to the sleeping spot (the cell at the centre
  of the domain at birth) → SLEEPS standing (−2 per turn, +1 from ProcessNeeds: ~100 s).
- **Breeding** (3000 turns): only if the flock has fewer members than it had (`maxMembers`): GIVES_BIRTH creates one
  of age 1 in the same flock.
- Speed: always `speedDefault` (cow, sheep, pig 0.75 m/s; horse 1.5; tortoise 0.25), so always the walking
  clip. Turning in MOVE_TO_POS: `Animal::SetTowardsAngle` 0x418560, at most turnAngle per turn (cow 34 = 6°);
  with the destination inside its turning circle (R = 2 × speed / turnAngle in radians) it turns |diff| − turnAngle × d / R,
  almost fully when close (0x4186B4..0x4186FC), so it does not keep circling the destination.

## Predators (lion, tiger, leopard, wolf)

Tiger and leopard use the lion's code (the tiger puts its lair in a forest); the wolf has its own. They wander, sleep,
breed and land with the herbivores' functions.

- **Felines** (`Lion::DecideWhatToDo` 0x41FE70): breed; after 22:00 (visual time) to sleep with the counter
  full; the leader moves the herd (stayTime lion 200); otherwise, hunger (lion 12000 turns, tiger 9000, leopard 1200),
  then sleep; otherwise, wander at 1.5 m/s.
- **Lairs** (`ECS/AnimalLairs.cpp`; audit_r4.md §1.8, lairs.md). The forest list (g_game +0x205BB4) goes from the
  newest to the oldest; the first is taken without scoring and the others score with `fn_0053AD00` =
  `SigmoidThreshold(-0,9, -(distancia MapCoords / 1000))` (the sigmoid of the number of trees is computed and discarded),
  changing only if they score strictly more. Every forest more than 1000 MapCoords (0.15 m) away scores 0.1144, so in
  practice **the lair is the second newest forest** (with only one, that one), at its grown tree nearest to the centre.
  - Tiger (`Tiger::CalculeLairPos` 0x421470): that rule; without forests or without a grown tree, where it is. Land5: the tiger at
    (2841, 2918), next to forest 19, puts its lair in forest 18 (1358, 3577), 1600 m away.
  - Wolf (`Wolf::CalculeLairPos` 0x421730): the nearest big forest; if there is none, the tiger's rule; if the chosen
    forest has no grown tree or there are no forests, the nearest tree (all of them); otherwise, where it is.
  - Lion and leopard (`Lion::CalculeLairPos` 0x420010): where it is. Land2 has no forests: the tiger stays where it is.
- **Wolves** (0x4216B0): to the lair (a square the size of the pack around the centre of the domain, see Lairs)
  and lying in it (HIDE_IN_LAIR, sleeping clip). When hungry (120 turns) they only
  go out after 23:00: towards the nearest flock that is not stronger at 1.2 m (in practice their own: the leader hunts
  where it is) or towards the nearest village.
- **Prey** (`fn_004196D0`): the first in a spiral of 64 cells (±40 m): another species, on the ground, alive, with meat
  (all except the tortoise), outside their turning circles; a cub only a downed prey. It is chased from the
  next check, at 8 m/s. In the original **villagers are also prey**; openblack not yet, because its
  villagers do not have the downed / eaten states. What cannot be eaten (+0x25 & 0x40) is never prey, nor what is
  controlled by a script (+0x24 & 0x400) except for a hunter that is in a script (see Scripts and flags).
- **Chase** (HUNTING_MOVE_TO_POS 41, 0x418DB0): stalks at 1.25 m/s (stalking clip) from 100 to 50 m, sprints at
  10 m/s below 50 m; pounces when it is at pounce stride × scale × 0.5 (~2.3 m) and the prey is within ±22.5° of
  its heading; gives up after 20 s (chaseTime) or beyond 100 m.
- **Pounce** (TARGET_POUNCE 40, 0x419010; clip POUNCE_HI, the wolf POUNCE): at 1 m or less the prey falls (life 0.05,
  DOWNED); when the pounce stride ends: if it fell, `FinishPouncing` (hunger 0, it places itself next to the prey to eat);
  otherwise, it goes back to chasing it.
- **The prey**: DOWNED (falling clip) → BEING_EATEN (lying down) 300 turns → DEAD, carcass 50 turns; the one that cannot be
  eaten (+0x25 & 0x40, 0x5EC4E0) goes to LANDED and stays alive.
- **Eating**: START_TO_EAT → EAT 15..24 cycles; if the prey disappears it goes back to deciding (`Lion::Eat` 0x41FE40; also
  at the end of the meal, so the getting-up clip is never seen).

## Birds (crow, dove, swallow, rock dove, seagull, bat)

All of them are the Dove class (constructor 0x41DCF0); only the clips and the info.dat values change. They are born at
`altitudeNormal` above the ground (20 or 40 m), at 8-10 m/s.

- **Leader** (`Dove::DecideWhatToDo` 0x41DE40 → `StartWander` 0x41DF50 → SPECIAL_MOVE_TO_POS 44): a leg to a
  random point 80 m away (domainRadius) **from where it is** (the flock drifts across the island), at its height ± altitudeVariance
  within altitudeNormal + [altitudeMin, altitudeMax]; another leg on arrival or every stayTime (100 turns).
- **Followers** (FOLLOW_FLOCK 45): a point 10 m from the leader, then a formation slot (`fn_0041E890`, read
  literally) and MOVE_TO_POS in 2D at a constant height above the ground; start over.
- **Height** (`Animal::MoveTo3D` 0x418AA0): in flight they keep their absolute height (they do not follow the hills) and rise or
  descend at most altitudeMovementChange per turn (0.2-0.6 m), never less than 2 m from the ground.
- **Banking**: when turning they tilt ±0.5 rad over 2 s (the Zoomer, `Animal::SetTowardsAngle`) and level out again over 2 s;
  `Dove::Draw` 0x41F680 rotates the drawn matrix around its forward axis (in openblack, in `MobileDrawing`).
- **They never land** in the original game: info.sleep is 0 in all of them, so LAND_AT_POS / SLEEPS are never reached. They
  have no hunger, breeding nor day / night (the bats fly by day).
- **Clips**: the moving one (and the deciding one) is a coin toss between flapping and gliding on each state change and on each
  SetSpeed (the swallow among three, the bat always flaps); its SetAnim never restarts the clip. Table in
  `ECS/AnimalAnimations.cpp` (`BirdClip`).
- They cannot be picked up nor hit (playerCanPickUp 0) and are not prey (more than 2 m up). When dead they fall with physics at
  the flight speed (`Dove::Dying` 0x41F1B0) and stay on the ground as a carcass.

## Moving (MobileWallHug, `dev\documentacion\animals\wallhug.md`)

`Living::SetupMoveToPos` (0x5F2830) uses the one-argument version of `SetupMobileMoveToPos`: **STEP_THROUGH**, a straight
walk without going around obstacles that in the animals re-aims every turn with their `SetTowardsAngle` (turn limited by
turnAngle). At the step that is one stride away it goes to FINAL_STEP and arrives (0xA) the next turn. Going around obstacles
(LINEAR → ORBIT → EXIT_CIRCLE) is only used by `SetupMoveToWithHug` (0x5F2890): going to the food (19) and fleeing again (6);
ported in `ECS/AnimalWallHug.*` (research `wallhug_circle.md`):

- **Circles** (`ObjectCircleIterator`, always those of the 10 m cell of the point being swept): those of each fixed object
  in the cell with collision data (not the fields, which the iterator skips; forests have none; a tree only its 0.3 m
  trunk and only in its cell; a multi-cell object in each cell its 7.1 m circle touches). That of a building or
  feature is the one of its box (centre, the largest half-axis × scale, minimum 1 m); if the box is more than 1.4 times longer than
  wide, a row of int(length / width) + 1 circles of the short half-axis. Then, the water cells (or outside the map) of
  the cell and its 8 neighbours, each a 7.2 m circle at its centre.
- **LINEAR** (`MoveToCircleHugLinearSquareSweep` 0x60CA50): the nearest circle that the ray of the step enters
  (the real entry, dot − √disc; behind −0.2 m it does not count); TurnsToObj = distance / step length (0xFF: none or more
  than 255 turns away). It walks straight, and **only re-aims** (`InitStepsXZ`, with the species' limited turn) and sweeps again
  when changing cell. When the count runs out it starts orbiting on the side the step passes (CW / CCW).
- **ORBIT** (0x60B0E4 / 0x60B40E): turns speed / radius × 0.0497 + 1 every turn. `MoveToCircleHugCircleSquareSweep`
  (0x6159F0 CW, 0x614C40 CCW) searches along the orbit for the first intersection with another circle of the cell (or the destination if
  it is inside the circle, 0.1° earlier), TurnsToObj = the arc / (1.5 × speed), and sets the tangent heading (towards the
  centre ± 90°, minus 64 per radius beyond 0.9 radii). Less than one turn away: the destination → direct STEP_THROUGH; another
  circle → switches to it and sweeps again (3 levels). It exits (EXIT_CIRCLE, straight outwards from the centre) when it is
  closer to the destination than when it started orbiting, with the destination on the inner side and ahead; outside the circle it goes back to
  LINEAR_CW / CCW.
- Consequence of the original: since LINEAR only re-aims when changing cell and with turnAngle (6° the cow), an animal whose
  heading no longer points at the destination (or that leaves an orbit outwards) moves away and does not arrive [the code; not seen in the
  original game]; if the destination lies inside the circled circle, it arrives via the final STEP_THROUGH. In numbers: the sheep
  turns at most 64 (11°) per re-aim (`Animal::SetTowardsAngle` 0x418560; the almost full turn only inside its
  turning circle, R = 2 × 0.075 m / 0.196 = 0.77 m) and walks 0.075 m per turn, so it re-aims once every ~133 turns;
  in Land2 with a pile next to them (`OPENBLACK_TEST_FOOD_PILE`) only 2 of 25 hungry sheep get to eat, and the
  food reaction lasts 2000 turns. Checked against the code 2026-10-01 (MoveTo 0x60B095, MoveToCircleHug
  0x60D800, InitStepsXZ 0x60BFA0, SetupMobileMoveToPos 0x60ABC0, AreWeThere 0x60AD60, Living::MoveToPos 0x5EC270,
  ProcessReaction 0x5F1270: nothing else re-aims).

`GUtils::Spiral` (dir 1, count 1: (−1, 0), (0, −1), (+1, 0) × 2, (0, +1) × 2...)
in all the spirals; `Collide(1)` is only water (the `hasWater` bit of the terrain cell) or outside the map,
`Collide(0)` nothing; `CalcRandomPos` (0x5ED080): two random points, each with a spiral of 25 cells, accepting if
it is on the map, without collision, outside the turning circles and (birds) over a terrain block; otherwise, the centre if it is
outside its turning circles, and otherwise, its own position. Angles with the trunc(65536 cos) tables and `LHArcTan`.
Initial heading 0 (+x). The flock list is in reverse order of creation: the leader is the first one that entered; the
birds' formation counts from the newest. The animals are processed from the newest to the oldest.

## Reactions

`Reaction::ProcessReactions`, which would distribute them every turn, depends on a debug switch that the game never
enables, so **each reaction is offered only once**, when created (`Reaction::CreateReaction` → `SpreadReaction`
0x6E3E10: (maxReactionDistance × 0.2)² cells of `GUtils::Spiral` within the radius; `ApplyReactionToLivingObjectsAtSquare`
0x6E3F90 for each animal: available (`IsAvailableForReaction`), distance = half Manhattan distance, score
min(255, priority × (1 + 0.5 × howImportantIsDistance × (max − d) / max)), and its **records** (3 at most, {type,
turn}: it does not take a type again before NumGameTurnsBeforeReactingAgain; they expire after 1800 turns). An animal that is already
reacting only changes if the new one scores more and the current one has already lasted 10 s (1 s if it was being picked up by the hand). Each type ends
after its own number of turns (`Living::ProcessReaction` 0x5F1270), if the initiator disappears (and then it goes back to the
saved state: DECIDE, or state 0 if it came from moving, and stays there like the original) or if the
reaction disappears (it stays in its state). Leaving to a state that is not a reaction one (the hand, dying, a need) drops the
reaction without changing the state (`Animal::ExitReaction` 0x41B170). The animals react to object (0), look (1),
spell (3), creature (6), food (7), fire (10), falling tree (27, which nobody creates) and predator (28); the
predators also to flying object (9). Tortoise and birds do not react. **No animal reacts to the hand**.
In openblack the distribution, the records, the scoring and the change rule are common to all living beings
(`ECS/Effects/Reactions`, `components::ReactionRecords`); what is specific to the animals (priorities 28/7/9, StartReacting,
turns of 28, states 49/6/30/19/20) is their handler in `ECS/AnimalFlee.cpp`. The villagers receive the same
reactions in the same distribution (fire and teleport ported).

- **Predator (28):** each predator creates it at birth (fn_0041FD30). Those with `isFleeingFromPredator` flee within
  25 m, if the predator is not stalking (at its speed2 or less it goes unnoticed); a predator only from a stronger one. Fleeing:
  state 49 at its flee speed (cow, sheep, pig 4 m/s; horse 9) ~10 m to one side of its trajectory; then
  6 (again if it is coming towards it or is closer than 30 m) or 30 (still, looking at it); ~80-88 turns or beyond 75 m.
- **Food (7, `Pot::SetupReaction` 0x66D660):** the map's food piles (CREATE_POT) on load, and a pot that the
  hand puts down; it is removed when it is picked up or emptied. **Hungry** herbivores within 35 m go to the edge of the pile (the sum of
  both radii), take 50 from it (the pile shrinks; when empty it disappears) and their hunger is set to 0. The piles the hand makes
  (MagicFood) have foodType 0: no animal goes to them, not in the original either.
- **Flying object (9, `Object::InitialisePhysicsFromHand` 0x637412):** what the hand throws makes the
  predators within 25 m flee if 2 × its speed exceeds its distance; the reaction is removed when the object lands.
- Not ported (they need systems that openblack does not have): spells (0, 3), village artefacts (1), creature (6),
  fire (10).
## Hand, flight and death

- Picking up: `GAnimalInfo.playerCanPickUp` (all the land ones except those of the puzzles). When picked up it leaves its flock for
  one of its own (`SeperateLivingIntoNewFlock`) → IN_HAND. Dropping or throwing: physics → FLYING (clip THROWN).
- At rest (`Animal::EndPhysics` 0x5F0D80): landType by the body's right row (y > 0.5 on the right side, < −0.5
  left, otherwise standing); alive → LANDED (getting-up clip according to landType; the predators, the waking one) → the
  flock centres itself where it fell → INTERACT_DECIDE → off wandering. **There is no
  drowning state** in the animals (`Animal::EndPhysics` has no water branch): in the sea the animal
  never stops, its density rises and after **~75 turns (7.5 s)** it exceeds 1 and `Living::HasSunk` 0x5ED370 kills it and
  deletes it (`SetDying`, state 15, `ToBeDeleted(0)`) — alive or carcass. In a shallow cell with water of altitude ≥ 2
  it lands **alive** (unlike the villager, who drowns); see
  [water.md](water.md#sinking-drowning-and-being-deleted). Dropping gently
  puts it into physics like the original, over the sea and on land (see [Differences and pending](#differences-and-pending)).
- Death (`Living::SetDying` 0x5EC390, nothing while it is flying): DYING (falling clip) → DEAD (lying according to landType; the
  predators with the sleeping clip) 600 turns (never if a script controls it) → its smoke (`CreateSmokyStuff`, below) and it disappears. A thrown carcass
  goes back to DEAD with another 600.

## Clips per species (AnimalAnimation.cpp 0x41C0E0..)

| species | walk | idle | eat | lower / raise head | hand | thrown | lands lt 0/1/2 | dead lt 1/other | falls |
|---|---|---|---|---|---|---|---|---|---|
| cow | 45 (41 runs) | 42 | 36/35 | 37 / 44 | 38 | 43 | 42/40/39 | 30/29 | 31 |
| sheep | 145 (141) | 142 | 133/132 | 136 / 140 | 137 | 143 | 142/139/138 | 131/130 | 134 |
| pig | 128 (125) | 126 | 117/116 | 120 / 124 | 121 | 127 | 126/123/122 | 115/114 | 118 |
| horse | 61 / trot 59 / 56 | 57 | 49/48 | 52 / 60 | 53 | 58 | 57/55/54 | 47/46 | 46 |
| tortoise | 172 | 171 for everything | | | | | | | |

Thresholds: `speedThreshold` entry 2 (cow, sheep and pig) and 3 (horse). Predators (lion, tiger, leopard, wolf):
stalking below speedDefault, walking, running above entry 6..9; in the table of `ECS/AnimalAnimations.cpp`. The clip
advances with the terrain covered while it moves (`Object::IsMoving`) and with time otherwise.

## Flocks, age and smoke

- **Picking up an animal** (`Flock::SeperateLivingIntoNewFlock` 0x52FE10): it always goes to a flock of its own where it was
  picked up, with the radius and distance of the old one, without a village and maximum 0; the old one, if it is left empty, is deleted, except
  a script flock (+0x25 & 4: it releases the animal's reference and stays even if empty, 0x419B75).
- **Merging** (`Animal::LookForFlocksInSpiral` 0x41A690): only after landing (±80 m), because `flocksCanMerge` is 0 in
  all of them; the biggest one keeps all of them if they are of the same species and the sum does not exceed `maxFlockSize`; a flock
  with a village does not merge; two script flocks do not merge and a foreign script one always keeps all of them
  (0x41A837, fn_005302A0 0x5302B4). Oddity: the maximum of the one that stays adds the other's once per member, clamped.
- **Age** (`Living::GetAge` 0x5ECAF0): 1500 turns per year since its birth turn. The young grow every 375
  turns (`escala += aleatorio(0,75 × (ageToScale[edad + 1] − escala))`) until `grownUpAge`; an adult is born at 0.9 and two
  rolls leave it in (0.95, 1.05]. **Animals do not die of old age**: checked across the whole executable
  (`dev\documentacion\animals\old_age.md`): `oldAge` / `retirementAge` are only read by `Villager::CheckDeathFromOldAge`
  (0x760CA0) and the death reason OLD_AGE (9) is only for villagers.
- **Being born** (GIVES_BIRTH): the newborn decides immediately, before the mother goes back to wandering.
- **Animal thrown into a food store:** it becomes food (its foodValue: cow and horse 1200, sheep 800,
  lion and tiger 900, wolf 700, pig 290) and disappears (`Animal::ReactToPhysicsImpact` 0x41BC10).
- **Smoke from the carcass** (`Object::CreateSmokyStuff` 0x63A810 → `SmokyStuff::Create` 0x823C90 mode 0, size 1): 15
  sprites of `Data\Textures\smoke.raw` with a random direction at 0.3..1 × size, grey 0x808080 with opacity life × 100,
  3 s, from 0.5 to 1.5 × size. The engine is `ecs::smoky_stuff` (`ECS/SmokyStuff.*`, from the water session, the same as the dust
  and the boat's splashes; it is drawn with the boat sprites); the animals only call `SmokyStuff::Create`.
- **In the hand** the animals have the villagers' grip (2D radius, drop 0.65). The landType is read from the body's matrix
  at the start of the turn.

## Hunted villagers

Villagers are prey like the animals (type 2, with meat) if they are outside their home. When downed, their health stays at 5 %
and they go to DOWNED (clip `P_ATTACKED_BY_LION`), then BEING_EATEN 300 turns (`P_DYING`) and they die (`Villager::BeingEaten`
0x76B380: l = GetLife(); SetLife(0); `VillagerDead(ANIMAL 3, su jugador (el dueño de su pueblo), l, 1)`, in
openblack `ecs::villager::VillagerDead` from the maps session, which kills it at the end of the turn). They are driven by the animal
AI (`components::DownedVillager`). The one that cannot be eaten (+0x25 & 0x40)
gets up (LANDED) instead of dying; openblack puts it in LANDED when the 300 turns end, without waiting for the clip (approximate).

## Scripts and flags (`dev\documentacion\animals\script_flags.md`)

- **Held by a script** (`ECS/ScriptHeld.*`, faithful): the original's script objects are slots of
  `ScriptManage` (511, 0xD967F8) with their reference count; in openblack the slot is the entity's `components::ScriptHeld`.
  Each LHVM variable that stores an object gives it a reference (callbacks of `LHVM::Initialise` in
  `Game.cpp`, `ADD_REFERENCE` / `REMOVE_REFERENCE`; the original ScriptLibraryR.dll calls those two natives on each POP
  of an object and when stopping a task); with the first one it becomes `IsInScript` (+0x24 & 0x200) and, if a
  script created it (`CREATE`, `CREATE_WITH_ANGLE_AND_SCALE`: `AddScriptGameThing(thing, 1)`), **controlled by the script** (+0x24
  & 0x400). After each `LookIn` (GScript::Process 0x6EB6DB → fn_0070D480) whatever no longer has references is released:
  it loses both flags and an animal goes through fn_0041AA00 (own flock if it has none; dead → SetDying, which gives the carcass another
  600 turns; alive → INTERACT_DECIDE_WHAT_TO_DO, in openblack one turn later: approximate).
- **Controlled by the script**: it is not prey except for a hunter that is in a script (HuntingMoveToPos 0x418DD7,
  fn_00419340, fn_004196D0), it does not take reactions (IsAvailableForReaction 0x5F120C), **its carcass does not expire** (Living::Dead
  0x5EC41E) and its flock is a script one (hand and merging, above).
- **Wolf in a script**: the leader puts the lair where it is (Wolf::CalculeLairPos 0x421774) and hunts from it at
  any time without looking at hunger (Wolf::HideInLair 0x421A2E).
- **Cannot be eaten** (+0x25 & 0x40, `components::CannotBeEaten`): it is set by the land-to-land vortex on everything that
  comes out of it (fn_005FE3B0 0x5FE5DD) and on the puzzle objects; it is not prey and, if it was already downed, it survives.

## API for other sessions (`ECS/AnimalAI.h`)

- **Spells** (Milagros): `CreateAnimal`, `MoveTo` (Living::SetupMoveToPos), `SetState` / `SetStateRaw` (vt+0x938),
  `Destination`, `SetFinalDestination` (SpellWolf +0x148), `Kill` / `DestroyedByEffect` (0x41B1B0), `Remove`, `SetAlpha`.
- **Death**: `AddDeathListener(fn)` → id / `RemoveDeathListener(id)`: all of them are notified at the start of Living::SetDying
  (vt+0x6A4), once per death and not while it is flying; `SetDeathCallback` is a single listener that replaces its own.
  `SetSpeciesDying(tipo, fn)` is a species' own SetDying and replaces the whole of Living::SetDying. It is used by
  SpellDove 0x41F5C0, SpellBat (shares SpellDove's slot) and SpellWolf 0x420CF0: a fade over
  GetNumTurnsToDieOver = 20 turns, without a carcass; Milagros registers it.
- **Script** (maps): `ScriptMoveTo(e, xz)` = the Living branch of MOVE_GAME_THING (GScript 0x6F8F6C): nothing in the hand; already
  there (AreWeThere vt+0x85C) → `SetScriptState(IN_SCRIPT 4)`, otherwise `SetupMoveToPos(pos, IN_SCRIPT 4)`.
  `SetScriptState(e, s)` = GScript::SetScriptState 0x6F82E0: StorePreviousState (0x417040: its final state),
  CallExitStateFunction (0x41A2C0, without looking at its answer), SetState(0, s) (no animal script state has
  an entry function, 0x41A310), its clip (SetAnim vt+0x8FC) and the counter +0x58 to 0. "On the map" for an animal = not
  in the hand [inferred].
- **Villagers** (maps): the eaten one dies via `ecs::villager::VillagerDead`; `villager::IsAtHome` is awaited (V4, excluding
  from the prey those who are at home) and the list of shepherds (V10).
- **Hand and physics** (water): `PlaceInHand`, `InitialisePhysics`, `EndPhysics`, `PutDown` (only for openblack's
  bodiless case, `physics::from_hand::PlaceWithoutBody`; the original ends the physics immediately, 0x5EFDF8). The
  physics and the hand reach them through `ECS/LivingPhysics` (the Animal class's physics handlers and
  `living::InterfaceSetInMagicHand`).

## Blobs and mesh of the animals

(Moved from rendering.md.)

### Blobs (done)

Report: `documentacion\render\animal_notes.txt`, data `animal_ebone_dump.txt`.
- In `fn_00812170`: if it is not human, with `IsHumanShadowed` (flag 0x4000000, `SetHumanShadowed(1)` in each species'
  Create; 0 while the creature holds it), and > 0.2 and a mesh with `ContainsEBone` → `fn_0081FFF0(obj, normal, ebone)`.
- EBone block (836 bytes) after the footprint ones (size at +8), UV2, name and extra metrics: `u32 tamaño; float m[16][12];
  int32 hueso[16]`. The positions of m[0..3] in their bone's space are used: P = object × bone × pos, y = ground + 0.2.
  Pair (0, 1) always, pair (2, 3) if bone[2] ≠ −1 (all the quadrupeds: 4 quads). Birds and bats have no EBone.
- **Oddity of the original**: the first quad of each pair receives V = D (it builds D + (P1 − P0)/2 but passes &D); the second
  D + (P0 − P1)/2.
- openblack: `L3DFile::GetEBone`, `L3DMesh::GetBlobPoints`, animal loop in `Renderer::DrawHumanShadows`.

### Creation: mesh and scale

- `CREATE_ANIMAL` (24, "ANNN": type, herd, village) and `CREATE_NEW_ANIMAL` (25, "ANNNN": + age) → `fn_00419D10`
  (herds and classes in [map-loading.md](map-loading.md#animals-and-flocks-create_flock-create_new_animal)). Mesh: `Object::CallVirtualFunctionsForCreation` 0x636BE0 gives the
  LH3DObject `GetDetailMesh(2, 1, 0)` (info +0x1FC + 4k: high, std, low) and the LOD is always 1: **the std one** (also
  `GetMesh`); openblack used the high one. Scale (`InitialiseScale` 0x417B20): young ones
  ageToScale[age − 1] + FloatRand(0.75·(ageToScale[age + 1] − s)); adults 1.05 − FloatRand(0.1). No initial angle.
- Land1 creates 116 (doves 40, seagulls 22, swallows 14, horses 12, cows 10, pigs 7, tortoises 6, bats 5);
  openblack creates them all, with the AI and the clips of this page.
- openblack: `components::Animal`, `AnimalArchetype`.

## Differences and pending

- The detour (`ECS/AnimalWallHug.*`): the order of the objects in a cell is that of openblack's registry, not the cell's
  list (the orbit sweep decides by bounds, so a tie or a different order can choose another intersection); the
  membership of the cell is computed (7.1 m circle; tree in its cell) instead of reading the map's list; a circle
  whose object is deleted is released [inferred]; the shared bookkeeping `g_CircleHugStateInfo` / `DoWallHuggerLookahead`
  (0x609A50, a ghost villager that simulates the path) is not ported; arcs and headings in metres, not in integer MapCoords.
  Two openblack checks that the original does not do (it reads `GetObjectPtr()` without looking): no circle at the start of the
  orbit or the sweep, only possible if its object was deleted [inferred]. `+0x76` is always set to 0 when preparing the walk
  (the original only with an entry in `g_CircleHugStateInfo`; nobody reads it before rewriting it) and the field `+0x78`
  (1 / 0x10) is not carried [inferred: unidentified, it is not read on the animals' path].
  openblack's port for villagers (`PathfindingSystem`) has its own bugs (wallhug.md §7).
- Lairs (`ECS/AnimalLairs.cpp`): `GUtils::GetDistance` is ported as is (`hypotenuse` 0x74F680 with the approximate
  inverse root of the 1024-entry table 0xDA5A10, ~0.1 %), but over openblack's float positions
  truncated to MapCoords. The tiger's water search is not ported (it does not change the result). `flock +0x5C`
  (without a lair) is not ported because the original only sets it to 0 (predator_ai.md). Distance ties between big forests
  and trees choose the newest object by creation index, like the original's head-insertion lists.
  The order of a forest's grown trees (`GrownTreesByDistance`, from "arboles") uses the 3D distance to the
  centre; the original (`DistanceToForest` 0x53A890 = `GetDistanceInMetres`) measures it only in x / z: on slopes it can
  change which tree is the lair.
- Eaten villager: dies via `VillagerDead`, but until milestone V12 of maps without a carcass, alignment, village notices
  nor mourning by the neighbours; the villagers do not
  flee from predators nor take reactions.
- Scripts (script_flags.md): the vortex does not exist in openblack, so nothing has the "cannot be eaten" flag yet
  (it needs Milagros: `script_held::SetCannotBeEaten` on what comes out of the vortex); there are no script flocks
  (FLOCK_CREATE / FLOCK_ATTACH not implemented: neither DisbandId nor its references); when releasing a villager
  `Villager::ReleaseFromScript` is not ported; `SetScriptState(0x20)` of the released animal is not ported (fn_0041AA00 overwrites it
  immediately); the death reason SACRIFICE (7), which deletes a carcass even if it is controlled, is not carried. The finders
  of openblack's CHL (CALL, GET_...) do not call `AddScriptGameThing`: the slot is created with the first reference
  [approximated]. When releasing an animal, "in physics or in the hand" (+0x24 & 0x44) is in openblack the physics
  component or the IN_HAND / FLYING states [approximated], and the flag g_game +0x14 & 0x8000 (scripts stopped: with it
  `GScript::Process` does not do `LookIn`, 0x6EB6C7) does not exist in openblack. The LHVM references are faithful: the
  original ScriptLibraryR.dll calls the natives ADD_REFERENCE / REMOVE_REFERENCE from POP (0x10008BC0: the new object
  and the old one of the variable) and when stopping a task (0x10006604); a task's parameters receive them through
  the POP of its prologue (script_flags.md §6).
- Flock merging (0x41A690 / 0x41A790): the original does not merge a flock with a shepherd (Flock +0x30, the villager of
  `VillagerBecomesShepherd` 0x768C1C: its own or the other) nor two of different players (`GetPlayer`); openblack has no
  shepherds nor flock player, so it only looks at its own flock's village [approximated].
- Shepherds, the leader priority of FLOCK_ATTACH, the birds' landing (unreachable in the game: info.sleep 0), the exact
  order of the lists of each map cell and the turn interleaved with the villagers.
- The random numbers go through `game_random` (GameRand over the synchronised seed, ffd386c8); the sequence is still
  not that of an original game while other systems drawing from the stream are not ported (engine-math.md «Random
  numbers») (approximate).
- Putting an animal down gently goes through physics like the original (`Object::InitialisePhysicsFromHand` 0x636F00,
  [physics.md](physics.md)): on land it leaves physics on the spot through `Animal::EndPhysics` 0x5F0D80. The three
  landing poses of `Villager::EndPhysics` are (pending, V13).

## Test hooks

`OPENBLACK_ANIMAL_TRACE=1` (state changes, landType and every 50 turns how many there are in each state),
`OPENBLACK_TEST_VIEW_ANIMAL="n[,distancia[,ángulo[,cada]]]"`, `OPENBLACK_TEST_THROW_ANIMAL="n,turno[,vx,vy,vz]"`,
`OPENBLACK_TEST_KILL_ANIMAL="n,turno"`, `OPENBLACK_TEST_ANIMAL_SPECIES=<AnimalInfo>` (n only counts that species),
`OPENBLACK_TEST_HUNGRY=<AnimalInfo>` (that species hungry on turn 1), `OPENBLACK_TEST_SPREAD_REACTIONS=<turno>`
(the predators spread their flee reaction again), `OPENBLACK_TEST_HUNT_VILLAGER="<especie>,<turno>"`,
`OPENBLACK_TEST_FOOD_PILE="<especie>,<turno>"`, `OPENBLACK_TEST_FOOD_BEHIND="<especie>,<turno>[,<tipo>[,<m>]]"` (the
first of that species, hungry, 8 m in front of the nearest building (0; not a field, which the circle iterator skips) or
feature (2) of 2..8 m radius or tree (1)
and a food pile m (6) behind it; its route every 2 turns; with the trace, each change of the detour),
`OPENBLACK_TEST_SMOKE=<n>`, `OPENBLACK_TEST_CORPSE_TURNS=<n>`, `OPENBLACK_TEST_LAIRS=<turno>` (lists the forests and
each predator leader recomputes its lair; with the trace, the chosen lair and the rule),
`OPENBLACK_TEST_SCRIPT_HELD="n,turno[,suelta]"` (the n-th animal is held by a script as if it had been
created by a CREATE; at `suelta` it loses the reference; every 25 turns its state, counter and flags),
`OPENBLACK_TEST_CANNOT_BE_EATEN=<turno>` (on that turn all the animals and villagers receive the vortex flag; with
`HUNT_VILLAGER` before the downing there is no hunt, after it the villager gets up),
`OPENBLACK_TEST_VIEW_LOCK=1` (the camera is placed every turn
next to the animal, for the birds). Land2 (`-s Land2.txt`) has lions, tigers and wolves. `dev\herramientas\animales_shot.sh <nombre> <fotogramas> <captura> [VAR=valor...]` launches a
private copy in `dev\animales_run`.
