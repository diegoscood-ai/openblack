# Animals: reactions, flocks and bodies

## Summary

Animals react to what happens near them through the same reactions as the villagers, but each reaction is offered
only once, when it is made: they flee from predators, go to food piles when hungry and run from things thrown from
the hand; no animal reacts to the hand itself. The hand can pick up the land animals and throw them; they land in a
pose by how they fell, die and lie as carcasses that smoke and vanish, and sink in the sea. They live in flocks that
split when one is picked up and merge after a landing; they age and grow but do not die of old age. Scripts can hold
them, and the vortex marks what comes out of it as not to be eaten. This page also owns the animals' ground blobs and
their mesh and scale at creation.

openblack reproduces the reactions, the hand, the flight and death, the clips, flocks, age, smoke, the scripts' hold
and the blobs; what is left is in Pending.

## The original

### Reactions

`Reaction::ProcessReactions`, which would distribute them every turn, depends on a debug switch that the game never
enables, so **each reaction is offered only once**, when created (`Reaction::CreateReaction` → `SpreadReaction`
0x6E3E10: (maxReactionDistance × 0.2)² cells of `GUtils::Spiral` within the radius;
`ApplyReactionToLivingObjectsAtSquare` 0x6E3F90 for each animal: available (`IsAvailableForReaction`), distance = half
Manhattan distance, score min(255, priority × (1 + 0.5 × howImportantIsDistance × (max − d) / max)), and its
**records** (3 at most, {type, turn}: it does not take a type again before NumGameTurnsBeforeReactingAgain; they expire
after 1800 turns). An animal that is already reacting only changes if the new one scores more and the current one has
already lasted 10 s (1 s if it was being picked up by the hand). Each type ends after its own number of turns
(`Living::ProcessReaction` 0x5F1270), if the initiator disappears (and then it goes back to the saved state: DECIDE, or
state 0 if it came from moving, and stays there like the original) or if the reaction disappears (it stays in its
state). Leaving to a state that is not a reaction one (the hand, dying, a need) drops the reaction without changing the
state (`Animal::ExitReaction` 0x41B170). The animals react to object (0), look (1), spell (3), creature (6), food (7),
fire (10), falling tree (27, which nobody creates) and predator (28); the predators also to flying object (9).
Tortoise and birds do not react. **No animal reacts to the hand**. The villagers receive the same reactions in the
same distribution ([Villager reactions](villager-reactions.md)).

- **Predator (28):** each predator creates it at birth (fn_0041FD30). Those with `isFleeingFromPredator` flee within
  25 m, if the predator is not stalking (at its speed2 or less it goes unnoticed); a predator only from a stronger one.
  Fleeing: state 49 at its flee speed (cow, sheep, pig 4 m/s; horse 9) ~10 m to one side of its trajectory; then 6
  (again if it is coming towards it or is closer than 30 m) or 30 (still, looking at it); ~80-88 turns or beyond 75 m.
- **Food (7, `Pot::SetupReaction` 0x66D660):** the map's food piles (CREATE_POT) on load, and a pot that the hand puts
  down; it is removed when it is picked up or emptied. **Hungry** herbivores within 35 m go to the edge of the pile
  (the sum of both radii), take 50 from it (the pile shrinks; when empty it disappears) and their hunger is set to 0.
  The piles the hand makes (MagicFood) have foodType 0: no animal goes to them, not in the original either.
- **Flying object (9, `Object::InitialisePhysicsFromHand` 0x637412):** what the hand throws makes the predators within
  25 m flee if 2 × its speed exceeds its distance; the reaction is removed when the object lands.
- `Living::IsAvailableForReaction` 0x5F11F0 (vt +0x984 of every Animal class) checks these conditions in order:
  IsFunctional (vt +0xD4, 0x5F1202; an animal always is); not controlled by a script (0x5F120C); not in the dance
  editor (0x5F121E); the final state (vt +0xB04) not one of the list 0x5F1227..0x5F1248; not dying (+0xB4 & 1,
  0x5F124A); not on a structure (+0x24 & 0x80, 0x5F1258, set only by `Living::MoveOnStructure`).
- Priorities: `Living::FleeFromPredatorPriority` 0x5F15D0 (at its speed2 or slower a predator goes unnoticed);
  `Lion::FleeFromPredatorPriority` 0x4203C0 (a predator only flees from a stronger one).
- More priorities: `ReactToFoodPriority` 0x5F1710 with `Animal::IsInterestedInFoodObject` 0x419BC0 (some food in it,
  not in the physics, near enough, and a hungry animal whose needsFoodTypes takes that food);
  `ReactToFlyingObjectPriority` 0x5F1800 (not while it flies or lands itself).
- Turn functions: `NumGameTurnsToReactToPredatorFunction` 0x5F1980 (0 unless the initiator is an animal; 0 while it
  looks at a predator that runs at it faster than 5 m/s), `Living::NumGameTurnsBeforeReactingAgainToPredatorFunction`
  0x5F1A00 (`NumGameTurnsBeforeReactingAgainToPredatorFunction`), which tests the minimum distance with the animal's own `GetDistanceInMetres` to the predator (0x5F1A45).
- "Coming towards me" is `Living::IsObjectTravellingTowardsMe` 0x5F1E60: the object's movement within acos 0.8 of the
  line to it, in 3D. Both points are `MapCoords::GetLHPoint` (0x605C40: x and z from 16.16, y the land's altitude there
  plus the altitude), and the movement is `GetMovementDirection` (vt +0x168): a thing in the physics gives its whole
  velocity (`Object::GetPhysicsMovementDirection` 0x6394E0), so a falling rock's drop counts; a walker its step, level.
- Every distance these reactions test (`FleeingFromPredatorReaction`, `FleeingFromObjectReaction`,
  `FleeFromObjectIfComingTowardsMe`, `LookingAtObjectReaction`, `FollowingObjectReaction`) is
  `GUtils::GetDistanceInMetres_0074cd50`, the same code as `GetDistanceInMetres` 0x74CD70: the whole-unit hypotenuse of
  the two MapCoords, then ConvertWholeDistanceToMeters.
- `Reaction::CreateReaction` 0x6E3D70 gets its player (+0x38) from the predator's GetPlayer (fn_0041FD30, 0x41FD5C),
  the pot's GetPlayer (`Pot::SetupReaction` 0x66D660), or the thrower's `GInterfaceStatus::GetPlayer` (0x637405).
- `Animal::SetupFleeFromPredator` 0x420410; `SetupReactToFood` 0x5F14C0.
- `StopReactingAndSetState` 0x5F11C0 → `Animal::ResetStateAfterReacting` 0x41A280: the stored state's return state
  (animalStateTable field0x1c).
- `Animal::GetFleeingPositionFromMovingObject` 0x420550: about 10 m sideways out of its path, on the side it stands
  on, ±4 m ((GameFloatRand(8) + side × distance) − 4, x first at 0x420688, then z at 0x4206B0).
- `Living::GetFleeingPositionFromStationaryObject` 0x5F2010: straight away from a still object; on it, it stays.
- Both flee points are the villagers' arithmetic: the side normalised once from the raw movement, the still case
  scaled by distance / length; the result is a MapCoords, so the goal is kept in 16.16.
- The flee states: `Animal::FleeingFromPredatorReaction` 0x4201F0; `Living::FleeingFromObjectReaction` 0x5F1D10 with
  `FleeFromObjectIfComingTowardsMe(obj, 30, 30)` 0x5F1D90 (the 30s are states, FLEEING_AND_LOOKING; the distance is the
  reaction's minDistanceToRunAwayFromObject); `SetupMoveToWithHug` is called at 0x5F1E3F.
- `Living::LookingAtObjectReaction` 0x5F23A0.
- Food: `Living::GotoFoodReaction` 0x5F2550 walks to the food's working point (SetupMoveToWithHug at 0x5F259A), then
  ARRIVES_AT_FOOD; `Animal::ArrivesAtFoodReaction` 0x41A0A0 sets hunger 0 and does RemoveResource(FOOD, min(50,
  amount)).
- The villagers' side of the same food and wood code (the jobs are on
  [Food and wood jobs](villager-jobs.md#food-and-wood-jobs-reactions-7-and-12)): `Living::SetupReactToFood` 0x5F14C0
  (vt +0x9BC) and `Villager::SetupReactToWood` 0x765B70 (vt +0x9C0) add the reaction (19 GOTO_FOOD_REACTION or 21
  GOTO_WOOD_REACTION, vt +0x990) and keep the object at +0xBC. `Villager::GotoFoodReaction` 0x7646A0 stops (back to its
  state) when +0xBC is gone or not available, else runs the Living one; `Villager::GotoWoodReaction` 0x7646D0 also stops
  when the wood is in the hand or on fire, else the Living 0x5F25C0 with FINAL 22. The interest scores, every product a
  float in this order: food `boost × frac × want × s × d + interact` (`Villager::IsInterestedInFoodObject`
  0x764EAE..0x764ED4); the town's `desire × frac × s × d`, then `interact +` it (0x764F05..0x764F28, and
  IsInterestedInWoodObject 0x7650E7..0x76510A); the drop-off `CalculateDesireForFood × frac × dm`
  (`Villager::ArrivesAtFoodReaction` 0x764A45..0x764A4F). `Villager::ArrivesAtWoodReaction` 0x764781..0x7647AE waits
  `ftol((qword)(1000 / msPerTurn) × 100.0)` turns (1000 at 100 ms). The turns before reacting again are
  `Living::StandardNumGameTurnsBeforeReactingAgainFunction` 0x5F1920 (vt +0xAC4); for wood
  `Villager::StandardNumGameTurnsBeforeReactingToWoodAgainFunction` 0x7648D0 (vt +0xAD8) gives 0 when the final state
  is the forester's work (47..50 or 52), else the standard (0 for reaction 12 too: info.dat's value is 0).
- ReactToFlyingObjectPriority (0x5F1800): not while it flies or lands itself
- fn_005F1E60: the object comes at it (its movement within acos 0.8 of the line to it)
- NumGameTurnsToReactToPredatorFunction (0x5F1980): 0 unless an animal; 0 while it looks at a predator that runs faster than 5 m/s at it
- `StartReacting`: Animal::SetupFleeFromPredator (0x420410)
- `StartReacting`: SetupReactToFood (0x5F14C0)
- Animal::GetFleeingPositionFromMovingObject (0x420550): ~10 m sideways out of its path, on the side it stands on, +-4 m; a still object: Living::GetFleeingPositionFromStationaryObject (0x5F2010), straight away (on it: stays)
- `FleeingPosition`: (GameFloatRand(8) + side x distance) - 4: x first (0x420688), then z (0x4206B0)
- FleeFromObjectIfComingTowardsMe(obj, 30, 30) 0x5F1D90: the 30s are states (FLEEING_AND_LOOKING), the distance is the reaction's minDistanceToRunAwayFromObject
- SetupMoveToWithHug (0x5F2890, called at 0x5F1E3F): a LINEAR walk round the obstacles (ECS/AnimalWallHug.cpp)
- `FleeingAndLookingReaction`: Living::LookingAtObjectReaction (0x5F23A0)
- Living::GotoFoodReaction (0x5F2550): to the food's working point, then ARRIVES_AT_FOOD
- SetupMoveToWithHug (0x5F2890, called at 0x5F259A): a LINEAR walk round the obstacles (ECS/AnimalWallHug.cpp)
- Animal::ArrivesAtFoodReaction (0x41A0A0): hunger 0 and RemoveResource(FOOD, min(50, amount))
- `IsPosValidForTurnAngle`: 0x41B229..0x41B246: R = ConvertWholeDistanceToMeters(ftol(2 x speed (+0x5A, fild; fadd st0, st0) / turn)), the speed in MapCoords per turn and turn = ConvertGameAngleTo3D(turnAngle). No test of the turn: with 0 the quotient is inf (or NaN with no speed), __ftol gives 0x80000000, R = -327680 m and both distances below exceed it (true)
- Reaction::CreateReaction(predator, 28) at a predator's construction (fn_0041FD30 0x41FD5C): the flee-from-predator reaction is spread once, then, to the animals within 25 m (the per-turn re-spreading is off in the shipped game)
- `BirdStartWander`: 0x41DFFD: (altitude + variance) - GameFloatRand(2 variance)
- fn_004196D0's "nearer than the stored candidate": 2 x Chebyshev(me, prey) (raw MapCoords) against Chebyshev of my CELL indices and the stored raw MapCoords (fn_0074CF30; the original's unit mix-up: in practice always nearer)
- Animal::GetFinalState (0x41A240): the top state if it is final, else the destination
- StandardNumGameTurnsToReactFunction (0x5F18C0) / ...BeforeReactingAgainFunction (0x5F1920)
- fn_0041DF20: the flock's altitude, else the info's altitudeNormal
- Dove::ReactToAnimalNeeds (0x41E600): no hunger, no breeding; info.sleep is 0 for every bird, so no landing
- Dove::SpecialMoveToPos (0x41E160): the leader re-steers every stayTime turns; a new leg on arrival
- Dove::FollowFlock (0x41E0B0)
- fn_0041E130: once the clip has played, flap or glide again, every turn until the state changes (the counter is not restarted)

### Hand, flight and death

- Picking up: `GAnimalInfo.playerCanPickUp` (all the land ones except those of the puzzles). When picked up it leaves
  its flock for one of its own (`SeperateLivingIntoNewFlock`) → IN_HAND. Dropping or throwing: physics → FLYING (clip
  THROWN).
- At rest (`Animal::EndPhysics` 0x5F0D80): landType by the turn-start matrix's right row (po+0xD8: y > 0.5 → 1, < −0.5
  or NaN → 2, otherwise 0; the villager's mapping reversed; a NaN gives 2 because `test ah, 1` at 0x5F0DBB takes the
  unordered compare as "<"), heading = Wrap(float(GetYAngle(current fwd row po+0xBC) + pi)) into SetYAngle (+0x4C and
  GameAngle), drawn afterwards at the GetYXZ yaw of the current matrix (RemoveObject 0x646B72). No body (Living
  0x5EFDF8 → EndPhysics(NULL)): heading unchanged, landType 3. A put-down goes IN_HAND → LANDED without FLYING; alive →
  LANDED (getting-up clip according to landType; the predators, the waking one) → the flock centres itself where it
  fell → INTERACT_DECIDE → off wandering. **There is no drowning state** in the animals (`Animal::EndPhysics` has no
  water branch): in the sea the animal never stops, its density rises and after **~75 turns (7.5 s)** it exceeds 1
  and `Living::HasSunk` 0x5ED370 kills it and deletes it (`SetDying`, state 15, `ToBeDeleted(0)`), alive or carcass.
  In a shallow cell with water of altitude ≥ 2 it lands **alive** (unlike the villager, who drowns); see
  [Sinking, drowning and being deleted](../world/water.md#sinking-drowning-and-being-deleted). Dropping gently puts it
  into physics (`Object::InitialisePhysicsFromHand` 0x636F00), over the sea and on land; on land it leaves the physics
  on the spot through `Animal::EndPhysics` 0x5F0D80.
- **Sinking** in detail: `Living::HasSunk` 0x5ED370 (the animals' vt +0x7B8) returns 0 for an animal that is not
  available (`GameThing::IsAvailable` 0x401810 at vt +0x2C, 0x5ED375: the GameThing flag byte +0xA & 0x01 clear);
  then, when a hand dropped it last (`GetInterfaceStatusWhoLastDroppedMe` 0x63A710), ConsiderMakingCreatureMimicPlayer(..,
  0x15 THROW_IN_THE_SEA, the animal) at 0x5ED3A9; then SetDying (vt +0x6A4, `Living::SetDying` 0x5EC390),
  SetTopState(15) (vt +0x8E8) and ToBeDeleted(0) (vt +0xC, `Animal::ToBeDeleted` 0x417B60), and 1.
- Death (`Living::SetDying` 0x5EC390, nothing while it is flying): DYING (falling clip) → DEAD (lying according to
  landType; the predators with the sleeping clip) 600 turns (never if a script controls it) → its smoke
  (`CreateSmokyStuff`, below) and it disappears. A thrown carcass goes back to DEAD with another 600.
- `Animal::InterfaceSetInMagicHand` is 0x419B60; `Living::InitialisePhysicsFromHand` 0x5EFD80 (every drop and throw is
  physics, FLYING).
- `Animal::EndPhysics` writes the landType into +0xB4 bits 4-5 (0x5F0E06..0x5F0E21) and altitude +0x1C = 0
  (0x5F0E28).
- `Living::IsDead` 0x417270 also counts an animal that is not functional: `Living::IsFunctional` 0x4172D0 =
  IsAvailable (vt +0x2C) == 1 and GetLife (vt +0x11C) != 0.
- `Living::Dead` deletes the vanished animal in its own turn (vt +0xC ToBeDeleted(0), 0x5EC462), off the list for the
  livings after it (the loop has already read its next).
- `Animal::DeleteDependancys` 0x417BA0 unlinks it from its town's list (+0xE0's list +0x984, 0x417BA4..0x417BF6,
  −−+0x988), sets +0xE0 = 0 (0x417BF7), then leaves its flock (0x417C02..0x417C18). No prey / hunter link is cleared:
  the readers ask IsAvailable.
- The death reason is `Animal::GetDeathReason` 0x417890 (+0x10C). `Living::Dead` 0x5EC400 deletes the carcass (smoke,
  ToBeDeleted) when not controlled and its counter was 0, **or** whenever `GetDeathReason` (vt +0x444, byte +0x10C) is
  7 SACRIFICE, controlled or not; the only writer of that byte is `Animal::ApplyThisToObject` 0x41B403, the hand giving
  an animal to a WorshipTotem (with ConsiderMakingCreatureMimicPlayer(0x28), the player's sacrifice count +0x1124 and
  `GAlignment::Update(.., 7)`).
- Living::StateDead (0x5EC400): off the flock; the corpse lies 600 turns, then its smoke puff and it goes. A script-controlled one (+0x25 & 4, 0x5EC41E) never times out. The counter is living::DeadTick (ECS/Villager/ VillagerDeath.h), shared with the villagers; Animal::GetDeathReason 0x417890 (+0x10C) is not kept, so the SACRIFICE 7 rule never applies to an animal yet
- Living::Dead 0x5EC400: the vanish deletes it in its own turn (vt +0xC ToBeDeleted(0), 0x5EC462), off the list for the livings after it (the loop read its next first: living_turn::WalkList)
- `IsDead`: 0x417270..0x417283: +0xB4 & 1, or the TOP state 15
- One animal's turn when the list reaches it (0x5EC8C6 ProcessReaction, 0x5EC8CF ProcessState vt +0x620 = Animal::ProcessState 0x417EE0): the brain made on its first turn, a lone predator's lair, then its state. One that vanishes is deleted at once (Living::Dead 0x5EC462)
- fn_004196D0 0x419702: target.IsReachable (vt +0x530) = Villager::IsReachable 0x756460 (IsAvailable, not at home, not +0x24 & 4, TOP != 236);
- `TargetPounce`: 0x5EC485 +0xB4 |= 0x80, 0x5EC491 SetLife(0.05) (vt +0x5B0), 0x5EC49D SetTopState(0x11 DOWNED) (vt +0x8E8 = Villager::SetTopState 0x752010), for every Living, a corpse too (no IsDead test): DEAD's exit (Living::CannotExitState 0x768640) refuses the state of a corpse.
- Villager::BeingEaten (0x76B380): l = GetLife; SetLife(0); VillagerDead(ANIMAL 3, GetPlayer (the villager's: Villager::GetPlayer 0x7502F0, its town's owner), l, 1) 0x76B3DB: it lies as a corpse (SetDying, 600 / 120 turns)

### Clips per species

| species | walk | idle | eat | lower / raise head | hand | thrown | lands lt 0/1/2 | dead lt 1/other | falls |
|---|---|---|---|---|---|---|---|---|---|
| cow | 45 (41 runs) | 42 | 36/35 | 37 / 44 | 38 | 43 | 42/40/39 | 30/29 | 31 |
| sheep | 145 (141) | 142 | 133/132 | 136 / 140 | 137 | 143 | 142/139/138 | 131/130 | 134 |
| pig | 128 (125) | 126 | 117/116 | 120 / 124 | 121 | 127 | 126/123/122 | 115/114 | 118 |
| horse | 61 / trot 59 / 56 | 57 | 49/48 | 52 / 60 | 53 | 58 | 57/55/54 | 47/46 | 46 |
| tortoise | 172 | 171 for everything | | | | | | | |

Thresholds: `speedThreshold` entry 2 (cow, sheep and pig) and 3 (horse). Predators (lion, tiger, leopard, wolf):
stalking below speedDefault, walking, running above entry 6..9. The clip advances with the terrain covered while it
moves (`Object::IsMoving`) and with time otherwise.

- `Animal::GetAnimId` 0x417FA0 always asks the species' function of the state (g_AnimalStateTable slot 0x60), with no
  info.dat fallback; −1 keeps the clip playing (`Animal::StandardAnimation` 0x4177D0). Animals have no into / out-of
  clips.
- The species' clip functions span 0x41C0E0..0x41CF10: `Cow::MoveAnimation` 0x41C720 (run above its walk speed; sheep
  and pig use the cow's threshold, entry 2), `Horse::MoveAnimation` 0x41CA70 (entry 3: walk, trot, run),
  `Cow::EatAnimation` 0x41C740 (GameRand(2) at 0x41C74C picks one of the two), `Cow::LandedAnimation` 0x41C760,
  `Cow::DeadAnimation` 0x41C790 ((status & 0x30) == 0x10 ? DEAD_ON_RHS : DEAD_ON_LHS).
- The birds' slots span 0x41BD20..0x41C080. Decide is `Animal::DecideAnimation` 0x41BD20, the Move function. The coin
  is GameRand(2) == 0 ? first : second (Dove 0x41BD39, Crow 0x41BEC9, Pigeon / Seagull 0x41BF39 / 0x41BFA9); the
  swallow's three-way draw is GameRand(3) at 0x41C019.
- The birds' Stand, Dead, Eat, Sleep and Thrown slots are fixed ids: the odd ones (e.g. the crow's stand = TAKEOFF)
  are what the code returns. Every other slot is −1.
- The birds' SetAnim (the one that never restarts the clip) is fn_0041E7E0.
- **The first clip** is chosen as the animal is made (`CallVirtualFunctionsForCreation` → `Living::SetAnim(1)`), in 43
  DECIDE_WHAT_TO_DO: the species' stand clip from the start, or a bird's move clip with its draw, keeping its time;
  the evidence is in [openblack's loop](../engine/loop.md#openblacks-loop).
- The clip's advance is `Animal::Draw` 0x51C310 → fn_0051AF00 (by the ground covered while `Object::IsMoving`,
  otherwise by time). `Animal::Draw` (0x51C34C), `Dove::Draw` 0x41F777 and `SpellWolf::Draw` 0x51C579 use the villagers'
  `Living::PreDrawScale` 0x51AF00: while moving (vt +0x8B8, Living 0x5EF350) and with the clip's flag 0x200 (+0x50)
  clear, `MoveAnimByDist` by speed (+0x5A) × [g_game +0x250540] / (scale (vt +0x120) × 655350.0 [0x8D8748])
  (0x51AF4C..0x51AFA3), otherwise `MoveAnimByTime`. So an animal's moving clip is advanced by the ground from its
  whole speed exactly as a villager's.
- An animal in IN_DANCE (state 5) would play row 5's dance clip.
- Cow::MoveAnimation 0x41C720 (Sheep and Pig use the cow's threshold, entry 2): run over its walk speed
- `MoveClip`: Horse::MoveAnimation 0x41CA70, entry 3: walk, trot, run
- `BirdClip`: Animal::DecideAnimation 0x41BD20 = the Move function
- Cow::DeadAnimation 0x41C790: (status & 0x30) == 0x10 ? DEAD_ON_RHS : DEAD_ON_LHS
- Cow::EatAnimation 0x41C740: GameRand(2) picks one of the two (0x41C74C)
- `AnimalAnimId`: Animal::StandardAnimation 0x4177D0
- Cow::LandedAnimation 0x41C760: lt 0 ? STAND : lt 1 ? LANDED_RIGHT_SIDE : LANDED_LEFT_SIDE
- Living::SetStateAnim (0x5ECB10): the state's clip from the start; the birds' SetAnim (fn_0041E7E0) never restarts it

### Flocks, age and smoke

- **Picking up an animal** (`Flock::SeperateLivingIntoNewFlock` 0x52FE10): it always goes to a flock of its own where
  it was picked up, with the radius and distance of the old one, without a village and maximum 0; the old one, if it
  is left empty, is deleted, except a script flock (+0x25 & 4: it releases the animal's reference and stays even if
  empty, 0x419B75).
- **Merging** (`Animal::LookForFlocksInSpiral` 0x41A690): only after landing (±80 m), because `flocksCanMerge` is 0 in
  all of them; the biggest one keeps all of them if they are of the same species and the sum does not exceed
  `maxFlockSize`; a flock with a village does not merge; two script flocks do not merge and a foreign script one
  always keeps all of them (0x41A837, fn_005302A0 0x5302B4). It does not merge a flock with a shepherd (Flock +0x30,
  the villager of `VillagerBecomesShepherd` 0x768C1C: its own or the other) nor two of different players
  (`GetPlayer`). Oddity: the maximum of the one that stays adds the other's once per member, clamped.
- **Age** (`Living::GetAge` 0x5ECAF0): 1500 turns per year since its birth turn. The young grow every 375 turns
  (`scale += random(0.75 × (ageToScale[age + 1] − scale))`) until `grownUpAge`; an adult is born at 0.9 and two rolls
  leave it in (0.95, 1.05]. **Animals do not die of old age**: checked across the whole executable: `oldAge` /
  `retirementAge` are only read by `Villager::CheckDeathFromOldAge` (0x760CA0) and the death reason OLD_AGE (9) is only
  for villagers.
- **Being born** (GIVES_BIRTH): the newborn decides immediately, before the mother goes back to wandering.
- **Animal thrown into a food store:** it becomes food (its foodValue: cow and horse 1200, sheep 800, lion and tiger
  900, wolf 700, pig 290) and disappears (`Animal::ReactToPhysicsImpact` 0x41BC10): the hit (`GetGameObjectWhoHitMe`)
  and the animal available, and the hit `IsResourceStore(FOOD)` (a storage pit, a worship site) → the hit's
  `DeleteObjectAndTakeResource(this, po +0x24)`: the thrower's interface, so the pit's town gets the desire, alignment,
  belief and the creature deed, the Supply help, reaction 22, the ghost; when it took, nothing more, else `Living`'s
  damage. The food is `ftol(GetFoodValue(3))` 0x41BC80 = `(GetFoodType & 3) ? foodValue : 0` (0x4026D0): an animal
  whose foodType has neither bit is still taken and deleted, worth nothing. Given from the hand on the press: valid
  only on a **StoragePit** (RTDynamicCast, 0x41B320; else `Living`'s totem test, the sacrifice); apply 0x41B360:
  `IsResourceStore(FOOD)` and not INDESTRUCTIBLE (+0x24 bit 0x4000) → taken, 3, else 0 (still held). A worship site
  puts the food in its food pot (`WorshipSite::AddResource` 0x77C5F0, [Spell core](../magic/spell-core.md)).
- **Smoke from the carcass** (`Object::CreateSmokyStuff` 0x63A810 → `SmokyStuff::Create` 0x823C90 mode 0, size 1): 15
  sprites of `Data\Textures\smoke.raw` with a random direction at 0.3..1 × size, grey 0x808080 with opacity life × 100,
  3 s, from 0.5 to 1.5 × size.
- **In the hand** the animals have the villagers' grip (2D radius, drop 0.65). The landType is read from the body's
  matrix at the start of the turn.
- `ageToScale` is at GAnimalInfo +0x27C, read at +0x280 + 4 × age by `SetScaleForAge` 0x417A5C; index −1 is the dword
  before the table (`InitialiseScale` 0x417B30 at age 0): +0x278, altitudeNormal.
- `fn_00417C50(town)` (SetTown) inserts the animal at the head of the town's list (a new node, ++ +0x988,
  0x417C66..0x417C85) but does not take it off the list of the town it had (only `Animal::DeleteDependancys` unlinks),
  so a second call can list it twice.
- Flock merging (`LookForFlocksAtPos` 0x41A790): FindTypeOnMap(4, 0) 0x41A7A7 .. FindTypeOnMap(4, obj) 0x41A876 walks
  the ANIMAL objects of the cell's mobile list from its head (type 4 does not count as fixed); the merge itself is
  fn_00530210.
- `Living::SetFlock` 0x5EE5F0: an old flock → RemoveLivingFromFlock(living, 1) (0x5EE5F3..0x5EE600), then +0xB8 =
  flock (0x5EE609 / 0x5EE617).
- AddMember fn_0052FA50: the old flock's +0x30 is cleared when it is this living (0x52FA55..0x52FA6B); fn_005ECFE0
  takes it out of its flock, even this one, with deleteWhenEmpty 1 (0x52FA74); already in the list → 0
  (0x52FA79..0x52FA8F); `Living::SetFlock` at 0x52FB2E.
- AddLeader (0x52FC75..0x52FDF4): if it is already the tail, nothing (0x52FC97); otherwise it is unlinked and inserted
  again by +0xD4 (0x52FCDA..0x52FDEE, no SetFlock); a non-member goes through AddMember (0x52FDF4).
- `Flock::RemoveLivingFromFlock` 0x52FB50(living, deleteWhenEmpty): a non-member still gets +0xB8 = 0 and returns
  false (0x52FB6D); otherwise it is unlinked (0x52FB93..0x52FBF7, −−count), +0xB8 = 0 (0x52FBFA) and, with
  deleteWhenEmpty and no member left, the flock's ToBeDeleted(0) (0x52FC00..0x52FC10).
- `SeperateLivingIntoNewFlock` 0x52FE10 calls RemoveLiving at 0x52FE1E, creates its own flock at 0x52FE41, and copies
  the old flock's domainRadius and flockDistance at 0x52FE4A..0x52FE57.
- `Flock::Flock(Living*)` 0x52F950 does not write +0x88 (maxMembers). `Flock::Flock` 0x52F780 hands its GFlockInfo and
  its player to its Container base (+0x28 / +0x2C; fn_0046B8A0, 0x46B8DC / 0x46B8F4); none for `Flock::Flock(Living*)`.
- Merge fn_00530210(keeper, other) walks the other's cursor (+0x44) from its head while it has a member
  (0x53021B..0x53022A); each step does keeper +0x88 += other +0x88 (0x53022C..0x530232), clamped to the maxFlockSize
  (info +0x260) of the animal at the keeper's cursor +0x44 (0x530238..0x530272); then AddMember(keeper, head)
  (0x530282 / 0x530291) empties and deletes the other.
- Merging a flock into itself (attaching a flock to itself) never empties `other` and hangs the game.
- fn_00530180(exclude) picks a random member: count 0 → null (0x53018A); r = GameRand(count − 1) when there is an
  exclude and more than one member, otherwise GameRand(count) (0x530190..0x5301B1, "Flock.cpp" 0x182 / 0x186); then,
  from the head, the first member at an index ≥ r that is not the excluded one (0x5301BE..0x5301EF).
- `Flock::FindLiving` 0x530510: from the head (the cursor +0x44, 0x53051E..0x530557), the first member the callback
  accepts.
- `Living::PosWithinDomain` 0x5ED010(pos, factor): GetDistanceInMetres from the domain centre (0x5ED025..0x5ED037,
  `Living::GetFlockPos` 0x5ECF60 = flock +0x14); outside when radius × factor < distance (0x5ED04E..0x5ED05F); **0
  without a flock** (0x5ED01E).
- FLOCK_ATTACH also makes its flock with the script id 0xABA52 at +0x8C (0x6EF6EA; FLOCK_CREATE at 0x6F2271).
- The flock's calm (+0x5C) is written by CHANGE_INNER_OUTER_PROPERTIES (ftol, 0x70F53C); the lairs' code only sets it
  to 0 (both readings hold, each in its scope). Its readers are the prey test and the wolf's
  ([Predators](animals.md#predators-lion-tiger-leopard-wolf)).
- The Flock class spans Flock.cpp 0x52F780..0x530590.
- Lairs use `GUtils::GetDistance` (`hypotenuse` 0x74F680 with the approximate inverse root of the 1024-entry table
  0xDA5A10, ~0.1 %); the tiger's water search does not change the result.
- `InteractDecideWhatToDo`: 0x417D83..0x417DA2: with a flock (+0xB8), LookForFlocksInSpiral(2 x domainRadius, merge = 1)
- Animal::LookForFlocksAtPos 0x41A790: FindTypeOnMap(4, 0) 0x41A7A7 .. FindTypeOnMap(4, obj) 0x41A876, the ANIMAL objects of the cell's mobile list (type 4 does not count as fixed) from its head
- ageToScale (GAnimalInfo +0x27C, read at +0x280 + 4 age by SetScaleForAge 0x417A5C): index -1 is the dword before the table (InitialiseScale 0x417B30 at age 0): +0x278, altitudeNormal (the field SpellFlock reads at +0x278);
- `ScaleForAge`: 0x417A5C..0x417A73: float(ageToScale[age + 1] - scale) x 0.75 (fmul qword, rounded to 24 bits, fstp dword)
- Animal::InitialiseScale 0x417B20: a young one (age < grownUpAge +0x138, jae) ageToScale[age - 1] (+0x278 + 4 age: at age 0 the dword before the table), an adult 0.9 (0x3F666666)
- Flock::Flock(Living*) 0x52F950 (0x41AA47): its own, radius info +0x25C (domainRadius), distance (int)info +0x21C (flockDistance) +0x88 (maxMembers) is not written by Flock::Flock(Living*) (0x52F950), as PlaceInHand's own flock
- The insertion of fn_0052FA50 (0x52FAB9..0x52FB25) and AddLeader 0x52FD79..0x52FDDB: before the first node from the head (members.back) with +0xD4 >= the new one's (cmp; sbb; inc: node >= new), else at the tail (members.front)
- Flock::Flock(Living*) 0x52F950: the living's Pos (+0x14), no id; then fn_0052FA50 (0x52FA1F)
- `SetFlock`: 0x5EE5F3..0x5EE600: an old flock -> RemoveLivingFromFlock(living, 1); 0x5EE609 / 0x5EE617: +0xB8 = flock
- `AddMember`: 0x52FA79..0x52FA8F: already in the list -> 0
- `AddMember`: 0x52FB2E: Living::SetFlock (its old flock is gone already: only +0xB8 = this one)
- `AddLeader`: 0x52FC75..0x52FC92: the tail's +0xD4 + 1, 5 without a tail (a byte: 255 + 1 is 0)
- `AddLeader`: 0x52FC97: the tail already
- `AddLeader`: 0x52FC9F
- `AddLeader`: 0x52FCDA..0x52FDEE: unlinked (--count) and inserted again by +0xD4 (++count), no SetFlock
- `AddLeader`: 0x52FDF4
- `RemoveLiving`: 0x52FB6D: not a member: +0xB8 = 0 all the same, 0
- `RemoveLiving`: 0x52FB93..0x52FBF7: unlinked, --count; 0x52FBFA: +0xB8 = 0
- `RemoveLiving`: 0x52FC00..0x52FC10: no member left and deleteWhenEmpty -> ToBeDeleted(0) (vt +0xC)
- `SeparateIntoNewFlock`: 0x52FE1E
- `SeparateIntoNewFlock`: 0x52FE41
- `SeparateIntoNewFlock`: 0x52FE4A..0x52FE4E
- `SeparateIntoNewFlock`: 0x52FE52..0x52FE57
- `Merge`: 0x530238..0x530272: the keeper's cursor +0x44 cast to Animal: +0x88 clamped to its info's maxFlockSize (+0x260). ⚠️ approximate +0x44 is not kept: AddMember moves it to the node it has just inserted (0x52FB09 / 0x52FB25), which is members.back only when that member's +0xD4 is 0; and at the first step it can be null (no clamp)
- `Merge`: 0x530282 / 0x530291: AddMember(keeper, head), which takes it out of the other (deleted when emptied)
- `RandomMember`: 0x53018A
- `RandomMember`: 0x530190..0x5301B1: GameRand(count - 1) with an exclude and more than one member, else GameRand(count)
- `RandomMember`: 0x5301BE..0x5301EF: from the head, index >= r and not the excluded one
- `FindLiving`: 0x53051E..0x530557: the cursor (+0x44) from the head, the first the callback accepts
- Living::SetFlock 0x5EE5F0: a flock already -> RemoveLivingFromFlock(it, living, 1); then +0xB8 = flock
- fn_0052FA50 (AddMember): fn_005ECFE0 first (out of its old flock, which goes when emptied); already a member -> false ("Living already in Flock" is the callers'); else inserted by +0xD4, ++count, Living::SetFlock. True when added
- Flock::RemoveLivingFromFlock 0x52FB50(living, deleteWhenEmpty): not a member -> +0xB8 = 0, false; else unlinked, --count, +0xB8 = 0 and, with deleteWhenEmpty and no member left, the flock's ToBeDeleted(0) (vt +0xC). True when removed
- Flock::SeperateLivingIntoNewFlock 0x52FE10(living, deleteWhenEmpty): RemoveLiving, then a flock of its own (Flock:: Flock(Living*) 0x52F950) with this one's domainRadius and flockDistance (0x52FE4A..0x52FE57). The new flock
- fn_00530210 (keeper, other): from the other's head, while it has a member: keeper +0x88 += other +0x88 (each step), clamped to an animal member's maxFlockSize (info +0x260), then AddMember(keeper, it), which empties and deletes the other at the end
- The members from the head (the original's walking order: FindLiving 0x530510, DisbandId, the script loops)
- fn_00530180(exclude): count 0 -> null; r = GameRand(count - 1) (Flock.cpp 0x182) when exclude and count > 1, else GameRand(count) (0x186); then from the head the first member at an index >= r that is not `exclude`
- Living +0xD4: its place in its flock's member list (0, Living::SetToZero 0x5EC03F; AddLeader 0x52FC9F sets it)
- `CreateAlone`: 0x419CB7 / 0x419CC2: fn_00417C50(town) for a grazing animal, fn_00417C50(0) otherwise
- Dove::IsPosValidForMapCellExistance (0x41F840) [the tie rules not read]: a land block under the point
- Animal::GetFinalState (0x41A240): the top state if it is final, else the destination
- Living::ExitInHand (0x5ED500): only thrown, landed or dying while held
- `CallExitStateFunction`: Living::ExitInFlying (0x5ED540): caught, landed or dying
- `FlockSteer`: fn_0074D090 [inferred |dx| + |dz|]
- Animal::CheckNeeds (0x418450): 3 ready to breed (else the breed counter restarts when the flock is full), 1 hungry, 2 sleepy, 0 none
- Cow::ReactToAnimalNeeds (0x41D310): breeding, then hunger, then sleep
- `CowReactToAnimalNeeds`: fn_00418220 again (ProcessNeeds counts it too)
- Living::KeepLeaderWithinDomain (0x41AAD0): the leader takes the herd to a new place every stayTime turns
- `DecideWhatToDo`: Animal::CheckNeeds (0x418450) == 3: ready to breed
- Animal::StartToEat (0x418280): 15..24 eat clips; the grazers' Cow::StartToEat (0x41D4A0) then makes it 20..34
- `StartToEat`: 0x41828F
- `StartToEat`: 0x41D4B4
- Animal::SeekSleep (0x4180D0): to a spot in a square around its sleep cell, 2 m per flock member (fn_0074F310)
- Living::Downed (0x5EC4B0): the Dying clip, then being eaten for 300 turns

### Scripts and flags

- **Held by a script** ✅ faithful: the original's script objects are slots of `ScriptManage` (511, 0xD967F8) with
  their reference count. Each LHVM variable that stores an object gives it a reference (the original
  ScriptLibraryR.dll calls the natives ADD_REFERENCE / REMOVE_REFERENCE from POP (0x10008BC0: the new object and the
  old one of the variable) and when stopping a task (0x10006604); a task's parameters receive them through the POP of
  its prologue); with the first one it becomes `IsInScript` (+0x24 & 0x200) and, if a script created it (`CREATE`,
  `CREATE_WITH_ANGLE_AND_SCALE`: `AddScriptGameThing(thing, 1)`), **controlled by the script** (+0x24 & 0x400). After
  each `LookIn` (`GScript::Process` 0x6EB6DB → fn_0070D480) whatever no longer has references is released: it loses
  both flags and an animal goes through fn_0041AA00 (own flock if it has none; dead → SetDying, which gives the
  carcass another 600 turns; alive → INTERACT_DECIDE_WHAT_TO_DO). With g_game +0x14 & 0x8000 (scripts stopped)
  `GScript::Process` does not do `LookIn` (0x6EB6C7).
- **The finders**: `AddScriptGameThing` 0x70D0F0 (53 call sites; created flag pushed: 1 for CREATE, CREATE_HIGHLIGHT /
  TIMER / MIST / REWARD(_IN_TOWN) / WITH_ANGLE_AND_SCALE / RANDOM_VILLAGER_OF_TRIBE, FLOCK_CREATE, FLOCK_ATTACH's new
  flock, DANCE_CREATE, POPULATE_CONTAINER, CREATURE_CREATE_RELATIVE_TO_CREATURE, INFLUENCE_OBJECT / POSITION,
  SPECIAL_EFFECT_POSITION / OBJECT, SPELL_AT_THING / POS, the first GET_ARENA call; 0 for the CALL family, GET_... and
  the vortex fn_005FE3B0) takes a slot when the thing is not in a script, else finds its own. The release of a slot
  without references (fn_0070D480 → ReleaseControlFromScript 0x70D540) only decrements a flock's members for a thing
  not controlled, and releases a controlled one into the game.
- **POPULATE_CONTAINER** (`GScript::PopulateContainer` 0x6F1F20): the subtype, the type (1..41, else "Invalid type"
  and a 0 left on the stack), the quantity (its low 32 bits as the count) and the container; count things about the
  container's place (its map x and z, the land's height there plus its own height above it), each by two
  GameFloatRand(2 × spread) less the spread, across and then in height, in line along z, spread = count × 0.1 + 4; each
  made as CREATE makes it, deleted when not living, added as a created script thing, then joining a dance (IN_DANCE) or
  a flock (MOVE_IN_FLOCK, "Living already in Flock" logged and on all the same) with a reference; "Not implemented -
  Thing not added to id" for another container.
- **The release**: `GScript::ReleaseScriptThingIntoTheGame` 0x70F600 gives an animal SetScriptState(0x20) or, in the
  physics (+0x24 & 0x40), SetScriptFlyingState(0x20) and then fn_0041AA00; a villager 0xA3 the same way and
  `Villager::ReleaseFromScript` 0x7531D0.
- **Controlled by the script**: it is not prey except for a hunter that is in a script (HuntingMoveToPos 0x418DD7,
  fn_00419340, fn_004196D0), it does not take reactions (IsAvailableForReaction 0x5F120C), **its carcass does not
  expire** (`Living::Dead` 0x5EC41E) and its flock is a script one (hand and merging, above).
- **Wolf in a script**: the leader puts the lair where it is (`Wolf::CalculeLairPos` 0x421774) and hunts from it at
  any time without looking at hunger (`Wolf::HideInLair` 0x421A2E).
- **Cannot be eaten** (+0x25 & 0x40): it is set by the land-to-land vortex on everything that comes out of it
  (fn_005FE3B0 0x5FE5DD) and on the puzzle objects; it is not prey and, if it was already downed, it survives.
- Releasing (fn_0041AA00): it does nothing while +0x24 & 0x44 (in the physics or in the hand) or g_game +0x14 &
  0x8000. It creates its own flock at 0x41AA47 (`Flock::Flock(Living*)`: radius info +0x25C, distance (int) info
  +0x21C). A dead animal released this way goes to SetDying at 0x41AA89; one that was already dying (status & 1) goes
  straight to DEAD. A living one calls vt +0xB88 `Animal::InteractDecideWhatToDo` 0x417D80 at once (0x41AAB7).
- `InteractDecideWhatToDo`: 0x417DAB: StartWander (vt +0xB48): the Dove class its own, Animal::StartWander 0x417C90 for the grazers, the hunters and the SpellWolf;
- Animal::Landed (0x417D50): CalculeLairPos, the flock now centres where it landed (the predators: their lair)
- The forest loop shared by Tiger (0x4214D2) and Wolf (0x42181A): the forest list newest first (g_game +0x205BB4, next +0x44), the first forest taken unscored, a later one replaces it only if it scores strictly more (fcomp; test ah, 1). The tiger also calls FindNearestDrinkingWater(me, 500) per forest; Returns the chosen forest's first grown tree (Forest +0x48: the one nearest its centre), none when there is no forest or it has no grown tree.
- `IsPrey`: +0x25 & 0x40 (0x4196F4): it cannot be eaten; +0x24 & 0x400 (0x41973B): a script's, only for a hunter in a script. Both are tested for the villagers too.
- GetFirstIterator 0x6034D0 at 0x4194E9: the fixed list, then the mobile one (0x41951B..0x419531), each from its head (GetMapChild 0x41950D); the first IsPrey (fn_004196D0, 0x4194F9) wins.
- `CurrentTargetOk`: the script test at 0x419376 (script_held::MayTarget)
- Wolf::HideInLair (0x4219E0): lies in the lair; hungry after 23:00 the pack goes hunting
- `HideInLair`: 0x421A0E: a leader in a script hunts from its lair at any hour, without the hunger test
- Living::IsSkeleton (0x416FF0): status bit 0x40 (never set for animals: their corpses are prey too)
- Animal::SetupMoveToTarget (0x418D20): a downed prey is eaten, any other chased (the state only, the clip stays)
- `PredatorReactToAnimalNeeds`: Wolf::ReactToAnimalNeeds (0x421950)
- Animal::ReactToAnimalNeeds (0x4183C0): hunger (no food: on to sleep), then sleep
- fn_005EC240: the current move, and on arrival SetTopStateToFinal
- Living::Downed (0x5EC4B0): its clip (P_ATTACKED_BY_LION), then being eaten for 300 turns
- Living::StateBeingEaten (0x5EC4D0): 300 turns, then dead (the corpse 50 turns)

### API for other systems

What the animals offer the rest of the game, by the original's functions:

- **Spells**: making an animal, a walk (`Living::SetupMoveToPos`), a raw state set (vt+0x938), the destination, the
  final destination (SpellWolf +0x148), a kill (`DestroyedByEffect` 0x41B1B0), the removal and the alpha.
- **Death**: every listener is told at the start of `Living::SetDying` (vt+0x6A4), once per death and not while it is
  flying. A species' own SetDying replaces the whole of `Living::SetDying`: SpellDove 0x41F5C0, SpellBat (shares
  SpellDove's slot) and SpellWolf 0x420CF0: a fade over GetNumTurnsToDieOver = 20 turns, without a carcass.
- **Script**: the Living branch of MOVE_GAME_THING (GScript 0x6F8F6C): nothing in the hand; already there (AreWeThere
  vt+0x85C) → SetScriptState(IN_SCRIPT 4), otherwise SetupMoveToPos(pos, IN_SCRIPT 4). `GScript::SetScriptState`
  0x6F82E0: StorePreviousState (0x417040: its final state), CallExitStateFunction (0x41A2C0, without looking at its
  answer), SetState(0, s) (no animal script state has an entry function, 0x41A310), its clip (SetAnim vt+0x8FC) and the
  counter +0x58 to 0. "On the map" for an animal = not in the hand [inferred].
- **Creatures**: the animals' part of `CanBeFrighteningToCreature` (vt+0x238): yes for Bat 0x41EF20, SpellBat
  0x41F000, Vulture 0x41F0D0 and Lion 0x41FC70, which Tiger, Leopard, Wolf and SpellWolf share; no for every other
  species, PuzzleLion and PuzzleWolf (0x4200E0, 0x421D90) included. A creature runs away from what frightens it
  ([What frightens a creature](../creature/mind.md#what-frightens-a-creature-and-the-fire-it-believes)).
- **Villagers**: the eaten one dies through VillagerDead; the villagers at home are not prey (IsReachable).
- **Hand and physics**: placing in the hand, InitialisePhysics, EndPhysics; the original ends the physics of a thing
  with no body immediately (0x5EFDF8).
- `ProcessState`: (SpellWolf's vt+0xB48 is Animal::StartWander 0x417C90 too: after a hunt it wanders, and its Wander runs on)
- `InitialisePhysicsWithoutBody`: 0x5EFE18: straight to Object::InitialisePhysics 0x637480
- Living::InitialisePhysics 0x5EFE10 without a body (a particle system takes the animal): already FLYING (0x5EFE18) -> true, nothing; else StorePreviousState (vt +0x8EC, 0x5EFE2B) unless IN_HAND 0x18, then SetTopState(FLYING) (vt +0x8E8) must take, else false (0x5EFE42..0x5EFE54: refused)
- fn_005EC2A0: SetTopStateToFinal, then SetState(0, flock+0x7C)
- `MobileWallHug::MoveToObjectPos` 0x60ADC0 (the STEP_THROUGH walk at an object; called every turn by
  `Animal::HuntingMoveToPos` 0x418DFE and `Living::MoveToObject` 0x5EC2C4): a gone or unavailable target (+0x60, vt
  +0x2C) returns 38 (0x60ADCC / 0x60ADDB → 0x60AECD); closer than the walker's speed in metres it returns 10 before any
  step (0x60AE0E..0x60AE1F); a walker that is not an animal (vt +0x454) first takes max(own speed, the target's
  `GetSpeedInMetres` × 1.25) (0x60AE41, [0x8CF998]); then the object's Pos is copied to the goal (+0x80) on every call
  (0x60AE6B..0x60AE84), a villager with Flags 0x80 rebuilds its steps (InitStepsXZ 0x60BFA0), and `MoveTo` 0x60AF20 runs.
- MobileWallHug::SetupMobileMoveToObject (0x60ACD0): STEP_THROUGH at it; then Living::SetState (0x5F2A80), raw

### Blobs and mesh of the animals

The animals' ground blobs and their mesh and scale at creation (the rendering pages link here).

#### Blobs

- In `fn_00812170`: if it is not human, with `IsHumanShadowed` (flag 0x4000000, `SetHumanShadowed(1)` in each
  species' Create; 0 while the creature holds it), and > 0.2 and a mesh with `ContainsEBone` → `fn_0081FFF0(obj,
  normal, ebone)`.
- EBone block (836 bytes) after the footprint ones (size at +8), UV2, name and extra metrics: `u32 size; float
  m[16][12]; int32 bone[16]`. The positions of m[0..3] in their bone's space are used: P = object × bone × pos, y =
  ground + 0.2. Pair (0, 1) always, pair (2, 3) if bone[2] ≠ −1 (all the quadrupeds: 4 quads). Birds and bats have no
  EBone.
- **Oddity of the original**: the first quad of each pair receives V = D (it builds D + (P1 − P0)/2 but passes &D); the
  second D + (P0 − P1)/2.

#### Creation: mesh and scale

- `CREATE_ANIMAL` (24, "ANNN": type, herd, village) and `CREATE_NEW_ANIMAL` (25, "ANNNN": + age) → `fn_00419D10`
  (herds and classes in
  [Animals and flocks](../scripts/map-script-reader.md#animals-and-flocks-create_flock-create_new_animal)). Mesh:
  `Object::CallVirtualFunctionsForCreation` 0x636BE0 gives the LH3DObject `GetDetailMesh(2, 1, 0)` (info +0x1FC + 4k:
  high, std, low) and the LOD is always 1: **the std one** (also `GetMesh`). Scale (`InitialiseScale` 0x417B20, then
  `SetScaleForAge` 0x417A40): young ones (age < grownUpAge) ageToScale[age − 1] + FloatRand(0.75·(ageToScale[age + 1]
  − s)), at age 0 the dword before the table (+0x278, altitudeNormal); adults 0.9, then t = (0.05 − FloatRand(0.1)) + 1
  and, if 0.9 < t, a second roll. No initial angle. The script's SET_PROPERTY Age is `Animal::SetAge` 0x4179C0.
- The draws of a making, in order (verified with xref): the age when none is given (GameRand(40) + 5 alone 0x419C34, GameRand(20) + 5 in a
  flock 0x419D47); the Living ctor's GameRand(startAge >> 1) (0x5EBF89, from the Animal ctor 0x416EB0 at 0x416EC0;
  every animal row has start age 20, so GameRand(10); its age is overwritten by `Animal::SetAge`); the scale for the
  age; a predator's reaction 0x1C spread (GameRand(1000) per spiral cell with another Living first, 0x6E3F18). Goat,
  Zebra, Vulture, CitadelDove and CitadelBat return before any ctor (no Living draw).
- Script properties: Speed `SetSpeedInMetres` (MobileWallHug 0x60C080 → `Animal::SetSpeed` 0x417FE0, the clip for the
  new speed), `GetSpeedInMetres` 0x60C070; `Living::IsDead` 0x417270; `SET_FOCUS` (see
  [Script properties and facing](villager-speed-and-scripts.md#script-properties-and-facing)).
- Land1 creates 116 (doves 40, seagulls 22, swallows 14, horses 12, cows 10, pigs 7, tortoises 6, bats 5).
- GameRand(range) + 5 (0x419C34 with 0x28, 0x419D47 with 0x14)

### Animals

Their AI, their states and the clip of each state per species are in [Animals: species and the turn](animals.md);
they use the same playback, distance sync, drawing between turns and sounds as the villagers
([Skeletal animation](animation.md)).

### Animals: blobs and mesh

The animals' EBone blobs (`fn_0081FFF0`) and the mesh / creation scale are under
[Blobs and mesh of the animals](#blobs-and-mesh-of-the-animals).

### Notes from the code

- as a CREATE whose result a script variable keeps (0x6F1BCD, then the VM's reference)
- Object::Collide(collideType) (0x6033B0): off the map collides with everything; type 1 with the water cells

### Sources

- The executable (bw1-decomp's animal files are empty stubs), read with the xref index; the oracle for the birds' way
  back to the land (3000 cases).

## openblack

**Done**: the reactions, the hand, flight and death, the clips, flocks, age, smoke, the scripts' hold and release,
POPULATE_CONTAINER, the blobs, the mesh and scale. The files are listed on [Animals](animals.md#openblack).

- **Reactions**: the distribution, the records, the scoring and the change rule are common to all living beings
  (`ECS/Effects/Reactions`, `components::ReactionRecords`); what is specific to the animals (priorities 28/7/9,
  StartReacting, turns of 28, states 49/6/30/19/20) is their handler in `ECS/AnimalFlee.cpp`. Not ported (they need
  systems that openblack does not have): spells (0, 3), village artefacts (1), creature (6), fire (10). The distances
  use `gutils::GetDistanceInMetres`; the flee points use the villagers' `ECS/Effects/FleeGeometry`.
- **The villagers' food and wood reactions** (`ECS/Systems/Implementations/VillagerResourceReactions.h`):
  `FoodInterestScore`, `TownInterestScore` and `FoodDropOffScore` are the three scores in the original's float order;
  `ReactionFallLimit` the arrival's wait in turns; `IsForesterWorkState` the forester's final states;
  `SetupReactToFood` / `SetupReactToWood` add reaction 19 / 21 with the object; `StandardTurnsBeforeAgain` and
  `TurnsBeforeReactingToWoodAgain` the turns before reacting again; `GotoFoodReaction` / `GotoWoodReaction` the
  walking states (the animals' `GotoFoodReaction` is in `ECS/AnimalAI`).
- **Hand, flight and death**: `Animal::EndPhysics` is in `ECS/Systems/Implementations/VillagerPhysics.cpp`; the rows
  are the original's (`living::BodyRows`: as they are from the hand, OriginalRows for the world path; (inferred, not
  verified in game), trace `OPENBLACK_ANIMAL_TRACE` `landing:` lines). The body-less case is
  `living::EndPhysicsWithoutBody`, which `animal_ai::PutDown` calls (only for openblack's bodiless case,
  `physics::from_hand::PlaceWithoutBody`). The physics and the hand reach the animals through
  `ECS/Systems/Implementations/VillagerPhysics` (the Animal class's physics handlers and
  `living::InterfaceSetInMagicHand`).
- **Clips**: the table is in `ECS/AnimalAnimations.cpp`. openblack still chooses the first clip in the frame after
  (`UpdateAnimalAnimations`); the pure choice is `ecs::living::AnimalFirstClip` (pending wiring, issue #12).
- **Food from an animal**: `AnimalReactToPhysicsImpact` (`VillagerPhysics.cpp`) and `ecs::held_apply` (the Animal
  rows), through `ecs::resource_stores`; the deletion is `ToBeDeleted` (out of its town's list and its flock, out of the
  physics and the cells). The totem's sacrifice is not ported (`HeldApply.cpp` 51), so ours always passes
  DeathReason::None (`AnimalAI.cpp` 1279) and nothing differs until it is.
- **Smoke**: the engine is `ecs::disappear_smoke` (`ECS/DisappearSmoke.*`, the same as the dust and the boat's
  splashes; it is drawn with the boat sprites); the animals only call `disappear_smoke::Create`.
- **Scripts**: `ECS/ScriptHeld.*`: the slot is the entity's `components::ScriptHeld`; the LHVM references are the
  callbacks of `LHVM::Initialise` in `Game.cpp` (`ADD_REFERENCE` / `REMOVE_REFERENCE`), faithful. A released living
  animal goes to INTERACT_DECIDE_WHAT_TO_DO one turn later ⚠️ approximate. The script flocks are in (FLOCK_CREATE,
  FLOCK_ATTACH, FLOCK_DETACH, FLOCK_DISBAND, FLOCK_MEMBER in `src/CHLApi.cpp` over `src/ECS/ScriptContainers.cpp`, tested
  in `test/test_script_flocks.cpp`), and POPULATE_CONTAINER (`script_populate`, `script_containers::PlaceOf` /
  `TakePopulated`; testbed `script.populate_flock`); the release is `script_held` ReleaseScriptThingIntoTheGame,
  `villager_script::ReleaseFromScript`, `animal_ai::ReleaseFromScript`. "Cannot be eaten" is
  `components::CannotBeEaten`. When releasing an animal, "in physics or in the hand" (+0x24 & 0x44) is the physics
  component or the IN_HAND / FLYING states [approximated], and the flag g_game +0x14 & 0x8000 does not exist in
  openblack.
- **API** (`ECS/AnimalAI.h`): `CreateAnimal`, `MoveTo`, `SetState` / `SetStateRaw`, `Destination`,
  `SetFinalDestination`, `Kill` / `DestroyedByEffect`, `Remove`, `SetAlpha`; `AddDeathListener(fn)` → id /
  `RemoveDeathListener(id)`, `SetDeathCallback` (a single listener that replaces its own), `SetSpeciesDying(type, fn)`
  (the spells register it); `ScriptMoveTo(e, xz)`, `SetScriptState(e, s)`; `IsFrighteningToCreature(e)` (raffclar's
  list misses Tiger, Leopard, Wolf and SpellWolf); the eaten villager dies via `ecs::villager::VillagerDead`;
  `villager::IsAtHome` is awaited and the list of shepherds; `PlaceInHand`, `InitialisePhysics`, `EndPhysics`,
  `PutDown`.
- **Blobs**: `L3DFile::GetEBone`, `L3DMesh::GetBlobPoints`, the animal loop in `Renderer::DrawGroundBlobs`.
- **Creation**: `components::Animal`, `AnimalArchetype`; `MakeAnimal` draws `living::ConstructorAgeDraw` first; the
  scale is `animal_ai::InitialScaleForAge` / `ScaleForAge`, shared by the creation, the growth tick and the script's
  `SET_PROPERTY` Age (`animal_ai::SetAge`); openblack used the high mesh before (the std one now). Living::IsDead is
  `animal_ai::IsDead`; SET_FOCUS goes `living::SetFocus` → `animal_ai::SetYAngle`. The animal AI's flock code and
  `ECS/Flocks.h` (the scripts' flocks) share one convention: `members` front is the leader (the original's tail).
  `LeaderOf`, `SetDomainCentre` and `PosWithinDomain` call `ecs::flocks`, and `CalcRandomPos` calls
  `living::CalcRandomPos` with the animal's vt tests; openblack's copy of PosWithinDomain once answered 1 without a
  flock. openblack creates all of Land 1's 116 animals, with the AI and the clips.
- **Birds' way back to the land**: `animals::birds::LandAllTheWay` / `WaySquareCentre` (`src/Animals/BirdRules`),
  tested in `test_bird_rules.cpp` (below).
- Porting decisions: [Port decisions: beings](../project/port-decisions-beings.md).

### Notes on the code

The code that reproduces the facts above, by file, with what the port adds or leaves out.

- `src/ECS/AnimalFlee.cpp`: `FinalStateOf`, `Priority`, `Standard`, `ComingTowards`, `TurnsToReact`, `StartReacting`, `FleeingPosition`, `FleeingFromObjectReaction`, `FleeingAndLookingReaction`, `GotoFoodReaction`, `ArrivesAtFoodReaction`, `AnimalState::Flying`, `AnimalState::FleeingFromPredatorReaction`, `AnimalState::GotoFoodReaction`, `AnimalState::FleeingAndLookingAtObjectReaction`, `AnimalState::ArrivesAtFoodReaction`.
- `src/ECS/AnimalAnimations.cpp`: `GrazerMove`, `MoveClip`, `BirdClip`, `AnimalAnimId`, `SetAnimalStateAnim`, `Slot::Move`.
- `src/ECS/AnimalAI.cpp`: `IsPosValidForMapCellExistance`, `IsPosValidForTurnAngle`, `FinalStateOf`, `CallExitStateFunction`, `FlockSteer`, `InteractDecideWhatToDo`, `CheckNeeds`, `CowReactToAnimalNeeds`, `LookForFlocksInSpiral`, `KeepLeaderWithinDomain`, `DecideWhatToDo`, `StartToEat`, `SeekSleep`, `Dead`, `ProcessState`, `ProcessAnimal`, `InitialisePhysicsWithoutBody`, `ScaleForAge`, `IsDead`, `AnimalState::Flying`, `AnimalState::Landed`, `AnimalState::InHand`, `Hunter::None`, `BeingEaten`, `AnimalState::BeingEaten`, `AgeScale`, `AnimalState::Dead`.
- `src/ECS/AnimalAI.cpp`, `InteractDecideWhatToDo`: the other species' (pending) as DecideWhatToDo
- `src/ECS/AnimalAI.cpp`, `AgeScale`: (openblack, guard) past the 20 floats the last one
- `src/ECS/AnimalAI.h`: `ProcessAnimal`, `SpreadPredatorReaction`, `InitialisePhysicsWithoutBody`, `InitialScaleForAge`.
- `src/ECS/AnimalBirds.cpp`: `FlockAltitude`, `BirdStartWander`, `BirdReactToAnimalNeeds`, `SpecialMoveToPos`, `FollowFlock`.
- `src/ECS/AnimalScript.cpp`: `ReleaseFromScript`, `CreateFor`.
- `src/ECS/AnimalScript.cpp`, `ReleaseFromScript`: 0x41AAB7: vt +0xB88 Animal::InteractDecideWhatToDo 0x417D80, at once (desvios.md P11)
- `src/ECS/AnimalDebugHooks.cpp`: `ScriptHeldTest`.
- `src/ECS/AnimalAIDetail.h`: `Collides`, `InteractDecideWhatToDo`.
- `src/ECS/AnimalAIDetail.h`, `InteractDecideWhatToDo`: Animal::InteractDecideWhatToDo 0x417D80 (vt +0xB88): with a flock (+0xB8) LookForFlocksInSpiral(Pos, info, 2 x domainRadius (+0x25C), merge 1) 0x41A690, then StartWander (vt +0xB48: the species' own)
- `src/ECS/AnimalLairs.cpp`: `BestForestLair`.
- `src/ECS/AnimalLairs.cpp`, `BestForestLair`: it only depends on its own position and never changes the choice, so it is not ported.
- `src/ECS/Flocks.cpp`: `Insert`, `CreateFor`, `SetFlock`, `AddMember`, `AddLeader`, `RemoveLiving`, `SeparateIntoNewFlock`, `Merge`, `RandomMember`, `FindLiving`, `MembersFromHead`.
- `src/ECS/Flocks.cpp`, `AddMember`: 0x52FA55..0x52FA6B: the old flock's +0x30 cleared when it is this living ((not ported) +0x30 is not kept) 0x52FA74: fn_005ECFE0, out of its flock (even this one) with deleteWhenEmpty 1
- `src/ECS/Flocks.h`: `SetFlock`, `AddMember`, `RemoveLiving`, `SeparateIntoNewFlock`, `Merge`, `MembersFromHead`, `RandomMember`, `FindLiving`.
- `src/ECS/Flocks.h`, `FindLiving`: Flock::FindLiving 0x530510: from the head, the first member `match` accepts (entt::null for none)
- `src/ECS/AnimalPredators.cpp`: `NearerThanStored`, `IsPrey`, `CurrentTargetOk`, `SetupMoveToTarget`, `PredatorReactToAnimalNeeds`, `TargetPounce`, `ProcessDownedVillagers`, `BeingEaten`, `HideInLair`, `CheckNeeds`.
- `src/ECS/AnimalPredators.cpp`, `IsVillagerPrey`: it replaces openblack's own test of the states 13..18
- `src/ECS/AnimalPredators.cpp`, `FindPrey`: IsPrey refuses all but villagers and animals (info type 2 / 4, 0x41975A) and its tests before take any object (no Living part)
- `src/ECS/Components/Animal.h`: `flockOrder`.
- `src/ECS/Archetypes/AnimalArchetype.cpp`: `RandomAge`, `CreateAlone`, `AnimalClass::Grazing`.
- openblack differs: `StepTowards` has no arrival test (10, no step) and no 38 return (`src/ECS/AnimalPredators.cpp:423`),
  a fix row.
- openblack differs: an animal's moving clip plays by time, not by the ground (`src/ECS/Animations.cpp:124`), a fix row.

### Test hooks

The animal hooks are listed on [Animals](animals.md#test-hooks).

## Pending

- openblack differs: animals play their moving clip by time, since `byGround` needs `animation.groundSpeed`, set for
  villagers only (`src/ECS/Animations.cpp:124`), a fix row (the original's rule is in
  [The original](#the-original), from `Animal::Draw` 0x51AF00).
- **Clips**: a held one-shot clip's sound events are not replayed each frame as the villagers' are
  (queued fix). An animal in IN_DANCE (state 5) plays the default Move clip, not row 5's dance clip (queued fix).
- **Lairs** (`ECS/AnimalLairs.cpp`): `GUtils::GetDistance` is ported as is, but over openblack's float positions
  truncated to MapCoords. The tiger's water search is not ported (it does not change the result). `flock +0x5C`
  (without a lair) is not ported because the lairs' code only sets it to 0. Distance ties between big forests and trees
  choose the newest object by creation index, like the original's head-insertion lists.
- **Cannot be eaten**: nothing in the game gives the flag yet (`script_held::SetCannotBeEaten` is reached only from
  the animal debug window; the vortex's Out does not set it on what comes out of it).
- **The script finders**: ours takes a slot for CALL, CALL_NEAR, CALL_IN, the towns, CALL_PLAYER_CREATURE, the hit
  records and the creates, but not for SPECIAL_EFFECT_POSITION / OBJECT (`CHLApi.cpp` 2274, 2285), INFLUENCE_OBJECT /
  POSITION (2412, 2417), SPELL_AT_THING / POS (3951, 3956), SPELL_AT_POINT (4295), GET_TOTEM_STATUE (6361) and
  GET_SPELL_ICON_IN_TEMPLE (6625); those get a not-created slot at their first reference (`ScriptHeld.cpp` 261). What it
  changes: the created ones are then not controlled by the script (the first reference sets control only for a created
  slot), and a found thing that no variable takes is never released at the next look-in. The other finders are not
  implemented in ours (NotImplemented, push 0).
- **The predator reaction's turns before reacting again**: openblack's `TurnsBeforeReactingAgain` (`AnimalFlee.cpp`
  267) tests the spread's half-Manhattan distance `d` instead of the animal's own distance, which is shorter or longer
  depending on the angle, so an animal may record the reaction or not where the original does the opposite.
  `flee::LivingTurnsBeforeReactingAgainToPredator` takes the true distance; wiring it is a behaviour commit of the
  animals.
- **Sinking**: openblack's animal branch of `ecs::HasSunk` (`VillagerDrowning.cpp` 131) has neither the availability
  test nor the creature's report (only villagers keep who dropped them: `RememberLastPlayerToInteract`), and goes
  straight to ToBeDeleted.
- **Flock merging**: openblack has no shepherds nor flock player, so it only looks at its own flock's village
  [approximated].
- **Shepherds, the leader priority of FLOCK_ATTACH, the birds' landing** (unreachable in the game: info.sleep 0), the
  exact order of the lists of each map cell and the turn interleaved with the villagers.
- **The random sequence**: the draws go through `game_random` (GameRand over the synchronised seed); the sequence is
  still not that of an original game while other systems drawing from the stream are not called in the original's
  order ([Random numbers](../engine/random.md)) ⚠️ approximate.
- **The three landing poses** of `Villager::EndPhysics` for the animals (pending).
- **The birds' way back to the land**: `Dove::IsPosValidForMapCellExistance` (0x41F840) walks the way back from the
  point to the bird, read whole and pinned by the oracle (3000 cases on a random block map, 3000 equal): each
  MapCoords to whole metres (`__ftol` of ×10/65536), then to the middle of its 16 m square (15 added below zero, `sar
  4`, `shl 4`, + 8); d = the bird's square − the point's, one sign each way (1 when above 0, else −1) and the lengths
  sign × d; from the point's square the axis behind the line (iz × |dx| − ix × |dz| above 0: x; below or 0: z; exactly
  0 or NaN: x when |dx| > |dz|) steps 16 m until both are covered, and each square stepped to must be a cell 0..511
  each way (`__ftol` of × 0.1) whose block exists (`g_index_block` and `g_ptr_blocks`). The point's own square is not
  looked at, and the same square passes at once. Before, ours looked only at the point's cell, and took the editor's
  cell bit 0x40 (which no code of the game reads) for "no block", so birds refused real cells that carry it (236 on
  Ultimate.lnd). Done; kept here until the wiki's audit of it.
- openblack differs: `StepTowards` sets the goal and steps without the "closer than its speed in metres → 10, no step"
  test and without the 38 return of `MoveToObjectPos` 0x60ADC0 (`src/ECS/AnimalPredators.cpp:423`), a fix row.
- **`Flock::info`** (+0x28 / +0x2C): +0x2C is read through `Container::GetPlayer` 0x462A50 (any GetPlayer on a
  flock) and Save (0x46B93D); +0x28 only by Save (0x46B932) among the Flock, Container and animal-flock functions (the
  `[esi+0x28]` reads in MoveInFlock 0x41AA52 / 0x41AA60 are the animal's own info). Still open: readers outside those
  classes, which xref cannot list for a heap offset.
- **`Flock::calm`**: its readers in the original are the prey test and the wolf's; openblack reads it nowhere (see
  [Animals, Pending](animals.md#pending)).
