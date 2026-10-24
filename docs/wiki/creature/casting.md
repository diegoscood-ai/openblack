# The creature casting miracles

## Summary

A creature casts a miracle the way it does any planned action: the planner picks a cast row of the action table whose
"may it cast" test passes, the row's construct builds a short agenda (an emote now and then, going near the target,
backing off, turning to face it, a gesture, then the cast itself), and the cast sub-action takes the casting pose and
casts as the pose's loop begins, holding the miracle three seconds. A cast that is not learnt well enough fizzles and the
creature shows it the next turn; a cast it cannot pay for, or that the miracle refuses, makes it hungry. Food, wood and
water are cast "from above": two beams go from the hands to a point above the target and the miracle falls from there.
The creature spells (freeze, itch, compassion and the others cast on a creature) have their own particle rules.

openblack casts at an object as the original does, with the original's constructs, predicate, fizzle, hunger, hold,
release, body cost, beams and the casts from above; the cast at a point (sub-action 40) has no action that builds it
yet. The creature spells' particle rules are ported bit for bit against the exe; the creature's surface walk is not.

## The original

The steps of a cast, as read in the v1.42 `runblack.exe` (with the 1.20 symbols). Tags: [V] read at the address given,
📼 1.20 only likely, [?] unknown.

#### Casting a miracle: the original's steps
<a id="casting-a-miracle-the-originals-steps-w120-read-2026-10-09"></a>

##### The sub-action

- A cast is the creature sub-action 39 (0x27) "CastSpellAtObject" or 40 (0x28) "CastSpellAtPos". The static table is
  built by the `CreatureSubAction` initialiser 0x4F8D70, entries at 0xBE2188 and 0xBE2218, 0x90 bytes each.
  `ProcessSubAction` 0x4FEDA0 finds sub-action n's name at `0xBE0B98 + n × 0x90` ("SubAction %s", 0x4FF0C4..0x4FF0DD)
  and dispatches its stage function at `0xBE0BE8 + (n × 9 + stage) × 0x10` (0x4FEE21..0x4FEE29, 0x4FF0E2..0x4FF0F9):
  Initiate / Perform / Finish at the name +0x50 / +0x60 / +0x70 [V]. (The same name is `0xBE0C28 + (n − 1) × 0x90`, the form once used, which gave numbers one too low.) 38 (0x26) is DestroyObject, 42 is
  "DancingDuringEditor", 43 "CastSpotVisual" (0x4FAA16 → 0xBE23C8) [V]. Other numbers used here: 0 Pickup, 1 Discard,
  6 TurnToFaceObject, 7 TurnToFacePos, 10 IndividualAction (play an animation), 0x3D (61) GoNearObject, 0x58 (88)
  Gesture, 0x87 (135) GetAwayFromObject. `EatCreatedObject` shares `Eat`'s functions, `ThrowAtPos` shares `Throw`'s.
- The stage functions return 0 (stay), 1 (failure: `FinishActionUnsuccessfully` "Stopping because SubState returned
  Failure", 0x4FF18E), 2 (next stage) or 3 (abort) (0x4FF0FF..0x4FF199). One stage function is called a turn
  (0x4FF0F9..0x4FF17D): 2 advances (fn_004FF570), and the loop goes on only past an empty stage slot (0x4FEE4A) [V].
- Each cast action builds its own agenda with the magic type as its argument (the action table: construct of action A
  at `0x9D1698 + A × 0x50`, its argument at +0x10, its name at −0x20, as `ConstructSubActionsFromActionAgenda`
  0x4D36A0 calls it; the table is made by `crt_xc_fn_CreatureAction_00491B90`). 0x49E190 is action 35
  HelpRepairHouse (pick up, go near, face, drop, Repair, DestroyObject), not a cast; 0x49E460 is 36 BringToTown,
  0x49E470 37 PutOutFire. The only `AddSubAction(0x26)` is the house repair's DestroyObject [V].
- The cast is the agenda's main sub-action (`AddMainSubAction` 0x4FF3A0, from fn_004FF4B0: it sets agenda +0x1C, mind
  +0xFC4, to its index; reaching it runs fn_00474C50, 0x4FEE8F..0x4FEE99, whose purpose is [?]). The agenda entry is
  0x60 bytes (entry n at agenda +0x30 + n × 0x60; the agenda is mind +0xFA8): +0x30 the sub-action, +0x34 the object
  reference, +0x38 an integer, +0x3C a point (3 dwords), +0x48 a float, +0x4C a magic type, +0x68 / +0x78 the two
  member functions. `SubArgumentObjectIntegerFloatAndSpell::SetArgumentOfSubActionAgenda` 0x4FF7F0 writes +0x34,
  +0x38, +0x48 and +0x4C [V]. fn_004FF4B0 sets pose +0x38 = 41 (0x29, the cast pose; 0x4FF4E8) and hold +0x48 = 3.0 s
  (`40400000`, 0x4FF4EF) for every cast at an object; the Food and Water constructs take pose 47 (the scatter pose) and
  the same 3.0 s ([below](#casting-from-above-the-food-wood-and-water-rows)). Casts at a position use
  `SubArgumentPointIntegerFloatAndSpell` (vtable 0x8CF2D8, built at 0x49CC19, 0x4A0178, 0x4A1FF1) [V].
- **Initiate** 0x500B70 starts the casting body action (`fn_00484310` with the sub-action's +0x38, 0x500B98) and sets
  the hold counter (Creature +0x58) to 0. fn_00484310 draws `GameRand(2)` (Creature3D.cpp line 0xBB1, when the 3D body's
  creature +0x4834 is set; `LocalRand(2)` without one) and calls `LH3DCreature::StartStaticAction(41, (+0x4A90 == 0) ?
  r : 0)` 0x4843A0, which starts at animation 41 − 1 = 40 (0x4843B3) [V]; the draw picks the mirrored variant
  ([the pose](#the-pose)).
- **Perform** at an object 0x500BC0 or at a position 0x500CD0 waits while `LH3DCreature` +0x4994 is not 3 (the body
  action's loop has not begun); then, when the magic (+0xFF4) is not NONE, it sets mind +0x21F4 = 1 and calls
  `CastSpellOnObject(magic, object, 10.0, 0)` (0x500C4E..0x500C63) or `CastSpellAtPos(magic, position, 10.0, 0)`
  (0x500D80..0x500D9C). A failed cast returns 1 (the sub-action fails) and sets no hold. A cast that worked holds for
  `ftol(sub-action +0x48 seconds × (1000 / ms per turn))` turns: the division an unsigned whole `div` of 1000 by the ms
  per turn ([0xD01A38], 100, set by `GGame::Init` 0x54F4A5), `fimul` exact at the game's 24-bit FPU, `__ftol`
  truncating, stored as a **16-bit word** at Creature +0x58 [V 0x500C76..0x500CB3, the same at 0x500DB1..0x500DEE].
  Perform sets the hold even when the magic is NONE (0x500C30 / 0x500D07 jump straight to it: no cast, but a hold).
- **Finish** 0x500E10 counts the hold down (returning 0); at 0 it lets go of the cast (fn_004F81F0), ends the body
  action (fn_004848C0) and returns 2 once `IsPerformingBodyAction` is false, else 0, so the release and the end are
  repeated every turn until the body stops [V]. With N = the hold, the cast is on turn T, N counts down on T+1..T+N and
  the release is on T+N+1: at 100 ms a turn the 3.0 s give N = 30, the release on the 31st turn after the cast.
- Mind +0x21F4 is written 1 at the two Performs (0x500C32, 0x500D0D), at 0x5004C9 (another Perform sub-state), 0x502ADE
  and 0x50372D (dance sub-states), and 0 at 0x475B98 in `StopWhatIAmDoing`; it is never read [L: no reader found in
  `.text`; the users of the block from +0x21D0 (0x47A2EE, 0x47DA6B, 0x47E708, 0x47EEA0, 0x4C6201, 0x4D84F8, 0x4E086B,
  0x4E09B2, 0x4E0B78, 0x4F885A, 0x5E6E57, 0x4CE407) show no read of it; a whole-mind copy is not ruled out [?]].
- `CastSpellAtPos` is called only from 0x500D9C. `CastSpellOnObject` has two other callers: 0x500C63 and the fight's
  CastStart state at 0x4853AA ([below](#the-fights-cast)). The float the callers pass (10.0; the fight 2.0) is never read
  (in `CastSpellOnObject` nothing reads its slot; in `CastSpellAtPos` the slot is reused for the magnitude, 0x4F8000);
  the int (0) goes to `DoPostSpellCastThings` [V].

##### The constructs

The three cast constructs and the rows they build [V] (action: construct, argument = magic type):

| Style | Construct | Actions |
|---|---|---|
| Lightning | 0x4A63B0 | CastLightningBolt 45 (4), PU1 99 (5), PU2 100 (6) |
| Helpful | 0x4A6910 | CastHealSpell 101 (0xA), HealPU1 102 (0xB), MagicFoodPU1 98, MagicWood 104 / 248 / 302, Teleport 182, ShieldPU1 183, PhysicalShield 184, Warming 136 / Cooling 137 (argument 0) |
| Playful | 0x4A6BE0 | CastMakeCreatureFreeze 105 (0x1A) .. Itchy 120 (0x29): all 16 |
| (a fourth) | 0x4A6650 | CastTornado 103, CastExplosionPU1 185, PU2 186 |

Other cast constructs, not read: MagicFood 0x49FDC0 (read below, for Food), MagicWater 0x4B7800 (read below, for water
on itself).

The forest's construct **`ConstructSubActionsForCastMagicForest`** 0x4A0010 [V], the only builder of the cast at a
point (sub-action 40, [below](#casting-at-a-point)) among the rows read:
1. 0x3D GoNearObject, ObjectAndFloat(target, h × 2.5, `__real@4@4000a000000000000000` at 0x8C581C; h through vt +0x42C
   of `CreatureAgenda` +0x1514), no face, the look `LookWhileGoingTowardsObject` 0x4D0BD0 (0x4A0017..0x4A009A);
2. 6 TurnToFaceObject, ObjectAndFloat(target, 0.1), the kind face 0x4C8730 and the look 0x4D16D0 (0x4A009F..0x4A0106);
3. the main sub-action 0x28 (40, `AddMainSubAction` 0x4FF3A0 at 0x4A01C4) with PointIntegerFloatAndSpell: the point is
   the target's MapCoords as the agenda is built (its x and z × 1/6553.6, its altitude + `GetAltitude`,
   0x4A010B..0x4A0154), the integer 0x2F (47, the scattering loop), the float 4.0 (`40800000`) and the spell 0xD (13,
   Forest, fixed in the construct, not the row's), with the cast face 0x4C87F0 (Grimace) and no look.
No emote, no back-off, no gesture.

##### The attack, shield and impress constructs

Read in v1.42 (2026-10-18) [V], for the rows imported from raffclar's #467 and #469. Faces are the `SetFaceForAction*`
member stored in each sub-action's face argument (Anger 0x4C86D0, Compassion 0x4C8730, Playfulness 0x4C8780, Smile
0x4C87D0, Amazed 0x4C8830; 0 = none); h = the creature's vt+0x42C; every turn is sub-action 6 with
`SubArgumentObjectAndFloat(target, 0.1)`; draws are `GameRand`.

- **The fourth construct 0x4A6650** (CastTornado 103 (0x12), CastExplosionPU1 185 (8), PU2 186 (9)): on
  `GameRand(5) == 0` action 0xA, animation 0x35, **Anger**; 0x3D GoNear 5h (0x8AB6E4 = 5.0), Anger; 0x87 GetAway 2h
  (`fadd st0, st0`), no face, looking at the object; the turn, Anger; the gesture 0x58 when `GMagicInfo[type]` +0x2C
  is not 0; the cast.
- **LightningStorm 0x4B0AA0** (95, 0x11): the same steps as the fourth construct, the type fixed to 0x11. One
  construct for both is right.
- **Fireball 0x49F7D0** (46 (1), PU1 96 (2), PU2 97 (3)): the clear area five 2D radii wide (`FindClearArea`, the
  action fails without one); 8 to within 2h of it, looking where it goes; on `GameRand(5) == 0` action 0xA, animation
  0x35, **Amazed**; GetAway 2h only when the object is living (vt+0x3C4); the turn, Anger; the gesture; the cast.
- **Explosion 0x49FB30** (47, 7): on `GameRand(3) == 0` action 0xA, animation 0x35, **Amazed**; GoNear 100 (0x8CF1D8),
  Anger; GetAway 2h; the turn, Anger; the gesture; the cast of 7.
- **Shield 0x4A1540** (54, 0x13): for a plan about nothing (+0x844, the agenda's belief about what the plan is for,
  is 0; 0x4A1546) it builds the idle agenda instead; else the turn at the object (+0x848) with **Compassion**
  (0x4A156D) and the cast of 0x13, from where it stands.
- **CastImpressiveSpell 0x49E8C0** (40): GoNear **4h** (0x49E8FD, 0x8AB418 = 4.0), no face, looking where it goes;
  GetAway 2h; on `GameRand(2) == 0` the point 0x51 at the object for 1.0 s, **Amazed**, noted as pointed; on
  `GameRand(3) != 0` action 0xA, animation 0x34, **Playfulness** (0x49EA90); the turn, no face; then of the four
  `impressiveSpells` (0x9D7CFC: 10, 16, 11, 24) the known one (`KnowsAction` 1) with the oldest
  `ImpressiveSpellDoneWhen`, the last of equals (`ja` skips only a later one), stamped with the game turn, and its
  cast if there is one; last, when it did not point first, the point for 1.0 s with a **Smile** (0x49EC02).
- **The looks** (read 2026-10-18 [V]; each `AddSubAction` takes the look struct pushed second, its +0 the member):
  every 0x3D GoNear has `LookWhileGoingTowardsObject` 0x4D0BD0 (0x4A6756, 0x4B0BA6, 0x49FC0E, 0x4A6A3D, 0x49E933);
  every 0x87 GetAway has `LookAtObjectArgument` 0x4D16D0 (0x4A67D8, 0x4B0C28, 0x49FC96, 0x4A6AB3, 0x49F9D6, 0x49E9BB);
  every turn 6 has it too (0x4A684D, 0x4B0C9D, 0x49FD0B, 0x4A6B28; Fireball's and Impress's through a stack slot,
  each traced: written at 0x49FA0A / 0x49EAF7 with the three arguments of `operator new` pushed, read back as
  `[esp+0x34]` into the look struct's +0 at 0x49FA4E / 0x49EB3C, the face struct holding Anger / none) but the
  **shield's**, whose look struct is zeros (0x4A15AC..0x4A15C3; its face struct holds Compassion). Fireball's walk to
  the clear area has `LookWhileGoingTowardsPoint` 0x4D0C80; the impress points 0x51 have `LookAtObjectArgument`. The
  emote 0xA, the gesture 0x58 and the cast have none. The food and water constructs
  ([Casting from above](#casting-from-above-the-food-wood-and-water-rows)) do the same: CastMagicFood 0x49FDC0's GoNear
  `LookWhileGoingTowardsObject` (0x49FE5B, the look struct's +0 at 0x49FE60) and turn `LookAtObjectArgument`
  (0x49FED3); CastMagicWater 0x4B6EA0's (0x4B6F09, 0x4B6F7B) and WaterTreeForTown 0x4C31E0's on the instrument
  (0x4C3257, 0x4C32C9); their gesture and main cast none.

Each of the three adds, in this order (target = the agenda's object to act on, `CreatureAgenda` +0x848; h =
`Creature::GetHeight` 0x477F50 = the 3D body's +0x90 × 15.0 (`41700000`), the caster's own, through `CreatureAgenda`
+0x1514) [V]:

| # | Sub-action | Argument | Lightning 0x4A63B0 | Helpful 0x4A6910 | Playful 0x4A6BE0 |
|---|---|---|---|---|---|
| 1 | only if `GameRand(5) == 0` (CreatureAction.cpp lines 0x7EE / 0x818 / 0x834): 10 IndividualAction | Integer anim | 0x35 = 53 (angry) | 0x40 = 64 (kind) | 0x43 = 67 (playful) |
| 2 | 0x3D GoNearObject | ObjectAndFloat(target, d) | 50.0 (`42480000`, 0x8CF1DC) | 2h (`fadd st0,st0`) | h × 5.0 (`40A00000`) |
| 3 | 0x87 GetAwayFromObject | ObjectAndFloat(target, d) | h + 10.0 (`41200000`) | 2h | 2h |
| 4 | 6 TurnToFaceObject | ObjectAndFloat(target, 0.1, `3DCCCCCD`) | | | |
| 5 | only if `GMagicInfo[magic]` (0xD37D10[magic]) +0x2C ≠ 0: 0x58 Gesture | Integer = that +0x2C | | | |
| 6 | fn_004FF4B0: `AddMainSubAction(0x27 CastSpellAtObject)` | ObjectIntegerFloatAndSpell(target, 0x29, 3.0, magic) | | | |

- The 1-in-5 emote is drawn when the agenda is built. GoNear and GetAway share one d in the helpful construct
  (0x4A69F3); the helpful construct also calls `GetHeight` and the target's `Get2DRadius` and drops both (0x4A69A8,
  0x4A69D5) [V; no effect, L].
- Each `AddSubAction` carries two member functions (entry +0x68 and +0x78, 0x4FF33D..0x4FF376) [V]:
  - +0x68, the head's look (called by fn_004D03B0 at 0x4D0585 into fn_00482C90): GoNear's 0x4D0BD0 (the target's top:
    its place + altitude + `GetHeight`); GetAway's and TurnToFace's 0x4D16D0 (the same, but none when the target is the
    creature itself, or while moving, `IsMoving` 0x484300 = state 1, with the target nearer than 10.0); none for the
    emote, the gesture and the cast.
  - +0x78, a face: called by `ProcessSubAction` (0x4FF024..0x4FF09C) when the sub-action's counter mind +0x1D40C is
    above `ftol((1000 / ms per turn) × mind +0x1D410)` (unsigned; the counter is set to −1 at each new sub-action,
    0x4FEE9E, so it fires on the sub-action's first turn), then the counter = 0 and mind +0x1D414 += `GameRand(7)`
    (line 0x131); with no face nothing is drawn. The faces call fn_00480C60(face animation, ms) (3D body
    +0x49B0 / +0x49B4), chosen by mind +0x1D414 & 1:
    - 0x4C86D0 (the lightning emote, GoNear, TurnToFace): even 0x12 (18, Growl) 2000 ms, odd 0x11 (17, Grimace)
      1000 ms;
    - 0x4C8730 (the helpful emote, GoNear, TurnToFace, and the playful TurnToFace): even 0x10 (16, Smile) 2000 ms, odd
      0x19 (25, Aah) 1000 ms;
    - 0x4C8780 (the playful emote, GoNear): even 0x10 Smile 1000 ms, odd 0x16 (22, Puzzled) 1000 ms;
    - 0x4C87F0 (the cast, fn_004FF4B0 0x4FF4C2): always 0x11 Grimace 2000 ms;
    - none on GetAway and the Gesture.

##### Going near, backing off and turning

The phase functions return 0 (stay), 1 (failure), 2 (next) or 3 (abort). The 3D body's +0x4994 is its animation
state: 0 standing, 1 moving (`SetDest` 0x4841E9 sets it; `IsMoving` 0x484300 is `== 1`), others actions (3 the static
action's loop); `IsPerformingBodyAction` 0x4842D0 is `!= 0`, fn_004842E0 is `0 or 1`. The creature's moves: to an
object fn_0047A7A0 → `LH3DCreature` 0x483F40, to a point fn_0047A880 → 0x483FE0. 0x483FE0 returns only 0 (the point
not valid: fn_00483850 = fn_00483890(point, 7.1, `40E33333`) on `LandAvoid`), 1 (busy: the state not 0 or 1, or
+0x51EC set, or no walk animations 1 / 2) or 4 (started, or already walking to within a squared distance 0.01,
`3C23D70A`, of the route's destination RPFollow +0x64074, 0x4840D1..0x484115) [V]. The sub-action turn counter mind
+0x1BDC goes up in `ProcessSubAction` before each phase call (0x4FF0AB) and is reset to 0 only by the Gesture while its
drawing is below 1 (0x506F10) and by AddVillagersToDance (0x502D53) [V; a block write elsewhere, ?].

- **The move to an object**, 0x47A7A0(ref, object, d, 0) / 0x483F40 [V]: R = the object's `GetRoutePlanRadius(creature)`
  (vt +0x7C4) + `LH3DCreature::GetNavRadius` 0x480A60 (physical +0x6C / body +0x90 × body +0x5228 for a creature not
  flagged +0x12A8, else +0x5228; `Creature::Get2DRadius` 0x477F40 is +0x5228); d ≤ 0 becomes 0.01; timeout = 2 ×
  distance + 5000.0 (`459C4000`); 3D body +0x4990 = sqrt(body +0x90) × (1.4, `3FB33333`, when the required speed
  +0x483C equals the creature's +0x28 → +0x258, else 3.0) [V; its meaning ?]; mind +0x1C70 = ref; Creature +0x1214 = the
  object's place. Then 0x483FE0(point, max = R + d, min = R, timeout): min = min(min, distance − 0.05, `3D4CCCCD`),
  max = max(max, min + 0.001, `3A83126F`) (0x48411E..0x484174), then `RPFollow::SetDest`. To a point (fn_0047A880(ref,
  point, d, x)) there is no R: max = d, min = x. No sub-action here sets a speed: it walks at the last required speed
  [V for 0x5056F0, 0x5057B0, 0x47A7A0, 0x47A880, 0x483F40, 0x483FE0]. The radii are on
  [The nav radius and the route plan](body.md#the-nav-radius-and-the-route-plan).
- **GoNearObject (0x3D)**, Initiate 0x5056F0: an Object with flag +0x24 & 2 or `IsTree` gets the move to it (d = +0x48)
  at once; Creature +0x1164 = its own place; returns 2. Perform 0x5057B0 [V]:
  1. r = the Object's `GetRoutePlanRadius(creature)`, or the creature's own `Get2DRadius` for a thing that is not an
     Object.
  2. A flag-2 Object or a tree, `IsObjectInMap`, and state 0: done.
  3. `GetDistanceInMetres(creature, thing)` (the integer `hypotenuse` of the MapCoords, `ConvertWholeDistanceToMeters`)
     below float(`Get2DRadius(creature)` + r + d) × 1.1 (`3F8CCCCD`, x87): `StopMoving`, done.
  4. State 0 or 1: the move again, every turn (to the Object, or to the point with d); 0 fails, anything else goes
     to 6. (A `== 3 → done` branch exists, 0x50594E, but no move returns 3: dead.)
  5. Any other state: `IsDirectionClose(thing, 6)` (0x4774F0 → 0x477550: the game angle of body +0x84 against
     `GetAngleFromXZ`, an unsigned 16-bit difference with no wrap, below 0x800 / 6 = 341, or the same 10 m cell in x and
     z); not close: the move again (always 1, busy, in these states); 0 fails. Then 6. So the heading test changes
     nothing.
  6. A failure (0 from the move): fn_004D8DF0(ref), the thing's point pushed on Creature +0x1220 (count +0x1224),
     `FinishActionUnsuccessfully("Stopping cos navigation failed", 1, 1)`, returns 1.
  7. Stuck: if mind +0x1BDC % 50 == 0 (signed), Creature +0x1164 = here; then if Creature +0x25 & 4 is clear, the count
     % 50 == 49 and the distance from +0x1164 is below 0.01: when the mind's desire +0xF50 is below 40,
     `SuppressDesire(it, 10.0)`; returns 1. Else 0.
- **GetAwayFromObject (0x87)**, Initiate 0x508850 [V]: waits (0) while the state is not 0 or 1. dist = the same integer
  map distance; far = float(d + 1.2 (`3F99999A`) × the Object's `Get2DRadius`), 0 for a non-Object. `!(dist < far)`:
  returns 2. dist ≤ 0.0001 (`38D1B717`): the direction (1, 0, 0); else (the creature's LHPoint − the thing's), 3D,
  times 1 / sqrt((z² + y²) + x²) unless all three are 0. The point = creature + direction × far, x and z only (`ftol`
  × 6553.6). fn_004DFBE0(point, `GetHeight`): the clear-area search on Creature +0x3D8 with width = the height and range
  +0x400 = 250.0 (`437A0000`), a grid of `ftol(250 / 10 + 1)` = 26 cells centred on the point's cell
  (0x4DFCAB..0x4DFD2F), cells tested at ×10 + 5; a creature confined to an area fails when the found point is not
  within +0x11B4 of +0x11A8 (0x4DFC2C..0x4DFC50). None found: returns 1. Then the move to the point with (1.0, 0): 0 or
  1 returns 1; 4 returns 2. Perform 0x508B00: done once the state is 0.
- **TurnToFaceObject (6)** [V]: d = (thing − creature) MapCoords × 10.0 × 1/65536 in x and z; len = fn_006E8160 [L: the
  2D length]. Initiate 0x5012D0: len < 0.1 returns 2; else fn_004F00E0 (`StopMoving` then `StartTurningAction` to the
  angle of d, fn_007FAA50) and returns 2 if that returned non-zero, else 0. Perform 0x5013B0: 0 while the state ≠ 0;
  len < 0.1: 2; |wrap(angle − body +0x84)| (fn_007FAAF0) ≤ the double `3FD921FB60000000` (the float `3EC90FDB`, π/8):
  mind +0x1BD8 = `ftol(+0x48 × (1000 / ms per turn))`, returns 2; else turns again, 0. Finish 0x501520: 0 while the
  state ≠ 0; +0x1BD8 == 0: done; else decrement, then len < 0.1: done; not facing: turns again; 0.

##### Casting at an object

- **`Creature::CastSpellOnObject`** 0x4F7970: first it lets go of the cast it holds (fn_004F81F0). In a fight
  (`IsFighting` 0x47B1C0) the cost is `costToCreate / 10000` clamped to 0..1; with less stamina (`LH3DCreature`
  +0x4AAC) than that it returns 0 at once (0x4F7998.., 0x4F7A07 → 0x4F7E09; no hunger), else the stamina loses it and is
  kept to 0..1.
- **The fizzle**: `n` = `NumTimesMagicActionMustBeSeenBeforeItIsLearnt(magic) − 1`, at least 1; `r` = the sightings
  (mind +0x17D38 + 4 × magic) / `n`; when `r` is not above 1 and below 0.999 the mind's +0x21D0 becomes
  `MessUpCastSpellOnObject` (0x50A1B0), a belief about the object goes to +0x21E0, +0x21F0 = the magic,
  fn_004C9FE0(0x27, 1, 0, 0, 0) (the creature help request, [below](#the-help-request-on-a-fizzle)), the sightings go
  up by 1, and it returns 0 (0x4F7BA2).
- Then it measures the object's bounding sphere and an angle (`Get3DAngleFromXZ`) whose result it throws away. The
  magnitude is the object's `Get2DRadius` × 1.1, or for a creature (`CastCreature`, vt +0xA4) its 3D body's size × 15 ×
  1.1; for a magic whose `GMagicInfo` +0x28 (`spellSeedType`) is 2 (the fire seed) it is the caster's `GetHeight` / 10,
  at most 1. The cast data is that magnitude, the effect's initial chants (+0x74), the creature's timer (+0x6C,
  fn_005FB7B0) and −1 objects.
- Then the energy test fn_004F82F0 (0x4F7CEA) and the class's rule on the object (`GMagicInfo` vt +0x2C, 0x4F7CFE, the
  object and the caster): either failing goes to 0x4F7DE5, the hunger ([below](#only-two-refusals-make-it-hungry)), and
  returns 0.
- Then a process info of zeros (power 1, enabled), `UpdateSpellInfo(NULL, info)` (0x4F7D95),
  `InitialUpdateSpellInfo(object, the object's MapCoords, magic, info)` (0x4F7DA5), the cast through fn_005FB520 (on
  the object, or at its position when the seed's `castOnObject` is 0, fn_005FB490; `ret 0x14` at 0x5FB556) into
  Creature +0x1078, and fn_004F81B0, `DoPostSpellCastThings(spell, 0)` (0x4F7DC4): with the int 0 and a held spell,
  `MaintainSpell(spell, costToCreate)` (vt +0x58; `GMagicEffectInfo[spell +0xB4].costToCreate` at 0xCC66A8 + magic ×
  0x11C), whose result is dropped (`fstp st(0)`, 0x4F81B0..0x4F81E8). Both callers pass 0, so the creature always pays.
  It returns 1.

##### Casting at a point

**`Creature::CastSpellAtPos`** 0x4F7E20: the same fizzle first (MessUp `MessUpCastSpellAtPoint` 0x50A1D0, the point
kept at mind +0x21E4, the help request at 0x4F7FBA), then the release (fn_004F81F0), the same unused angle, magnitude 10
(0x8D161C, its own constant `41200000`, written into the argument's slot at 0x4F8000) or the fire seed's rule, the same
cast data, the energy test (0x4F8085) and the class's rule at the position (vt +0x30, 0x4F8098), the same hunger on
failure (0x4F8174..0x4F8193), `UpdateSpellInfo(NULL)`, `InitialUpdateSpellInfo(NULL, position, magic, info)`
(0x4F8134), the cast at the position (fn_005FB490) and `DoPostSpellCastThings` (0x4F8153). There is no fight branch.
Its `UpdateSpellInfo` call at 0x4F8126 lies in `CastSpellAtPos`, although the symbol ranges name the address
`CastSpellOnObject`+0x7b6. The point is the land's: x = `ftol(sub +0x3C × 6553.6)`, z = `ftol(sub +0x44 × 6553.6)`
(mind +0xFE4 and +0xFEC; 6553.6 is the float 0x45CCCCCD = 6553.60009765625 at 0x8AC400), height 0: any height the
sub-action holds is dropped [V 0x500D1F..0x500D72].

##### Only two refusals make it hungry

In both cast functions the energy test and the class's rule jump to `UnsuppressDesire(4)` 0x4DC2D0 (`[desires + 0xA8 +
4d] = 0`: the suppressed turns of HUNGER) and `MakeDesireFullyDominant(4, the creature info [Creature+0x28])` 0x4DC920:
only if `IsActivated(d)` (0x4DCDA0: the desire's flag, or the game flag 0x2000), the value (+0x148 + 4d) = its maximum
(+0x468 + 4d) and every other of the 40 values = info +0x274 (the species' floor), activated or not. No source is filled
and nothing is activated (that is the ...AndChangeSource variant 0x4DC9A0, not called here). The fight's stamina refusal
and the fizzle return 0 with no hunger [V].

##### The help request on a fizzle

fn_004C9FE0(0x27, 1, 0, 0, 0) is the creature help request: 39 `CREATURE_HELP_TYPE_ACTION_MESSED_UP` and 1
`CREATURE_MESSED_UP_ACTION_SPELL` (bw1-decomp `include/chlasm/CreatureEnum.h`: the help enum and the `CREATURE_MESSED_UP` enum), thiscall on the creature, the pushes at
0x4F7B68..0x4F7B79, the call at 0x4F7B83 (0x4F7FBA at a point) [V]. Its gates (0x4C9FE0..0x4CA0C0): the type row at
`0x9D9CA0 + type × 0x128` (built at run time, zero in the file); Creature +0x21C not below the row's +0x08; `g_game`
+0x205A28 not 1; only the local player's creature (`GetPlayer` against the player at `g_game` +0x205A59, 0x4CA036).
Types 12..25 (the phase messages) skip the next test; any other, 39 included, needs the creature's phase (+0x1268) at
least 13 (0x4CA05A) and `HelpSystem` (`g_game` +0x25005C) +0x4608 not 0. Then the 5th argument, the current action's
jump table 0x4CA1DC (mind +0xF60 − 9, for types 0x22..0x28), `HelpSystem::IsDialogueControlled`, Flags +0x24 bits
0x10 / 0x400, Creature +0x10C0 and the last-help turn [0xC82850]. `fn_004CA290(type, index)` is "not shown recently": a
list at Creature +0x220 + type × 8, the turns since it was shown ÷ (1000 / ms per turn) against 60 s (0x8C36A8), or 5 s
for type 0x29 (0x8AB6E4). Shown: `fn_004CA490` (a help-stack entry, `CreatureHelpStackEntry` 0x4CA6F0), [0xC82850] =
the turn, `fn_004CA240` (a per-type function from the run-time table, with `ShellExecuteA` imported in the same body);
otherwise `fn_004CA340` stacks it. No `GameRand` is called in 0x4CA240..0x4CA740; the per-type functions at row +0x18
and 0x9D9CAC + n × 8 are filled at run time and not read [?]. So it is presentation only, at most once in 60 s, for the
local player's creature from phase 13, with no draw [V].

##### The fizzle shown

0x509EC0 is the shared body of `MessUpCastSpellOnObject` 0x50A1B0 and `MessUpCastSpellAtPoint` 0x50A1D0 (both forward
their two arguments; the second, the magic, is not read, `ret 8`, L). It runs from fn_004F0370 in `ProcessState`
(0x47369D) on the turn after the failure, in place of choosing the next activity (vt +0x8C8) [V]. fn_004F0370 runs it
only when the plan is not valid or its agenda is spent (+0x8A4 ≥ +0x8AC or +0x8A8 ≥ 3), mind +0x1C14 == 0, Creature
+0x94 == 0, +0x384 == 0, `fn_004C74E0(+0x1BF8)` == 0 and +0x21D0 is set; it calls the member pointer +0x21D0 with
(+0x21E0 the belief, +0x21F0 the magic) at 0x4F049F..0x4F04B5, and after it sets mind +0x1C14 = 1 (a forced plan) and
+0x21D0..+0x21DC = {0, 0, 0, −1}. `ProcessState` calls it before `ProcessSubAction` (0x4737B0), so a cast that fails on
turn T shows its fizzle on turn T+1 📼 1.20 only.

- `ForceActivityAndForceAction(18 ManifestState, NULL, 23 CommunicateState, the belief, NULL, 1, 1)` 0x4C4450 (7
  arguments, `ret 0x1C`): `CreaturePlan(desire, action, belief1, belief2, belief3, FLT_MAX)` (FLT_MAX 0x7F7FFFFF at
  0x8CF33C; the constructor 0x4F1230, mangled `(DESIRES, ACTION, Belief*, Belief*, Belief*, float)`), then
  `ForceActivityAndForceAction(plan&, a6, a7)` 0x4C44B0: if Creature +0x94 is set, vt +0x998 [?];
  `FinishActionUnsuccessfully("stopping because new action forcing me", a7, a7)` (0x9D9744; through
  `FinishActionGeneric` and `StopWhatIAmDoing(a7, a7)`, whose `fn_0048F710(1, 1)` ends the body's states and calls
  `StopMoving`); the plan's +0x04..+0x2C copied into mind +0xF4C..+0xF74; `CreatureAgenda::ConstructSubActionsFromActionAgenda`(mind
  +0xF60) on mind +0x710, and only if it returns non-zero: `CopyPlanToPreviousContext(a6)` 0x4F1600 (a
  `CreatureContext` built with the dominant source and a6 as its int argument), `CreatureDesires::SuppressDesiresAfterDecidingOnActivity`(mind
  +0xF50) 0x4DC210 (the call at 0x4C4560, ecx = mind+8) and Creature +0x105C = 1. Nothing in 0x4C44B0, 0x4DC260 or `CopyPlanToPreviousContext` (`GetDominantSource` 0x4DE5D0, the `CreatureContext` constructor, `PreviousContextStack::Push`) zeroes a source's drive [V for those bodies]. Mind +0x1C14 = 1 is the same flag the leash's forced plans set (0x4C45AD). So a6 = 1 is the learning context's
  int (not decoded, ?) and a7 = 1 means "stop the body and the walk". Action 0x17 = 23 = `CREATURE_COMMUNICATE_STATE`
  ("CommunicateState" at info.dat 0x4D440; the action rows start at 0x4BD40 (row 0, L), 0x100 bytes apart: "MoveToPos" (1) 0x4BE40, "Fight" (21) 0x4D240,
  "FollowPlayer" (22) 0x4D340, "ShowPlayerAnObject" (24) 0x4D540); desire 0x12 = 18 =
  `CREATURE_DESIRE_TO_MANIFEST_STATE` [V].
- Then the sub-action agenda is reset (fn_004FF5C0: mind +0xFA8 +8 = 1, the counts 0, the 32 entries' +0x34 cleared,
  0x4FF5C0..0x4FF600), throwing away what action 23 just built 📼 1.20 only, animation 73 (`AddSubAction(10, 0x49)`),
  `GameRand(2)` (0x509F67), and on 0 animation 56 (`AddSubAction(0xA, 0x38)`) and `ChangeSource(0x30,
  GameFloatRand(0.3))` (`3E99999A`, 0x509FFA). Mind +0x1C14 = 1 (a forced plan).
- 0x4DC210 (size 0x50): for each of the 40 desires i, when `g_CreatureDesireDependency[desire][i] < 0` (the table at
  0xC85CD0, 40 × 0xB0: a 0x10 header and 40 floats; the row at 0xC85CE0 + 0xB0 × desire; `fcom` against 0.0 at
  0x8AA398), `SuppressDesire(i, −that × 240)` (240.0 at 0x8CFFB8), the diagonal included and no draw. `SuppressDesire`
  0x4DC260: turns = `__ftol((uint64)(1000 div [0xD01A38]) × seconds)`; only when the turns are above the counter (+0xA8
  + 4i, unsigned) it stores them and calls `LHStack<CREATURE_DESIRES,41>::SetToZero` (0x4F12B0, zeroing +0x0C..+0x2C) on
  mind +0x718 + 0x30 × i. In the game's data (the "Info" block's table at block offset 0x63278) only Compassion↔Anger
  (−0.86, −0.92) and Sadness → Compassion, Play (−0.1) are negative, giving 2064, 2208, 240 and 240 turns; positives:
  Compassion↔BeFriends 0.5, Tiredness → 23..37 0.3, rows 34..39 → Play 0.2; the diagonal is all 0 [V; L for the table's
  start in info.dat].

##### The fight's cast

`LH3DCreature`'s fight states are run by the dispatcher fn_004851A0 (`jmp [state − 2]×4 + 0x485D5C`; 0x20 → 0x4852A8,
0x21 → 0x485575, 0x22 → 0x4853C9), from `UpdateTime` 0x48256A before `UpdateBuffers` 0x48257F, and from `StateAction`
0x486BC0 [V]. The fight itself is on [Fights](fights.md).

- `CheckFightQueue` 0x489540, in stance (+0x4994 == 0x17), takes a queue entry (+0x4AC4) with bit 0x4000 (bit 0x8000 is
  a fight action, `StartFightAction`), keeps its magic type (entry & 0x3FFF) at +0x4B28, `StateSet(0x20)` and +0x49A0 =
  0x6F (111), and fires `HelpProfile::Trigger(0x26)` for the local player's creature (0x4896DA..0x489736).
  `StateSet(0x20)` 0x484F8F calls `StartDestruction`, checks anims 0x6E and 0x6F (`GetAnim(.., 1)`) and sets +0x52A4 =
  0, the time +0x47D0 = 0 and +0x5238 = 0.
- **CastStart** 0x4852A8 plays anim 110 (`+0x49A0 − 1`) into the slot +0x47C0, each update stepping
  `ftol(fn_0047F1C0() × ms +0x48B4)`: `fn_0047F1C0` 0x47F1C0 = `[0x9D06EC] × 0.6f` = 1.1f × 0.6f = 0x3F28F5C3 (0.66), or
  × 0.9f = 0x3F7D70A4 (0.99) when +0x49A0 is in 0x89..0x8C. At its end, `fn_00617A80(anim, time, step)` [?], then: with
  +0x5238 set (the fight being ended, set by `fn_004845F0` 0x4846AA, also written by `fn_004848C0` / `fn_004848F0`, L)
  it goes to CastEnd with no cast; else, only with its game creature (+0x4834) and an opponent (+0x528C): when the
  effect row's +0xF0 (`isAggressiveSpellWhichIsUsedInCreatureFightArena`; the table 0xCC6630, 0x11C bytes a row, so
  0xCC6720) is set, `CastSpellOnObject(magic, the opponent, 2.0, 0)`; else when +0xF4 (the defensive flag) is set,
  `CastSpellOnObject(magic, itself, 2.0, 0)`; else no cast. The arena call before it (`fn_004250A0`, when creature
  +0x10A0 is set) is empty: `fn_00425090` is `xor eax, eax; ret 8`. The result is ignored, +0x4B2C = 0 and it enters
  Cast (0x21).
- **Cast** 0x485575 adds the update's ms to +0x4B2C and, once that is above 2.0 (0x8AB478) or +0x5238 is set, lets go of
  the cast (0x4855B6) and enters CastEnd: the first update with more than 2 ms, one update at the normal speed [L: with ms = `ftol(phys+0x48 × ms per
  turn)`], not anim 111's length. `StateSet(0x21)` only checks anim 0x6F and sets the time to 0 (0x485021).
- **CastEnd** 0x4853C9 plays anim 112 at the same step, then `fn_00617A80`, +0x49A0 = 0, `StateSet(0x17)` Stance, and
  `CheckFightQueue` unless +0x579C == 2 (0x485446).
- In the game's data (`Scripts/info.dat`, 582066 bytes, one "Info" block at file 0x2C of size 0x8E186, the size of the 1.20 `InfoConstants`; `GMagicEffectInfo`, the flags at +0xE0 / +0xE4 of the file's 0x10C-byte rows from file 0x1130,
  block offset 0x1104, up to 0x3d28 where the 30 spell seeds of 0x180 bytes start) 12 spells are aggressive (FIRE 3500,
  FIRE_PU1 7000, FIRE_PU2 10000, LIGHTNING_BOLT 5000, its PU1 7500 and PU2 10000, BEAM_EXPLOSION 16000, its PU1 32000
  and PU2 60000, STORM 8000, STORM_PU1 15000, CREATURE_SPELL_WEAK 8000; the numbers are `costToCreate`) and 4 defensive
  (HEAL 6000, HEAL_PU1 9000, SHIELD 7000, CREATURE_SPELL_STRONG 8000); none is both, and the tornado (STORM_PU2), the
  physical shield, teleport and the other creature spells are neither. The file's row: `GEffectInfo` 0x24, debugString
  +0x24 (0x30), `costToCreate` +0x68, `useTribalPowerMultiplier` +0xBC (9 × u32, `Tribe::_COUNT` = 9), the flags +0xE0 / +0xE4, four help
  texts, `aggressiveAttackValue` +0x104 (Fire 0.60 / 0.65 / 0.69, Lightning 0.70 / 0.75 / 0.79, the explosion 0.90 /
  0.95 / 1.00, Storm PU1 0.80, Tornado 0.89, the ground flock 0.5), `computerCastDuration` +0x108 [V info.dat].
- The other fight site: 0x48B239 in the fight handler (`fn_0048A5E0`, in the range of the `fn_0048A580` symbol) calls
  `fn_0048B6E0` on the opponent (+0x528C) with (…, 1.0f, its own creature), then releases the opponent's held cast
  (`[+0x528C]+0x4834` → `fn_004F81F0`): the blow landing on a casting opponent [L meaning].

##### May it cast: the action's predicate

The action table's function slots (`crt_xc_fn_CreatureAction_00491B90`, 0x50-byte rows, row i at 0x9D1678 + 0x50 × i,
i the info.dat action row): +0x00 the name, +0x10 / +0x14 the predicate (a member pointer: function, this-adjust),
+0x20 / +0x24 `ConstructSubActions` (called at 0x4D36CB), +0x30 its argument, +0x34 the object filter (called by
fn_004E3880 at 0x4E38D6), +0x38 / +0x3C two more object filters (fn_004E38E0), +0x40 the town-attitude function
(`Town::UpdateAttitudeToCreature` 0x7438C1 calls [0x9D16B8 + 0x50 × i]). CastLightningBolt = 45 and CastHealSpell = 101
in both the table and info.dat (file rows 0x100 apart: 0x52240 − 0x4EA40 = 56 rows); the info.dat row's magic type is
at +0xA4 (Heal 10, Lightning 4, Itchy 41, Fireball 1, FireballPU1 2), and the exe's copy (stride 0x110) has the
learning types at 0xC6C53C / 0xC6C540, the magic at 0xC6C544 and the next field (info.dat +0xA8) at 0xC6C548 [V].

- There is no `CanCreatureCastSpell` symbol in the 1.20 symbols: the rule is an unnamed predicate at **0x4D2940**,
  swallowed by `crt_xc_fn_sentinelNegOne_CreatureMental_004D2930` (size 0xB; the real starts are 0x4D2940, 0x4D2A20,
  0x4D2AF0, 0x4D2B20, 0x4D2B80). It is the +0x10 predicate of 47 rows, every cast with an executor among them [V].
- 0x4D2940 (plan&, action), thiscall, `ret 8` [V]: the row's magic (0xC6C544 + 0x110 × action) must be 1..41 (`magic
  <= 0` or `>= 0x2A` gives 0 at 0x4D295D, 0x4D2966); share = (float)(uint32) sightings (mind +0x17D38 + 4 × magic) /
  `NumTimesMagicActionMustBeSeenBeforeItIsLearnt(magic)` (0x4F8CA0 = the info.dat times to see × CreatureInfo
  +0x37C, float), passing when above 1 (1.0 at 0x8AA390) or not below 0.5 (0.5 at 0x8AA3B4) (0x4D2999..0x4D29DE), so
  the rule is share ≥ 0.5 against the full times needed; a NaN share (0 seen, 0 needed) sets C0 and fails. Then, only
  if [0xD00E00] is non-zero and there is a player (`GetPlayer`, vt +0x1C), `GPlayer::IsMagicTypeEnabled(magic)`
  0x64C220 (the cheat +0x92C, or a holder count +0x970 + 4 × magic) (0x4D29E0..0x4D29FE); last the energy test
  fn_004F82F0 (`0.85` (0x3F59999A at 0x8CEFAC) − `ConvertSpellChantsToEnergyDiff` of the cost (0xCC66A8 + 0x11C × magic) >
  exhaustion), the same test `CastSpellOnObject` runs. [0xD00E00] (`GGlobal` +0x2D2E0, past `.data`'s raw bytes
  0xC3D000, its address held once in the image, at this read) is set to 0 by the GGlobal constructor (0x590F72) and
  never written again, so the player test never runs [V]; it sits in the block of flags 0xD00DD8..0xD00E04 with
  0xD00DE0 / 0xD00DE8.
- The power-up rows (96 FireballPU1, 97 FireballPU2, 98 MagicFoodPU1, 99 LightningBoltPU1, 100 LightningBoltPU2, 102
  HealSpellPU1, 183 ShieldPU1, 185 ExplosionPU1, 186 ExplosionPU2) use 0x4D2AF0: the development phase (+0x1268) ≥ 8
  (signed, `jl`) and 0x4D2940. SprinkleMagicWaterPU1OnCrops (325) uses 0x4D2940.
- Other cast predicates [V]: 0x4D2A20 CastImpressiveSpell (40): true when any magic of {14 Food, 10 Heal, 16
  StormWindRain, 11 HealPU1, 24 FlockFlying} (the list 0x9D7CF8..0x9D7D0B) is `KnowsAction(1, m)`, passes the energy
  test and has a share above 1 or above 0.5 (strict), the energy before the sightings and with no player gate. 0x4D2B20,
  the Teleport rows (182, 196, 216): 0x4D2940 and a distance from home (+0x1200, through fn_0074CD90 →
  `ConvertWholeDistanceToMeters`) above 150 (0x43160000 at 0x8CC7E8) [the distance's exact kind L]. 0x4D2B80
  (LookOutToSea, SitDownOnBeach): phase ≥ 3; not casts.
- The planner: the action loop of a plan with no goal (fn_004F0D60, called at 0x4F0B78 under `FindBestActionForPlan`
  0x4F0A70) takes each of the plan's up to 30 actions (`GetNthActionForPlan`), and when the row's predicate returns 0
  skips it, with its priority (mind +0x1D48C), novelty and +0x1D494 set to 0 (0x4F0D91..0x4F0DA9 → 0x4F0E29); then the
  two learning types (+0x53C, +0x540, 6 = none) by `KnowsAction(0, t)`, then a non-zero magic by `KnowsAction(1,
  magic)`; only then `FillInActionPriorityAndNoveltyModification`. `CanCreatureCurrentlyPerformAction` 0x4F1B60 runs the
  same predicate (with the knowledge checks and `IsConfinedToArea` with mind +0x1C9C[action]), for
  `CalculateLearningPriority` 0x4E0D10, `MimicForceCopyingAction` (0x4EA1EA, 0x4EA32B, 0x4EA46C), fn_004F1C40 and
  fn_00509C50 [V].

##### The teleport, warming and cooling rows

Read from the action table's initialiser (`_$E90` 0x491BA0; rows 0x9D1678 + 0x50 × i, written as immediate stores)
and info.dat's rows (the frozen `Scripts/info.dat`, read only) [V]:

| # | Name | info.dat desire / magic | predicate (+0x10) | constructor (+0x20), argument (+0x30) | act-on filter (+0x34) | attitude (+0x40) |
|---|---|---|---|---|---|---|
| 136 | CastWarmingSpellOnCreature | 1 / **0** | 0x4D2940 `CanCreatureCastSpell` | 0x4A6910 Helpful, 0 | 0x4C3760 (`??_9` vcall thunk, slot +0x30) | 0x768560 |
| 137 | CastCoolingSpellOnCreature | 1 / **0** | 0x4D2940 | 0x4A6910 Helpful, 0 | 0x4C3760 | 0x768560 |
| 182 | CastTeleport | 1 / 12 | 0x4D2B20 `CanCreatureCastTeleport` | 0x4A6910 Helpful, 12 | none | 0x768560 |
| 196 | ExploreAndCastTeleport | 10 / 12 | 0x4D2B20 | 0x4B18F0, which only pushes 12 and calls 0x4A6910 Helpful | none | 0x768540 |
| 216 | CastTeleportAndUseIt | 40 / **0** | 0x4D2B20 | 0x4ABFE0 `ConstructSubActionsForCastTeleportAndUseItToGetToMarker`, 0 | none | 0x768540 |

- `CanCreatureCastTeleport` 0x4D2B20 (thiscall, `ret 8`): the distance from the creature's position (+0x14) to
  `Creature::HomePos` (+0x1200, a `MapCoords`, taken as `JustMapXZ`), through fn_0074CD90 = fn_0074CD10 (a whole
  distance, not read) then `GUtils::ConvertWholeDistanceToMeters`; then `CanCreatureCastSpell` 0x4D2940; true only when
  that passes and the distance is above 150.0 (`fcomp [0x8CC7E8]`, `test ah, 0x41`). The field before the home,
  +0x11FC, is not tested [V]. The 1.20 symbols name it `HomeExists`, but [the creature's home](players-creature.md)
  finds it is not a flag of that home point (its writers and readers are listed there); what it means is not settled
  here, only that this predicate does not read it.
- **Rows 136, 137 and 216 can never pass their predicate:** `CanCreatureCastSpell` reads info.dat's magic, which is 0
  for the three, and refuses a magic of 0 (0x4D295D). So the planner never weighs them, and
  `CanCreatureCurrentlyPerformAction` 0x4F1B60 (the learning, the mimic copy) refuses them too [V]. Whether a script's
  CREATURE_DO_ACTION reaches their constructors without the predicate is not read.
- **Rows 182 and 196 are the helpful construct with the teleport (12):** they differ only in their desire (1 and 10)
  and attitude. With no act-on filter, the plan's object is the creature's own belief (the planner's plan without a
  goal), so the cast goes near the creature itself and casts teleport at it [the result of a creature's teleport cast
  at itself, through `CastSpellOnObject`, not read].

##### The spell to amuse another creature

Row 297 `CastAmusingSpellOnCreature` (bw1-decomp `CreatureAction.cpp:411`, `CreatureEnum.h:431`): predicate
`Creature::CanCastAmusingSpellOnCreature` 0x4E4680, construct `ConstructSubActionsForCastAmusingSpellOnCreature`
0x4C0640, magic NONE, object filter `GameThingWithPos::IsCreatureWhoSeemsFriendly` (the Creature override 0x4E45E0,
vt +0x3DC; every other thing answers 0) [V].

- **The predicate** 0x4E4680: for m = 0x1A .. 0x29 (Freeze .. Itchy), true at the first m that is
  `KnowsAction(1, m)` (0x4E2890, mind +0x1A9FC) and `HasEnoughEnergyToCastSpell(m)` (0x4F82F0, the energy test above).
  No sightings share and no development phase [V].
- **The construct** 0x4C0640 collects the same list, in type order, on the stack [V]:
  - none: one `AddSubAction(10, Integer 0x43 = 67)` with the playful face 0x4C8780, and nothing else;
  - else `GameRand(n)` (line 0x1461) picks the spell, **then** `GameRand(5)` (0x1463): on 0 the emote 10 (67, playful
    face); GoNear 0x3D at `GetHeight() × 4.0` (the 80-bit constant at 0x8AB418), playful face, look 0x4D0BD0; GetAway
    0x87 at `2 × GetHeight()`, no face, look 0x4D16D0; TurnToFace 6 for 0.1, the kind face 0x4C8730, look 0x4D16D0;
    the gesture 0x58 only when `GMagicInfo[m] +0x2C ≠ 0`; `AddCastSpellSubAction(target, m)` 0x4FF4B0.
  - So it is the playful construct of [The constructs](#the-constructs) with 4h in place of 5h, the spell drawn
    first [V].
- **The object filter** 0x4E45E0 is called on the candidate (this) with the planning creature as its argument, as
  every `GameThingWithPos::Is...(Creature*)` filter [V]:
  - the candidate's mind takes a belief about the planner (`AddBeliefAboutObject` 0x4D7BD0) and
    `FindBestGoalForActivityObject` 0x4C5280 gives its best desire about it (theirs, ebx);
  - the planner's mind does the same about the candidate (mine, eax);
  - it seems friendly unless `theirs == 2 (Anger)`, `theirs == 5 (Fear)` or `mine == 2` (0x4E4643..0x4E4650).
  - So the candidate's anger or fear rules it out, and only the planner's own anger [V].
- **The best desire about one thing**, `FindBestGoalForActivityObject` 0x4C5280 → `AssessParticularObject` 0x4C5130:
  over the 40 desires, each activated (0x4DCDA0), not held back (mind +0xB0 + 4d = 0), with
  `CreatureDesireTableEntry::Table[d]` (0xC87850, 0x28 a row) `IsApplicable` passing when set, the initial desire's
  +0x30 set (info.dat `field0x20`: the same +0x10 shift as its distance weight, +0x68 here for `field0x58`) and the
  row's +0x10 object predicate passing on the belief's thing, and not left out. Its score is strength (mind +0x150 +
  4d) × `GetActivityObjectUsefulness` 0x4E9900 × `CalculateDistancePriority` 0x4F1EF0; anger scores 0 against a thing
  of the creature's own player when mind +0x1D488 is set. It takes over when its score is above the stored best's
  usefulness × nearness (fn_004C5246: the strength of the best is not in the comparison). The returned desire is the
  local plan's +8, which `SetToZero` 0x4F12B0 does not clear: with no desire scoring it is whatever the stack held
  (vanilla bug, guarded in openblack as "wants nothing") [V].
- `GetActivityObjectUsefulness` reads the activity-object trees at mind +0x2478 (the act-on trees are at +0x2518, as
  [mind.md](mind.md) has for hunger), after its own tests: the leash's tied-to thing, the confined area (with an
  exception for desire 0x23), and for anger a list at Creature +0x3D0 of each of the two, either holding the other
  giving 0 [V; that the list is the friends list is a reading, L; the trees' mapping onto openblack's two not
  compared].

##### The heal's target

CastHealSpell's object filter (row +0x34, 0x4C35F0, `jmp [vt +0x228]`) is called through fn_004E3880 (the planner's
`FindBestObjectToActOn` of the town, flock, forest and citadel beliefs through 0x4E3800, the object choice fn_004F0F70 /
fn_004F10A0, and the mimic copy at 0x4EA208). fn_004E3880 first rejects the object when the row's info.dat +0xA8 is
non-zero and the object is phys (+0x160) +0x28; for CastHealSpell +0xA8 = 0. vt +0x228 is answered only by `Villager`
(and `SpecialVillager`) with `Villager::CanBeHealedByCreature` 0x4E4C90: `GetLife()` (vt +0x11C, `Object::GetLife`
0x402600, `Object` +0x48) ≤ 0.7 (0x3F333333 at 0x8AB238), or no number (0x4E4C98..0x4E4CAF); every other class answers
0 (`GameThingWithPos::CanBeHealedByCreature` 0x401BD0) [V]. `Creature::CanBeHealedByHealSpell` 0x47DF40 (`CanBeHealedByHealSpell`, Creature
vtable slot 0xB18, `mov eax, 1; ret`, where Living has 0x5EE550 and the birds 0x41EAB0) is the heal spell's own target
test, not this. Its declared size 0x10 is right, but the next symbol is 0x47EF40, so the tools give its name to the
unnamed functions 0x47DF50, 0x47E000, 0x47E060, 0x47E1B0, 0x47E760 and 0x47EC10. 0x47E25E is in fn_0047E1B0, an
`AddSubAction(0x1F, ...)` in an emote builder (`GameRand(8)` + 8, a loop count L; `GameRand(2)`; `GameRand(5)` into a
jump table at 0x47E740) that fn_0047E060 calls when a feedback value is 0. fn_0047E060 (from
`Creature::GetImpressiveValue` 0x47B40B) makes the current player's creature react: `ForceActivityAndForceAction`
desire 9, action 0x52 or 0xA8 by `GameRand(3)`, then 0x47E1B0 / 0x47E760 / 0x47EC10 by the value's sign; nothing in it
heals. The calls of `SetCheatDominantDesire` 0x4DCE20 there are at 0x47E733 and 0x47EBCD (9, and 0x1E for 180 s) [V].

##### The point the miracle flows from

- **`Creature::InitialUpdateSpellInfo`** 0x4F8590: info +0x00 = the target point (the MapCoords in metres, y = the land
  there + the MapCoords' altitude). When the magic is cast from above (`GMagicInfo` +0x40 == 1, fn_005FB7E0): +0x0C =
  +0x00 raised by 10, or by the target's `GetHeight` × 1.3 when that is not below 10; the two beams (+0x107C, +0x1080)
  are closed down and made again with `GParticleContainer::CreateSpotVisual(point, 0x14 MAGIC_BEAM, size, the
  creature)`, the first at the mirror of the hand bone, the second at the hand bone (`LH3DCreature` +0x51C8, the first
  action point the body's file holds: `LoadBinary` 0x4EB6EB), each at the bone's own translation in the safe buffer
  (`GetBonePos` 0x4813F0: `GetSafeBuffer` + bone × 0x30 + 0x24), size = `0.4 + (1 − 0.4) × GetScale × 0.5` (the pair at
  0x8D1628; with the FPU at 24 bits `1 − 0.4` rounds, a tie, to the float 0.6 = 0x3F19999A, and halving is exact, so
  this gives the bits of `0.4f + 0.3f × scale` for every scale that does not go subnormal) [V 0x4F86BF..0x4F873C], each
  aimed at +0x0C (0x4F870C, 0x4F873C): fn_0063E380 calls the container's effect (+0x2C) vt +0x110 =
  `GJPSysInterface::AddTarget(const LHPoint&)` 0x55ED80, which is `SpellTargets::AddTarget(point)` 0x670CF0 on the
  PSys's targets (+0xB8). fn_0063E380 is also passed `[magicType × 4 + 0xD37D10]`, the per-type entry fn_005FB7E0 reads;
  what it does with it is not read [?]. The raise's test is `fcomp` against 10 with `test ah, 1` (0x4F861D..0x4F863A): a
  NaN height, or a NULL object, keeps 10. The beams are closed down (`GParticleContainer::CloseDown` 0x63E370,
  0x4F8648..0x4F8661) and made again only here, for a cast from above (0x4F85D9..0x4F85E0 skips them for any other
  cast; 0x4F8648..0x4F8731 closes the old ones and makes the new ones). The mirror bone is `GetMirrorBone` 0x481390:
  the table at `LH3DCreature` +0x51F0 ([the mirror table](#the-mirror-table)).
- **`Creature::UpdateSpellInfo`** 0x4F8750 (vt +0x5C), each turn of a spell it made and once before the initial one:
  unless the spell is cast from above (a NULL spell is not), +0x0C = `FillAverageHandPos` 0x48CC80; from above only that
  is skipped (0x4F8775 jumps to 0x4F87A1). Then, for every cast, +0x18 = +0x00 − +0x0C, normalised as `1 / sqrt((z² +
  y²) + x²)` unless all three are 0 (an unordered part counting as 0) (0x4F87A1..0x4F8842). The first call comes before
  +0x00 is written, so a cast's first direction is the normalised −hands, until the spell's first turn; from above it is
  then straight down, from the raised point to the target, every turn.
- **`FillAverageHandPos`** 0x48CC80 is not the hand bone's own place: `fn_0083A0E0(safe, model, &a, hand +0x51C8)`,
  then `fn_0083A0E0(safe, model, &b, GetMirrorBone(hand))`, each component `(a + b) × 0.5f`. fn_0083A0E0 walks the bone
  records (fn_0083A140; records at the model's +0x10, 0x3C bytes each: parent, first child, right sibling): from the
  start bone along its right siblings, a bone with no child adds its translation (buffer[bone × 0x30 + 0x24..0x2C]),
  rounded to a float after each add, and is counted; a bone with children recurses into its first child. The sum is
  multiplied by `1 / count`. Every add and multiply rounds to a float and both means come from the same buffer. Every
  species' hand bone has children and no right sibling, so the point is the mean of the fingertips (the ape's 61, 59,
  57, 55, 53 and the mirror's 43, 41, 39, 37, 35). The buffer is `GetSafeBuffer` 0x4842B0: +0x5178, the posed skeleton
  of the **game turn** (`Creature::ProcessState` calls `LH3DCreature::UpdateTime` at 0x473939, which calls
  `UpdateBuffers` 0x4EC590, posed on the world matrix), or its frozen copy +0x5180 while +0x5270 is set (fn_004806E0,
  cleared by `ReconnectToGame`); not the drawn blend +0x47F0. Besides `UpdateSpellInfo`, sub-action 43's Perform
  0x504DA0 calls it.
- **Which turn's pose each reader sees** [V for the order]:

  | Reader | Reads | Runs | Pose |
  |---|---|---|---|
  | The cast's `UpdateSpellInfo(NULL, info)` → `FillAverageHandPos` | the fingertip means | `CastSpellOnObject` 0x4F7D95 / `CastSpellAtPos` 0x4F8126, from Perform inside `ProcessSubAction` (0x4737B0) or the fight's CastStart inside `UpdateTime` 0x48256A, both before `UpdateBuffers` 0x48257F | the turn before's |
  | The cast's `InitialUpdateSpellInfo` beams (from above only) | `GetBonePos` of the hand and its mirror | the same, 0x4F7DA5 / 0x4F8134 | the turn before's |
  | `Creature::ProcessSpells` 0x4F8B80, moving the beams | `GetBonePos` (0x4F8BD7 / 0x4F8C38) | `ProcessState` 0x472EBB, before `UpdateTime` 0x473939 | the turn before's |
  | `Spell::ProcessMaintainRequest` 0x72055D → vt +0x5C `UpdateSpellInfo(spell, info)` (not from above) | the fingertip means | `GGame::ProcessTurn`: `Living::ProcessLiving` 0x54E65B (every creature posed) before `Spell::ProcessSpells` 0x54E66F → `ProcessMaintainRequests` 0x720390 | this turn's |
  | Beams aimed at a creature: `GetPSysBeamTargetPos` 0x4F8B30 (vt +0x150) | the head bone +0x51B8 through `fn_004813B0` (the safe buffer) | the particle side, not traced | [?] |

- **`Creature::ProcessSpells`** 0x4F8B80 [V], called from `Creature::ProcessState` at 0x472EBB, every creature every
  turn, right after the spell state machine (`fn_004F48C0`, 0x472EB4) and before the pen and the held-creature branch
  (0x472F77), so a creature in the hand runs it too. It reads `GetCreature3D` 0x477850 once, then, in this order: the
  held spell +0x1078 is cleared when it is no longer `IsAvailable` (vt +0x2C, `GameThing::IsAvailable` 0x401810: the
  to-be-deleted bit of +0x0A clear) (0x4F8B8D..0x4F8BA4); the first beam +0x107C is cleared when it is not available,
  else it is placed at the mirror of the hand bone, `GetBonePos(GetMirrorBone(3D +0x51C8))` (0x4F8BC5..0x4F8BD7),
  turned into `MapCoords` by 0x603160 (`MapCoords::Set` 0x603340: x and z × 6553.6 truncated by `__ftol`, y less the
  land's altitude there) and given to the container's vt +0xFC, `GameThingWithPos::SetPos` 0x401940 (the position at
  +0x14) (0x4F8BDC..0x4F8C0D); the second beam +0x1080 the same at the hand bone itself `GetBonePos(3D +0x51C8)`
  (0x4F8C2F..0x4F8C6E); last +0x1088 is cleared when it is not available (0x4F8C74..0x4F8C87; +0x1088 has no other
  reader than `Save`, `Load` and `ToBeDeleted`). The bones are the game turn's pose, as posed at the end of the
  creature's previous turn. The containers take that place for their effects in
  `GParticleContainer::ProcessParticleContainers` 0x63E090, later in the game turn: `Process` 0x63E280 sets the effect's
  origin (vt +0x124) from `GetLHPoint` of +0x14 (0x63E33D..0x63E349), so the beams start each turn at the hands of the
  pose before. `GParticleContainer::CloseDown` 0x63E370 only closes the effect (vt +0x118 of +0x2C): a closed-down
  container stays available, and `ProcessSpells` goes on placing it, until `Process` returns 5 (its effect gone,
  0x63E365) and `ProcessParticleContainers` deletes it (0x63E0BC).

##### The mirror table

[V] `LH3DCreature::LoadBase` 0x4EAC90 makes the table at +0x51F0, one int per bone (+0x47B8, the count that sizes the
bone buffers), every entry −1 (0x4EAD76..0x4EADC4). `LoadBinary` 0x4EB430 (creature version 21 at most, 0x4EB4C6) reads
it last in the "Creature" segment, one int per bone (0x4EBC5A..0x4EBCB5; `LoadBinary` 0x4EBC7D); nothing is read after
it. In a version 20 file a non-zero first number is the second bone's mirror: table[0] = 0, table[1] = the number, and
the reading goes on at table[2] (0x4EBC7A..0x4EBCA9), so the file holds one number fewer. A mirrored clip
(`UpdateBuffers` 0x4EC590, flags +0x522C / +0x5230) passes the table as the clip sampler fn_00860E00's remap: bone j's
key goes to bone table[j], its y and z angles negated (0x860EAC..0x860F75). The table does not follow the morph. All 17
species files are version 21; in each the last numbers pair the base mesh's bones (every bone's mirror's mirror is
itself): 46↔28 for the ape, 34↔27 for the cow, 33↔24 for the tiger and the lion. Pairing the bones by their rest pose
leaves 2 to 8 bones near the middle of the body on themselves in 16 of the species (the tortoise is the same both
ways).

##### The release

fn_004F81F0 [V 0x4F81F0..0x4F8259], in order: the held spell (+0x1078), when its duration (+0x50) is below 0 (or
unordered), is closed down (vt +0x530), and +0x1078 cleared either way; the two beams closed down
(`GParticleContainer::CloseDown`, 0x4F821F..0x4F8238), their pointers kept, so a released beam goes on following the
hand while its effect dies away; the thing at +0x1084 deleted (vt +0x0C `ToBeDeleted(0)`, then +0x1084 = 0).

- +0x1084 is the spot visual of sub-action 43 "CastSpotVisual" (the entry at 0xBE23C8, its name copied from 0xBE6234,
  the stages written at 0x4FAA92..0x4FAAC9): Initiate 0x504D70 (`fn_00484310(0x2C)`, the hold +0x58 = 0); Perform
  0x504DA0, once the body's loop state (+0x4994 == 3) is reached, `FillAverageHandPos` (the point turned into map units,
  × 6553.6, `ftol`) and a hold of `ftol(sub +0x48 × (1000 / ms))` (0x504E10..0x504E30), then
  `CreateSpotVisualWithSpecifiedDuration(hands, type = sub +0x38, 1.0, hold turns, the creature)` stored at 0x504E5F;
  Finish 0x504E80 (count down, end the body action). +0x1084 is zeroed at 0x474166 (the constructor), saved at 0x4E53D6
  and loaded at 0x4E65ED as a pointer, and handled in `Creature::ToBeDeleted` 0x47508B / 0x47509B. The only
  `AddSubAction(0x2A, …)` is at 0x479186, in the function at 0x479090 (the symbol says `ApplySingleEffect`, which
  swallows it): it sets a forced plan (`fn_004F1270` with desire 0x18, action 0x5A and the belief about itself; mind
  +0x1C14 = 1; the sub-action agenda cleared; a `SubArgumentPoint`; then `SetControlledByScript(1)`). No call, jump or
  pointer to 0x479090 exists in the image (rel32 E8 / E9 targets and the 32-bit value searched over the whole image), so
  a miracle cast never fills +0x1084 [V; L that it is never set].
- It is called by the two cast functions, `StopWhatIAmDoing` 0x475AAE, the fight blow that lands on a casting opponent
  0x48B239 (the opponent's cast), the 0x1B handler `NetworkFriendlyStartLockedSelect` 0x5DA950 at 0x5DA9EC (the hand
  starting a locked select on the creature: after the status add 0x5DC110 it copies the pose +0x5178 into +0x5180 at
  0x5DA9CA, without setting +0x5270, then lets go; the address is not in `GInterface::PlaceObjectInMagicHand`
  0x5DA6F0..0x5DA8A0), Finish 0x500E1C, and the fight's Cast state 0x4855B6. The release closes only spells with a
  duration below 0: a lasting spell keeps calling `UpdateSpellInfo` on its creator [V for the calls; L for the effect].

##### What a cast costs the body

`Creature::ConvertSpellChantsToEnergyDiff` 0x4F8260 = `chants / ((GetUserSize × info +0x390 + physical +0x0C + 1) ×
info +0x294)`; `Creature::GetMaxSpellChantsAvailable` 0x4F82A0 = `(GetUserSize + physical +0x0C + 1) × (physical +0x1C −
info +0x298) × info +0x294`; `Creature::MaintainSpell` 0x4F8350 (vt +0x58): an amount ≤ 0 is returned as it is; the
most chants ≤ 0 gives 0; it pays the amount, or the most when that is not above it; `d` = the energy difference of what
it pays, below 0 taken as 0 and above 0.7 as 0.7; energy (physical +0x1C) −= `d` × the magic type's share; below 0 it
is 0, else it is kept to `max(GetUserSize, 1)`; exhaustion (physical +0x30) += `d`; it returns what it paid. The
can-cast test fn_004F82F0 (`HasEnoughEnergyToCastSpell`) is `0.85 − d(costToCreate) > exhaustion`. The info offsets
are the species row's in memory; in the file (and in `GCreatureInfo`) they are 0x10 lower: `spellSizeFactor` 0x380,
`chantsPerEnergy` 0x284, `spellEnergyFloor` 0x288. `GetUserSize` (0x4EF4F0) is the 3D body's size (`LH3DCreature`
+0x90), which `Creature::GetScale` 0x47B190 also returns. The share is `CreatureMagicActionKnownAboutEntry[magic]` +0x6C
in memory (the table at 0xCA9C70, 0x70 bytes each, made by fn_004E2DE0 and filled by `load_variables` from
DETAIL_CREATURE_MAGIC_ACTION_KNOWN_ABOUT), the file's `field0x5c`: the table's data starts 0x10 into each entry, as
`NumTimesMagicActionMustBeSeenBeforeItIsLearnt` 0x4F8CA0 shows (it reads memory +0x54, the file's +0x44, the times to
see it). The magic type is the spell's +0xB4. The 0.85 bar and the 0.7 cap are the executable's constants (0x8CEFAC,
0x8AB238).

#### Casting from above: the Food, Wood and Water rows
<a id="casting-from-above-the-food-wood-and-water-rows-w120-read-2026-10-15"></a>

This page owns the creature casting from above; the miracles themselves are on
[Food, wood, water and heal](../magic/food-wood-water-heal.md).

- **The code table** (`CreatureAgenda::ActionTable` 0x9D1678, 0x50 bytes a row, filled by the static initialiser
  `_$E90` 0x491BA0) [V]: +0x00 the name, +0x10 the predicate (`IsPossible`), +0x20 the constructor
  (`ConstructSubActions`), +0x24 its this-adjust, +0x30 the magic passed to the constructor, +0x34 the act-on filter (a
  `??_9` vcall thunk on `GameThingWithPos`), +0x38 the instrument filter, +0x40 the attitude, +0x44 needs an
  instrument. `ConstructSubActionsFromActionAgenda` 0x4D36A0 clears the sub-action agenda (0x4FF5C0) and calls row
  +0x20 with row +0x30 as its only argument (0x4D36BC..0x4D36CB). The predicate 0x4D2940 reads **info.dat's** magic
  (0xC6C544 + 0x110 × action, the file row's +0xA4), not +0x30.

  | # | Name | info.dat desire / magic | predicate | constructor, argument | act-on filter / instrument |
  |---|---|---|---|---|---|
  | 48 | CastMagicFood | 1 / 14 | 0x4D2940 | 0x49FDC0 CastMagicFood, 14 | vt +0x2BC `CanHaveMagicFoodCastOnMe` |
  | 98 | CastMagicFoodPU1 | 1 / 15 | 0x4D2AF0 (phase ≥ 8) | 0x4A6910 Helpful, 15 | none |
  | 104 | CastMagicWood | 1 / 21 | 0x4D2940 | 0x4A6910 Helpful, 21 | vt +0x2C0 `CanHaveMagicWoodCastOnMe` |
  | 128 | ShowFriendSpellMagicFood | 16 / 14 | 0x4D2940 | 0x4A75D0 ShowFriendCreationSpell, 14 | vt +0x3A8 |
  | 129 | ShowFriendSpellMagicWood | 16 / 21 | 0x4D2940 | 0x4A75D0, 21 | vt +0x3A8 |
  | 238 | PutOutFireWithMagicWater | 1 / 22 | 0x4D2940 | 0x4B6EA0 CastMagicWater, 22 | vt +0x298 `IsOnFire` |
  | 239 | SprinkleMagicWaterOnCrops | 1 / 22 | 0x4D2940 | 0x4B6EA0, 22 | vt +0x334 `BenefitsFromHavingWaterSprinkledOnIt` |
  | 240 | CastMagicWater (on itself) | 15 / 22 | 0x4D32C0 `ShouldCreatureCastWaterOnHimself` | 0x4B7800 CastMagicWaterOnMyself, 22 | none |
  | 246 | CastMagicFoodByWorshipSite | 1 / 14 | 0x4D2940 | 0x49FDC0, 14 | vt +0x308 `IsWorshipSite` |
  | 248 | CastMagicWoodByBuildingSite | 1 / 21 | 0x4D2940 | 0x4A6910 Helpful, 21 | vt +0x318 `IsBuildingWhichIsBeingBuilt` |
  | 302 | CastMagicWoodByWorkShop | 1 / **0** | 0x4D2940 | 0x4A6910 Helpful, 21 | vt +0x314 `IsWorkshop` |
  | 304 | WaterTree | 1 / 22 | 0x4D2940 | 0x4B6EA0, 22 | vt +0x33C `IsTree` |
  | 305 | WaterField | 1 / 22 | 0x4D2940 | 0x4B6EA0, 22 | vt +0x320 `IsFieldWhichNeedsWatering` |
  | 325 | SprinkleMagicWaterPU1OnCrops | 1 / 23 | 0x4D2940 | 0x4B6EA0, 23 | vt +0x334 |
  | 326 | WaterTreeForTown | 1 / 22 | 0x4D2940 | 0x4C31E0 WaterTreeForTown, 22 | none / `IsTree`, needs one |
  | 327 | WaterTreePU1ForTown (its name pointer is "WaterTree", 0x9D7F4C) | 1 / 23 | 0x4D2940 | 0x4C31E0, 23 | none / `IsTree`, needs one |

  Not casts, although they touch these miracles: 31 GiveMagicFoodToStoragePit and 33 GiveMagicWoodToStoragePit
  (0x4ADC20 / 0x4AE020: they carry what a miracle left lying, instrument `IsAFoodPileOutsideStoragePit` vt +0x348;
  magic 0 both sides) and 34 HelpBuildHouse (info.dat magic 21, code table 0, constructor 0x49E100). **302 can never be
  planned**: its info.dat magic is 0, so the predicate fails at 0x4D295D.
- **The filters' overrides** [V]: `CanHaveMagicFoodCastOnMe` (vt +0x2BC) is 0 in the base (0x401CF0) and `Villager`
  (0x4E4B40), and `IsBuilt` (vt +0x890) in `StoragePit` (0x4E4B50) and `WorshipSite` (0x4E4B60): a built storage pit or
  a built worship site. `CanHaveMagicWoodCastOnMe` (vt +0x2C0) is 0 in the base (0x401D00) and 1 in `StoragePit`
  (0x4E4B70): any storage pit, built or not. `IsWorshipSite` (vt +0x308) is 1 only in `WorshipSite` (0x55DC90).
  `IsBuildingWhichIsBeingBuilt` (vt +0x318) is the `MultiMapFixed` override, as in the deed table. `IsOnFire` (vt
  +0x298) is `Object::IsOnFire` 0x637CC0 (through 0x402AA0). `BenefitsFromHavingWaterSprinkledOnIt` (vt +0x334) is the
  field's vt +0x320 in `Field` (0x527F50) and 1 in `Tree` / `MagicTree` (0x55D940). `IsFieldWhichNeedsWatering` (vt
  +0x320) is 1 in `Field` (0x4E4970) when `Field::GetFoodValue` 0x529700 is exactly 0.0 (`fcomp`, `test ah, 0x40`: a
  NaN is not equal). `IsTree` (vt +0x33C) is 1 in `Tree` / `MagicTree` (0x55D920).
- **Where the planner finds them** [V]: the compassion-for-town table gives, by town desire, 0 Food: 28, 29, **48**,
  31, 285, **305**, **325**; 1 Wood: 49, 32, **104**, **326**, **327**, 247, **248**, **304**, 49; 5 Abodes: 247,
  **248**, **326**, **327**; 10 Rain: **239**, **326**, **327**, **325**; 12 Repair: 35, 101, 102, 229, **326**,
  **327**. The desire action table holds 238 in row 1 (read for no desire, see
  [Compassion and towns](plans-and-needs.md#compassion-and-towns)), **240** in row 15 (RestoreHealth) and 128, 129 in row
  32 (EducateFriend). 98, 246 and 302 are in no table: only forced plans reach them (`CanCreatureCurrentlyPerformAction`
  0x4F1B60, `MimicForceCopyingAction` 0x4EA1EA / 0x4EA32B / 0x4EA46C) [L for which forcers name them].
- **What they are cast at**: every one casts at an object (sub-action 39); none uses sub-action 40. On a town the town
  is the plan's goal and the object is the member that passes the row's filter (`CreatureBeliefAboutTown::
  FindBestObjectToActOn` 0x4DAD10 over the abodes, villagers, fields and the totem; fn_004E3800 applies row +0x34): 48
  a built storage pit (or a built worship site, if one is ever a member [?]); 104 a storage pit; 248 a building being
  built; 239 and 325 a field that needs watering or a tree; 304 a tree; 305 a field that needs watering; 326 and 327 any
  member, with a tree as the instrument; 238 a burning thing; 240 itself; 98 anything; 246 a worship site.
- **The constructs** [V] (h = the creature's `GetHeight`; the target is the act-on belief, agenda +0x848; the
  instrument +0x84C; sub-actions 61 going near an object looking at it, 6 turning to face it, 88 a gesture, 39 the
  main cast; 0.1 = 0x3DCCCCCD):
  - **CastMagicFood** 0x49FDC0 (48, 246): f = 2.0 (0x49FDD5), or 20.0 (0x49FE01) when the target is a
    `ShowNeedsVisuals` (`__RTDynamicCast` 0x49FDF5, the floating needs sign; no row's filter accepts one 📼 1.20 only); 61
    GoNear(target, f) with the face `SetFaceForActionCompassion` 0x4C8730 (0x49FE78); 6 TurnToFace(target, 0.1),
    Compassion (0x49FEEA); `GetAltitude` of the target, dropped (0x49FEFC); a gesture (88) when `GMagicInfo[argument]`
    +0x2C is not 0 (0x49FF6D; none in the tables); the main 39 (target, pose **47**, 3.0 s, magic **14**, a constant)
    with `SetFaceForActionGrimace` 0x4C87F0 (0x49FFE9). No `GameRand`, no emote, no getting away.
  - **CastHelpfulSpellOnObject** 0x4A6910 (98, 104, 248, 302): the helpful construct above, pose 41, the magic its
    argument.
  - **CastMagicWater** 0x4B6EA0 (238, 239, 304, 305, 325): 61 GoNear(target, **2h**), no face (0x4B6F26); 6
    TurnToFace(target, 0.1) with `SetFaceForActionSmile` 0x4C87D0 (0x4B6F92); `GetLHPoint` of the target, dropped; the
    main 39 (target, pose **47**, 3.0 s, magic **22**, a constant), Smile (0x4B701B). No draw, no emote, no getting
    away, no gesture. So **325 casts plain Water**, not WaterPowerUpOne, although its predicate tests magic 23.
  - **WaterTreeForTown** 0x4C31E0 (326, 327): with no instrument, `FinishActionUnsuccessfully("no object to use", 1,
    1)` (0x4C3389) and it returns 1; else the 0x4B6EA0 steps on the **instrument**: 61 GoNear(instrument, 2h), 6
    TurnToFace(instrument, 0.1) Smile, the main 39 (instrument, 47, 3.0, **22**) Smile (0x4C3369). 327 casts plain
    Water too.
  - **CastMagicWaterOnMyself** 0x4B7800 (240): `FindClearArea(&point, 1.7 h, 0)` 0x4DFBE0 (1.7 = 0x8CF314), failing →
    `FinishActionUnsuccessfully("failed to construct subactions")`; the clear area is looked for about the creature's
    own position (+0x14, 0x4B7827..0x4B7844), flag 0; 8 going to the point (`MoveToPos`, a `SubArgumentPointAndFloat`
    of the point and 2h, 0x4B7887..0x4B78B8, the float the distance it arrives within, as the walk back's; callback
    `LookWhileGoingTowardsPoint` 0x4D0C80; no face); the main 39 (its belief about itself, 47, 3.0, 22) Smile; then a
    second main sub-action 0x86 (134) with no argument (0x4B79B5): `StopBeingOnFire`, whose table entry (name at
    0xBE56F8, the table 0xBE0B98 + 0x90 per sub-action, its stages from +0x50) has one stage, 0x500A10, that sets the
    creature's temperature to the ground's at its position and is done (2). `Creature::FindClearArea` 0x4DFBE0 is also
    called with h at 0x508A8A (fn_00507265), the search the getting away uses 📼 1.20 only; found, it refuses a spot farther
    from home (+0x11A8) than the confinement radius (+0x11B4) while `IsConfinedToArea` 0x47CCE0.
  - **ShowFriendCreationSpell** 0x4A75D0 (128, 129), 0x750 bytes, belongs to the friends' rows [?].

##### The pose

[V] `LH3DCreature::StartStaticAction(n)` 0x484310 draws `GameRand(2)` (0x484329) for the mirrored variant (kept only
when +0x4A90 is 0, 0x48433D..0x48434F) and plays n − 1, n, n + 1, each checked with `GetAnim` (0x4843A0..0x4843DF,
failing → 0, also while `IsPerformingBodyAction`). Body state 2 (0x486A61) plays n − 1, state 3 (0x486C3E → 0x4875D0)
n and state 4 (0x486CEE) n + 1, from `StateAction` 0x486A00 (jump table 0x48752C). The draw comes before the start, so
it is drawn on every Initiate try: a start that fails returns 0 (stay, 0x500BAE) and is tried again next turn, **one
`GameRand(2)` per turn** while the body is busy or the species lacks the three animations. In `Data/ctrspec27.txt` (the index is the line, not counting `=` lines) 40, 41, 42 are `start_cast`,
`cast`, `end_cast` and 46, 47, 48 `start_scatter`, `scatter`, `end_scatter`. So Food and Water are cast with the
**scatter** pose, Wood and FoodPU1 with the cast pose.

##### The cast and what falls

[V except where marked] The cast step is the one [above](#casting-at-an-object) (magnitude the object's `Get2DRadius` ×
1.1, chants the effect's `initialChants`: 5000 for FOOD, WOOD and WATER, 8000 for FOOD_PU1;
`timerWhenCreatureCasting` −1 for all five; `costToCreate` 7000 FOOD, 10000 FOOD_PU1, 7000 WOOD, 5000 WATER, 7000
WATER_PU1). The FOOD, WOOD and WATER seeds have `castOnObject` 0, so the spell is cast at the object's position
(fn_005FB490). The SF_Food / SF_Wood effect sprinkles grains or logs from the process info's +0x0C, the raised point
above the target; each landing grain is paid with `PayFor(costPerUnit × n)` by the creator (the creature's
`MaintainSpell`), n = 200 then 18 for food, 20 for FOOD_PU1, 500 then 20 for wood, and `Pot::AddResourceToPos` 0x66F270
puts it in the storage pit when within 1.2 × its 2D radius, else in a pile. The water falls the same way [L: that the
effects use +0x0C for a creature's spell as for the hand's is not re-read]. The hold is 30 turns at 100 ms; at its end
`DestroySpell` 0x4F81F0 closes the spell down (its duration is −1) and the beams, which fade following the hands, and
the body ends the pose (`end_scatter` 48 or `end_cast` 42): the food, wood or water falls for exactly the 3-second hold.

#### The creature spells' particles

The creature spells start their file at `Init` (particle type 85 `SF_CreatureSpellGeneric`, 87..89 the itch files,
`SF_CreatureSpell`, `SF_CreatureSpellCompassion`, `SF_CreatureSpellFreeze`); `InitWithObject` gives the creature to the
effect as its object target. Five rules run them; their maths is inline in each `ModifyAtomCollection`, called through
vtable +0x18 by the PSys step (no direct caller).

- **`UR_CreatureSpell`** (ctor 0x677E70: flags `|6 ^4`, kept alive while not closing, not a creator; default sound
  +0x2C 78 `SOUND_SPELL_CREATURE_WHISPS`; MAC 0x677F90). Its collection data (0x560560: +0x20 first = 1, +0x24 the
  turn last seen, +0x28 the creature). The first step: first = 0; with object targets (`SpellTargets::GetNumObjects`
  0x6710C0) one is taken (`TakeTargetObject` 0x671030) and cast to `Creature`; no creature: the rule returns false
  (its collection lets it go). Else one atom (`AtomCore::Create`, init with the rule's creator), its render
  particle the creature (fn_006C7E70, +0x128), its place the 3D body's +0x78 (once: it is never moved again), the hum
  started (`AtomCore::StartSound` 0x6745D0, +0x2C), and unless `PSysManager::IsScriptCasting` 0x6735A0 the casting
  sound (+0x44), its action replaced by 0x98 (`SOUND_SPELL_CREATURE_SPELL_CAST_BY_OTHER_CREATURE`) when
  `IsCreatureCasting` (fn_00673560: `Spell::IsCreatureCasting` 0x720220, spell +0x48 == 1). Every step: a creature
  that is no longer `IsAvailable` (vt +0x2C) is let go (+0x28 = 0), else +0x24 = the turn; closing (`IsInState(1)`),
  every atom's sounds stop. It returns true.
- **`UR_CreatureSpellGeneric`**, the wisps (StaticCreate 0x6BFDA0: flags `|6 ^4`; NumAtoms +0x2C 10, EmitDuration +0x30
  2, PhiSpeed +0x34 1, ThetaSpeed +0x38 2, the flight +0x3C 2, DelayBeforeEmit +0x40 1, TaperFrac +0x44 1, the wobble
  +0x48 0.2 and its time rate +0x4C 1; MAC 0x678D50). In order, returning false (letting go) at the first that fails:
  a creator (+0x28); the gesture point (`GetCurrentGesturePosn` 0x673600); a parent atom (+0x3C), the first modifier
  of that atom's collection that is a `UR_CreatureSpell` (RTTI), its creature (+0x28), `GetCreature3D` 0x477850; the
  spell (`PSysManager` +0xA8) as a `SpellCreature` (vt +0x544) and its kind (`GetMagicInfo` 0x722500, +0x58
  `creatureReceiveSpellType`) other than 16. Then: the first step keeps the 3D body's +0x78 and the gesture point
  (collection data +0x20, +0x2C); after DelayBeforeEmit (`GetAge > delay`, `test ah,0x41`) and while NumAtoms > due,
  due = N × dt / EmitDuration + due, capped at N (0x678F2C..0x678F7D); while made (an int, +0x40) < due a wisp is
  made: its AtomData (0x560760: angles 0, theta rate 0, phi rate 1, size 1) takes the atom's size (+0x74), theta rate
  = `PSysFloatRand(0.5, 1.5)` × ThetaSpeed, negated when `PSysFloatRand(1)` < 0.5, phi rate =
  `PSysFloatRand(0.5, 1.5)` × PhiSpeed. Kind 7 (invisible), not closing and the 3D body's +0x482C set (its 3D object,
  made at the creature's creation): the collection's alpha byte (+0x50) = `__ftol((1 − ratio) × 255)`, the ratio
  fn_004F5360 on the creature's `CreatureReceiveSpell` slot 7 (+0x370 + 0xAC: off and waiting 0, starting
  1 − left / (turns a second × start), holding 1, finishing left / (turns a second × finish), not clamped).
  Not closing and the creature casting: the swing (below). The box of the safe buffer's bones (`GetSafeBuffer`
  0x4842B0, +0x47B8 bones, translations at +0x24; min with `test ah,1`, max with `test ah,0x41`; centre x and z the
  halved sums, bottom the least y, height the y span, radius 0.5 × `fsqrt(dz² + dx²)`). Every atom of the collection
  (an AtomData is made for one that has none): theta and phi = `__CIfmod(dt × rate + angle, 2π)` (2π the double
  6.2831854820251465, the float 2π exactly); the orbit (`WispOrbit`), the flight from the gesture point over 2 s
  (`LHPoint(orbit − hand) × clamp(age / 2, 0, 1) + hand`), written as the atom's position (+0x80, no hierarchy in the
  files), its size the AtomData's × the 3D body's +0x90, its alpha `__ftol(clamp(age / 0.5, 0, 1) × 30 + 20)`.
- **The swing** (0x6790FE..0x6791FD): the two flat ways from the hand to the creature, then and now (x and z of
  +0x20 − +0x2C, and of the 3D body's +0x78 − the gesture point), each length squared x² + z², normalised by
  `InverseSquareRoot` 0x841170 (the 3D engine's table guess and one Newton step; of 0 it gives 0x5F87C000, so a way of
  no length becomes (0, 0) and its dot is 0), their dot x·x + z·z; `fn_00722510` (`SpellCreature::GetMagicInfo` +0x98,
  `maxDirnChangeWhenCtrCasting`) is taken as radians by `fcos`, and `cos(limit) > dot` (`test ah,0x41` after
  `fcomp`) calls the spell's vt +0x530, `Spell::CloseDown`. Every product and sum rounds to a float (the game's 24-bit
  precision); only the cosine keeps more. Run in the exe through the oracle (`InverseSquareRoot` after
  `LH3DMath::MakeInverseSqrtLookupTable` 0x8411D0, its `LH3DMem::Alloc` stubbed): 32 inputs (the edges 0, denormals, the
  largest, 1 ± one bit, −0 and infinity, and a spread). Near the limit the table's root and the exact length decide
  differently, and a limit of 0 closes even a cast straight on, the table making each way a little short.
- **`UR_CreatureSpellItch`**, the flies (StaticCreate 0x6BFB60: flags 6, a creator; NumAtoms +0x2C 5,
  PauseBeforeGotoCreature +0x30 5, OrbitSpeed +0x34 1; MAC 0x6781A0). A creator (+0x28) or false; the first step
  makes NumAtoms atoms; then the parent atom's `UR_CreatureSpell` creature and its 3D body as above, or false. The
  right eye `GetBonePos(3D +0x51D0)` 0x4813F0 and its mirror `GetBonePos(GetMirrorBone(...))` 0x481390, both from the
  safe buffer; the point (`ItchOrbit`) while the collection is older than the pause, else the gesture point, written
  to every atom's position. `UR_Flocking` then flies them after it.
- **`UR_CreatureSpellCompassion`**, the hearts (StaticCreate 0x6BFF00: flags `|6 ^4`, kept alive while not closing, not
  a creator; MaxAtoms +0x2C 20, DieAge +0x30 5, InitSpeed +0x34 10, DefineProperties 0x6B1D70; collection data 0x560820:
  +0x20 first, +0x24 due, a float, +0x28 made, an int, which bw1-decomp names NumEmitted and EmitTime, both long; MAC
  0x678A50). In order, returning false (letting go) at the first that fails: a creator (+0x28); a parent atom
  (collection +0x3C); the first modifier of its collection that is a `UR_CreatureSpell` (RTTI) and that rule's
  collection data (made there when missing, so with no creature); its creature (+0x28); `GetCreature3D`; the rate
  MaxAtoms / DieAge above 0 (`fcom` 0, `test ah,0x41`: 0, below or not a number lets go). The first flag is only
  cleared. Then due = dt × rate + due (each product and sum rounded, then stored); unless closing (`IsInState(1)`),
  while made < due (`fild`, `test ah,1`: a due that is not a number keeps it going) and the collection's count (+0x44) <
  MaxAtoms: made + 1, `AtomCore::Create`, fn_00674DD0 (NextGroups +0x20), the creator's `CreateParticle` (vt +0x10, into
  +0x128), the size +0x74 × the 3D body's +0x90 (stored), the parent atom's `GetRandomSurfacePos` 0x673E60 (its render
  particle's vt +0x110 with the atom's matrix, fn_00673E40; with no render particle the point is left as it was,
  uninitialised stack), `GlobalToLocal` 0x675410 into +0x80, and the velocity +0x34 = `PSysRandR3` 0x6729F0 × (InitSpeed
  × the 3D body's +0x90, kept on the FPU), each part stored. Last, closing or not, every atom older than DieAge
  (`GetAtomAge`, `test ah,0x41`: older, not as old) is deleted (`DeleteFromAtomList` 0x674C00). It returns true.
  `SF_CreatureSpellCompassion` gives MaxAtoms 20, DieAge 5, InitSpeed 6, its collection (group 4) under the first rule's
  atom (NextGroups 1 4), so each heart's point is one of the creature's body (the walk in [Pending](#pending)).
- **`UR_CreatureSpellFreeze`**, the steam (StaticCreate 0x6BFC80: flags `|6 ^4`; MaxAtoms 20, DieAge 5, InitSpeed 10,
  MinHeight +0x38 5, DefineProperties 0x6B1C60; collection data 0x560660, as the hearts'; MAC 0x678610). The hearts'
  tests but the rate's: a rate of 0 or below makes nothing and one that is not a number makes up to MaxAtoms; due =
  (MaxAtoms / DieAge) × dt + due. The same loop, with after the size: the spread = the 3D body's +0x90 × the float at
  0x935798, 0x3E999999 (a bit below 0.3, whose float is 0x3E99999A), stored; `PSysFloatRand(1)` 0x6729B0 below 0.5
  (`test ah,1`) takes the right foot, 3D +0x51CC (the second bone the body's file holds: `LoadBinary` 0x4EB701, after
  the hand's +0x51C8), else `GetMirrorBone` 0x481390 of it; that bone's translation in the safe buffer (`GetSafeBuffer`
  0x4842B0, +0x24); the start = the foot + `PSysRandR3` × the spread (x's product goes straight into its sum, y's and
  z's are stored first), `GlobalToLocal` into +0x80; the velocity as the hearts'. Last, for every atom: the ground
  `GetAltitude` 0x803090 at `ftol((x × 65536) × 0.1)`, `ftol((z × 65536) × 0.1)` of its +0x80 (the floats 0x8AC408 and
  0x8AC404), the lift MinHeight × the 3D body's +0x90 (kept on the FPU); when y − ground is below the lift (`fcomp`,
  `test ah,1`: also when not a number) y = lift + ground, stored; then an atom older than DieAge is deleted.
  `SF_CreatureSpellFreeze` gives MaxAtoms 0 (MinHeight 0, InitSpeed 1.01): the original's frozen creature lets out no
  steam at all.
- **The hearts' and the steam's rounding**, run in the exe through the oracle (each stretch entered with its stack
  frame laid out by hand; `PSysFloatRand`, `GetMirrorBone`, `GetSafeBuffer`, `PSysRandR3`, `GetRandomSurfacePos`,
  `GlobalToLocal`, `GetAtomAge`, `GetAltitude` and `DeleteFromAtomList` stubbed): 20 000 cases each of the due and of
  the hearts' rate test (0x678BC3..0x678C05), the steam's due (0x678783..0x6787A6), a puff made (its size, start,
  foot and velocity, 0x6787FE..0x678941), a heart made (0x678C5D..0x678CF2) and the land and age test
  (0x678969..0x678A3E): 100 000, every one the exe's bits. With the spread taken as 0.3's float, 1 297 of 20 000
  starts come out a bit off.
- **The orbits' rounding.** The wisps' orbit (0x67941C..0x6795C3) takes each `fsin` / `fcos` unrounded into the
  operation after it: cos φ + 1, cos θ × width and sin θ × width, and the wobble's sums sin(1.3t) + sin(t),
  sin(1.13t) + sin(0.53t) and sin(0.9t) + sin(2.72t), each sum rounded once. The flies' circle (0x678398..0x6785AA):
  |out| is `fsqrt((y² + z²) + x²)` and |across| `fsqrt((z² + y²) + x²)`; across (out.z − 0, 0, 0 − out.x) is rescaled
  unless each part tests equal to 0 (`test ah,0x40`, so a part that is not a number counts as 0); across × the sine
  unrounded, out × the cosine stored to a float first (the argument of `LHPoint::operator*` 0x44E9F0), then
  (out × cos + centre) + across × sin (`LHPoint::operator+` 0x4605F0 twice). Run in the exe through the oracle (each
  stretch entered with its stack frame laid out by hand, `AtomCollection::GetAge` 0x674ED0 stubbed): 150 000 wisp
  orbits and 100 000 fly points, every one the exe's bits. With each sine and cosine rounded to a float first, 2 595 of
  50 000 orbits and 9 293 of 100 000 fly points differ. The x87 rules are on
  [The x87 arithmetic](../engine/floating-point.md#the-x87-arithmetic).

### Creature miracles (pending)

The 16 creature rows of info.dat are MAGIC_TYPE 26..41 (the last block of
[the info.dat tables](../formats/info-dat.md#infodat-tables)); rows 31, 32 and 36..40 (Fat, Thin, Hungry, Frightened,
Tired, Ill, Thirsty) read "NONE" with zero help ids: creature spells unused in the data. What is known and still waits:

- Creature cast rules: vt 0x30 0x5FA7E0 = 0 at a position; vt 0x2C 0x5FA7F0 on an object, a creature with no more
  than 5 spells waiting in its `CreatureReceiveSpell` queue (fn_004F5230; not a test of its mind)
  ([Cast rules](../magic/spell-core.md#cast-rules)); `IsCreatureCastFromAbove` 0x5FB7E0.
- The creature's desires on casting (fn_00721730, at the start of `Spell::InitWithPos` 0x71FE50) and the stealing of
  spells by the creature (worship).
- The mimicry (`ConsiderMakingCreatureMimicPlayer`) in forest and the creature in the explosion and the lightning
  (`Creature` 0x47B1E0 `CanBeDestroyedBySpell` = 0; the lightning's creature case, `creature+0x12B0` the fizz, is
  ported: [lightning](../magic/fireball-and-lightning.md)). The tornado's faint (fn_00477060) is ported.
- Creature hooks still to port: `ConsiderMakingCreatureMimicPlayer` 0x4EA900 (action from `fn_004E9DF0`), the creature
  defence multipliers 0x478C00, `Creature::ReduceLife` 0x47DD00, and the town magic theft `fn_0073D500` /
  `fn_0073D5A0`. The deed a player's miracle shows when it reaches something is on
  [Spell hits](mimicry.md#spell-hits). A miracle reaching a creature (`Creature::ApplyEffect` 0x478C80, the defence
  0x478C00, `ReduceLife`, `DestroyedByEffect` 0x476F70) is on
  [A creature taking an effect](../magic/spell-core.md#a-creature-taking-an-effect). `Creature::MaintainSpell` 0x4F8350
  and `Creature::UpdateSpellInfo` 0x4F8750 are ported (above and below).

### Sources

- The v1.42 `runblack.exe` (1.20 symbols from bw1-decomp), xref and the oracle runs named above.
- `Scripts/info.dat` of the frozen game (the action rows, `GMagicEffectInfo`, the desire dependencies) and the species
  files and `Data/ctrspec27.txt`.

## openblack

Status: **done** for a cast at an object (the constructs, the predicate, the fizzle and its show, the hunger, the hold,
the release, the body's cost, the beams, the casts from above, the fight's cast) and for the creature spells' particle
rules; the cast at a point (sub-action 40) for the forest (raffclar's #477, read in v1.42) and the help request on a
fizzle, at a thing or a point (ours, from the exe: `creature_help::Kind::MessedUpAction` raised through
`CreatureHelpSystemInterface::Raise`, whose gates are the help's own); **not ported**: the learning context of the
fizzle's plan, the creature's surface walk for the particles. The port decisions against raffclar's code are on
[Port decisions: creature](../project/port-decisions-creature.md).

#### Casting miracles: what openblack does

A creature casts a miracle as the original's cast at an object does ([the original's
steps](#casting-a-miracle-the-originals-steps)). The cast at a point (sub-action 40) is built by the forest's row:
`creature_mind::CastOrder::place`, tried by `mind_detail::TryMiracleAt` and cast by `MagicSystem::CastByCreatureAt`
([below](#casting-miracles-what-openblack-does)).

- **Who reaches it.** The planner: the 18 casting actions have their rows (CastLightningBolt at anything, in the
  lightning style; CastHealSpell at `Target::HurtVillager`, a villager at seven tenths of its life or less, or with a
  life that is no number, as `Villager::CanBeHealedByCreature` 0x4E4C90; the 16 CastMakeCreature* at another creature,
  in the playful style; CastAmusingSpellOnCreature at `Target::FriendlyCreature`, `Build::CastAmusing`, below).
  Imported from raffclar's lanes #467 and #469, their constructs read against v1.42 on
  2026-10-18 ([the attack, shield and impress constructs](#the-attack-shield-and-impress-constructs)), the faces
  corrected to the original's: CastLightningBoltPU1 and PU2
  in the lightning style; CastHealSpellPU1 at `Target::HurtVillager`, CastPhysicalShield and CastShieldPU1 at anything,
  in the helpful style; CastTornado, CastLightningStorm, CastExplosionPU1 and PU2 at anything, `Build::CastAggressive`
  (angry one time in five, near to five times its height, back off to twice it); CastExplosion, `Build::CastExplosion`
  (the angry animation with an amazed face one time in three, near to 100, back off to twice its height); CastFireball and its two power-ups,
  `Build::CastFireball` (`CastFireballAt`: first to within twice its height of the middle of the nearest clear area
  five times its 2D radius wide, `mind_detail::FindClearArea`, none without one; the angry animation with an amazed
  face one time in five; it backs off to
  twice its height only from a living target, a villager, an animal or a creature as `Target::Living`; then the turn,
  the gesture and the cast); CastShield at anything, `Build::CastShield` (`CastShieldAt`: the turn, kind, and the cast,
  from where it stands; for a plan about nothing, `Situation::activity` empty, `creature_mind::BeIdle` and no cast, as
  0x4A1540), and CastShieldAroundTown (286) at `Target::BuiltStoragePit` (a storage pit that is `abodes::IsBuilt`) the
  same way, as 0x4BF770; CastMagicForest (49) at a villager, `Build::CastForest` (`CastForestAt`: near to 2.5 times its
  height, looking where it goes, the turn, kind, looking at the villager, then the forest at `CastInfo::place`, the
  villager's place as the agenda is made, in the scattering pose for four seconds, grimacing; none without the place);
  CastImpressiveSpell at a villager, `Build::CastToImpress` (`CastToImpress`: near to four times its
  height, back off to twice it, pointing at the villager amazed for a second before the cast on `GameRand(2) == 0` and
  otherwise after it with a smile, beckoning (52) playfully unless `GameRand(3) == 0`, then the turn and the cast of
  `creature_mind::PickImpressiveMiracle`'s miracle: of the heal, the storm, the heal's power-up and the flying flock it
  knows, the one any creature picked longest ago, the last of equals, by the land's
  `RegistryContext::impressiveMiracleCastTurn`; with none it casts nothing). Every new cast step grimaces as the
  original's cast does; his builders pull no face there. Going near, each of these agendas has the look of the walk
  towards the target; backing off and turning, its head on the target (`Gaze` with the object), and so do the food and
  water casts (`CastFoodAt`, `CastWaterAt`) going near and turning; the shield's turn has no
  look of its own ([the looks](#the-attack-shield-and-impress-constructs)). The impress row passes its own predicate,
  not `MayCast`'s
  (`creature_mind::CanCastToImpress`: one of food, the heal, the storm, the heal's power-up or the flying flock known,
  payable and seen more than half the times needed, a strict test). Food and wood ([their
  rows](#casting-from-above-the-food-wood-and-water-rows)): CastMagicFood at `Target::FoodStore` (a built storage pit or
  a built worship site) and CastMagicFoodByWorshipSite at `Target::WorshipSite`, both `Build::CastFood` (`CastFoodAt`:
  near to 2, or 20 at a needs sign, kind; the scattering pose; always Food; no draw); CastMagicFoodPU1 at anything,
  CastMagicWood at `Target::WoodStore` (any storage pit), CastMagicWoodByBuildingSite at `Target::BuildingSite`
  (`abodes::IsBeingBuilt`, the deeds' test) and CastMagicWoodByWorkshop at `Target::Workshop`, all
  `Build::CastHelpful`; the last is never weighed, its row having no miracle, but a forced plan casts wood there
  (`Executor::castMagic`, the construct's own argument 21; raffclar's vanilla bug 81). Water: SprinkleMagicWaterOnCrops and
  SprinkleMagicWaterPU1OnCrops at `Target::Sprinklable` (a field whose food, `fields::GetFoodValue`, is exactly 0, or
  any tree), WaterTree at `Target::AnyTree` and WaterField at `Target::DryField`, all `Build::CastWater`
  (`CastWaterAt`: near to twice its height, the scattering pose, always plain Water, smiling); WaterTreeForTown and
  WaterTreePU1ForTown at any town member with `Build::CastWaterOnInstrument`, on the plan's instrument, and not
  possible without one (the planner chooses no instrument yet, so only a forced plan with one carries them out).
  PutOutFireWithMagicWater (238) at `Target::Burning`, anything on fire (`FireSystemInterface::IsOnFire`, the row's
  act-on filter vt +0x298), with `Build::CastWater`: the same water cast at the burning thing, with no douse of its
  own, as its construct is the watering's 0x4B6EA0 with 22; the water miracle's fall cools the fire (lane #544
  import, read in v1.42). The planner looks for it among everything it may come to know of, as for `Target::Anything`,
  and the fields.
  CastMagicWater (240, on itself) with `Build::CastWaterOnItself` (`CastWaterOnItself`: to within twice its height of
  the nearest clear area 1.7 times its height wide about where it stands, `mind_detail::ClearSpotToCastOnItself`,
  then Water on itself in the scattering pose, smiling, and last a `Step::Kind::CoolDown` that puts its own fire out,
  `FireSystemInterface::PutOut`, as the original's second main sub-action 0x86
  (`CREATURE_SUB_STATE_ACTIONS_STOP_BEING_ON_FIRE`, bw1-decomp `CreatureAction.h`; its one stage, 0x500A10, sets the
  creature's temperature to the ground's at its position, `Object::SetTemperature(GetTemperature(pos), nullptr)`, and
  is done at once, returning 2: the same as `PutOut` and the step, done as it starts ✅); none without a clear area),
  weighed only while the creature is
  on fire (`MayTry`, the original's `ShouldCreatureCastWaterOnHimself` 0x4D32C0: on fire, then 0x4D2940). The
  planner weighs a cast only when `MayCast` passes, the original's predicate on the action's row (0x4D2940,
  [above](#may-it-cast-the-actions-predicate)): seen at least half the times it needs (a share that is no number
  fails), able to pay for it, and a power-up only past the seventh stage (0x4D2AF0). It has no player test: [0xD00E00] is always 0, so the
  test never runs and openblack matches. The mimic copy checks `MayCast`
  too, as the original's copy goes through the same predicate. A chosen cast's agenda carries what it casts, its
  gesture and its height (`CastInfoFor`). The fight tries its spell as its CastStart state ends: an aggressive one at
  its opponent, else a defensive one at itself, and none without an opponent (`creature_fight::FightSpellTarget`);
  whatever the try gives, it goes on to Cast, which lets go after the first update whose whole milliseconds, added up,
  pass 2 (`creature_fight::HoldFightCast`), as the original's states ([the fight's cast](#the-fights-cast)).
- **The spell to amuse** ([the original](#the-spell-to-amuse-another-creature); raffclar's PR #610, unverified against
  v1.42). `mind_detail::MayCast` lets the row be weighed while `mind_detail::AmusingSpells` (the spells 26..41 known
  and payable, `CanPayFor`) is not empty; `mind_detail::CastInfoFor` with the tables gives that list, and
  `creature_mind::CastAmusingAt` draws the spell, then the 1-in-5 emote, and builds `CastAt` in the
  `CastStyle::Amusing` style (4h near, 2h back, playful then kind, Grimace on the cast). `Target::FriendlyCreature`
  asks `CreatureMindSystem::SeemsFriendly`: `creature_mind::SeemsFriendly(mine, theirs)` over
  `mind_detail::DesireAboutCreature` for each of the two (raffclar's #433 slice, refitted). That weighing is the
  planner's own: our act-on `Usefulness` × `DistancePriority` from where the other creature stands, over the desires
  activated, not held back and with `searchesForGoal`; impress never, anger only at a creature it may be angry with
  (not itself, not a `CreatureFriends` friend, not knocked out, `canCreatureAttackMe`). The original's per-desire
  object predicates, its activity-object trees and the believed place are not modelled (Pending). "Wants nothing"
  counts as neither angry nor afraid (vanilla reads an unset value; guarded). Debug: the creature window's mind
  section, "Amusing another creature". Scenario `miracles.creature_casts_amusing_spell` (a script's do-action at the
  other creature).
- **What a plan is about.** The agenda's `Situation::activity` is what the plan says it is about (the town compassion
  helps), else the thing it acts on for a desire that looks for a goal, else nothing
  (`creature_learning::PlanActivity`, as `CreatureAgenda` +0x844, the agenda's belief about what the plan is for, whose
  +0x30 is the thing: 0x4A1510 reads it so; no instruction writes +0x844 by itself, the agenda is filled whole, so the
  fallback is raffclar's reading, not traced). A forced plan is about what it is forced about, so a script's
  (`CREATURE_DO_ACTION`) is about nothing; a cast the debug tools tell (`TellCast`) is about its target.
- **The cast at a place.** `mind_detail::TryMiracleAt` tries a cast order with a place as `CastSpellAtPos` 0x4F7E20 does
  ([casting at a point](#casting-at-a-point)): no fight branch (0x4F7970 has `IsFighting` at 0x4F7998, 0x4F7E20 none);
  the fizzle first (0x4F7F6B..0x4F7FDA), then the release `DestroySpell` (0x4F7FDF); a fizzle is shown about
  `CreatureCasting::lastFizzleTarget`, the thing of the last fizzled try at a thing, as the original keeps its belief at
  +0x21E0 (the place's fizzle writes the point at +0x21E4 and the type at +0x21F0, not +0x21E0; raffclar's vanilla bug
  80); both fizzles raise the help's messed-up action, `SendCandidateHelpScript(0x27, 1, 0, 0, 0)` (0x4F7B79 at a
  thing, 0x4F7FB0 at a place; `creature_help::Kind::MessedUpAction`, `k_MiracleFizzled`); the energy test
  (`HasEnoughEnergyToCastSpell` 0x4F8085) and the class's rule at the place (`GMagicInfo` vt +0x30 at 0x4F8098:
  `CanCast(MapCoords)`, 1 in the base 0x5FB420 and its shield, water, storm, flock and radius classes, the forest's
  0x5FAE80 `InBounds`, `IsLand`, no abode, `ValidPlaceForTree`) make it hungry (`UnsuppressDesire(4)` and
  `MakeDesireFullyDominant(4)`, 0x4F8174..0x4F8193). Ours: `MagicSystem::CanCreatureCastAtPlace`,
  `cast_rules::CanCreatureCastAtPlace` = `CanCastAt`, the class's check alone. `MagicSystem::CastByCreatureAt` casts at
  the place on the land (its height above the land dropped) at the size 10 of 0x8D161C, or a fire seed's by the
  caster's height over 0x8D1620 (10.0), at most 1 (0x4F800D..0x4F803D; `creature_spell_casting::CastMagnitudeAtPlace`);
  the cast at a thing and at a place share `MagicSystem::StartCreatureCast` (the body's payment, the cast, the beams
  and the hold).
- **`CreatureCastAgenda`**: the order a creature goes about a cast in. One time in five it first shows how it feels
  (angry, kind, playful), then goes near the thing, backs off to a distance, turns to face it and holds still a tenth of
  a second, draws the miracle's gesture if it has one, and last takes the casting pose and casts as the pose's loop
  begins, holding the miracle three seconds. A lightning bolt is cast from 50 away, backing off to ten more than the
  creature's height; a helpful miracle from twice its height; a creature spell from five times its height, backing off
  to twice it. These are the original's three constructs (0x4A63B0, 0x4A6910, 0x4A6BE0).
- **The turn.** While the mind has a step its turn count goes up; the going near, getting away or turning in hand goes
  on a turn before the mind thinks, and a new one starts with the step's seconds. The locomotion's move leaves these
  three to the mind.
- **`CreatureCastMoves`**: how it measures those moves. The clearance it keeps walking up to a thing by how tall each
  is, when it has arrived, how far away "away" is and where that point lies, the circles a fixed thing keeps clear, and
  the nearest clear square of cells as wide as the creature is tall.
- **The try** (`mind_detail::TryMiracle`, given the miracles' service so that tests use a fake), in the original's
  order of `CastSpellOnObject`: it lets go of what it held; in a fight it pays the miracle's stamina, and can't without
  enough; short of all but one of the sightings it needs it fizzles, which counts as one more sighting; without the
  energy for it, or at something the miracle's own rule (`MagicSystemInterface::CanCreatureCastAt`) refuses, it wants
  food above all (hunger unsuppressed and made fully dominant), and on no other refusal; then the service casts it
  (`CastByCreature`). A failed try gives the agenda up.
- **The fizzle shown** (`ShowFizzle`, the original's 0x509EC0), before the mind chooses what to do next, the turn after
  the try: it stops what it was doing (`Replan`: the walk stops and the body's animations end); the plan is
  CommunicateState under ManifestState, about what it cast at; deciding on it holds back the desires ManifestState
  opposes (`SuppressOpposed`, the original's `SuppressDesiresAfterDecidingOnActivity` 0x4DC210); then embarrassed (73),
  and on `GameRand(2) == 0` sad (56) and sadder by `GameFloatRand(0.3)`, on the game's synchronised stream, after the
  stop and the plan.
- **The hold.** A cast that worked is held for the step's seconds in the game's whole turns, `seconds × (1000 / ms per
  turn)` with the division a whole one and the fraction dropped (the mind's senses carry the turns a second); 30 turns
  for the cast's 3 s at 100 ms a turn. While held, the casting pose keeps its loop, as a still step keeps a sit. At the
  hold's end it lets go and the pose ends.
- **The cast from above**: the point it flows from is ten above the target, or 1.3 times the target's height when that
  is not below ten, a height that is no number giving ten (`MagicSystem::CastFromAboveRaise`); both beams are aimed at
  that point as they are made (`MagicSystem::StartCreatureBeams`, through
  `ParticleSystemInterface::AddSpotVisualTarget`, the spot visual's effect's point target); and each turn of the spell
  the way is from that point down to the target, only the hands being left alone (`CreatureSpellInfo`), as the
  original's `InitialUpdateSpellInfo` and `UpdateSpellInfo` ([above](#the-point-the-miracle-flows-from)).
- **The beams each turn.** `MagicSystem::KeepCreatureBeams`, called from `ecs::creature_loop::ProcessTurn` right after
  the creatures' spells step, clears a held miracle or beam that is no longer available (`ecs::IsAvailable`) and sets
  each remaining beam's object Transform to `creature_hands::BeamPointsOf` (the mirror bone's and the hand bone's
  translations in `CreatureTurnPose`, frozen pose first); the particle manager's turn takes a moved Transform as the
  container's new map coordinates, as `SetPos` does. Going through every creature at once rather than inside each
  creature's turn gives the same places: nothing between the spells and the end of the creatures' turn poses a body,
  and the containers are processed after the creatures. It is not the same for what is let go of: something that
  another creature's turn makes unavailable is dropped the same turn in the original and one turn later in openblack;
  no case reaches this: `ReleaseCreatureCast` clears the held miracle itself, and a beam goes only with its effect, in
  the containers' turn, which comes after the creatures' in both.
- **The releases** (`MagicSystemInterface::ReleaseCreatureCast`): the try itself, the hold's end, the fight's Cast
  state's end, a fight blow that lands (the opponent's cast), stopping what it does (the mind's `Replan` and the leash's
  `PullAwayFromAction`), and the creature's removal. With nothing held it does nothing. It closes each beam with
  `ParticleSystemInterface::CloseDownSpotVisual` (`particles::manager::CloseDownSpotVisual`: the container's effect closes
  down and the container is kept), keeps `CreatureCasting::beams`, and `KeepCreatureBeams` goes on moving them; the
  manager's turn destroys the beam's object with its finished effect. `CastByCreature` sets the beams only for a cast
  from above, so the closing beams of the cast before stay on the hands through any other cast, as in the original.
- **`CreatureSpellCasting`**: what a cast costs its body, read against the exe and matching
  ([What a cast costs the body](#what-a-cast-costs-the-body)). Chants to energy is
  `chants / ((size * sizeFactor + strength + 1) * chantsPerEnergy)`, the most chants it has to give is
  `(size + strength + 1) * (energy - floor) * chantsPerEnergy`, paying tires it by at most 0.7 at a time, its energy is
  kept between 0 and its size (at least 1), and it cannot cast what would leave it past 0.85 exhausted. Also the fight's
  stamina cost, the share of sightings at which it tries a miracle and at which a try stops fizzling, and how big it
  casts. A spell whose creator is a creature is paid for by its body (`magic::creator::MaintainSpell` for
  `Kind::Creature`: `creature_spell_casting::MaintainSpell` on `Creature::size` and `strength` and the `CreatureNeeds`
  energy and exhaustion, the share from `creatureMagicActionKnownAboutEntry[magic].field0x5c`).
- **`CreatureSpellMind`**: what the mood and need spells do to its desires. The spell's desire is made fully dominant
  and most others are held down for the cheat's time; the body's needs only when all are to be; never its wish to idle
  or play with the player, to restore its health, make friends, show its state, rest, hang around at home or look
  around. As a spell wears off its desire drops below the weakest of the others. Its cheat is the original's timed
  dominant desire ([The timed dominant desire](leash.md#the-timed-dominant-desire)). The failed try's hunger is its
  `MakeFullyDominantOverOthers`.
- **`ecs::components::CreatureCasting`**: the creature's casting state (the move it is making, the turns it has gone
  on, where it set off for, the miracle it holds and its two beams, and a try that fizzled, kept until it is shown). It
  is created the first time the creature starts a going near, getting away or turning, fizzles, or casts.
- **The mirror table.** `CreatureRig::MirrorBones` reads the species file's last numbers as the table, refused when one
  is not a bone, and refuses a version 20 file, whose start can't be told from its end; the animation's mirror is that
  table, and the rest pose's pairing only without one.

**The creature spells' particles**: `Particles/ParticleCreatureSpellRules.cpp`, the maths in
`Particles/CreatureSpellMaths`, the body in `ECS/Systems/Implementations/ParticleWorldLiving.cpp`, the spell's side in
`Magic/Core/Spell.cpp`'s sink. The swing is `particles::maths::SwungTooFar` with `gutils::ModelInverseSquareRoot` (the
original's bits for the 32 oracle inputs) and the cosine in double. The hearts and the steam are
`particles::maths::HasAtomRate`, `AtomsDue`, `FlungVelocity`, `SteamStart` (`k_SteamSpread`, the original's constant)
and `SteamOffLand`, through `gutils::x87`; the map coordinates are `object_surface::LandTestCell` and the ground
`map_coords::ToWorld`, as the emitters' land test. `WispOrbit` and `ItchOrbit` go through `gutils::x87` in the
original's order. openblack's own guards: a species with no right eye or no mirror for it leaves the flies where they
are (the original reads the bone regardless); likewise a species with no right foot or no mirror for it, or a body not
posed yet, lets no steam out; a heart whose atom gives no point starts at that atom's place (the original's point is
then uninitialised stack); a heart from a creature's body, whose walk is not ported, is made and taken out again at
once (its making's draws stay, as the original's), once logged; a body with no bones yet gives an empty box (the
original's box is then uninitialised stack); the ratio is `creature_spells::Ratio`, clamped to 0..1;
`MaxDirectionChange` is none for a spell with no creature spell row (the original always has one).

**The creature spells' class**: `SpellClass::Creature` (`Magic/Spells/SpellCreature`) holds a miracle on the creature it
is cast on and runs each creature turn ([the creature's openblack section](overview.md#openblack)); cast at a place it
does what a plain `Spell` does. The creature cast rule on an object is `cast_rules::CanCastOn`. The deed a miracle shows
is chosen by `magic::MiracleDeed` (`Magic/MiracleDeeds`, checked against `fn_004E9DF0`'s jump table for every magic
type), which the magic system's `PlayerAffected` calls (`MagicSystem.cpp`).

### Test hooks

- The testbed's `TellCast` plans the cast action of a miracle as told, and its `KnowMiracle` teaches the miracle as the
  scripts' `CREATURE_SET_KNOWS_ACTION` does (`SetKnowsAction`: known, the sightings set to the times its species must
  see it, cut towards zero), so that the debug tool and the scripts make the same mind.

## Pending

- **The spell to amuse** ([the original](#the-spell-to-amuse-another-creature)): the best desire about another
  creature as `AssessParticularObject` weighs it (each desire's `IsApplicable` and object predicate on a creature, the
  activity-object trees, the believed place, the own-player anger flag mind +0x1D488) is approximated by the planner's
  own weighing; a port row of its own.
- **Casting miracles** ([what openblack does](#casting-miracles-what-openblack-does), [the original's
  steps](#casting-a-miracle-the-originals-steps)):
  - `CopyPlanToPreviousContext(1)` (0x4F1600) of the fizzle's forced plan: what its int means is not decoded;
  - the gesture's reset of the sub-action counter (mind +0x1BDC, 0x506F10) and `AddVillagersToDance`'s (0x502D53): no
    cast has a gesture in the game's tables;
  - a pose that stops before its loop: the original's Perform waits until something stops it; queued as a fix;
  - the hold is a 16-bit word (Creature +0x58) that wraps above 32767 turns (3276.7 s at 100 ms); no seconds of the
    game's reach it; a ms per turn that does not divide 1000 (SET_GAME_TICK_TIME, for example 16 ms: 1000 / 16 = 62 in
    the original) must keep the whole division, and 0 turns a second gives a hold of 0;
  - the hold with no magic: the original's Perform holds without casting when the step's magic is NONE;
  - `SuppressOpposed` is gated on `ConstructSubActionsFromActionAgenda` succeeding (0x4DC210); queued as a fix; it also
    skips the diagonal, which the original does not (inert with the game's data, whose diagonal is 0) and does not do
    the `SetToZero` of the per-desire stack at mind +0x718 + 0x30 × i;
  - the fizzle's plan float `FLT_MAX` is not mapped to openblack's plan priority [?];
  - whether `StopWhatIAmDoing` draws in the fizzle's path (`StateSet` 0x484F4D and `GetNthActionForPlan` 0x4F1909 are
    conditional `GameRand` sites; after the failed sub-state's own stop the old plan's desire is 0x28, 0x475A91, so
    `GetNthActionForPlan` is probably skipped, L; `StateSet` is not read);
  - the cast pose's variant: fn_00484310's draw is ReverseAnim (+0x522C, the mirror table), unless +0x4A90; queued as
    a fix; what +0x4A90 is is not read;
  - the cast step's face: the original pulls Grimace for 2000 ms as the cast begins, as ours does now
    (`creature_face::Cue::Grimace` on the cast step); whether ours makes the face's own `GameRand(7)` draw there is not
    compared;
  - the fight's Cast state: the original's ms are the fight's own step (`ftol(physical +0x48 × ms per turn)`), ours the
    fight update's; the fight's step is not read; what the anim slot +0x47C0 holds in state 0x21 is not read;
  - the fight being ended: the original skips the cast at CastStart's end while 3D +0x5238 is set (fn_004845F0);
    queued as a fix;
  - a creature the hand may not hold: the turn's start (0x5DA950) locks it all the same, as ours now does, and the
    original's CREATURE hand state (19) strokes and slaps it; ours can send the leash click from that state but
    neither strokes nor slaps a creature its creature hand does not hold; queued as a fix. The frozen pose copy
    (+0x5270, [The body posed for the turn](pose-and-animation.md#the-body-posed-for-the-turn)) waits on the 3D
    creature's states;
  - the teleport rows (182, 196): the plan's object being the creature itself (the plan without a goal), the 150 m from
    home test and a creature's teleport cast through `CastSpellOnObject` are not ported; rows 136, 137 and 216 need no
    executor, their predicate never passing ([the teleport, warming and cooling
    rows](#the-teleport-warming-and-cooling-rows));
  - the beams' aim: what fn_0063E380 does with the magic's entry is not read;
  - the fingertip mean is not bit-exact in the game until the pose's extremity scales (fn_004EC0C0 scales the hand's
    local 3×3 by `1.5 − 0.25·clamp(s, 0, 2)`), the ±0.3 lean, the slots blended as locals and the `hypot` rows land; it
    is right at size 2 with no lean and a single slot 📼 1.20 only; the beams (the bone's own translation) do not depend on it;
  - the going near: the original goes on when the move is busy (1), moves again every turn while standing or walking,
    tests `IsObjectInMap` in its done rule, adds no radii for a point, uses `GetNavRadius` (equal to the 2D radius while
    physical +0x6C equals body +0x90, L), integer MapCoords distances, the confined-area rule of the clear-area search,
    and the failure's bookkeeping (+0x1220 list, fn_004D8DF0); not all are compared with openblack's moves;
  - the rows that cast from above: the planner chooses no instrument, so 326 and 327 are carried out only by a forced
    plan that names a tree; 240's second main sub-action 0x86 (134, no argument, 0x4B79B5) was read: its one stage
    0x500A10 only gives the creature the ground's temperature, as `PutOut` does; `Creature::FindClearArea` 0x4DFBE0
    also refuses a spot farther from home than the confinement radius when
    the creature is confined (0x4DFC2C..0x4DFC50), and so does ours for every caller of the search
    (`mind_detail::FindClearArea`, `creature_cast_moves::WithinKeptArea`). Open:
    whether a worship site is ever a town member (48's town path); how the food, wood and water effects use the
    process info of a creature's spell (+0x0C as where they fall from, +0x18 as the way down); `ShowFriendCreationSpell`
    0x4A75D0 (128, 129); which forcers name 98 and 246; 238 has its row (lane #544), the rest of the fire's rows are
    N-R-3;
  - the rows imported from raffclar's #467 and #469: their constructs are read
    ([the attack, shield and impress constructs](#the-attack-shield-and-impress-constructs)) and match but for the
    faces, the impress distance, the shield with no object and the looks of the going near, the getting away and the
    turn, corrected; the shield idles for a plan about nothing (+0x844, the agenda's belief about what the plan is
    for), not for an action with no object (+0x848), corrected with #477;
  - the cast at a point, imported from raffclar's #477 and read (above): whether `AddCastSpellSubAction` 0x4FF4B0 pulls
    a face of its own for the shield's cast is not read (ours grimaces); which rows other than the forest build
    sub-action 40 is not searched;
  - unread: fn_00474C50 (reaching a main sub-action), fn_004D8DF0, the face animation of fn_00480C60 (+0x49B0), the
    meaning of the 3D body's +0x4990.
- **The species file's mirror bones**: the table is read and used ([The mirror table](#the-mirror-table)). Left: a
  version 20 file's table (none of the game's files), which openblack refuses, and the block's other numbers before
  it. The hands' `creature_turn_pose::MirrorBone` still reads one entry of the tail itself.
- **The creature spells' particles** ([The creature spells' particles](#the-creature-spells-particles)):
  - the creature's surface for `SF_CreatureSpell`'s emitter and `SF_CreatureSpellCompassion`'s hearts (the render
    particle's body mesh walk) is not ported: the emitter lets nothing out, as from any creature, and no heart is made;
  - the 3D body's place (+0x78) is taken as the creature's Transform: not checked against the body's own update.
- **Creature miracles** ([Creature miracles (pending)](#creature-miracles-pending)), and everything that waits for the
  creature: enemy creatures in the heartbeat, the creature in water and forest. Landed since: the falling film's copy
  of the creature ([the copy of the creature](../magic/falling-spell.md#the-copy-of-the-creature)), the lightning's
  creature case and the tornado's faint (`TornadoSystem::CatchCreature`).
