# Villagers: reactions

## Summary

Villagers react to what happens near them through the game's reactions: they run from frightening miracles and
watch the nice ones, point at or run from a thing thrown from the hand, flee and hide from predators, and are eaten
when a predator downs them. Each reaction a villager takes up also impresses it: the town's belief in the miracle's
player grows, the player's alignment moves and the town tires of that kind of miracle.

openblack reproduces the miracles' reactions (3, 21 and 37), the thrown thing (9 and state 162), the impression
from miracles, the hunted villager and the flight from predators (28), with its hiding and looking out, and the
villager's side of a creature's reaction (6): it inspects the creature, respects it or runs from it by its town's
attitude. Every creature makes that reaction as it is made (`CreatureArchetype::Create`).
Fire is on [Fire](../world/fire.md); the reaction machinery shared by every Living is on
[Villager states](villager-states.md#exits-of-the-reactions-exitreaction-exitreacttoteleport).

## The original

### Hunted villagers

Villagers are prey like the animals (type 2, with meat) if they are outside their home. When downed, their health
stays at 5 % and they go to DOWNED (clip `P_ATTACKED_BY_LION`), then BEING_EATEN 300 turns (`P_DYING`; `Living::Downed` 0x5EC4B0 sets +0x58 = 0x12C at 0x5EC4BC) and they die
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
  maxDistanceToRunAwayFromObject 75. The speed in metres a second is divided by the info's raw WalkingSpeed (a whole
  distance a turn, in the thousands: raffclar reads 6554 for every hunter), so in play the factor stays a hair over
  0.5 and the urgency about 80 whatever the hunter does; the low-byte wrap needs inputs the game never gives (vanilla
  bug, kept: [villagers and towns](../project/progress/vanilla_bugs/villagers_and_towns.md)).

### A shield's fall, reaction 36

- **The row.** `Villager::SetupReactToMagicShieldDestroyed` (bw1-decomp `VillagerReaction.cpp`) is
  `SetupPanicReaction(reaction, thing->Pos)`; its priority is always the row's (`ReactToMagicShieldDestroyedPriority`),
  and its turns are the standard ones (`Reaction.cpp`'s type table).
- **The set-up** `SetupPanicReaction` 0x76B0D0 (xref callees): first `SetupGoAndHideInNearbyBuilding` (above); with
  nowhere to hide, the bearing from the point (`Get3DAngleFromXZ`) and two `GameFloatRand`, the row's run distances
  (`GetInfo` three times), `GetPosFromAngle`, and `AddMoveToPosReaction`. Read in v1.42 (2026-10-26) ✅: the point
  (+0x10C) is the shield's position (0x76B0F4); the turn is `GameFloatRand(pi / 4)` less pi / 8 ([0x8C6CA0],
  0x76B11F) and then the bearing added (0x76B12D), each step a float, so (draw − pi / 8) + bearing, not
  (bearing + draw) − pi / 8, which differs by an ulp about one time in three; the distance `GameFloatRand(max − min)`
  (the row's +0x44 − +0x40) + min (0x76B16B); the final state is PANIC_REACTION (240), which turns to
  look back at the point (`LookAtPos(+0x10C, 2)`, 0x76B1DB) and then plays its clip and goes to 163 (0x76B1ED). The set-up gives the villager no
  ReactionObject of its own (his vanilla bug 62), and nothing clears the old one: on a change of reaction
  `ApplyReactionToLivingObjectsAtSquare` (0x6E428A-0x6E42E1) runs `SetReactionDoneWhen`, unlinks the villager from the
  old reaction's `ReactingLivings` and calls `StartReacting` (vt+0x994), never `StopReacting`, the only code that
  clears ReactionObject (+0xBC, 0x5F11A6). `SetupGoAndHideInNearbyBuilding` 0x763CB0 ignores its argument and writes
  only +0x110 and +0x10C, and `AddMoveToPosReaction` 0x5F10B0 leaves +0xBC too. So the villager keeps the object of
  whatever reaction it followed (a shield it was amazed by, a flying rock, a fire), hides from it with that reaction's
  run distances, and goes on watching it; with none the next ProcessReaction stops it (xref #943, #944).
  `AddMoveToPosReaction` 0x5F10B0, called with leave-dance 1 at 0x76B1A5: StorePreviousState when not reacting
  (vt +0x8EC), a dancer out of its dance (vt +0x978), `SetupMoveToWithHug`(the point, 240) and the reaction's link,
  with no `UpdateHowImpressed` and no `SetTopState`. openblack the same since 2026-10-25
  (`villager_cover::SetupReactToMagicShieldDestroyed` through `villager_reactions::AddMoveToPosReaction`, the watched
  object passed on); before, it took the reaction as `AddReaction` does, impressed once and with a change of state to
  240 before the walk.
- **The hide holds no reaction.** Rows 236 and 237 have ReactionValidate as their validate (`GStates.cpp:248-249`): a
  villager hiding from a fall with no ReactionObject (one that followed no reaction with an object) is popped at
  its first turn there, into the state it stored (none: INVALID_STATE, his vanilla bug 63).

### A script's reaction (CREATE_REACTION, REMOVE_REACTION_OF_TYPE)

✅ v1.42 (2026-10-21):

- `GScript::CreateReaction` 0x6FA070 pops the reaction, then the thing; no thing: "no reaction for thing" (0xC0D6B8)
  and nothing. Else `Reaction::CreateReaction(thing, (uint8) reaction, NULL, 0)` 0x6E3D70 (0x6FA0B1..0x6FA0B7): no
  player (a null `GPlayer`, not the neutral player) and no stamp. Its own "none" test, `(type & 0xFF) == -1`
  (0x6E3D7B..0x6E3D86), never holds, so a reaction of type 255 is made too.
  Its row is read through `Reaction::GetInfo` 0x6E4700: `ReactionInfos` 0xD4F6B0 + 100 × type; the constructor
  0x6E39D0 reads +0x34 (whether it grows) and +0x2C (its reach). For type 255 that is 0xD55A4C, inside `LandAvoid`
  (0xD559B0), the land-avoid grid `FloodAnalyse` writes at run time, so the row cannot be pinned.
- `GScript::RemoveReactionsOfType` 0x6FA180 pops the reaction, then the thing; none: the same message. A script
  container (vt+0x3F8) keeps the reaction in the script's loop value (+0x24, 0x6FA1EC) and walks its members through
  its type's loop (0xC0C73C + 24 × type) with `RemoveAllReactionsOfTypeLoopFn` 0x6FA160; anything else goes straight
  to `Reaction::RemoveAllReactionsOfTypeInitiatedByObject(thing, reaction)` 0x6E4780, which shuts down every reaction
  in the list (g_game +0x205BDC, next +0x40) whose initiator (+0x14) is the thing and whose type (+0x24, a whole
  dword) is the one asked for.
- Row 0 of the reaction type table (0xC09CC0, filled by 0x6E0E80, read in the oracle after running it): setup
  0x6E5090 (vt+0x9A4 `SetupFleeFromObject`, as row 3), priority 0x6E51E0 (vt+0xA2C), turns 0x6E4FE0 (vt+0xAC0) and
  again 0x6E5080 (vt+0xAC4), the standard ones as row 3's; the same-type switch (+0x04) 0.
- openblack: `CreateReaction` and `RemoveReactionOfType` in `src/CHLApi.cpp`; the villager's row
  `k_FleeFromObject` (`VillagerReactions.cpp`) and the animal's `case 0` (`AnimalFlee.cpp`).
  `ScriptGuideOgreNatives.AScriptsReactionIsOfNoPlayerAndGoesByItsKind`.

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
  step +0x64 / +0x6C). A creature's is `Creature::GetMovementDirection` 0x477AC0 (Creature vt +0x168), the same one
  the animals read ([Animals](animal-life.md#reactions)): no physics test, 10 m ahead of its body's yaw, the x part
  repeated for z (the x = z diagonal), zero when it faces along z. Both villager readers, `IsObjectTravellingTowardsMe`
  0x5F1E60 (0x5F1F69) and `Living::GetFleeingPositionFromMovingObject` 0x5F2160 (0x5F2175; Villager vt +0xAFC), call
  the object's vt +0x168, so a villager sees a creature's way as an animal does. So a villager within 50 m of a thing still flying runs across its way, with the two
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

### A creature, reaction 6

A creature makes its own `ReactToCreature` reaction as it is made (`Creature::Initialise` 0x474350, the call at
0x47448F, [Random draws](../engine/random.md#where-the-creation-draws-fall) step 7), and every villager it reaches takes
it up by its town's attitude to the creature ([Mimicry](../creature/mimicry.md#town-step-16): 0 new, 1 none, 3 fear,
4 respect; 2 is never written). The v1.42 bytes equal bw1-decomp's 1.20 C (`VillagerReactionToCreature.cpp`) for every
function below; the exe adds the exact comparisons and constants.

- **The row** (`ReactionFunction::Functions` 0xC09C80 + 6 × 0xC0, filled by `_$E137` 0x6E0E90 at 0x6E14FE..0x6E160C):
  not time limited, `CanSwitchToSameType` 0, the setup thunk 0x6E53B0 (vt +0x9B8 `Villager::SetupReactToCreature`
  0x767630), the priority thunk 0x6E5130 (vt +0xA48 `Villager::ReactToCreaturePriority` 0x7676E0), the turns
  `Living::NumGameTurnsToReactToCreatureFunction` 0x5F1B10 and `Living::NumGameTurnsBeforeReactingAgainToCreatureFunction`
  0x5F1C00 (vt +0xAC8 / +0xACC, no Villager override), no impressive multiplier (+0x9C = −1: the info's 1.0), the
  standard GetPos and ModifyAlignment (alignmentModifier 0: no move).
- **The setup** 0x767630: `creature = __RTDynamicCast(thing)` (not null-checked); not `IsAvailable` (vt +0x2C) → nothing;
  **+0xBC (ReactionObject) = creature, before AddReaction** (the 146 clip reads it); no town (vt +0x48) → `AddReaction(r,
  146)`; else `Town::CreateCreatureInfo` 0x743720, then `GetTownAttitudeToCreature` 0x7436F0, `cmp eax, 4; ja` and the
  jump table 0x7676C0: 0, 2, 4 → 146, 3 → 159, 1 → nothing (the object stays the creature). Attitude 1 is never reached
  through the spread: the priority gives 0 for it first.
- **The priority** 0x7676E0 (returns AL): dancing (vt +0x978, `Living::IsDancing`) → 0; no creature → 0; creature
  +0x12C0 (`IgnoredByVillagers`, written only by `SetToZero` and `Load`) or +0x106C (`Invisible`, the invisibility
  spell) set → 0; `d = GetDistanceInMetres` 0x74CD50, `fcomp [GetInfo() + 0x44]` (maxDistanceToRunAwayFromObject, 100)
  `test ah, 0x41; je` → 0 when d > 100 (NaN goes on); no town → the low byte of `ReactionInfos[6].priority` (60); else
  by the attitude (jump table 0x767888): 0 → `ftol(priority × 1.5f)` (0x8AB24C) = 90; 1 → 0; 2 → the low byte of the
  priority; 3 → `fild {ReactionInfos[0].priority, 0}; fadd st0, st0; ftol`, of which AL is kept: 150 × 2 = 300 gives
  **44**, a vanilla wrap kept; 4 → the town's player (vt +0x1C) against the creature's: different → the low byte of
  `ReactionInfos[2].priority` (50); the same and a disciple (+0xE0 bit 0x200) → 0; else the final state's row
  (`GVillagerStateTableInfo::Infos` + s × 0x114): its desire t (+0x14) out of 0..16 (signed) → 50, else `((town
  +0x19C[t] + +0x108[t]) + +0xC4[t]) × amount (+0x18)` (raw + boost + boostA) `fcomp 0.0; test ah, 0x41; je`: above 0 →
  0, else (NaN too) 50; > 4 → 0. Pinned with the oracle (52 cases).
- **The turns to react** 0x5F1B10 (thing, type, d; `ret 0xC`): `f = ((M + R) − d) × h / (M + R) + (1 − h)`, each step a
  float (M `_MetresPerMapCell` 0x92B630 = 10, R maxReactionDistance 55, h howImportantIsDistance 0.5); no creature →
  0; the creature's `GetImpressiveType` (vt +0x1A8, `CreatureActionInfos[+0x1120].+0x1C`, the last action carried out
  to its end) 25 → f + f, 26 → f × 3; a town that fears it (attitude 3) → × 4 (0x8AB418); `ftol(turns (+0x14, 300) ×
  f)`, a far creature giving a negative count. Pinned with the oracle (46 cases).
- **The turns before reacting again** 0x5F1C00: `f = h × d / (M + R) + (1 − h)`; the impressive type 25 → × 0.5, 26 →
  × 0.33333334 (0x8AB26C); the cast is not null-checked; the creature's current action (mind +0x164 → +0xF60) below
  328 (signed) whose `ImpressiveType` is not 24 → 0; a town whose attitude is 3 or 4 (signed `jl` / `jg`) → 0; else
  `ftol(turns (+0x18, 600) × f)`. Pinned with the oracle (45 cases).
- **The states** (`StateTable` 0xD09198, rebuilt from `_$E32` 0x59D030; every row's exit is the ExitReaction thunk
  0x5B0100 and its validate `ReactionValidate` 0x756A00, but 167's `WallhugAndReactionValidate` 0x756A80 = the empty
  `WallhugValidate` 0x756990 then `ReactionValidate`). Each state function starts with the object test: +0xBC none or
  not available → `StopReactingAndSetState` (vt +0x99C), 1. "UA" below is `UpdateAttitudeToCreature` 0x7683F0 (a
  `jmp` to `UpdateReactiveStateFromAttitudeToCreature` 0x768400). InspectDistance is +0x10C (float),
  InspectTurnsUntilApproach +0x110 (the same storage as the predator's hiding fields), the respect counter the
  Living's +0x58.
  - **146** `InspectCreatureReaction` 0x7678A0: distance 20.0f; `min = ftol(perSec × 1.5f)`, then `GameRand(ftol(perSec
    × 3.0f))` (line 160); turns = draw + min; `SetTopState(147)`, UA.
  - **147** `PerformInspectCreatureReaction` 0x767970: `--turns` (32-bit, stored first); distance below 1.0 (`fcomp 1.0;
    test ah, 1`, NaN too): turns 0 → `StopReactingAndSetState` (no UA), else UA; else turns 0 → `SetTopState(148)`, UA,
    else `LookAtObject(obj, 1)` 0x5EC520, UA.
  - **148** `ApproachCreatureReaction` 0x767A00: `d = GetDistanceInMetres`; `d − I > 1.0` → the walk: the angle
    `GetAngleFromXZ` 0x74D240, `MetersToWholeX / Z` 0x74E1D0 / 0x74E200 of `(float)(d − I)` (`ftol((float)ftol(m) / 10 ×
    65536)`: **the metres cut to a whole number first**), `GetX/ZByAngleHugeDistance` 0x74D3E0 / 0x74D400, dest = pos +
    the step, `SetupMoveToCreatureReaction(dest, 148)`, UA; else (<=, NaN) `min = ftol(perSec × 1.5f)`, `GameRand(ftol(
    perSec × 7.0f))` (line 231), distance − 8.0f (0x8C2C70), turns = draw + min, `SetTopState(147)`, UA. So 20 → 12 → 4
    → −4 m, and the third look ends it.
  - **155** `InitialiseRespectCreatureReaction` 0x767BA0: `GetNearestEdgeOfObject` 0x636CD0 (vt +0x834) = the point of
    `GetNearestPosOfObject` 0x636D30 on the villager towards the creature (the two radii), in metres as `x ×
    0.000152587890625f` and back as `ftol(m × 6553.6f)` (altitude 0); `SetupMoveToCreatureReaction(dest, 160)`, UA.
  - **160** `TurnToFaceCreatureReaction` 0x767C80: `LookAtObject(obj, 2)` == 1 → +0x58 = 0, `SetTopState(156)`; UA.
  - **156** `PerformRespectCreatureReaction` 0x767CE0: `|GetAngleFromXZ − (uint16)GameAngle|` (32-bit, `cdq; xor; sub`,
    no wrap round the turn: a vanilla quirk kept) `cmp 0x100; jbe`: above → `SetTopState(160)`, 1 (no UA); else
    `++(word)+0x58`, `(int16)+0x58 > ftol(perSec × 4.0f)` → `SetTopState(157)`; UA. 41 turns of facing.
  - **157** `FinishRespectCreatureReaction` 0x767DC0: `StopReactingAndSetState`, the object there or not.
  - **159** `FleeingFromCreatureReaction` 0x767E00: `flee = (float)(GetReaction()->GetInfo()->maxReactionDistance ×
    1.2f)` (0x8C6C98) = 66; `flee × 1.5f < d` or NaN (`fcompp; test ah, 1`) → `StopReactingAndSetState`; the point
    `Living::GetFleeingPositionFromStationaryObject` 0x5F2010 (vt +0xB00, flee m straight away); off the map
    (`MapCoords::InBounds` 0x6042C0) → UA; the creature (`CastCreature` vt +0xA4) with a plan whose act-on belief (mind
    +0xF58) holds an object (+0x30): `GameRand(10)` (line 353) ≠ 0 → 1; 0 → `SetSpeed(info +0x108 FleeingSpeed / 2,
    1)` 0x750ED0 (toward zero) and `SetupMoveToCreatureReaction(dest, 159)`, 1 (no UA); else `SetSpeed(FleeingSpeed,
    1)`, 1 (no UA).
  - **167** `MoveTowardsCreatureReaction` 0x767F70: `MobileWallHug::MoveTo` 0x60AF20; 10 (arrived) and
    `GetDistanceInMetres(pos, GetDestPos)` 0x74CD70 `fcomp 0.5f` (`_HalfMetre` 0x99A9F4; <= or NaN) →
    `SetTopStateToFinal` 0x5ECA80; UA.
  - **`SetupMoveToCreatureReaction`** 0x768510: `SetupMoveToWithHug(dest, state)` 0x5F2890 (result not read), then
    `LivingAction::SetState(0, 167)` 0x5ECC90 on +0x8C: a raw store of 167 in the top state and 0 in the turns in the
    state (+0x04), no exit, entry, clip or speed; the final state stays `state`.
- **UA** 0x768400: `CheckNeededForWorship` 0x76BA60 == 1 → return; +0xBC no creature → return; the creature's arena
  (+0x10A0) set → `StopReactingAndSetState`, then the first reaction of the game's list (g_game +0x205BDC, next +0x40)
  whose initiator is that arena (each arena makes its own `ReactToFight`, type 19, as it is made: fn_004246F0 at
  0x424764) → `Villager::SetupReactToFight` 0x766E30 (vt +0x9EC; states 143 / 144, the clip `WatchFightAnimation`
  0x423540); return. No town → return. The attitude: 1 → `StopReactingAndSetState`; 3 → `SetTopState(159)` unless the
  final state is 159; 4 → `SetTopState(155)` unless the final state is 155..157 or 160; 0, 2 → nothing.
- **The clips**: `InspectCreatureAnimation` 0x423EB0 (146, 147, 148, 160): a woman or a child (`IsWoman` 0x752620, vt
  +0xAF8): `GameRand(3)` (line 657) == 0 → 355 P_SCARED_STIFF; +0xBC set and `GetDistanceInMetres` 0x74CD70 `< 8.0f`
  (`test ah, 1`, NaN too): `GameRand(3)` (line 667) == 0 → 311 P_LOOK_AT_HAND; then `GameRand(8)` (line 673) > 2 → 395
  P_TALKING_AND_POINTING, else 385 P_STAND. `RespectCreatureAnimation` 0x423F60 (155-157): `GameRand(5)` (line 692): 0
  → 227 P_CROWD_IMPRESSED_1, 1-2 → 385 P_STAND, 3-4 → 343 P_PRAY. 159 has the info table's clip; 167 `MoveToPosAnimation`
  0x423400.
- **Random draws**: none in the setup, the priority, the turns, UA and 147, 155, 156, 157, 160 and 167; 146
  GameRand(30); 148's look again GameRand(70); 159 GameRand(10) while the creature acts on something; the clips
  above as each state is entered.

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
     4t) + it, below 0 → 0. `GetUpdateOfBoredomValue` (vt +0x1B4) is `GBeliefInfo` +0x20 `defaultBoredomOfMe`
     (0xC58660) for every class but the scaffold, whose 0x6EA9E0 gives −1.0 (0x8AB678).
     ✅ How fast a town tires of one miracle (oracle, 2026-10-29: 0x438790 run with info.dat's −0.02, a town of 9 and
     `populationForUnmodifiedBelief` 30): the share × −0.02 is the same tiring per pour whatever the town's size,
     about −0.6 when its villagers all see it. One pour of food takes the look-at-nice-miracle boredom (row 21) from 1
     to 0.4000467 (0x3ECCD2EC); the fold gives back row 21's +0.00056 a turn, so 199 turns later it is 0.5114838
     (0x3F02F09B) and the next pour wears it to 0, its last villager impressed by nothing. A miracle of one kind
     repeated faster than about 1,790 town turns (1 / 0.00056) impresses less and less, down to the turn's 0.00056 ×
     the gap, so a town given one kind of miracle in quick succession levels off below what winning it needs (the
     scene `objects.town_won_by_food` shows it). Test `TownBeliefTest.AMiracleSeenAgainSoonFindsTheTownStillTiredOfIt`.
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
  food piles and the trees take it). The second balance is `GLandBalance::Values[1]` (+0x4, 0x639390, the object's
  vt +0x66C); a miracle reads `Values[2]` (+0x8). The order of the product, from the stack slots: (((artifact ×
  boredom) × power) × distance share) × ((balance × ImpressiveValue) × the reaction's multiplier), unlike a miracle's.
  A thing that is only a `GameThingWithPos` (a town...) gives 0 (0x405300), and the rest of UpdateHowImpressed still
  runs: no belief, but the alignment and the boredom move. `GetUpdateOfBoredomValue` (vt +0x1B4) is BeliefInfo +0x20
  for all but a scaffold (−1, 0x6EA9E0). `Creature` 0x47B200, `Scaffold` 0x6EA770, `MissionaryControl` 0x756920,
  `PhysicalShield` 0x72D7F0 (the Object's, 0 for 13 as the shield's). The creature's side is
  `Creature::UpdateHowImpressed` 0x4F2780 (see [the creature pages](../creature/overview.md)).
- **A thrown villager** (2026-10-30, the vtable slots): `Villager`, `SpecialVillager` and `Living` take `Object`'s
  +0x1B0 (0x639860) and +0x66C (0x639390), and `GameThingWithPos::GetPower` 0x56FE60 at +0x128, so a villager thrown
  past others impresses them as any thrown object, by its own villager row's `ImpressiveValue`, read as each watcher
  takes the reaction up. openblack: the same path (`ObjectImpressiveValue`, the row from the physics' object info),
  test `ImpressionTest.AThrownVillagerImpressesByItsOwnInfosValueAsAnyObject`. raffclar's #225 gives the reaction a
  value fixed at the release instead; not taken, ours reads the thrown thing at each impression as the original does.
- **The creature** (`Creature::GetImpressiveValue` 0x47B200), from the stack slots: ((((f × boredom) × power) ×
  distance share) × X) × GLandBalance value 1 (0x47B2AB..0x47B2BF). f = fn_0047D590: s = the drawn size (creature
  +0x160 +0x58 +0x90) + the creature info's impressiveSizeAddition (+0x388), stored as a float; 1 when `s < 1` or not
  a number (`fcomp 1.0; test ah, 1`), 4 when `s > 4` (`test ah, 0x41`), else s; times impressiveSizeMultiplier
  (+0x38C). The boredom is the villager's `GetBoredomMultiplier(reaction)` (its town's, 1 without one); the power is
  `GameThingWithPos::GetPower`, 1; the distance's share is `DistanceChangeToBelief(GetDistanceInMetres(villager,
  creature), ReactionInfo +0x2C)`. X = `Reaction::GetDefaultReactionMultiplier(villager)` × the
  `CreatureActionInfo` ImpressiveValue (row +0x20, file +0x10) of creature +0x1120, the last action carried out to
  its end, stored as a float; but when the villager has a town whose `GetTownAttitudeToCreature` is 0 (no record, or
  a new one: 0x47B23D..0x47B256), X = the town info's oneOffBeliefForCreature (+0x154), with no multiplier. Its
  boredom update is the default (`GetUpdateOfBoredomValue` is Object's for a creature). openblack:
  `magic::CreatureImpressiveValue` with `CreatureActionInfo::impressiveValue` and `creature_pose::DrawnSize`, read
  in `villager_reactions::UpdateHowImpressed` (VillagerReactions.cpp), 2026-10-20.
- **The scaffold** (`Scaffold::GetImpressiveValue` 0x6EA770): k = 1 (0x6EA77B) without a site (+0x68); with one,
  the site's building (`BuildingSite::GetBuilding` 0x43BC70), and that building's town (vt +0x48, `GetTown`): when the
  building is an abode (vt +0x208, `IsAbode`: every Abode class, not the citadel) k = Town +0x1B0 + Town +0x11C + Town
  +0xD8, else Town +0x1B4 + Town +0x120 + Town +0xDC (0x6EA7A7..0x6EA7D9). With the town's desire at +0x34 these are
  raw (+0x168), boost (+0xD4) and boostA (+0x90) of desire 5 (FOR_ABODES) or 6 (FOR_CIVIC_BUILDING), added in that
  order: `Town::GetRawDesire(5 or 6)`. Then the Object's value (0x639860, stored as a float), and the scaffold's own
  count fn_006E9D10 (ftol((GetScale − 0.5) × 5 + 0.5), the scaffold's value) as a 64-bit integer: value = (count ×
  Object's value) × k (0x6EA80B..0x6EA814), each step a float (the 24-bit FPU). The building is not checked: a site
  whose building is gone would crash the original.
- **info.dat** (ReactionInfo): 3: multiplier 1, alignment −0.01, alignmentForSFX 2 (evil); 21: 1, +0.01, 1 (good);
  37: 1, 0, 1; 13: 1, +0.05 by the protection desire (3), 1. maxReactionDistance 50 / 60 / 60 / 35.
- **Random draws:** none in the impression itself.
- **The belief sound** (`GBelief::AddToBelief` 0x437EB0 → `BeliefSFX` 0x437F40 → fn_0071BF70): with the villager as
  the thing, the people's awe at the villager, heard from the local interface (see
  [the audio engine](../audio/engine.md)): only when the player's belief in the town is below the strongest one; the
  guidance's time check draws twice on the local stream every time it is asked, then the sample's own draws when it
  is time; GOOD_AWE / EVIL_AWE by the reaction's alignmentForSFX (a coin for 0), 3D at the villager, up to 200 m.

### Reactions that only impress, 34, 39 and 40

Read in the v1.42 exe (2026-10-22). ✅

- **The table.** The type table is filled at start-up by `_$E137` 0x6E0E90; its third flag (+0x48,
  `BeliefButNotReaction`) is 1 for exactly three types, 34 REACT_TO_MAGIC_WATER_PUTTING_OUT_FIRE (0x6E3345),
  39 REACT_TO_MISSIONARY (0x6E3824) and 40 REACT_TO_FIGHT_WON (0x6E395B), and 0 for every other one. The three
  have no set-up, no priority and no turns to react (34's row 0xC0B600: +0x50, +0x60, +0x70 all 0); their turns
  before reacting again are the standard ones (the thunk 0x6E5080 to vt +0xAC4, `Living` 0x5F1920 for a villager).
  bw1-decomp's `Reaction.cpp` table agrees.
- **Who is impressed.** In the spread's per-Living body `ApplyReactionToLivingObjectsAtSquare` 0x6E3F90, after
  `IsAvailableForReaction` (vt +0x984), the shield test (`fn_0072B990`) and the turns before reacting again
  (0x6E40B3), `IsAvailableForBeliefButNotReaction` (vt +0x988) is asked (0x6E40C2). `Villager`'s 0x763410 is
  true when the type has the flag or the villager cannot be reached (`IsReachable`, vt +0x530); `Living`'s 0x417090
  (animals, creatures) is always 0. When true (0x6E430C): `Records(type, again)` (fn_006E4340), and if it records,
  `UpdateHowImpressed(reaction, 1)` (vt +0x98C, [belief](#belief-from-the-miracles-villagers-react-to)). Nothing
  else: no state, no reaction held, nobody walks to the fire.
- **The cell ends there.** Both ways (recorded or not) the body returns (0x6E432D) instead of going on to the next
  object of the cell: the rest of that map cell is not offered the reaction. So in each cell only the first
  villager available for it is impressed.
- **A villager that cannot be reached, for every type.** `Villager::IsAvailableForBeliefButNotReaction` 0x763410
  (read 2026-10-22, bw1-decomp's `VillagerReaction.cpp` agrees) is, in this order: the type's flag (+0x48 of row
  type x 0xC0 at 0xC09C80) set -> 1; else `IsReachable` (vt +0x530, `Villager` 0x756460) false -> 1; else 0.
  `IsReachable` is: `IsAvailable` (vt +0x2C, 0x751D50: not the GameThing unavailable flag, bit 0 of byte +0xA, and
  `GetFinalState` vt +0xB04 not 14 DYING) and not at home (byte +0xE0 & 4, 0x756474) and not held back from a change
  of state (byte +0x24 & 4, 0x756483: in the hand, [the flag](villager-life.md)) and TOP (+0x8C) not 236
  GO_AND_HIDE_IN_NEARBY_BUILDING (0x756497). `SpecialVillager` keeps the same
  three functions; `PieceVillager`, `PuzzleVillager`, `Creature` and the animals have `Living`'s 0x417090, always 0.
  So any reaction, of any type, that reaches a villager at home, in the hand's hold or hiding, and finds it
  available for it, only impresses it and ends the cell, exactly as the three above.
- **Its turns before reacting again** are the type row's function (+0x80, this-adjust +0x84, called at 0x6E40B3
  for every type before the test): the standard ones (`Living` 0x5F1920) for every type but 6, 12, 13, 28 and 33.
  13 REACT_TO_MAGIC_SHIELD's, `Villager` 0x764230: a town (`GetTown` vt +0x48) whose last attack (+0xEB0) is more
  than the villager's own info's (+0x28) numGameTurnsAfterAggressionInterestedInShield (+0x364, 0x764256) turns ago -> 0x7FFFFFFF; else a
  thing that is a shield spell (vt +0x4C8) whose `IsUnder(villager goal +0x80, 0)` (0x72BD20) is false -> 0; else
  the standard ones. 33 REACT_TO_BURNING_OBJECT_IN_HAND's, `Villager` 0x764130: the villager's own distance to the
  thing (`GetDistanceInMetres` 0x74CD50) at least the reaction info's minDistanceToRunAwayFromObject (+0x40) and the
  thing's `GetSpeedInMetresPerSecond` (vt +0x154) at most 3.0 (0x8C2C50) -> the standard ones, else 0. The speed is
  0 for a thing that does not walk (`GameThingWithPos` 0x570260) and, for a walker (`MobileWallHug` 0x60C0F0), its
  whole speed a turn (+0x5A) in metres times 10.0 ([0x8AB414], ten turns a second; checked with the oracle).
- **33 REACT_TO_BURNING_OBJECT_IN_HAND taken up** (read 2026-10-25). Its priority, `Villager` 0x767430: the
  villager's distance to the thing (`GetDistanceInMetres` 0x74CD50) not above the type row's
  minDistanceToRunAwayFromObject (+0x40; `test ah, 0x41`, so a NaN counts as near) -> the row's priority (+0x10,
  0xD503A4); farther, the thing's speed (vt +0x154) at least 3.0 (0x8C2C50; a NaN counts as slow) -> the priority,
  else 0. Its setup, 0x767490: the thing kept (+0xBC), then `AddReaction`(r, 6 FLEEING_FROM_OBJECT_REACTION). Its
  turns to react are the standard ones (0x764110 forwards to vt +0xAC0). openblack: the slot row
  `k_ReactToBurningObjectInHand` (`VillagerReactions.cpp`, the pure `villager_reactions::BurningObjectInHandPriority`;
  tests `VillagerReactionsTest.ABurningThingInTheHandIsTakenUpNearOrWhenItMovesFast` and
  `AVillagerRunsFromABurningThingInTheHandNearIt`).
- **Who makes them.** 34: the water miracle on a burning object, and a storm whose rain cools a fire
  ([water](../magic/food-wood-water-heal.md), [storm](../magic/storm-and-tornado.md)); 39: a missionary.
  40: a creature's won fight (read 2026-10-30): the fight action's running sub-state 0x503830, the opponent's life
  gone (0x50395A, after the local player's general-good remark, before the wins count + 1 and `EndFighting`), and
  `LH3DCreature::DoFightActionAction` 0x48B2F8 (bw1-decomp Creature3D.cpp:5359, the knocked-out blow): `CreateReaction
  (winner, 40, its player, 1)` when `GetPlayer()` is not null. A map script's creature always has one (an unknown name
  is the neutral player, `GPlayer::GetPlayerFromText` 0x64B5E0); a challenge script's generic CREATE makes it with no
  player (ebp 0 at 0x6F14A8 in fn_006F11A0), so it makes none. openblack: `CreatureFightSystem::Win`
  (`src/ECS/Systems/Implementations/CreatureFightSystem.cpp`), for any owner but no player (`PlayerNames::_COUNT`);
  test `CreatureFightTurnsTest.WinningAFightImpressesThoseWhoSeeItForTheWinnersPlayer`.

### Fainting and confused, reactions 25 and 26

Settled in the frozen exe (2026-10-18): who makes them, and what takes them up. 🔍 (read, not run)

- **Fainting (25) has one maker.** `LH3DCreature::DoStaticAction` 0x4875D0: on the poo clip (C_STATIC_POO 32,
  +0x49A0) the counter +0x51E4 counts up each call; at 5 (`jl` at 0x48763B) the plan's action (mind +0xF60) is read.
  When it is CREATURE_FART (177, `cmp esi, 0xB1` at 0x487662) the creature lets off 15 particle drops
  (`GameFloatRand` ×3 each) and a smoke (`LH3DSmoke::Create` 0x48776C) instead of the lump, and when the body's
  alignment (+0x9C) is below −0.5 (0x8CEFCC, 0x4877A4) it makes `CreateReaction(creature, 0x19, its player, 0)`
  (0x4877D1). The counter goes back to 0. bw1-decomp `Creature3D.cpp` `DoStaticAction` reads the same.
- **The Fart action.** info.dat's desire table lists two actions for the poo desire (7): CREATURE_POO (19) and
  CREATURE_FART (177) (the row at file offset 0x60874). Its agenda is the poo's (`ConstructSubActionsForFart` calls
  `ConstructSubActionsForPoo`, bw1-decomp `CreatureAction.cpp`).
- **Confused (26) has no maker.** Of the 45 calls to `Reaction::CreateReaction` 0x6E3D70, none pushes 0x1A; the
  six that take the type from data never give 25 or 26 either:
  - the miracles' GMagicEffectInfo +0x94 (`Spell::InitWithPos` 0x720099, `ApplyDefaultSpellEffect` 0x720EFD) give
    3, 21 or 37 ([Running from and watching miracles](#running-from-and-watching-miracles-reactions-3-21-and-37));
  - the pots' GPotInfo +0x128 (`Pot::SetupReaction` 0x66D690): 7 or 12 in all 19 rows of info.dat;
  - the spot visuals' GSpotVisualInfo +0x48 (`GParticleContainer` 0x63E188, skipped for 0x29): 0x29 in all 50 rows;
  - the script's CREATE_REACTION (`GScript::CreateReaction` 0x6FA0B7): the one call in `Scripts/Quests/challenge.chl`
    (instruction 23494) makes type 0;
  - the creature's ADD_REACTION_TO_OBJECT sub-action (57; its stage 0x5055D0 sits at row 57 + 0x60 of the sub-action
    table 0xBE0B98, rows 0x90): no `AddSubAction` / `AddMainSubAction` call is given 57.
  So the confused reaction's setups (the villager's states 195 / 196, the creature's forced examine) are never
  reached in the original.
- **What takes 25 up.** The type table's row (bw1-decomp `Reaction.cpp`): `SetupReactToFainting`,
  `ReactToFaintingPriority` (the table's priority). A villager (`VillagerReaction.cpp`) makes itself the object and
  goes into FAINTING_REACTION (194), whose state does nothing until the reaction ends; a creature
  (`CreatureReaction.cpp`) is forced to sleep (tiredness, CREATURE_SLEEP). The animals' rows are not read here.
- **info.dat** (ReactionInfo 25): priority 200, 600 turns to react, 1200 before again (the same for creatures),
  impresses 1, finishes in the hand 1, maxReactionDistance 50, howImportantIsDistance 0.4, grows by 0.1 a turn. 26
  carries the same numbers with priority 190.
- **openblack:** a creature whose fart makes others faint (`creature_fart::MakesFaint`, its body's alignment below
  -0.5) makes reaction 25 for its own player when the fart's lump would fall (`CreatureMindSystem`); 26 is never made,
  as in the original.

### A new building, reaction 15

- **Made by** `MultiMapFixed::Built` 0x52EBB0, after its site's deletion: a civic abode (`Abode::IsCivic` 0x405FF0:
  totem, storage pit, creche, workshop, wonder, graveyard, town centre, pitch, dispenser; every other class answers 0,
  so the citadel test never holds) that is not the pitch, with a town: `CreateReaction(this, 15, its town's player,
  0)` (0x52EBF7..0x52EC04), not started (its first taker stamps it). Built is reached from `BuildBy` 0x52ED40 and
  `SetPercentBuilt` 0x52EDD0.
- **The row**: time limited (0x6E0FCD), no same-type switch, not belief-only; info.dat's priority 60, 1200 turns to
  react, 2400 before reacting again, 35 m. Villagers and creatures take it at `Living::ReactToNewBuildingPriority`
  0x5F1880: 0 while dancing in any dance (+0xD8), else the row's priority; animals never (0x417650). Turns: the
  standard ones.
- **Setup** (`Villager::SetupReactToNewBuilding` 0x766010): `Get2DRadius` < 10.0 [0x8AB414] → dance row 4 (small
  building, 300 turns), else row 5 (large, 500). The reaction's first taker makes the dance (`Dance::Dance` 0x50B6A0:
  at the building, for it, its row's duration, not a script's, for the taker's town) and the reaction keeps it
  (+0x28); later takers join that one. The reaction's object is set first. With a group in the dance
  (`FindDanceGroup` 0x597400): its place (`CalculateDancePosition`) and `Living::AddMoveToPosReaction` 0x5F10B0 to it
  with final state 203 DANCE_WHILE_REACTING, staying in the dance (no impression, no state change but the walk's);
  else `AddReaction(r, 7)` LOOKING_AT_OBJECT_REACTION. No draws.
- **203** (`Villager::DanceWhileReacting` 0x766130): a dancer standing on the dance's arrive point (x + 0x8000)
  but not at its place walks to its place (`SetupMoveToPos`, 203); one in no group decides what to do; else
  `PerformDance` with 203. Its exit is ExitReaction. `Villager::StopReacting` 0x7637D0 takes a dancer of 203 out of
  its dance.
- **The end**: 1200 turns after its first taker the reaction shuts down; its deletion (`Reaction::ToBeDeleted`
  0x6E3AA0) deletes its dance, whose remaining dancers decide what to do (`Dance::ToBeDeleted` 0x50B970).

### A scaffold, reaction 38

- **Made by** a scaffold a player puts up for a building, started at once (`scaffolds::BuildBuilding`, `ECS/Scaffolds.cpp`).
- **The row** (0xC09CC0 + 0xC0 × 38): setup thunk 0x6E5140 (vt +0x9A8 `Villager::SetupLookAtObject` 0x763AA0, which
  only calls `Living::SetupLookAtObject` 0x5F13B0: `AddReaction(r, 7)` LOOKING_AT_OBJECT_REACTION, the scaffold as
  the object), priority thunk 0x6E4EB0 (vt +0xABC `Villager::ReactToScaffoldPriority` 0x763980: the row's priority
  byte, [0xD50598] = ReactionInfos + 0xEE8, whatever the villager), the standard turns (0x6E4FE0 / 0x6E5080).
- **openblack:** the slot row `k_ReactToScaffold` (`VillagerReactions.cpp`), with the shared look set-up;
  `test/test_villager_reactions.cpp` `ScaffoldReactionTest`; the testbed scenario `objects.scaffold_put_down_watched`
  (its command `HandPlantScaffold` ends the hand's gentle put-down of a scaffold).

### A held villager, reaction 32

- **Made by** `Villager::InterfaceSetInMagicHand` 0x753080 (the call at 0x7530F2), the villager's own part of being
  put in a hand: the picked-up flag (+0x2C |= 0x20), a partner it was having sex with let go to decide afresh, then,
  when the interface's player is the villager's own and it is of breeding age (`IsSexuallyActive`), reaction 32 with
  that player, not stamped; then `Living::InterfaceSetInMagicHand`. The handler's one caller is
  `GInterface::PlaceObjectInMagicHand` 0x5DA77C, so the player's and the computer players' hands make it, a creature's
  take does not. The row is time limited: 500 turns to react (info.dat's reaction row 32: priority 30, 500 turns, 0
  before again, reach 10, the initiator's hand not ending it; read 2026-10-31). Leaving the hand removes the
  villager's reactions of type 32 only through LANDED's state (`Villager::Landed` 0x7606E0, at 0x76076F for a
  disciple and 0x760904 otherwise, `RemoveAllReactionsOfTypeInitiatedByObject(this, 0x20)`): a villager whose
  previous state goes back after landing, or one a script controls, is put back in that state at once by
  `Villager::EndPhysics` (0x5F0D0C..0x5F0D6F, [the hand](villager-hand-and-death.md)), so LANDED's state never runs and
  its call lasts out its 500 turns. openblack's `VillagerLanded` and `villager_physics::SetLandedState` do the same:
  in the real-input run of 2026-10-31 the man the testbed told to stand kept his call after the put-down
  (`refs/realinput/land2_quests.log`, press 3).
- **The row** (row 32): setup `Villager::SetupReactToVillagerInHand` 0x7672F0 (`AddReaction(r, 235)` WAIT_FOR_MATE,
  the held villager as the object); priority `Villager::ReactToVillagerInHandPriority` 0x767320: the initiator a
  villager whose final state is IN_HAND (24), this villager of breeding age, of the other sex (the infos' +0x1F8), of
  the same player and not controlled by a script → row 30's (REACT_TO_BREEDER) priority (ReactionInfos +0xBC8), else 0;
  the standard turns.
- **235** `Villager::WaitForMate` 0x7673A0: the distance in metres to the object below row 32's reach (ReactionInfos
  +0xCAC) → `LookAtObject(object, 1)` and, facing it and `IsReadyForNewAnimation(1)`, `SetStateAnim`; else
  `StopReactingAndSetState`. Its table row has the shared reaction exit and validate (0x5B0100 / 0x756A00, read
  through the oracle).
- **openblack:** `villager_mate` (`src/ECS/Systems/Implementations/VillagerMate.cpp`): `HeldByHand` from the
  villager's in-hand step (`villager_physics::InterfaceSetInMagicHand`, given the hand's player by the player's hand
  and the computer players'), the slot row `k_ReactToVillagerInHand` (`VillagerReactions.cpp`) and row 235;
  `test/test_villager_reactions.cpp` `AHeldVillagerCallsThoseOfTheOtherSexOfItsPlayerWhoStopAndLookAtIt`. The
  spread reads the held villager's own place each time, so the reaction goes where the hand carries it.
- **raffclar's #225** (2026-10-30 survey): the same reaction, made in his hand's grab (the player's hand only) and
  moved after its villager each turn by his reaction system. Ours already does the same job at the original's point
  (the villager's in-hand step, every hand), so nothing of it is taken.

### A thing to look at, reaction 1

- **Made by** a fixed object the hand puts down that becomes an artefact of a town or worship site it did not last
  belong to (`Fixed::EndPhysics` 0x52DF50, at 0x52E034 and 0x52E0AB; `ECS/ObjectPhysics.cpp`). The row is time
  limited (the reaction shuts down after its turns to react).
- **The row** (row 1 "LookAtStaticObject"): setup `Villager::SetupLookAtObject` 0x763AA0, which only calls
  `Living::SetupLookAtObject` 0x5F13B0 (`AddReaction(r, 7)` LOOKING_AT_OBJECT_REACTION, the thing as the object);
  priority `Living::LookAtObjectPriority` 0x5F1610 (the row's priority, whatever the villager); the standard turns.
- **4 LOOK_AT_SPELL** shares the setup (`Villager::SetupLookAtSpell` 0x763AC0), but no effect of info.dat makes it and
  no shipped script does, so openblack gives it no row.
- **openblack:** the slot row `k_LookAtObject` (`VillagerReactions.cpp`) with the shared look set-up;
  `test/test_villager_reactions.cpp` `LookAtObjectReactionTest`.

### A magic tree, reaction 8

- **Made by** each tree of a forest miracle as it is planted, and again when one put down by the hand stops burning
  (`MagicTree::MagicTree` 0x5FCFD4, `MagicTree::EndOnFire` 0x5FD0FD; `Magic/Objects/MagicTree.cpp`). The tree is the
  initiator, so a villager's impression is the object's (`Object::GetImpressiveValue`).
- **The row** (row 8): setup `Villager::SetupReactToMagicTree` 0x763820 (`AddReaction(r, 149)`, then the tree as the
  object), priority `Living::ReactToMagicTreePriority` 0x5F17F0 (the row's priority, whatever the villager), the
  standard turns. The four state rows (149 to 152) have the shared exit and validate of the reactions (read off the
  exe's state table, 0x5B0100 / 0x756A00, as the creature's rows).
- **149** `InitialiseBewilderedByMagicTreeReaction` 0x764AA0: the object gone or unavailable, or not a forest
  miracle's tree → `StopReactingAndSetState`; else the next tree's number = GameRand(the forest's trees − 1), none
  looked at, → 150.
- **150** `PerformBewilderedByMagicTreeReaction` 0x764B50: the same tests; with n trees, while looked < n −
  GameRand(n / 2) (drawn again each time): the object becomes `Forest::GetTree(next)` 0x53A5B0 (its growing list first,
  then its grown list, each sorted by distance to the forest's centre; past the end, none), looked + 1, next = (next +
  1) % n; the villager walks (`SetupMoveToWithHug`, final 151) to the old tree's `GetNearestEdgeOfObject` 0x636CD0
  (`GetNearestPosOfObject`: the tree's place + its and the villager's 2D radii towards the villager, through a
  landscape point and back, altitude 0); the look = GameRand(ticks(3 s)) − ticks(−2 s) in 8 bits, both the turns left
  and the look's length. Else `StopReactingAndSetState`.
- **151** `TurnToFaceMagicTree` 0x764D10: object there and available, else stop; `LookAtObject(object, 1)`; facing it
  → 152.
- **152** `LookAtMagicTree` 0x764D70: object there and available, else stop; at half the look `SetAnim(1)` (the
  state's clip again: `LookAtLargeObjectAnimation` 0x423A80 looks at the hand for the first half, about for the rest);
  the turns left − 1, at 0 → 150.
- **openblack:** the slot row `k_ReactToMagicTree` (`VillagerReactions.cpp`) and rows 149 to 152
  (`villager_magic_tree`, `src/ECS/Systems/Implementations/VillagerMagicTree.cpp`), the pure rules in
  `ECS/MagicTreeWatchRules.h` (`test/test_magic_tree_watch.cpp`), the forest's order in `ecs::ForestTreesGrowingFirst`
  (⚠️ approximate: a tree counts as grown once full size, where the original moves it to its grown list only from the
  growing list's head); the testbed scenario `miracles.villagers_look_at_magic_trees`. Refitted from raffclar's
  e7989e41a (`docs/wiki/open-work/conflicts.md`).

### A town's take-over, reaction 31

- **Made by** the take-over (`GPlayer::ClaimTown` 0x649810, after `AddTownToPlayer`): from the town centre (+0x9A4),
  else from the town, for the new owner, not started, whoever the new owner is: the belief fold (fn_004383D0, 0x438733
  / 0x43875D) and `Town::SetTownEmpty` call it for the neutral player too, so a town without a centre that goes
  neutral (a town believing in no one, at its first fold) makes it and its villagers watch it
  (`town_belief::TakeOverTown`, `ECS/Town/TownBelief.cpp`).
- **The row** (0xC09CC0 + 0xC0 × 31): setup thunk 0x6E5340 (vt +0xA18), priority thunk 0x6E5000 (vt +0xAA4), the
  standard turns (0x6E4FE0 / 0x6E5080).
- **Priority** (`Villager::ReactToTownCelebrationPriority` 0x767180): 0 for a disciple (+0xE0 & 0x200), a dancer
  (`Living::IsDancing` 0x5ECC10, +0xD8) or a villager with no town (vt +0x48 `GetTown`); else the row's priority
  byte ([0xD502DC] = ReactionInfos + 0xC2C) when the reaction's initiator (+0x14) is its own town, 0 otherwise. A
  take-over made from a town centre is therefore never taken up by a villager.
- **Setup** (`Villager::SetupReactToTownCelebration` 0x766FD0), the initiator a town (vt +0x1B8 `Town::IsTown`
  0x739250 → 1): the reaction's first taker makes its dance (+0x28) — `Dance::Dance` 0x50B6A0 at the town's position,
  dance row 6 (DANCE_AROUND_PERSON, 0xCC4FA0 = 0xCC4B80 + 6 × 0xB0), about no thing, that row's duration
  ([0xCC4FB0]), not a script's, for the town; every taker then has the town as its reaction object (+0xBC) and
  `AddReaction(r, 7)` LOOKING_AT_OBJECT_REACTION. Nobody joins that dance. The other branch (a town centre: dance row 8
  DANCE_TOWN_CELEBRATION about the centre for its town, `RemoveFromDance` for a dancer, `FindDanceGroup` 0x597400 and
  the walk with footpath `AddMoveToPosWithFootpathReaction` 0x5F1010 to its place, final state 203) is never reached,
  as the priority takes only a town's own.
- **openblack:** the slot row `k_ReactToTownCelebration` (`VillagerReactions.cpp`: `ReactToTownCelebrationPriority`,
  `SetupReactToTownCelebration`); the reaction's going takes its dance, as for 15; the unreachable centre branch is
  not ported. `test/test_villager_reactions.cpp` `TownCelebrationReactionTest`; the testbed scenario
  `objects.town_won_without_a_centre`; in play, the debug window's Towns list marks a town with no centre, and its
  take-town button takes it over.

### Who the spread offers a reaction to

Read in the v1.42 exe (2026-10-26). ✅

- **One gate first, for every type.** The spread's per-Living body `ApplyReactionToLivingObjectsAtSquare` 0x6E3F90
  asks `IsAvailableForReaction(type)` (vt +0x984, 0x6E401D) of every Living of the cell before anything else: before
  the shield test (`fn_0072B990`, 0x6E4031), the turns before reacting again (0x6E40B3), the impress-only test
  (vt +0x988, 0x6E40C2), the first take with no reaction held (+0x94 = 0: the score at 0x6E40E4, the records at
  0x6E40FC, the setup vt +0x994 at 0x6E4129) and the switch to a better one (+0x94 set, 0x6E4134..0x6E42E1). When it
  answers 0 the body goes straight to the next object of the cell (0x6E42E7). The miracles' types (fire 10,
  teleport, shield, death) take this same path: there is no other per-villager entry.
- **`Villager::IsAvailableForReaction` 0x763390**, in order: the final state (vt +0xB04, the low byte); at the
  worship site (+0xE0 & 2) → 0; the state table's can-react column (row +0xFC, 0x7633BA) 0 → 0; playing football
  (+0xE0 & 0x80) → 0; `IsDeathState` (vt +0x970, `Living` 0x5ECAA0: 13 ≤ s ≤ 17, SET_DYING, DYING, DEAD, DROWNING,
  DOWNED) → 0; the life (vt +0x11C) at or below the info's `LifeWhenCrawlsWounded` (+0x380) → 0 unless the type is 7
  REACT_TO_FOOD; then `Living::IsAvailableForReaction` 0x5F11F0: not functional (vt +0xD4) → 0; controlled by a
  script → 0; in the dance editor → 0; the final state 15, 16, 17, 18, 14, 23 or 5 (0x5F1227..0x5F1248) → 0; dying
  (+0xB4 & 1) → 0; on a structure (+0x24 & 0x80) → 0.
- **So a dying or dead villager takes no reaction.** Its death clip is played by `PlayAnimThenSetState(DEAD)`: TOP
  23 WAIT_FOR_ANIMATION with FINAL 15 DEAD, then 15. Whichever of the two the final state reads, it is turned away
  (23 and 15 by the Living's list, 15 also as a death state). A fire, a teleport, a shield or a death next to a
  corpse is not taken up by it, and no switch reaches it.
- **The dead villager is only its death reaction's initiator.** `Villager::Dying` makes REACT_TO_DEATH with itself as
  the initiator (`CreateReaction(this, 23, NULL, 0)`, 0x76A5C2); the reaction's constructor 0x6E39D0 writes nothing on
  the initiator, and the current reaction (+0x94) is written only by `Living::AddReaction` 0x5F0F30 (at 0x5F0F75, after
  `StorePreviousState` vt +0x8EC when +0x94 was 0 and `SetTopState` vt +0x8E8), which only the reaction setups call (49
  sites, every one a `Setup*`), reached through `StartReacting` (vt +0x994) from the spread's two calls behind the gate
  above and from `CheckWorshipActivity` (0x76BC07). Through the spread the corpse never follows its own death reaction or any other.

### The switch to a better reaction

Read in the v1.42 exe (2026-10-26). ✅

- **Where.** The spread's per-Living body `ApplyReactionToLivingObjectsAtSquare` 0x6E3F90, after the availability
  (vt +0x984), the shield test, the type's turns before reacting again (0x6E40B3) and the belief-only test (0x6E40C2),
  reads the reaction the Living follows (+0x94, 0x6E40D0). None: the first take (0x6E40DA, `Priority` above 0,
  `Records(type, again)`, the reaction's start turn if still 0, then `StartReacting` vt +0x994). One: the switch
  (0x6E4134), the same body for every type and every Living class.
- **The scores.** The current one: `Priority` 0x6E4620 of the reaction followed, with the new one as `other`, at the
  distance from the Living's MapCoords (+0x14) to the centre of the cell of the followed reaction's position
  (`GetDistanceInMetres` 0x74CD90 with a `JustMapXZ`). The new one: its `Priority` at the spread's distance, with no
  `other`. Both bytes made floats.
- **The same type** (0x6E41B3): only when the type's row has the same-type flag (+0x44 of the row at 0xC09C80, 1 for
  10, 28 and 35), and never to the reaction followed itself.
- **The rule** (0x6E41D8): the current score below the new one; h = cur / new x 20 [0x8C7658] - 10 [0x8AB414], at
  least 10; seconds = (turn - the record's turn of the followed type, `GetGameTurnReactionStarted` 0x5F0FB0) / (1000
  / ms per turn), both unsigned divisions, made a float; below h the switch waits, unless the seconds are at least 1
  [0x8AA390] and the followed type is 16 REACT_TO_HAND_PICK_UP (`reactions::MaySwitch`).
- **The switch** (0x6E428A): `SetReactionDoneWhen(new type)` 0x6E44A0 (the record gets the turn, added when there is
  none), the Living taken out of the followed reaction's list of followers (0x6E4292..0x6E42D3, with no record
  refresh and +0x94 left as it is), then `StartReacting` (vt +0x994, `Living` 0x6E4590: the type row's setup at
  +0x50). No `Records`, and the reaction's start turn is not set (0x6E4109 is the first take's alone).
- **The setup while the old one is held.** Every miracle setup takes the reaction through `AddReaction` (vt +0x990,
  `Villager` 0x763440: `UpdateHowImpressed` vt +0x98C, then `Living::AddReaction` 0x5F0F30), which stores the state
  (vt +0x8EC, `StorePreviousState`) only when +0x94 is null (0x5F0F3C). In a switch +0x94 is still the old reaction,
  so the state stored before the first reaction stays. The fire's 0x765540, the teleport's 0x766250, the shield's
  0x765C60 and the death's 0x7665B0 all call vt +0x990.

### Sources

- bw1-decomp: `src/Black/VillagerReaction.cpp`, `LivingReaction.cpp`, `Reaction.cpp`, `Animations.cpp`; the rest
  read in the exe with the xref index; the predator rules checked with the oracle.

## openblack

**Done**: reactions 3, 21, 37 and 9, state 162, the impression from miracles and from objects
(`magic::ObjectImpressiveValue`) and from scaffolds (`magic::ScaffoldImpressiveValue`,
`villager_reactions::ScaffoldSiteWant`, and the scaffold's boredom update `magic::k_ScaffoldBoredomUpdate`)
and from creatures (`magic::CreatureImpressiveValue`), the hunted villager, the flight from
predators (28) and the villager's side of a creature's reaction (6), which every creature makes as it is made.

- **Hunted villagers**: `ecs::villager::VillagerDead` kills the eaten villager at the end of the turn; the downed
  villager is driven by the animal AI (`components::DownedVillager`). The one that cannot be eaten is put in LANDED
  when the 300 turns end, without waiting for the clip ⚠️ approximate. The corpse counter is the shared
  `living::DeadTick`.
- **Predators**: the rules are pure functions in `ECS/Effects/PredatorFlee` (`flee::VillagerFleeFromPredatorPriority`,
  `flee::LivingTurns*` / `flee::VillagerTurns*`, `flee::VillagerPredatorFleeStep`, `flee::VillagerLookIfSafeStep`;
  `test/test_predator_flee.cpp`, pinned to the exe's numbers). They are wired through the slot row `k_FleeFromPredator`
  of `villager_reactions::SlotTypeOf` (its priority `FleeFromPredatorPriority`, turns `TurnsToReactToPredator` /
  `TurnsBeforeReactingAgainToPredator`, set-up `SetupFleeFromPredator`), the state 231 `FleeingFromPredator`, and
  `villager_cover`'s 236 / 237, which hide by the predator's row (`VillagerHiding.threatKind`) and look out by
  `VillagerLookIfSafeStep` (`BuildingHiddenIn`, `HideAgain`). The flee-or-watch step is the shared
  `villager_reactions::FleeFromObjectIfComingTowardsMe` (the miracles' 6 uses it with 30 / 30, the predator's 231 and
  237 with 7 / 7). A predator's movement for the coming-at test is its last step (`animal_ai::MovementDirection`), its
  speed `animal_ai::SpeedInMetresPerSecond` (the whole distance a turn in metres, ten turns a second), its walking
  speed the info's `speedGroup.speed3`. `villager::IsAvailableForReaction` is ported. Testbed:
  `animals.predators_village` (ported from raffclar PR #425, unverified against v1.42). Ported from raffclar PR #425,
  behaviour from the exe: his own rule set (`animals::predators`) is not taken, ours already holds the exe's, pinned
  to its numbers; where both say the same (the urgency about half the table's, the 20 turns of looking about) they
  agree.
- **Miracles**: the rows `k_FleeFromSpell` / `k_LookAtNiceSpell` of `villager_reactions::SlotTypeOf`, the priorities
  `FleeFromSpellPriority` / `LookAtNiceSpellPriority` and the states `Fleeing` (6) and `Watching` (7 and 30) in
  `ECS/Systems/Implementations/VillagerReactions.{h,cpp}`; the pure flee point, coming-towards test and urgency in
  `ECS/Effects/FleeGeometry.{h,cpp}`; the rows 6, 7 and 30 of `k_VillagerStateTable` (`LivingActionSystem.cpp`).
  `map_coords::InBounds` uses its 512 cells. A spell's movement is `components::Spell::movementDirection`; a
  creature's is `ecs::creature_reaction_facts::MovementOf`, checked before the physics (the animals'
  `effects::flee::CreatureMovement`, pinned by `AnimalCreatureReactions.ACreatureGoesAlongADiagonalAheadOfItsBody`). Tests:
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
  flying-thing tests of `VillagerReactionsTest`). Testbed: `physics.villagers_watch_throw`. A dancing villager
  (`Dancer`) notices the thrown thing only when it comes at it (`living_rules::FlyingAt` on the thing's body), and a
  thing with no body or place never.
- **The switch to a better reaction**: `SwitchAllowed` with `reactions::MaySwitch` for every type a villager is
  reached by while it follows a reaction, a slot one or a miracle one (`VillagerReaction`, `ApplySlotReaction` in
  `VillagerReactions.cpp`). A slot type switches through its row's setup (`AddReactionWith` lets the miracle maps go);
  a miracle type (fire, teleport, shield, death) through `SwitchToMiracleReaction`: the type's record by
  `SetReactionDoneWhen`, its setup alone (`villager_fire::SetupReactToFire`, `villager_teleport::SetupReactToTeleport`,
  `villager_shield::SetupReactToMagicShield`, `villager_mourning::SetupReactToDeath`, with no score or records test of
  their own), then the old slot or miracle map let go with its record put back. The fire's, teleport's and shield's
  setups store the state only for a villager that follows no reaction, as `AddReaction` (the mourning's already did).
  Tests: `VillagerReactionsTest.FoodToWoodSwitchAfterTenSeconds`,
  `AMiracleReactionSwitchesToAnotherMiracleReactionAfterTenSeconds`,
  `ASlotReactionSwitchesToAMiracleReactionWithoutItsRecordsTest`,
  `AMiracleSetupStoresTheStateOnlyForAVillagerThatFollowsNoReaction`, `MiracleTypeMeetingSlotHolder`. Testbed
  `miracles.fire_draws_mourners` (`src/Debug/TestbedScenarioBlastFire.cpp`): three villagers mourning a dead one of
  their town by a hut a fireball sets alight leave the death (23) for the fire (10) once they have mourned ten seconds;
  the testbed's `VillagerReactions` command logs each villager's state and the reaction it follows.
- **The first clip** (since issue #12): `VillagerArchetype::Create` calls `villager_animate::SetFirstClip` right
  after `InsertMapObject` (the clip component, `StateClip` with the carried object first,
  `living::VillagerFirstClip`, then `SetAnim` from the start), so `VillagerIsReadyForNewAnimation` sees the first
  clip, as in the original. Tests: `test/test_villager_core.cpp` (`AVillagerIsMadeWithItsBornStatesClip`,
  `AScriptStateInTheCreatingTurnPlaysItsClip`). The per-frame first clip of `UpdateVillagerAnimations` is gone.
- **The impression**: `villager_reactions::UpdateHowImpressed` (`VillagerReactions.cpp`), called by the slot's
  `AddReaction` and by `villager_shield::SetupReactToMagicShield`; the pure rules in `Magic/Impressiveness.{h,cpp}`
  (`ImpressiveValue`, `ReactionMultiplier`, `ShieldImpressiveValue`, `TownShare`, `ImpressionAlignment`); the
  belief through `town_stores::AddToBelief` and `town_belief::AddToBoredomMultiplier`; the alignment through
  `effects::alignment::UpdateForImpression`. The initiator's own value is a missionary's
  (`villager::MissionaryImpressiveValue`), a miracle's, a creature's, a scaffold's or, for any other object (a thing
  thrown from the hand among them), the object's (`magic::ObjectImpressiveValue`); a town, a script marker or a dance
  impresses by nothing, and the belief, alignment and boredom parts still run. The scaffolds' own pass calls it too,
  without drawing the belief (`ECS/Scaffolds.cpp`). openblack differs: the predators write a villager's life straight
  instead of through `Villager::SetLife` (`TargetPounce` and `ProcessDownedVillagers`, `src/ECS/AnimalPredators.cpp`), a fix row. openblack's shields make 13, 35 and 36 with the spell as the initiator
  (`magic::spell_shield`), so both shield miracles take the override. The town's last aggressor is
  `components::Town::aggression.lastAggressor`, none before any attack, so an attack in the first turn counts
  (2026-10-24; test `ImpressionTest.AShieldReadsTheTownsLastAttackerWhateverTheTurnAndNobodysMatchesATownNeverAttacked`). `DistanceChangeToBelief` is in
  `Magic/Impressiveness`. Tests: `test/test_magic_foundation.cpp`.
- `ECS/Effects/EffectValues.cpp` and `ECS/Fire/FireEffect.cpp` use `gutils::GetDistanceInMetres(vec3, vec3)`, which
  truncates the positions to 16.16 each time. As long as openblack stores the positions in float, that can lose one
  unit (0.15 mm) relative to a stored MapCoords.
- The pure `SameTypeSwitch` (`Reactions.h`) reads the type table's +0x04; openblack's port once registered only 7 and
  12, the original has one table for every Living class.
- **A shield's fall** (raffclar's code, read against v1.42 on 2026-10-31: `SetupGoAndHideInNearbyBuilding` 0x763CB0, 236 `GoAndHideInNearbyBuilding` 0x763D90, 237 `LookToSeeIfItIsSafe` 0x763F80 and 240 `PanicReaction` 0x76B1C0 agree with bw1-decomp's C and with his, but for one thing: the hiding states read the keep-away distance (+0x40) and the look-about reach (+0x2C) from `GetReaction()`, the reaction the villager follows now, which after a fall is 36 itself; his read the row of the reaction it watched before. openblack reads the row of the reaction it follows (`villager_cover::HidingReactionKind`, its slot's, else the kind it set off with), so a predator's flight, where the two are the same, is unchanged): the row `k_ReactToMagicShieldDestroyed` of
  `villager_reactions::SlotTypeOf` with `ReactToMagicShieldDestroyedPriority`; the set-up, the hide, the look about and
  the panic in `ECS/Systems/Implementations/VillagerCover.{h,cpp}` (rows 236, 237 and 240 of `k_VillagerStateTable`,
  236 and 237 keeping the original's ReactionValidate), his pure rules in `ECS/VillagerHidingRules` and the states in
  `components::VillagerHiding` / `VillagerPanic`. What it hides from and goes on watching is the object of the reaction
  it followed, of whatever kind (`villager_reactions::FollowedReactionOf`, read before `AddReaction` drops it), as the
  original keeps the old ReactionObject (2026-10-21; it was only the shield it was amazed by).
  His tree has no validate on 236/237, so his villagers hide 15 to 30 s whatever they watched: a candidate for the "fix
  vanilla bugs" switch. Tests: `test/test_villager_hiding.cpp`. Testbed: `miracles.shield_fall_hide`,
  `miracles.shield_fall_panic`; debug: Magic > Running > Break on a shield.
- **A creature, reaction 6**: the pure rules in `ECS/Effects/VillagerCreatureRules.{h,cpp}`
  (`effects::creature_reaction`: `VillagerPriority`, `TurnsToReact`, `TurnsBeforeReactingAgain`, `SetupState`, the
  inspection's `PerformInspectStep` / `ApproachFurther` / `ApproachStep` / `MetresToWholeDistance`, `RespectWalkGoal`,
  `FacingAway`, `FleeDistance` / `FleeGivesUp`, `StepFromAttitude`), pinned to the exe's numbers
  (`test/test_villager_creature_reactions.cpp`); the slot row `k_ReactToCreature` of `villager_reactions::SlotTypeOf`;
  the setup, the priority, the two turn counts, the states and UA in `ECS/Systems/Implementations/VillagerCreature.{h,cpp}`
  (`villager_creature`), the rows 146-148, 155-157, 159, 160 and 167 of `k_VillagerStateTable` (167's validate
  `MoveTowardsCreatureValidate`); the look's distance and turns in `components::VillagerInspection` (kept apart from
  `VillagerHiding`, which the original shares them with), the respect counter `LivingAction::turnsUntilStateChange`.
  The setup's object is set before the state changes by the `AddReaction` overload that takes it (the other setups
  still store theirs after, as their originals do). The town's record is `town_creature_attitude::CreateRecord` /
  `AttitudeOf`; the creature's last action carried out is `CreatureMindState::lastCompletedAction`, its current action
  and the thing it acts on are those of the plan it holds, `CreatureMindSystemInterface::HeldPlanOf`
  (`mind_detail::HeldPlan`: the planner's plan, else the idle mind's activity as a plan of the row it counts as, done
  to the step's thing or itself; none once a plan has ended); its invisibility `components::CreatureSpells::invisible`; `IgnoredByVillagers` is
  taken as always 0 (nothing sets it in the game). The creature's arena is `CreatureFightSystem::ArenaOf`. The walk's
  arrival test is `living_turn::WallHugMoveToResult`. The inspecting clip is `villager_animation::InspectCreatureClip`
  with the look at the hand within 8 metres of the reaction's object (`k_InspectLookAtHandWithin`, the distance from
  `VillagerAnimate.cpp`'s `ReactionObjectDistance`); the
  respect's is `RespectCreatureClip`. Every creature makes its reaction as it is made (`CreatureArchetype::Create`,
  through `effects::reactions::CreateReactionBeforeFiling`), so these paths run wherever a creature is
  near villagers. Debug: Path Finding, the selected villager's "Reaction to a creature" (closed): its states, the creature it
  reacts to, its look and its town's attitude to each creature it keeps a record of.
- **A new building, reaction 15**: `abodes::Built` makes it for its town's player; the slot row `k_ReactToNewBuilding`
  of `villager_reactions::SlotTypeOf` with `ReactToNewBuildingPriority` and the setup `SetupReactToNewBuilding`
  (`VillagerReactions.cpp`); `villager_reactions::AddMoveToPosReaction`; the reaction's dance
  (`effects::reactions::Reaction::dance`), deleted with it by `ReactionSystem::Remove`; 203 is
  `villager_dance::DanceWhileReacting` (`VillagerDance.cpp`). Tests `NewBuildingReactionTest.*`
  (`test/test_villager_reactions.cpp`). Debug: the Villagers window shows a reaction's dance and its dancers. Not
  ported: AddReaction's leaving of a dance (left as it was until the scripts' dances bring their dancers back).
- **Reactions that only impress** (34, 39, 40; 2026-10-22): `effects::reactions::BeliefButNotReaction` is the
  type table's flag; the villagers' handler sends them to `ImpressOnly` (`VillagerReactions.cpp`): available for the
  reaction, the standard turns before reacting again, `Records`, then `UpdateHowImpressed` when it records. The
  handler returns `CellWalk::Leave`, and the spread (`effects::reactions` `Spread`) leaves the cell; the animals'
  and creatures' handlers always return `Next`. Tests: `VillagerReactionsTest.OnlyThreeTypesOnlyImpress`,
  `AFirePutOutByMagicWaterIsOnlyRecordedAndEndsTheCell`, `AVillagerNotAvailableLetsTheOnlyImpressingReactionGoOn`,
  `ReactionSpread.ALivingThatEndsItsCellLeavesTheRestOfTheCellOut`. Debug: the villager trace's "impressed only"
  lines; the reactions themselves are in the Miracles window's reaction list.
- **A villager that cannot be reached** (every type; 2026-10-22): `ImpressOnly` runs first for every reaction.
  Available for it (`villager::IsAvailableForReaction`, the whole one) and
  `villager_reactions::IsAvailableForBeliefButNotReaction(flag, villager::IsReachable)` true, it records the type by
  `TurnsBeforeReactingAgain` (a slot row's own; `villager_shield::TurnsBeforeReactingToShieldAgain`;
  `villager_reactions::BurningObjectTurnsBeforeReactingAgain`; else the standard ones), impresses when it records and
  returns `Leave`; else the reaction keeps its own course, unchanged. openblack's `IsReachable` reads "held back from
  a change of state" as in the hand (`fire::traits::InHand`), which is what the flag is: only the hand's add sets it
  ([villager life](villager-life.md)). Tests:
  `VillagerReactionsTest.OnlyImpressedWhenTheTypeOnlyImpressesOrTheVillagerCannotBeReached`,
  `AVillagerAtHomeIsOnlyImpressedByAnyReactionAndEndsTheCell`, `AVillagerAtHomeNotAvailableLetsTheReactionGoOn`,
  `ABurningThingInTheHandIsReactedToAgainOnlyFarAndStill`, `AShieldIsReactedToAgainByItsTownsLastAttack`. Debug:
  the same trace lines. The shield's own spread to a villager that can be reached (`villager_shield::ApplyReaction`)
  records by the same `TurnsBeforeReactingToShieldAgain` at the spread's distance, as the original's spread does for
  every type. Test:
  `VillagerReactionsTest.AShieldsSpreadWaitsTheShieldsOwnTurnsBeforeReactingAgain`.
- **Who the spread offers a reaction to** (every type; 2026-10-26): `ImpressOnly` (`VillagerReactions.cpp`) asks
  the whole `villager::IsAvailableForReaction(villager, type)` first, and a villager not available is passed over
  (`CellWalk::Next`) for every type, the miracles' ones too, as the original
  ([Who the spread offers a reaction to](#who-the-spread-offers-a-reaction-to)). Before, a miracle type (fire,
  teleport, death) went on to its own `ApplyReaction` with only the short check (the final state's column, not held
  or thrown): a corpse playing its dying clip (TOP 23, FINAL 15) took a fire up. The miracles' own short check stays
  after it. Test: `VillagerFire.ADyingOrDeadVillagerTakesUpNoFire`. Debug: the villager trace (no `SetTopState ...
  -> 215` on a corpse).
- **The reaction's own turn** (`Living::ProcessReaction` 0x5F1270): `villager::ProcessReaction` runs the slot
  types' (`ProcessSlotReaction`) and the shield's (`villager_shield::ProcessReaction`, with the
  shield's turns to react and their two draws, [shields](../magic/shields.md#the-villagers-reactions)). Tests:
  `VillagerShieldPure.TurnsToReactToAShieldDrawFourThenFifty`,
  `VillagerReactionsTest.AShieldsReactionEndsAfterItsTurnsToReactUnlessItsTownWasJustAttacked`. The fire's, the
  teleport's and the mourning's still end only through their own states and validate.
- **A fire** (the detail is on [Fire](../world/fire.md#joining-and-choosing-a-fire)): a villager reacting to a fire,
  or beating one, turns to it a step of an eighth of a turn a turn and decides or strikes only once it faces it
  (`villager_fire::ReactToFire`, `PutOutFireByBeating`); a villager on its way to worship walks round the fire and goes
  on; the will to fight is `fire::WillFightFire` (`src/Fire/FireModel.cpp`), pinned to the oracle. Tests
  `VillagerReactionsTest.AFireReactionTurnsToTheFireAStepATurnBeforeItDecides`,
  `BeatingAFireTurnsToItBeforeTheFirstBlow`, `OnItsWayToWorshipAVillagerWalksRoundAFireAndGoesOn`,
  `NotOnItsWayToWorshipAVillagerStopsAtAFire`, `FireModel.AVillagerFightsAFireAsTheOriginalGameDecides`.
- **A script's reaction, the two branches no shipped script reaches** (2026-10-28): the only reaction a challenge
  script makes is CreatureGuide.txt's `attach reaction Guide 0` (`refs/chl/CreatureGuide.txt:93`, kind 0,
  FLEE_FROM_OBJECT, the row described in [A script's reaction](#a-scripts-reaction-create_reaction-remove_reaction_of_type)),
  and no land script command takes a reaction kind, so neither branch below is ever run in the game:
  - kind 255: the original makes it, reading its row from the land-avoid grid past the reaction table, values the
    land's run-time grid holds and nothing pins; openblack refuses it (`CreateReaction` in `src/CHLApi.cpp`);
  - reaction 6 (a creature's) with an initiator that is not a creature: the original's setup and its turns before
    reacting again cast the initiator to a creature without a test, so it would fail there; openblack takes nothing
    up and reacts again at once.
- Porting decisions: [Port decisions: beings](../project/port-decisions-beings.md).

- **The shield scene's late amazement** (2026-10-26, `miracles.shield_people`): with joint 249's code a villager that
  ends its look at the shield at turn 123 was sometimes reached by the shield's reaction that same turn, and sometimes
  only at 139, when the reaction list's cursor came round to it again; the order the list's reactions are offered in
  was rewritten with joint 250 (a reaction spreads over as many cells as its initiator's power gives), and since then
  the scene goes the same way in every run (eight runs on 251 and 252, alone and in a batch: 139, the cursor's turn). Its
  reference is from joint 252 and the joints watch it.

### Test hooks

None of its own; the throw is shown by the testbed scenario `physics.villagers_watch_throw`.

## Pending

- **A creature in a fight's arena** (UA): openblack stops the villager reacting, as the original does first, but the
  original then takes up the arena's own reaction 19 (`ReactToFight`, made by each arena as it is made) through
  `Villager::SetupReactToFight` 0x766E30 and its states 143 / 144 to watch the fight. openblack's script arenas now
  make it as they are made (`FeatureScriptCommands::CreateArena`, as `GArena::GArena` 0x4246F0: the arena its
  initiator, no player, not started, the reaction's own radius, 80; it spreads from the arena's own map place as the
  script gave it (`Arena::at`, the original's initiator `Pos`, bw1-decomp Reaction.cpp:109 and 515),
  `MapPosOf`); the creature fights' arenas are not those entities yet, so no fight is "on" in them and the villagers'
  priority for it stays 0, as the original's does while no fight is on. The take-up waits for the fight's arena. Whether the
  original's creature +0x10A0 is set for both fighters (openblack's `ArenaOf` answers for both) is not read.
- **A creature's current action**: the original reads mind +0xF60, the action of the plan it holds. Every end of a
  plan (finished 0x475450, failed 0x475730, reconsidered 0x4E1570) clears it through StopWhatIAmDoing →
  `CreatureAgenda::Clear` 0x475A86 (+0xF60 = 0, +0xF58 = 0, +0xF50 = 0x28), and every activity is a plan there (the
  agendas are built from +0xF60 only: 0x4734F8, 0x4C453B, 0x4F15C7). openblack's idle mind stands in for the planner
  there, so its activity is taken as a plan of the row it counts as (`ActionForActivity`), for that row's desire;
  an activity with no row holds no plan, which the original never shows.
- **Precision of the walks**: the walk's goal is kept in metres (148's and 155's whole map units go through a float
  and back), as every MapCoords walk's is in openblack.

- **The flight from predators, the rest**: `FleeingFromPredator` returns 0 for a reaction object that is not an
  animal, as the exe's state does; what that return does in the original's state loop is not read (no such object
  makes the reaction). Only an animal's and a creature's movement are known to the coming-at test; a walking villager
  still counts as still.
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
- **The land under the thing**: `GetHeightAt` at the truncated map point, not compared bit for bit with `GetAltitude`
  0x803090.
- **The movement of a villager as a reaction's object** (its step): no reaction a villager runs from has one yet.
- **The flag +0xB4 & 0x400**: who sets it (perhaps only a saved game's read of the whole object); `TestSpecial` is not
  ported (queued fix).
- **The scaffold's site with no building** (openblack): where the original would read a gone building and crash,
  `ScaffoldSiteWant` gives 0 (no town, no desire).
- **The fire, teleport and mourning setups** do not call `UpdateHowImpressed` yet (queued fix).
- **The creature's impression** (`Creature::UpdateHowImpressed` 0x4F2780): not ported (queued fix).
- **The first clip of a loaded villager**: when saved games are loaded, each villager resolved runs
  `villager_animate::SetFirstClip` in its loaded state, as `Object::ResolveLoad` 0x63A020 calls vt +0x658 first (with
  the save loading).
- openblack differs: the predators' life writes (a downed villager's 0.05, an eaten one's 0) do not go through
  `Villager::SetLife` 0x756B40, so a town's injured count does not see them; the original's do (vt +0x5B0 at 0x5EC491
  from `Animal::TargetPounce` 0x4190C6, and at 0x76B3C2 in `Villager::BeingEaten`) (`TargetPounce` and
  `ProcessDownedVillagers`, `src/ECS/AnimalPredators.cpp`; `ecs::life::SetLife` is in `src/ECS/Life.cpp`), a fix row.
