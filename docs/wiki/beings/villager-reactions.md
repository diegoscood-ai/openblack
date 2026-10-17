# Villagers: reactions

## Summary

Villagers react to what happens near them through the game's reactions: they run from frightening miracles and
watch the nice ones, point at or run from a thing thrown from the hand, flee and hide from predators, and are eaten
when a predator downs them. Each reaction a villager takes up also impresses it: the town's belief in the miracle's
player grows, the player's alignment moves and the town tires of that kind of miracle.

openblack reproduces the miracles' reactions (3, 21 and 37), the thrown thing (9 and state 162), the impression
from miracles and the hunted villager; the flight from predators (28) is ported as pure rules but not wired yet.
Fire is on [Fire](../world/fire.md); the reaction machinery shared by every Living is on
[Villager states](villager-states.md#exits-of-the-reactions-exitreaction-exitreacttoteleport).

## The original

### Hunted villagers

Villagers are prey like the animals (type 2, with meat) if they are outside their home. When downed, their health
stays at 5 % and they go to DOWNED (clip `P_ATTACKED_BY_LION`), then BEING_EATEN 300 turns (`P_DYING`) and they die
(`Villager::BeingEaten` 0x76B380: l = GetLife; SetLife(0); `VillagerDead(ANIMAL 3, its player (the owner of its
town), l, 1)`). They are driven by the animal AI ([Animals](animals.md#predators-lion-tiger-leopard-wolf)). The one
that cannot be eaten (+0x25 & 0x40) gets up (LANDED) instead of dying.

An eaten villager lies as a normal corpse ([Death](villager-hand-and-death.md#death)). A corpse is reachable, so a
predator may pounce on it: fn_005EC480 gives it the downed mark and life 0.05 as to any Living, and its
SetTopState(17) (`Villager::SetTopState`) is refused by DEAD's exit (to check in the game). The corpse counter of
animals and villagers is shared (`Living::Dead` 0x5EC400).

### Villagers fleeing from predators

Read in the v1.42 exe (bw1-decomp `VillagerReaction.cpp` first; every branch below checked against the bytes). The
villagers take the predator reaction (28) through the same spread as the animals.

- **Priority** `Villager::FleeFromPredatorPriority` 0x763850: 0 unless the initiator is an Animal (dynamic_cast);
  farAway = `GetDistanceInMetres` (0x74CD50) > info +0x40 (minDistanceToRunAwayFromObject); 0 at home (+0xE0 & 4),
  in top state 236 GO_AND_HIDE_IN_NEARBY_BUILDING (+0x8C), or with the action's final 236 (+0x8D) and farAway.
  Unless GetFinalState (vt +0xB04) is 237 LOOK_TO_SEE_IF_IT_IS_SAFE with +0x90 >= 20 turns: 0 for a predator in top
  state 35 (asleep) or 51 (in its lair), and 0 when farAway, `GetGameTurnReactionStarted(28)` 0x5F0FB0 > 0 and not
  `IsObjectTravellingTowardsMe` (asked only then). Else priority = ReactionInfos[28].priority (0xD501B0, loaded
  64-bit) x (speed (vt +0x154) `fidiv` the predator info's **whole** WalkingSpeed (+0x110) x 0.5 + 0.5), every step
  rounded to a float, `__ftol` and only **AL** returned: the low byte, so a fast predator wraps (150 x 2.75 = 412 ->
  156). Oracle: 418 cases run in the exe with its callees stubbed (the dynamic cast, the distance, the info, the
  final state, the start turn, the coming-at test, the speed), all equal to openblack's rule; 40 are pinned in the
  test.
- **Turns** `Villager::NumGameTurnsToReactToPredatorFunction` 0x7640A0: 0x7FFFFFFF while the final state is 236 or
  237, else the Living's 0x5F1980 (0 for a non-animal; 0 in top state 7 when the predator is faster than 5.0 m/s
  (0x8AB6E4) and coming at it; else the standard vt +0xAC0, the Living one 0x5F18C0 for villagers and animals).
  `...BeforeReactingAgain...` 0x7640E0: 0 in 237, else the Living's 0x5F1A00: 0 for a non-animal or a predator
  faster than 5 m/s; then it measures **its own** `GetDistanceInMetres` to the predator (0x5F1A45) against
  ReactionInfos[type] +0x40 (0xD4F6F0) and gives 0 when nearer; else the standard vt +0xAC4 with the spread
  distance. Oracle runs of all four agree with openblack's rules.
- **Setup** 0x763990: final state 236 -> only HideTurns (+0x10C) = 0; else `AddReaction(reaction, 231)` (vt +0x990)
  and ReactionObject (+0xBC) = the predator.
- **FLEEING_FROM_PREDATOR_REACTION (231)** 0x763B40: ReactionObject gone or not available ->
  StopReactingAndSetState (vt +0x99C); not an Animal -> returns 0; distance > info +0x44
  (maxDistanceToRunAwayFromObject) -> stop; final 237 and `GetAbodeToHideInAtPos(HidePos +0x110)` ->
  SetupMoveToOnFootpath(abode, its arrive pos, 236); speed > 5 and coming at it, or else `GameRand(6)` == 0
  (0x763C5D, drawn only when the first test fails) -> `SetupGoAndHideInNearbyBuilding`; failing that
  `FleeFromObjectIfComingTowardsMe(obj, 7, 7)` (0x763C82: the states are LOOKING_AT_OBJECT_REACTION, not the animals'
  30).
- **Hiding place** `SetupGoAndHideInNearbyBuilding` 0x763CB0: no town -> 0; the town's abode list (+0x754, next
  +0x9C) in order, distance to `GetArrivePos` (vt +0x104) by `GetDistanceInMetres` 0x74CD70, taken when
  `CanBeHiddenIn` (vt +0x924; Abode's is IsFunctional) and **strictly** nearer than the best (100.0 at the start);
  then HidePos = its position, HideTurns = 0, SetupMoveToOnFootpath(.., 236).
- **GO_AND_HIDE_IN_NEARBY_BUILDING (236)** 0x763D90: an unavailable ReactionObject is cleared; no abode at HidePos
  or one that cannot be hidden in -> stop; not there (`AreWeThere(arrive, 0)` vt +0x85C) -> walk on; there with the
  predator within info +0x40 -> stays without counting; else ++HideTurns > `GameRand(150)` + 150 (0x763E6E,
  unsigned) -> HideTurns = 0, `FindPosOutsideAbode`, SetupMoveToWithHug(.., 237).
- **LOOK_TO_SEE_IF_IT_IS_SAFE (237)** 0x763F80: the predator within info +0x2C (maxReactionDistance), an animal (vt
  +0x454) and not in 35 / 51 -> back into the abode at HidePos (HideTurns = 0) or, with none,
  FleeFromObjectIfComingTowardsMe(obj, 7, 7); else ++HideTurns > `GameRand(25)` + 25 (0x764072) -> stop.
- Who can take it: `Villager::IsAvailableForReaction`. In info.dat every tribe villager info has
  isFleeingFromPredator 1; reaction 28 has priority 160, maxReactionDistance 25, minDistanceToRunAwayFromObject 30,
  maxDistanceToRunAwayFromObject 75 (so the priority wraps once the predator runs faster than 2.2 times its walking
  speed).

### A shield's fall, reaction 36

- **The row.** `Villager::SetupReactToMagicShieldDestroyed` (bw1-decomp `VillagerReaction.cpp`) is
  `SetupPanicReaction(reaction, thing->Pos)`; its priority is always the row's (`ReactToMagicShieldDestroyedPriority`),
  and its turns are the standard ones (`Reaction.cpp`'s type table).
- **The set-up** `SetupPanicReaction` 0x76B0D0 (xref callees): first `SetupGoAndHideInNearbyBuilding` (above); with
  nowhere to hide, the bearing from the point (`Get3DAngleFromXZ`) and two `GameFloatRand`, the row's run distances
  (`GetInfo` three times), `GetPosFromAngle`, and `AddMoveToPosReaction`. 🧪 From raffclar's reading in Ghidra: the turn
  is ± pi/8 and the distance min + GameFloatRand(max − min); the final state is PANIC_REACTION (240), which turns to
  look back at the point by 0x100 steps and then plays its clip and decides afresh. The set-up gives the villager no
  ReactionObject of its own (his vanilla bug 62): the next ProcessReaction stops it, unless it was watching another
  shield.
- **The hide holds no reaction.** Rows 236 and 237 have ReactionValidate as their validate (`GStates.cpp:248-249`): a
  villager hiding from a fall with no ReactionObject (one that was not amazed by another, standing shield) is popped at
  its first turn there, into the state it stored (none: INVALID_STATE, his vanilla bug 63).

### Running from and watching miracles, reactions 3, 21 and 37

- **The reaction type table** (initialiser 0x6E0E80): 0xC09CC0 + 0xC0 × t, one row for every type and every Living
  class. +0x04 the same-type switch ("may switch to another reaction of the same type": 1 only for 10, 28 and 35),
  then member-function pointers of 16 bytes: +0x10 the setup, +0x20 the priority, +0x30 the turns to react, +0x40
  the turns before reacting again (each a thunk `mov eax, [ecx]; jmp [eax + vt]`, so every class answers with its own
  virtual), +0x80 the type's name. Types 34, 39 and 40 have no setup.
  - 3 FLEE_FROM_SPELL: 0x6E5090 (vt +0x9A4 SetupFleeFromObject), 0x6E53C0 (vt +0xA38 FleeFromSpellPriority),
    0x6E4FE0 (vt +0xAC0 `StandardNumGameTurnsToReactFunction` 0x5F18C0), 0x6E5080 (vt +0xAC4
    `StandardNumGameTurnsBeforeReactingAgainFunction` 0x5F1920). Type 0 FLEE_FROM_OBJECT shares its setup.
  - 21 LOOK_AT_NICE_SPELL and 37 REACT_TO_IMPRESSIVE_SPELL: the same row, 0x6E5230 (vt +0x9B0
    SetupLookAtNiceSpell) and 0x6E4F90 (vt +0xA40 LookAtNiceSpellPriority), the standard turns.
  - 4 LOOK_AT_SPELL: 0x6E51D0 (vt +0x9AC SetupLookAtSpell) and 0x6E4F40 (vt +0xA3C `LookAtSpellPriority` 0x5F1690:
    row 4's priority). No effect of info.dat makes it.
- **The setups** (Villager vtable 0x8F7960): +0x9A4 is `Living::SetupFleeFromObject` 0x5F1360: AddReaction(r, 6)
  (vt +0x990, `Villager::AddReaction` 0x763440), then +0xBC = the object. +0x9B0 `Villager::SetupLookAtNiceSpell`
  0x763AE0 calls vt +0x9AC `Villager::SetupLookAtSpell` 0x763AC0 = `Living::SetupLookAtObject` 0x5F13B0:
  AddReaction(r, 7), then +0xBC.
- **The priorities.** `Living::FleeFromSpellPriority` 0x5F1630 (the Villager's): `GUtils::FastDistance` 0x74CE10
  (max + min / 2 of |dx|, |dz| in whole units) from the villager (+0x14) to the initiator (reaction +0x14, its
  +0x14); below 600000 (0x927C0, about 91.5 m) `(600000 − d) × 100 / 600000` (signed, through the multiply by
  0x6FD91D85 and `sar 0x12`) plus row 3's priority [0xD4F7EC]; else that priority; a byte (0x5F1647..0x5F167F). The
  test "the caster never runs" is the creature's override (`Creature::FleeFromSpellPriority` 0x4F2AB0: the spell's
  +0xA0 == this → 0, else the Living's), not the villager's. `Living::LookAtNiceSpellPriority` 0x5F16A0: the
  initiator cast to a Spell (`__RTDynamicCast`), its creator +0xA0 the villager → 0; else row 21's priority
  [0xD4FEF4], for 37 too.
- **The states** (the state table's wrappers 0x5AFFE0 / 0x5B0060 / 0x5B00E0 jump to vt +0x888 / +0x88C / +0x890;
  exit 4 ExitReaction 0x7527A0, validate 4 0x756A00 for all three):
  - 6, `Villager::FleeingFromObjectReaction` 0x763B00: +0xBC none or not available (vt +0x2C) →
    `Living::StopReactingAndSetState` 0x5F11C0 (vt +0x99C), 1. Then its `GetPlayer` (the result unused) and the
    Living's 0x5F1D10 (named `PuzzleHorse::FleeingFromObjectReaction` in the symbols): `GetReaction` 0x5ECA60 none →
    stop, 1; d = 0x74CD50 (named `ReactionInfo::GetInfo` in the symbols: ConvertWholeDistanceToMeters of the
    hypotenuse of dx, dz, the heights ignored) above the reaction's +0x44 maxDistanceToRunAwayFromObject → stop, 1;
    else `FleeFromObjectIfComingTowardsMe(o, 30, 30)` 0x5F1D90: d above +0x40 minDistanceToRunAwayFromObject and not
    coming (fn_005F1E60) → SetTopState(30) (vt +0x8E8) and `LookAtObject(o, 1)` 0x5EC520 (LookAtPos 0x5EC550 of its
    position, mode 1: at most 0x80 2048ths a turn), 1; else vt +0xAFC `GetFleeingPositionFromMovingObject(o, 10.0)`
    (0x41200000) and, in bounds (`MapCoords::InBounds` 0x6042C0: the high words, unsigned, below the game's map size
    g_game +0x59C8 / +0x59C4, no land asked), `SetupMoveToWithHug(p, 30)` 0x5F2890, 1.
  - 7, `Villager::LookingAtObjectReaction` 0x7642C0: +0xBC none, no reaction, or the object not available → stop, 1;
    else the Living's 0x5F23A0: d above +0x44 → stop, 1; else LookAtObject(o, 1), 1. 30,
    `Villager::FleeingAndLookingAtObjectReaction` 0x764310, jumps to vt +0x88C: the same as 7.
- **The flee point.** `Living::GetFleeingPositionFromMovingObject` 0x5F2160: the object's `GetMovementDirection` (vt
  +0x168; a Spell's 0x721340 copies its +0x2C, which starts at (1, 0, 0): both Spell constructors 0x71FA30 / 0x71FB40
  call the Spell's SetToZero fn_0071FCF0 (0x71FAE4 / 0x71FBF5), which writes +0x2C = 1, +0x30 = +0x34 = 0, so a
  miracle that applied no event is not still); its 3D length not above 0 → `GetFleeingPositionFromStationaryObject`
  0x5F2010 (vt +0xB00). Else across = (−dir.z, ?, dir.x), made one long (its y is the stack slot [esp+0x10], which
  the function never writes); `GameFloatRand(8)` (LivingReaction.cpp line 0x2F1) + across.x × 10 − 4 (0x8AB418),
  then `GameFloatRand(8)` (line 0x2F2) + across.z × 10 − 4: both drawn before the side test; side = across.z × dz +
  across.x × dx (villager − object), below 0 (or NaN) → villager − f, else villager + f, each axis `ftol(m × 65536 /
  10)`. 0x5F2010: (villager − object) in x and z (y forced to 0), both 0 → it stays; else × distance / sqrt(z² + x²).
  The intermediate values stay at the FPU's precision, and the flee point is truncated to map units before the walk.
- **Coming towards** fn_005F1E60: (villager − object) in three dimensions, each height `GetAltitude` 0x803090 + its
  altitude, made one long when not zero; the movement made one long; the dot product summed z, y, x; at least 0.8
  (0x8C4A04, `test ah, 1`) is coming. The heights count: a miracle high above, flying level, is not coming.
- **info.dat** (ReactionInfo): 3: priority 150, 80 turns to react, 0 before again, maxReactionDistance 50,
  howImportantIsDistance 0.1, run 50 / 100. 21: 120, 80, 15, 60, 0.25, 50 / 100, finishes in the hand. 37: 120, 50,
  100, 60, 0.2, 50 / 100, finishes in the hand. So a villager within 50 m of a frightening miracle always starts
  running; it watches from beyond 50 m while the miracle is not coming at it, and gives up beyond 100 m. It stops
  after the standard turns (80 × (1 + 0.5 × (R − d) / R × 0.1), R = 60: 80 to 84 turns), when the miracle's reaction
  is shut down, or when the miracle is no longer available.
- **Who makes them** (GMagicEffectInfo reactionType, on cast / on event): 3: FIRE and its power-ups (both),
  LIGHTNING_BOLT and BEAM_EXPLOSION and their power-ups (event), STORM and its power-ups (both). 21: HEAL, FOOD,
  WOOD, WATER (event), NATURE (both), SHIELD and PHYSICAL_SHIELD (cast), CREATURE_SPELL_STRONG and
  CREATURE_SPELL_COMPASSION (both). 37: FLYING_FLOCK (both). TELEPORT, GROUND_FLOCK and the other creature spells
  make none.
- **Random draws:** only the two `GameFloatRand(8)` on the game stream, x then z, when the villager runs from a
  moving miracle. None for a still one, for watching or for the priorities. (The state changes may draw as before:
  the pause roll, the wounded speeds.)
- The animals take the same Living functions for 3 ([Reactions](animal-life.md#reactions)).

### A thing thrown from the hand, reaction 9

- **Who makes it.** `Object::InitialisePhysicsFromHand`, a throw that does not land (0x6373DB..0x637421): a villager
  drops what it carries, then `CreateReaction(obj, 9, the thrower's player)` (0x63740F), then
  `CheckAllCreaturesForCatching`. It is the only `push 9` before a `CreateReaction`: knocks, vortices and script
  throws make none.
- **The type table's row** (0xC0A380 = 0xC09CC0 + 0xC0 × 9, written at 0x6E1830..0x6E188C): +0x04 0 (no switch to
  another reaction of the type); setup 0x6E5110 → vt +0x9C8 `Villager::SetupReactToFlyingObject` 0x7639D0; priority
  0x6E53A0 → vt +0xA58 `Villager::ReactToFlyingObjectPriority` 0x765260; the standard turns to react (0x6E4FE0, vt
  +0xAC0) and before reacting again (0x6E5080, vt +0xAC4).
- **info.dat** (ReactionInfo 9): priority 142, 80 turns to react, 0 before again, maxReactionDistance 25,
  howImportantIsDistance 1, finishes in the hand, run 50 / 100, whetherItImpresses 0. The spread visits the cells
  whose spiral points are within 25 m of the thing, and scores half the sum of the two distances along the map
  against the same 25.
- **The priority** 0x765260:
  1. dancing (vt +0x978 `Living::IsDancing` 0x5ECC10: +0xD8 set) and the thing not coming at it (fn_005F1E60 with
     the reaction's initiator) → 0;
  2. a current reaction (+0x94) of type 13 REACT_TO_MAGIC_SHIELD (its +0x24): +0xBC none → 0; d =
     `GetDistanceInMetres(Reaction::GetPos(new), Reaction::GetPos(current))` (0x7652DF); the shield's radius (vt
     +0x60: `SpellShield::GetRadius` 0x72B450 → `Get2DRadius` 0x72B440 → `GetSpellMagnitude`), `fcomp; test ah,
     0x41; je`: only a radius strictly above d goes on, else 0;
  3. `Living::ReactToFlyingObjectPriority` 0x5F1800: TOP (+0x8C) 10 FLYING or 11 LANDED → 0, else row 9's priority
     byte [0xD4FA44].
- **The setup** 0x7639D0: po = `PhysicsObject::SearchForPhysicsObject(obj)` 0x646950, none → nothing; d =
  `GetDistanceInMetres(villager +0x14, obj +0x14)` 0x74CD70 (in the plane); speed = √((vz² + vy²) + vx²) of the
  PhysicsObject's +0x104 / +0x108 / +0x10C (the body's velocity, PhysOb +0xDC); `fadd st0, st0; fcompp; test ah,
  0x41; jne` (0x763A4C..0x763A59): **d < 2 × speed → AddReaction(r, 6)** (vt +0x990), **else (at it, beyond it,
  unordered) AddReaction(r, 162)** (0xA2); then +0xBC = obj (0x763A6C / 0x763A8E).
- **162 POINT_AT_FLYING_OBJECT_REACTION**, `Villager::PointAtFlyingObjectReaction` 0x765450 (the state table's slot
  0xD0ECB8): po of +0xBC none → 1; d to the reaction's initiator (+0x94 → +0x14, its +0x14) against 2 × speed as in
  the setup: nearer → `SetTopState(6)` (vt +0x8E8, 0x7654D7), 1; else `LookAtObject(+0xBC, 1)` 0x5EC520, and once
  `!Object::IsActuallyInTheAir` → `StopReactingAndSetState` (vt +0x99C, 0x765523); 1. Its exit and validate are the
  shared `ExitReaction` and `ReactionValidate`; it always reacts to the town emergency.
- **In the air**, `Object::IsActuallyInTheAir` 0x639410: IN_PHYSICS (+0x24 & 0x40) and a physics object, else no;
  the speed (the same sum) **> 3** (0x8C2C50) → yes; else (centre y +0xCC − `GetAltitude` 0x803090 at (ftol(x ×
  65536 × 0.1), ftol(z × 65536 × 0.1)) + PhysicsObject +0x178, the body's radius) **> 0.2** (0x8AB244) → yes; else
  no.
- **162's clip**, `Villager::PointAtFlyingObjectAnimation` 0x423FD0 (the state table's animation slot 0xD0ED18):
  `IsWoman` 0x752620 or `IsChild` (vt +0xAF8, +0xE0 bit 3) → `GameRand(3)` (Animations.cpp line 0x2D3), 0 → 355
  SCARED_STIFF; then (anyone left) `GameRand(3)` (line 0x2D8): 0 → 310 LOOKING_FOR_SOMETHING, 1 → 385 STAND, 2 → 395
  TALKING_AND_POINTING.
- **12's and 161's clip**, `Villager::LookAtFlyingObjectAnimation` 0x423AD0: hidden (−4) when the previous state's
  clip is; else 311 LOOK_AT_HAND while the reaction object (+0xBC) is more than 3.0 above the land under it, 385
  STAND otherwise or with none. No draw. Details in [Which clip plays](animation.md#which-clip-plays).
- **When the clip is chosen.** Only `Living::GetAnimId` (vt +0x900) calls it, from `Living::SetStateAnim` 0x5ECB10 and
  `Living::SetAnim(n)` 0x5ECB80. `SetStateAnim` runs as a state is entered (`Living::SetTopState` 0x5F2940, with no
  out-of clip) and once an into or out-of clip has played (`Villager::FinishedIntoOutOfAnimation` 0x7500EF, called by
  `Villager::ProcessState` 0x750037 only while flag 0x800 is set). `PointAtFlyingObjectReaction` calls neither,
  unlike `WatchFlyingObjectReaction` 0x7653F0, which calls `SetAnim(1)` every turn. So the clip is drawn once as the
  villager enters 162, and again only after a transition clip: never because the 162 clip itself ended.
- **The first clip** is chosen once more, as the villager is made: `Villager::CallVirtualFunctionsForCreation`
  0x74FC70 → `Living::CallVirtualFunctionsForCreation` 0x5EC9B0 → `SetAnim(GetAnimId, 1)`, in the state the
  constructor set (85 CREATED: 385 P_STAND; 16 DROWNING: 252 P_DROWNING), from the start, with no draw
  ([openblack's loop](../engine/loop.md#openblacks-loop)). `GetAnimId` is `Villager::GetAnimId` 0x750110 (vt
  +0x900): a state of 0 or ≥ 255 gives 385 (0x750126); otherwise `SetStateCarriedObject` 0x750133 runs first, then
  the row's animation function (`StateTable` +0x60) or, with none, the table clip. `Villager::Create` 0x74FBE0 calls
  vt +0x658 right after the constructor (0x74FC56), before it returns to its caller; the villager's own part then
  only sets the skeleton mesh (0x74FCB7..). So a `SetScriptState` in the creating turn finds a clip and sets its
  state's.
- **Loading a saved game** sets the first clip too: `Villager::ResolveLoad` 0x754870 → `Living::ResolveLoad`
  0x5EED20 → `Object::ResolveLoad` 0x63A020, whose first call is vt +0x658 (0x63A02E), in the loaded state.
- **6 for a thrown thing** is the miracles' `Villager::FleeingFromObjectReaction` 0x763B00 (above). The way the thing
  goes is its `GetMovementDirection` (`GameThingWithPos` 0x56FF20, `MobileWallHug` 0x60C040): IN_PHYSICS → vt +0x16C
  `Object::GetPhysicsMovementDirection` 0x6394E0, the PhysicsObject's +0x104.. (0 without one); else 0 (a Living, its
  step +0x64 / +0x6C). So a villager within 50 m of a thing still flying runs across its way, with the two
  `GameFloatRand(8)` of `GetFleeingPositionFromMovingObject` (0x5F2298 / 0x5F22C0) for each flee point.
- **Random draws**, on every throw that does not land, for each villager it reaches that takes it: pointing, one
  `GameRand(3)` for a man and one or two for a woman or a child as it enters 162 (and again after a transition clip);
  running, two `GameFloatRand(8)` each time state 6 picks a flee point. The priority, the setup and 162 itself draw
  nothing; SetTopState's pause roll is as for any state.
- **The Living's flag +0xB4 & 0x400**: tested by `Villager::EndPhysics` 0x5F0D11 (LANDED goes back to the previous
  state) and cleared by `Villager::TestSpecial` (0x753364 / 0x753376); no instruction of the exe sets it. Every store
  to the word +0xB4 masks other bits (poisoned 0x2 at 0x416FBC; on a footpath 0x8 at 0x5EDC8B / 0x5EDFEA; the
  landType 0x30 at 0x5F0BA1 / 0x5F0CFD; skeleton 0x40 at 0x41701D / 0x756315) or zeroes it (`Living::SetToZero`
  0x5EC05D), and no `or` with 0x400 or with 4 on the byte +0xB5 exists. Perhaps only a saved game's read of the
  whole object sets it.

### Belief from the miracles villagers react to

- **When.** `Villager::AddReaction` 0x763440 calls `Villager::UpdateHowImpressed(reaction, 1)` 0x7634C0 first, then
  `Living::AddReaction`. So a villager is impressed once each time it takes up a reaction (a first take or a
  switch), at the moment of the spread that gives it the reaction: the cast or the event that creates the miracle's
  reaction (`CreateReaction` spreads at once), or the reaction list's turn (`Reaction::ProcessReactions`) that
  spreads it again. The belief goes to the pending belief and reaches the town's belief at the town's next fold; the
  alignment goes to the pending change and reaches the alignment at the player's next process.
- **UpdateHowImpressed 0x7634C0**: the reaction's player (`Reaction::GetPlayer` 0x55C7B0, +0x38) none → nothing; the
  villager's town (vt +0x48) none → nothing. Else:
  1. v = the initiator's (reaction +0x14) `GetImpressiveValue(villager, reaction)` (vt +0x1B0, 0x7634FA), for any
     initiator: ReactionInfo +0x24 whetherItImpresses is never read, and `Villager::AddReaction` 0x76344D calls it on
     every reaction with no test. A thrown object's value is `Object::GetImpressiveValue` 0x639860
     (DefaultReactionMultiplier (row +0x48) × vt +0x66C × DistanceChangeToBelief(d, row +0x2C) × vt +0x128 × boredom ×
     the artifact modifier), so whetherItImpresses 0 does not stop a thrown thing from impressing.
  2. share = (GTownInfo +0x130 `populationForUnmodifiedBelief` + 0.001) / ((the town's adults +0x618 + children
     +0x61C, unsigned, as a 64-bit integer) + 0.001), both 0.001 at 0x8AA3B0, rounded to a float only once
     (0x76350D..0x763541).
  3. `GBelief::AddToBelief(player, share × v, the villager, draw, ReactionInfo +0x5C alignmentForSFX)` 0x437EB0 on
     the town's belief (+0x798).
  4. The reaction type table's +0x70 member function (0xC09CC0 + 0xC0 × type; the CRT initialiser 0x6E0E80 puts
     0x6E4DC0 in every row): the reaction's player none or no villager → nothing; a = ReactionInfo +0x58
     `alignmentModifier`; with a town and ReactionInfo +0x54 `correspondingTownDesireForAlignment` ≠ −1, a =
     `Town::GetRawDesire(d)` 0x73E420 (raw + boost + boostA) × a; then `GAlignment` 0x414320 on the player's
     alignment (GPlayer +0x60): pending (+0x0C) += `fn_00414660(a)` (the weighing by the current alignment), and the
     alignment history 0x414EE0 (not kept, nothing reads it).
  5. `GBelief::AddToBoredomMultiplier(reaction, share × GetUpdateOfBoredomValue)` 0x438790: boredom[type] (+0xE8 +
     4t) + it, below 0 → 0. `GetUpdateOfBoredomValue` (vt +0x1B4) is the same for every class: `GBeliefInfo` +0x20
     `defaultBoredomOfMe` (0xC58660).
- **Spell::GetImpressiveValue 0x721630**: m = `Reaction::GetDefaultReactionMultiplier(villager)` 0x6E48A0 (the type
  table's +0x50 function when the row has one, only 7 and 12: 0x6E48E0, the town's desire + boost + boostA of
  ReactionInfo +0x50 `correspondingTownDesire`, 1 with no town or −1; else ReactionInfo +0x48
  `defaultReactionImpressiveMultiplier`); d = `GetDistanceInMetres(villager, spell)` 0x74CD70 (in the plane);
  `DistanceChangeToBelief(d, ReactionInfo +0x2C maxReactionDistance)` 0x438770 = `SigmoidThreshold(−0.9, −d /
  max)`; `GetPower` (vt +0x128, `GameThingWithPos::GetPower` 0x56FE60 = 1 for every spell class); the boredom
  `GameThingWithPos::GetBoredomMultiplier(reaction)` 0x56FE70 of the villager (its town's boredom[type], 1 without a
  town). The value is ((boredom × power) × distance's share) × ((m × GMagicEffectInfo +0xC4 `impressiveValue`) ×
  GLandBalance value 2, impressiveness, 0xD1A288). Every spell class but the shield takes this one.
- **The shield's overrides** (`SpellShield::GetImpressiveValue` 0x72BA80): for a villager (vt +0x2C8) with a town
  whose last aggressor (Town +0xEAC) is the shield's player (vt +0x1C) and a reaction 13 or 35 → 0
  (0x72BAC5..0x72BAD8); else the Spell's, × 4 (0x8AB418) for 36 (0x72BAED).
- **What is not a miracle**: `Object::GetImpressiveValue` 0x639860 (GLandBalance 2 × the object's info
  `ImpressiveValue` × the artifact modifier 0x639900, with the same distance, power and boredom; the villagers, the
  food piles and the trees take it), `Creature` 0x47B200, `Scaffold` 0x6EA770, `MissionaryControl` 0x756920,
  `PhysicalShield` 0x72D7F0 (the Object's, 0 for 13 as the shield's). The creature's side is
  `Creature::UpdateHowImpressed` 0x4F2780 (see [the creature pages](../creature/overview.md)).
- **info.dat** (ReactionInfo): 3: multiplier 1, alignment −0.01, alignmentForSFX 2 (evil); 21: 1, +0.01, 1 (good);
  37: 1, 0, 1; 13: 1, +0.05 by the protection desire (3), 1. maxReactionDistance 50 / 60 / 60 / 35.
- **Random draws:** none in the impression itself.
- **The belief sound** (`GBelief::AddToBelief` 0x437EB0 → `BeliefSFX` 0x437F40 → fn_0071BF70): with the villager as
  the thing, the people's awe at the villager, heard from the local interface (see
  [the audio engine](../audio/engine.md)): only when the player's belief in the town is below the strongest one; the
  guidance's time check draws twice on the local stream every time it is asked, then the sample's own draws when it
  is time; GOOD_AWE / EVIL_AWE by the reaction's alignmentForSFX (a coin for 0), 3D at the villager, up to 200 m.

### Sources

- bw1-decomp: `src/Black/VillagerReaction.cpp`, `LivingReaction.cpp`, `Reaction.cpp`, `Animations.cpp`; the rest
  read in the exe with the xref index; the predator rules checked with the oracle.

## openblack

**Done**: reactions 3, 21, 37 and 9, state 162, the impression from miracles, the hunted villager. **Partial**: the
flight from predators (28) is pure rules, not wired; the impression of what is not a miracle.

- **Hunted villagers**: `ecs::villager::VillagerDead` kills the eaten villager at the end of the turn; the downed
  villager is driven by the animal AI (`components::DownedVillager`). The one that cannot be eaten is put in LANDED
  when the 300 turns end, without waiting for the clip ⚠️ approximate. The corpse counter is the shared
  `living::DeadTick`.
- **Predators**: the rules are ported as pure functions in `ECS/Effects/PredatorFlee`
  (`flee::VillagerFleeFromPredatorPriority`, `flee::LivingTurns*` / `flee::VillagerTurns*`;
  `test/test_predator_flee.cpp`), not wired: `VillagerReactions.cpp` 608: `SlotTypeOf` has no row for 28, so the
  villagers' handler drops it. `villager::IsAvailableForReaction` is ported.
- **Miracles**: the rows `k_FleeFromSpell` / `k_LookAtNiceSpell` of `villager_reactions::SlotTypeOf`, the priorities
  `FleeFromSpellPriority` / `LookAtNiceSpellPriority` and the states `Fleeing` (6) and `Watching` (7 and 30) in
  `ECS/Systems/Implementations/VillagerReactions.{h,cpp}`; the pure flee point, coming-towards test and urgency in
  `ECS/Effects/FleeGeometry.{h,cpp}`; the rows 6, 7 and 30 of `k_VillagerStateTable` (`LivingActionSystem.cpp`).
  `map_coords::InBounds` uses its 512 cells. A spell's movement is `components::Spell::movementDirection`. Tests:
  `test/test_villager_reactions.cpp` (`VillagerFleeGeometry.*`, the `Flee*` and `Watch*` tests).
- **A thrown thing**: `physics::from_hand::InitialisePhysicsFromHand` → `animal_ai::SpreadFlyingObjectReaction`; the
  row `k_ReactToFlyingObject` of `villager_reactions::SlotTypeOf`, its setup `SetupReactToFlyingObject`, the priority
  `ReactToFlyingObjectPriority` and the state `PointAtFlyingObject` (162), with the shared `Fleeing` (6); the pure
  rules `RespondToFlyingObject`, `FlyingSpeed`, `ShelterSeesFlyer` and `IsActuallyInTheAir` in
  `Physics/LivingRules.{h,cpp}`; the clips `villager_animation::PointAtFlyingObjectClip` and
  `villager_animation::LookAtFlyingObjectClip` (`VillagerAnimation.cpp`), chosen by `VillagerApplyStateClips` and
  `VillagerWaitsForTransition` at the original's two points. openblack measures the distances with
  `town_queries::GetDistanceInMetres` on the MapCoords, takes IN_PHYSICS as `DynamicsSystem::IsFlying` (flying, not
  a resting proxy), the velocity, centre and radius from the body, and the land from `GetHeightAt` at the map point
  the centre truncates to. Tests: `test/test_villager_reactions.cpp` (`VillagerFlyingObjectRules.*` and the
  flying-thing tests of `VillagerReactionsTest`). Testbed: `physics.villagers_watch_throw`.
- **The first clip** (since issue #12): `VillagerArchetype::Create` calls `villager_animate::SetFirstClip` right
  after `InsertMapObject` (the clip component, `StateClip` with the carried object first,
  `living::VillagerFirstClip`, then `SetAnim` from the start), so `VillagerIsReadyForNewAnimation` sees the first
  clip, as in the original. Tests: `test/test_villager_core.cpp` (`AVillagerIsMadeWithItsBornStatesClip`,
  `AScriptStateInTheCreatingTurnPlaysItsClip`). The per-frame first clip of `UpdateVillagerAnimations` is gone.
- **The impression**: `villager_reactions::UpdateHowImpressed` (`VillagerReactions.cpp`), called by the slot's
  `AddReaction` and by `villager_shield::SetupReactToMagicShield`; the pure rules in `Magic/Impressiveness.{h,cpp}`
  (`ImpressiveValue`, `ReactionMultiplier`, `ShieldImpressiveValue`, `TownShare`, `ImpressionAlignment`); the
  belief through `town_stores::AddToBelief` and `town_belief::AddToBoredomMultiplier`; the alignment through
  `effects::alignment::UpdateForImpression`. openblack differs: it scores only a Spell initiator and returns for
  anything else (`src/ECS/Systems/Implementations/VillagerReactions.cpp:876`), a fix row. openblack differs: the
  predators write a villager's life straight instead of through `Villager::SetLife` (`src/ECS/AnimalPredators.cpp:646`,
  `:719`), a fix row. openblack's shields make 13, 35 and 36 with the spell as the initiator
  (`magic::spell_shield`), so both shield miracles take the override. The town's last aggressor is
  `components::Town::aggression.lastAggressor`; "never attacked" is its turn 0. `DistanceChangeToBelief` is in
  `Magic/Impressiveness`. Tests: `test/test_magic_foundation.cpp`.
- `ECS/Effects/EffectValues.cpp` and `ECS/Fire/FireEffect.cpp` use `gutils::GetDistanceInMetres(vec3, vec3)`, which
  truncates the positions to 16.16 each time. As long as openblack stores the positions in float, that can lose one
  unit (0.15 mm) relative to a stored MapCoords.
- The pure `SameTypeSwitch` (`Reactions.h`) reads the type table's +0x04; openblack's port once registered only 7 and
  12, the original has one table for every Living class.
- **A shield's fall** (raffclar's lane, unverified against v1.42): the row `k_ReactToMagicShieldDestroyed` of
  `villager_reactions::SlotTypeOf` with `ReactToMagicShieldDestroyedPriority`; the set-up, the hide, the look about and
  the panic in `ECS/Systems/Implementations/VillagerCover.{h,cpp}` (rows 236, 237 and 240 of `k_VillagerStateTable`,
  236 and 237 keeping the original's ReactionValidate), his pure rules in `ECS/VillagerHidingRules` and the states in
  `components::VillagerHiding` / `VillagerPanic`. The shield it was amazed by is `villager_shield::ReactionObject`.
  His tree has no validate on 236/237, so his villagers hide 15 to 30 s whatever they watched: a candidate for the "fix
  vanilla bugs" switch. Tests: `test/test_villager_hiding.cpp`. Testbed: `miracles.shield_fall_hide`,
  `miracles.shield_fall_panic`; debug: Magic > Running > Break on a shield.
- Porting decisions: [Port decisions: beings](../project/port-decisions-beings.md).

### Test hooks

None of its own; the throw is shown by the testbed scenario `physics.villagers_watch_throw`.

## Pending

- **The flight from predators**: the rules are ported but not wired (no row 28 in `SlotTypeOf`); the wiring order is
  in the plan for issue #84.
- **The y of the vector across the way** (0x5F2160 reads [esp+0x10], which the function never writes): the original
  reads an unwritten stack slot; openblack uses 0.
- **Precision**: the original keeps the intermediate values at the FPU's precision; openblack rounds each step to a
  float (the flee point, the share's sums, the products of `ImpressiveValue`). The flee point goes to the walk in
  metres; the original truncates it to map units first.
- **The animals' reaction 3**: they take the same Living functions, but they are not ported for them. Their own copy
  of the flee point (`ECS/AnimalFlee.cpp`) asks a coming-towards test in the plane and scales a still run after
  normalising: to move to `effects::flee` once measured (queued fix).
- **The clips of 6, 7 and 30** are the animation table's (`villager_animation::ClipChoiceOf`); not checked against a
  run.
- **A dancing villager** notices a thrown thing only when it comes at it: the scripts' dancers dance, but this test
  (`VillagerReactions.cpp`) is not connected to the dance yet (queued fix).
- openblack differs: a thrown thing impresses nothing, where the original scores every reaction by its initiator's
  GetImpressiveValue (`Object::GetImpressiveValue` 0x639860 for a thrown object)
  (`src/ECS/Systems/Implementations/VillagerReactions.cpp:876`), a fix row (it needs the Object, Creature, Spell and
  MissionaryControl GetImpressiveValue ports).
- **The land under the thing**: `GetHeightAt` at the truncated map point, not compared bit for bit with `GetAltitude`
  0x803090.
- **The movement of a Living as a reaction's object** (its step): no reaction a villager runs from has one yet.
- **The flag +0xB4 & 0x400**: who sets it (perhaps only a saved game's read of the whole object); `TestSpecial` is not
  ported (queued fix).
- **The impressive value of what is not a miracle**: until it is ported a reaction to food, wood, fire, teleport, a
  death or a scaffold impresses nothing (no belief, no alignment, no boredom), where the original adds belief, moves
  the alignment and tires the town. The fire, teleport and mourning setups do not call `UpdateHowImpressed` yet; the
  scaffold's `ImpressTowns` is not done (queued fix).
- **"No player"**: a reaction with no player is told by the spell's `hasPlayer`; openblack's reactions store NEUTRAL
  for none, so a non-miracle initiator's "none" cannot be told from the neutral player yet.
- **The creature's impression** (`Creature::UpdateHowImpressed` 0x4F2780): not ported (queued fix).
- **The first clip of a loaded villager**: when saved games are loaded, each villager resolved runs
  `villager_animate::SetFirstClip` in its loaded state, as `Object::ResolveLoad` 0x63A020 calls vt +0x658 first (with
  the save loading).
- openblack differs: the predators' life writes (a downed villager's 0.05, an eaten one's 0) do not go through
  `Villager::SetLife` 0x756B40, so a town's injured count does not see them; the original's do (vt +0x5B0 at 0x5EC491
  from `Animal::TargetPounce` 0x4190C6, and at 0x76B3C2 in `Villager::BeingEaten`) (`src/ECS/AnimalPredators.cpp:646`,
  `:719`; `ecs::life::SetLife` is `src/ECS/Life.cpp:56`), a fix row.
