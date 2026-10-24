# Fireball and lightning

## Summary

The fireball (FIREBALL and its two power-ups) is thrown from the hand or cast by a script, the computer or the
creature: a burning ball that bounces, sets fire where it lands and can be caught by another god's hand or fed into a
held fire seed. The lightning miracle (LIGHTNING_BOLT and its power-ups) channels a tree of forks from the hand to up
to a few targets in a narrow cone ahead of the camera, striking each with fire, and two bolts that meet clash. The
same bolt code makes the storms' lightning, the weather's fork strikes and the electric arcs over what a bolt struck.

openblack ports the fireball, its catch and feed, and the lightning with its fork tree, the cut by the land, the
clash, the hand offset, the weather's fork strike and the arcs faithfully. The fire they light is on
[Fire](../world/fire.md#fire). Their particle rules are on [Fireball rules](../physics/particle-rules.md#fireball-rules) and [Lightning rules](../physics/particle-rules.md#lightning-rules).

## The original

### Fireball and lightning

Everything below has been read in the exe except what is marked unverified or inferred.

#### The size of the fireball cast with the hand

`Spell::InitWithPos` 0x71FE50 gives the PSys the magnitude `SpellCastData[0]` without checking whether it is 0
(`PSysInterface::Create` 0x68E910 → `GJPSysInterface::Create` 0x68F3DA stores it in the manager +0xA0, which
`MagnitudeFloatProvider` 0x69DA90 reads). In `SpellSeed::Cast` 0x729520 that value comes from the gesture packet
(+0x14, fn_0071FA10), which is `GInterface` +0x1B8 copied whole into packet 0x12 (`SendApplyToMapCoord` 0x5D362D →
fn_00550E90 → format 15 of `SendPacketCompressed`, a 0x18-byte block, not quantised) and which **only the circle
writes** (0x5CF57A and 0x5D33BA; the `GInterface` is born zeroed). But right afterwards, `SpellSeed::DoPreCastThings`
0x729460 does `if (magicInfo.spellSeedType == FIRE) castData.magnitude = 1.0` (0x729502..0x72950B): the programmers
fixed the fireball at magnitude 1 whatever the gesture. info.dat leaves `GMagicInfo` +0x28 (`spellSeedType`) at −1 in
every row, but the game fills it at load with the first seed of each magic type (`fn_0042B400`, see
[dispensers and fireflies](worship.md#dispensers-and-fireflies)), so the branch is live for FIREBALL, FIRE_PU1 and
FIRE_PU2 (their first seed is FIRE, and only the FIRE seed has them). Without the branch the fireball would come out
with the size of the last circle, or 0 → 0.01 (4 cm, it barely heats) if one was never drawn; the user remembers
(2026-10-01) that a fireball cast from the hand **always came out big**, with any gesture, which the fill explains.
Result: atom scale 1 × 4.0168 of the root sprite, the fireball is visible in flight and sets fire to the house and the
tree where it lands. With `SPELL_AT_POS` the magnitude is still the script's radius (10 in the test), because it does
not go through the seed.

#### Fireball (MAGIC_TYPE 1-3, seed 2 FIRE)

- **`MagicFireBall`** (0x682970-0x683390; 0x60 bytes, vtable 0x93595C): an invisible Object **that is not in the map cells** (which is why
  `IS_FIRE_NEAR` does not find it), created by the rule `AttatchFireBallToAtom` and stuck to its atom. Its
  `FireEffect` starts at the effect's `initialTemperature · strength` and is what burns; the row of
  `GMagicFireBallInfo` (3 rows, 0xD4E3B0) is chosen by the upgrade level. It is deleted when it drops below
  `deletionTemperature` (the atom stays marked as deflected) or when its atom disappears.
  - **Catching**: `ValidForPlaceInHand` 0x682DD0 accepts any that is not the player's own; `fn_00682EA0` creates in
    the hand a new FIRE seed **already ready** (`fn_00729900(0)`) and charged, and the ball goes away.
  - **Absorbing**: `DeleteAndPutIntoSpellSeed` 0x682E10 multiplies the power of the held FIRE seed by `1 +
    catchIncreaseFactor`.
  - `MagicFireBall` is created by `fn_00682B70` (Object(pos, info); the scale comes with the first follow of its
    atom) at the head of the fireball list (g_game +0x205C9C). `MagicFireBall::ToBeDeleted` 0x682C30 takes it off that
    list, and its fire goes too. The ball dies with its atom through the atom data's dtor 0x682FA0.
  - The fire's temperature is the manager's strength (+0x54) × `initialTemperature`. The fire's player is the ball's
    (`MagicFireBall::GetPlayer` 0x682BF0: its manager's player).
  - Catching: `InterfaceTap` 0x682E50 and `InterfaceSetInMagicHand` 0x682E80 both lead to `fn_00682EA0`, which makes
    the seed with `fn_00728300` (seed 2, pu −1, multiplier 1). Absorbing is called from
    `SpellSeed::ApplyToMagicFireBall` 0x728AE0 and multiplies the seed's power at +0x8C.
  - **The heat row**: `AttatchFireBallToAtom::ModifyAtomCollection` 0x682FD0 calls `GetPowerUpLevel` (0x683096),
    takes `ecx = (eax == 0)` (0x68309B..0x6830A1), times the stride 0x10C (0x6830A6..0x6830AD), and passes
    `GMagicFireBallInfo::Infos + ecx·0x10C` (0x6830B1) to the ball's creation (0x6830BE).
    `PSysManager::GetPowerUpLevel` 0x673510..0x673526 is the spell's `GMagicInfo` +0x30, −1 without a spell. So the
    base fireball (level 0) takes row 1, its two power-ups and a ball with no spell take row 0, and row 2 is never
    used. The ball keeps its row at +0x28: the creation reads `initialTemperature` from it (0x6829D8), the follow reads
    `deletionTemperature` (0x683166, 0x683170), and the absorb reads `catchIncreaseFactor` from **the ball's own
    row**: `DeleteAndPutIntoSpellSeed` 0x682E10..0x682E3B is `seed[+0x8C] *= 1 + [[ball+0x28]+0x108]`, then
    `ToBeDeleted(0)`, and gives 1. info.dat's three rows: `initialTemperature` 6000 / 10000 / 14000,
    `deletionTemperature` 400 and `catchIncreaseFactor` 0.05 in each, so a seed that takes a ball in is 1.05 times as
    strong whichever row the ball has.
  - **The hand sees it** through an invisible draw collision. `Spell::DrawSpells` 0x7203F0 calls fn_00682950 at
    0x720400 every frame; it walks the fireball list (g_game +0x205C9C, next +0x54) through fn_00682F30
    (0x682950..0x68296E). For each ball whose atom (+0x5C) exists and whose atom alpha byte +0x8F is above 0x1E (the
    `jbe` at 0x682F44) it sends `GInterface::SendInvisibleDrawCollision(ball, the atom's drawn position +0xF4, the
    atom's +0x78 × +0x74)` (0x682F46..0x682F84): the radius is the rule scale times the base scale, the scale
    `AttatchFireBallToAtom` gives the ball (0x683155..0x683160). There is no player test, so every ball is under the
    hand's cursor, the player's own too. The atom's drawn position is written by the effect's post-update, so the
    collision is the position and alpha of the last step.
  - **The empty hand's press** (`StartGrab` 0x5D1740): `ValidForPlaceInHand` (vt 0x6FC) is 0 for the hand player's
    own ball (0x682DE4..0x682DF6), and `InterfaceMustBeInInfluenceForInteraction` is the Object's 0x4028A0 = 1, so the
    player's own ball, or any ball out of the influence, goes to the tap path 0x5D3930 (0x5D17A5..0x5D17B8).
    `InterfaceValidToTap` 0x682E40 is always 1 and `InterfaceTap` 0x682E50 asks the catch, which refuses the player's
    own. Another player's ball in the influence enters the grab (state 13, 0x5D17CD); `IsTuggable` is 0 (0x682A40) and
    the ball has its Game3DObject +0x40 (`Create` 0x6829AA → `CallVirtualFunctionsForCreation` 0x636BE0 → vt 0x538
    `Object::Create3DObject` 0x6365F0, which writes +0x40 at 0x636602), so it takes the 225 ms timer path: +0x4908 = 1
    and +0x450 = 1 (0x5D17E7..0x5D1808), with no `CHand::PickUp` (fn_0046DC30), the same branch as a big forest. A
    release within 225 ms (`State_Grab` 0x5D5250, ≤ 0xE1) taps it: `InterfaceTap` deletes the ball when the catch gave
    1 and returns 1 (0x682E5D..0x682E71). Held for 225 ms, `GenericPickup` sends the place in hand, and
    `PlaceObjectInMagicHand` 0x5DA6F0 calls the ball's `InterfaceSetInMagicHand` 0x682E80: the catch, the ball deleted
    if it caught, and always 0, so the ball itself never goes into the hand (the new seed does, from the catch).
  - **The place in the hand** (`GInterface::PlaceObjectInMagicHand` 0x5DA6F0): the object's `InterfaceSetInMagicHand`
    (vt 0x700) runs before the add, so a seed is ready when CHand caches its hold type (`SpellSeed::GetHoldType`
    0x728680: ready ? the info's hold : MAGIC). The caught fireball's catch (fn_00682EA0, from `MagicFireBall::InterfaceTap` 0x682E50 and its
    `InterfaceSetInMagicHand` 0x682E80) tests
    `IsHandReadyForObject` and `ValidForPlaceInHand`, makes the fire seed (fn_00728300), places it through
    `GInterfaceStatus::PlaceObjectInMagicHand` (the seed's InterfaceSetInMagicHand first, the hand takes it on 1), then,
    whatever the place gave, readies the seed (fn_00729900(0)) and adds its chants; both callers then delete the
    fireball. A successful place creates `Reaction::CreateReaction(status, 16 REACT_TO_HAND_PICK_UP, player, 1)`
    (0x6E3D70). `IsHandReadyForObject` 0x5DC890 also refuses while the status's +0x24 & 0x20 (locked select) is set.
    The seed's setup writes the status's +0x34 (the last seed type) and +0x38 (the turn of the last seed set in or out
    of the hand, fn_005DCA20). +0x38 is otherwise only zeroed by SetToZero (0x5DBABD) and saved and loaded (0x5DCD9A /
    0x5DD111); no game reader was found (it is one dword after +0x34's record in GInterfaceStatus::Save).
  - **The feed**: a FIRE seed in the hand takes in the ball under the cursor with no button. At the start of every
    power-up pass, after its early outs (0x5CF310..0x5CF35A), `GInterface::ProcessPowerUpSystem` 0x5CF300 takes the
    first object in the hand and the cursor's object (GInterface +0x400, 0x5CF360..0x5CF36D); when the first is a
    SpellSeed (vt 0x4C4) whose seed info is row 2 FIRE (fn_0072B200: (info − 0xD9D678) / 0x190) and the second
    `IsMagicFireBall` (vt 0x218), it calls fn_005D30D0 (0x5CF373..0x5CF3AA), the hand's own "send the apply to an
    object" (its other callers are the holding press 0x5D1684, the apply on release's release 0x5D490D and the locked
    apply on an object, state 11, 0x5D4D58). The pass runs once a frame and once a turn.
    `SpellSeed::CanCast(Object*, GPlayer*)` 0x729190 tests a return point of the hand's player first, then, for a
    MagicFireBall, only `ValidToApplyToMagicFireBall` 0x728A20 (0x7291EF..0x729210): `GetSeedType() == 2 &&
    object->IsMagicFireBall()`, with no player test, so the player's own ball is taken in too, and no castOnObject or
    cast rule. The apply packet's `SpellSeed::ApplyThisToObject` 0x728D10 tries the dispenser (0x728C80), the worship
    site (0x728A50), then `ValidToApplyToMagicFireBall` → `ApplyToMagicFireBall` 0x728AE0 (0x728D90..0x728DB7):
    `dynamic_cast<MagicFireBall>` and `DeleteAndPutIntoSpellSeed`, 1. The handler's `ApplyChangesToHand` 0x5DA100
    takes 1 as "kept": the seed stays in the hand, stronger, and the ball goes. The new power is the exe's to the bit
    (run natively with the oracle: `1 + factor` rounded to a float, times the power rounded to a float).
  - **The action state after the feed.** fn_005D30D0 ends in action state 18 (fn_005D2980(0x12) at
    0x5D32B5..0x5D32B9, the interface's +0x44), both after it sends the packets and when an apply packet still waits
    (+0x444, 0x5D313D). State 18's process 0x5D4D90 does only `if (+0x39 & 0x40) ResetActionState`. The button
    messages latch their bits in +0x39 (press 0x5D9930: 8 and 2; release 0x5D9960: 0x40). The message loop
    fn_005D9A20 clears the whole dword +0x38 before each message while its low byte (the action in progress) is 0
    (fn_005D9A10, called at 0x5D9ACD), then runs the message's handler and the interface pass; otherwise a latched bit
    stays until a state process zeroes the dword: the NORMAL state 0x5D5190 does so on a release (0x5D51A3..0x5D51A9)
    before its press tests, and the press (`ActionPressed` 0x5D1330) is reached only from it (0x5D5207). The feed
    leaves the low byte at 0, so the release flag lasts from its message to the next message or the next NORMAL pass,
    not one frame, and a press made in state 18 is lost: state 18's process ignores it, and the next message, its
    release, wipes it before it latches 0x40, which ends state 18; the NORMAL pass after drops the release. State 18's
    table row (0xD18108, filled by 0x5D0890) has no end and needs no object (+0x24 and +0x28 are 0), so a cursor that
    leaves the ball does not end it. `InterfaceActionValidateObjectsForStates` 0x5D1010, at the start of every
    interface pass (fn_005D1120 0x5D1123), runs `EndAction` (fn_005D1260, which resets the action state) when the
    action's object +0x400 is no longer interactable (`IsInteractable` 0x5701B0 = !(+0xA & 1); 0x5D1070..0x5D10B8).
    The turn's interface pass (`ProcessGameInputs` 0x54C3D0: the superpackets, then `GInterface::Process` 0x5CEC10)
    runs after the apply's handler deleted the ball (`MagicFireBall::ToBeDeleted` 0x682C30 → `Object::ToBeDeleted`
    0x636670), while +0x400 still holds it from the last draw, so the feed's state 18 normally ends at the turn the
    ball is taken in; a press before that turn, or once the cursor left the ball before the handler ran, is lost.
    fn_005D2980 writes +0x44 without the previous state's end: a FIRE seed armed by a press (states 8 / 9, the apply
    on release) loses its armed throw when the feed takes over, and that state's end 0x5D4940 (`+0x38 = 0`,
    fn_005D27B0) does not run.
- **PSys rules**: `UpdateRuleGravityWithFloor` 0x6A1880 (the bounce uses `DampingHorozontalBounce` /
  `DampingVerticalBounce`, the ground drag and the impact sound per size class), `CreateWithInitialDirection`
  0x69E950, `AttatchFireBallToAtom` 0x682FD0, `EventAlways`, `SetAtomHasBeenDeflected` 0x6A26C0, `UR_SideSpin`,
  `AR_FadeAlphaWithHeightAboveLandscape`, `AddSubCollectionsToAtom`, `UR_Trail`, and the conditions for deflection,
  nearby water and steam. The deflection off shields is on [the shields' particles](shields.md#the-particles).
  - `CreateWithInitialDirection` 0x69E950, non-human cast (script, computer, creature): the new 30° solution is
    applied if `|v|² > 0.01` (`fcomp qword [0x8C7620]`, the double 0.01, at 0x69EC60) and `v.y / sqrt(vx² + vz²) >
    tan(0.52370351552963257)` (double [0x9375F0], `fptan` 0x69EC77), all in the FPU. The new time (0x69EC92..0x69ECC1)
    is `sqrt((Δy − D·tan)·(−2/g))`, with the radicand set to 0.1 if it comes out negative (Δ [esp+0x64] 0x69EB1B, D
    [esp+0x38] 0x69EB3F).
  - `CreateWithInitialDirection`, script / computer / creature cast: a ballistic arc from the gesture position to 0.8
    of the way to the origin, in max(0.025·D, 0.5) s (0x69EB43..0x69EB5A), v = delta / t − (0, −g·0.5·t, 0)
    (0x69EB62..0x69EC39); g is the group's `UpdateRuleGravity` +0x24, 30 without one (0x69E9A0). The gesture position
    G (`GetCurrentGesturePosn` 0x69E987) and the origin O (`GetOrigin` 0x69E9DB): when the origin is within
    x² + z² < 0.1 ([0x8AB22C], the sum in x87; 0x69EA33..0x69EA67) of the gesture position it is moved to the gesture
    position + (0.1, 0, 0.1) (0x69EA5C..0x69EAAA); the target is gesture + 0.8 (origin − gesture) (0x69EAB1..0x69EAE5,
    `push 0x3F4CCCCD`), delta = target − gesture (0x69EAF6..0x69EB26) and D = sqrt(dx² + dz²) × 0.025
    (0x69EB3D..0x69EB43).
  - fn_0069E6B0: the hand's direction pitched up 0.3 rad (clamped to ±π/2): the yaw `fpatan` of z over x (0x69E6BC)
    stays on the stack; the pitch is `fpatan` of y over `fsqrt`(x x + z z) (0x69E6D8) `fadd` the float 0.3, kept
    when above −π/2 and below π/2 (the floats 0xBFC90FDB, 0x3FC90FDB; not above, or unordered, gives −π/2; not below
    gives π/2); then `fcos` of the pitch stored as a float c, x = `fcos`(yaw) × c, y = `fsin`(pitch), z = `fsin`(yaw)
    × c, each product rounded once and stored (0x69E710..0x69E749). fn_0069E620: the hand's speed through (0
    → 0, 50 → 50, 450 → 200), 200 above 450; the sound size class comes from speed / 200 (fn_0069E610).
  - For the local interface's cast the ball's atom gets a `DrawOffset` (fn_006C7840) that draws it from the hand to
    the start.
  - `UR_SideSpin` 0x69E300 (+0x20 ScaleAngularVelocity, +0x24 MaxAngularVelocity, +0x28 TimeToFade): the first time,
    with a player behind the effect, w = curl (manager +0x58) × scale, clamped to ±max, then w·(w/max)²; every step the
    velocity turns about y by w·(1 − age/TimeToFade)·dt. 🔍 inferred defaults; the fireball file sets 0.5, 1.2, 1.5.
  - `AR_FadeAlphaWithHeightAboveLandscape` 0x6A4DE0: alpha from AlphaAtZero on the land to AlphaAtRefHeight at
    RefHeight above it (and beyond); by default 255, 255 and 100 (0x6BC010), and the throw files set all three
    ([the rule](../physics/particle-rules.md)).
  - `AddSubCollectionsToAtom` 0x69F220: once per atom (atom data +0x20), the NextGroups under it.
  - `UR_Trail` (MAC 0x6A40F0, first step fn_006A41B0, then fn_006A4280): NumAtoms atoms behind the parent atom along
    the history of its last NumGameTurnsToSpreadOver positions (a ring at collection data +0x24, the whole ring moved
    back along the velocity at 0x6A4308), covering at most MaxTrailLength × the parent's scale, spaced 0.5·k²
    (UseNonLinearSpacing) or evenly, the newest last; fn_006A7E30 gives the i-th newest sample and fn_006A8020 lerps
    samples. The first and last atoms take the HeadGroup / TrailGroup init (−1: no creator, not drawn). 🔍 inferred
    defaults.
- **The double constants** (all the .rdata addresses the magic, particle, fire, weather, effects and worship code
  cite that the exe reads with `qword ptr`): [0x8C7620] is the double 0.01 (above) and [0x900C70] of `MapShield` is
  0.30000001192092896 = 0.3f widened, not 0.3. Also: [0x8C79D8] (1e-4f widened: gestures, the utility effects),
  [0x8D45D8] (2π float widened: MapShield, Forest, Storm, SpellSeedGraphic), [0x9361E8] (−π/2 float), [0x9003C0]
  (0.99e30), [0x900AE8] (1.1), [0x980518] (1/30), [0x8AB260] (0.5), [0x8CF7D8] / [0x9375E8] (0.6 / 0.3 of the sound
  size).

#### Lightning

MAGIC_TYPE 4-6, seed 6 LIGHTNING_BOLT. `UR_Lightning` 0x6914C0 is a creation rule that maintains a structure of forks
while it is channelled. Per collection it stores a `UR_Lightning_CollectionData` (0xC0 bytes).

- **Origin and direction.** With `CastingFromHand` the origin is the gesture position (`GetCurrentGesturePosn`
  0x673600 = `PSysProcessInfo` +0x0C); otherwise, the parent atom. The heading of the cone is **fn_00673650 =
  `atan2(cameraForward.z, cameraForward.x)`**, that is the camera's forward (not the hand's heading; verified: the
  `PSysProcessInfo` starts at manager+0x24, so manager+0x3C/+0x44 are `cameraForward.x/z`).
- **Targets** (`fn_00690F50`, three modes):
  - from the hand (`fn_006901E0`): spiral of `4·ceil(R/10)²` cells of 10 m from the origin with R = `SearchRadius`
    (60 in the base lightning); valid is the available object that is not a spell seed (fn_00690090) and whose
    horizontal direction satisfies `dot(normalize(dx,0,dz), (cos heading, sin heading)) > cos(SplitAngle)`: a cone of
    half-angle 0.2 rad ≈ 11.5°. Stops at `MaxLightningObjects`;
  - if fewer than `MinLightningObjects` come out, it fills in with **ground points**: angle `heading + rand(pi/4)`
    (verified: it is `+rand(0, pi/4)`, not centred; the constant is 0x3F490FDB), distance `rand(0.6·R)`, y = terrain +
    2 (0x8AB478);
  - `TakeTargetsFromManager` takes the `SpellTargets`; the parent mode (fn_00690880) uses `DefaultSearchRadius` 20;
    the manager mode is fn_00690C70 (unread).
  - **A creature in sight takes every fork** (0x690721..0x690857, the end of the hand mode, after the ground points):
    each target that is an object (+0x14), with an object (+4, fn_00693BC0) that `IsCreature` (vt +0x34; only
    `Creature` 0x474080 answers 1), cast to `Creature`, with its fizz now `+0x12B0` (fn_0047ACC0) below 0.5, goes into a
    list. With any, every target i, ground points included, gets `+4 = list[i % n]`, `+0 = the game turn` and `+0x14 =
    1`; its ground point, +0x15, +0x16 and +0x18 stay. openblack: `StrikeOnlyCreatures` after `AddGroundPoints` in the
    hand mode, with `particles::maths::CreatureRetargets` (`test_lightning_maths`); the +0 turn has no field (no reader
    found).
- **Structure** (`CreateForkStructure` 0x691190): deletes the collection's atoms, creates **one** atom and hangs from
  it `2 · number of targets` subcollections of the `ForkGroup` (0x69119B..0x6911A6), each with `MaxJointsPerFork` atoms
  of `ParticleChainCreator`. It removes the interpolation from each fork (`and byte [edi+0x38], 0xFD` at 0x6912A1;
  also 0x691B42 and 0x692B1D): bit 2, 3 by default in the ctor 0x675CA2. fn_00679920 only interpolates between the
  last two steps with that bit (0x67999E); without it it draws the current PSR, so the lightning changes shape
  abruptly at each step (10 Hz). With `CastingFromHand` and the own hand (fn_00691B80) each joint of each fork receives
  a `DrawOffsetLT` (0x69130C..0x69134C; see below). When re-searching targets (fn_00691390) the structure is **not**
  rebuilt: if there are more targets than forks, the recursion stops (0x692786).
- **Order of each step** (`ActualModify` 0x6914C0): origin and heading; the first time, search and structure; the
  collision with another lightning (fn_006916B0, 0x6915E5); re-search (fn_00691390, 0x6915EE); the sound; **all** the
  forks are released from the root on **every** step, whether or not the manager is in state 2 (0x691651..0x691681);
  and only in state 2 `UpdateForkStructure`.
- **Each step** (`UpdateForkStructure` 0x691BB0):
  - the scale of the forks is `ForkScale · FP_ForkScale` (+0xAC);
  - field +0x18 of each target ("cooldown") goes down one by one (0x691C26..0x691C31; signed, `jle`). It is written
    when searching (`PSysRand(ftol(AverageLightmapLife / ([0xD01A38] · 0.001)))`, 0x6910C9) and at each strike
    (0x692935), but **nobody reads it to skip a target**. It is just data;
  - without collision with another lightning it picks each active target with probability `min(+0x68, (N+1)/2) / N`
    until reaching that number (0x691C5B..0x691CF3). If the lightning is linked with another (data +0x20 or +0x24, the
    collision of fn_006916B0 / fn_00691AD0) it picks **one**: the first active one from `PSysRand(N)`
    (0x691CF5..0x691D56);
  - before the recursion, `PSysRand(2 · forks)` picks the collection of +0xA4 (0x691D98..0x691DC3), which nobody uses;
  - if none is picked, that step has no forks (0x691D59..0x691D6E);
  - with `SoundLightning` (+0x80) not NO_SOUND, every step the root atom's sound of that action (`GetSoundOfAction`
    0x674550) is started when it has none while the manager is in state 2 (`StartSound` 0x6745D0), and stopped when
    it has one otherwise (`StopSound` 0x674500) (0x691601..0x69164C);
  - re-searches (fn_00691390) when `RenewSearchEvery` > 0 and the age since the last search (+0x5C, set to 0 at
    0x690F66) exceeds it, or with `RenewTargetsOnMove` if the origin has moved more than `RenewTargetsOnMoveFrac · R`
    from +0x3C. fn_006916B0 is not "re-search": it is the collision of two lightnings (list 0xD4EC60), see below.
- **Collision of two lightnings** (fn_006916B0 / fn_00691AD0):
  - the data of each collection goes into a global list, the newest at the front (0xD4EC60, count 0xD4EC64; ctor
    0x68FED0..0x68FEEE), with the creation turn at +0xB0 (g_game +0x205A40, 0x68FEA5) and `CastingFromHand` at +0x59
    (0x68FECA). The dtor fn_0068FF50 takes them out and undoes the links;
  - each step, a lightning cast from the hand and not linked (+0x59, +0x20 = 0) sets +0xB4 to 0 and searches the list
    for an **older** lightning (+0xB0 smaller; one from the same turn does not count) cast from the hand, with targets
    (+0x74) and without a link or linked with it. With d1 = its centroid (+0x48) − its origin and d2 = its centroid −
    this origin, the collision point is `T = centroid − 0.7 · 0.5 · (normalize(d1) + (cos h, 0, sin h)) · min(|d1|,
    |d2|)`, h the heading of this lightning (0x6918E8..0x6919F3). They are linked if the headings go towards the same
    side (`cos h' cos h + sin h' sin h > 0`, 0x6918AF..0x6918E2), `|T − origin| < SearchRadius` (fn_006901D0) and
    `normalize(T − origin)` is less than 1.0367 rad from h (double 0x936D90, 0x691A47..0x691A72). Otherwise, the old
    one that was linked is unhooked;
  - linking (fn_00691AD0(old, new, CommonGlowGroup)): old +0x20 = new, new +0x24 = old, new +0xB4 = T, and if the old
    one had no link, a `CommonGlowGroup` on the last joint of its fork 0 (fn_00691B50) without interpolation
    (0x691B42). On unhooking, that joint loses its subcollections (fn_00673B20);
  - in fn_00691F30, depth 0: the new one (+0x24) takes the trunk to its +0xB4 and there it **stops** (neither branches
    nor strike, 0x692110..0x692136); the old one (+0x20) takes the trunk to the new one's +0xB4 and continues with
    **one** more fork, depth 1, from there to its target with the scale × this +0x60 = **3.0** (only the ctor 0x690183
    sets it; it is not a property), so opaque and thick (0x69213B..0x692172, 0x6927EC..0x6928EA). Its strike has
    strength 2 and the same event also goes to the new one's spell (fn_00690070, 0x692AA0..0x692AB9);
  - `CommonGlowGroup` (the scale-5 glow of SF_LightningBolt) is **only** used here (0x691AAF is its only read);
  - the original touches the other lightning's atoms in the step of the one searching.
- **Search**: spiral `GUtils::Spiral` 0x74D7E0 with `dir = count = 1` (0x6902A7..0x6902BB): it first updates (`--count
  == 0 → dir++, count = dir/2`) and then returns `table[dir & 3]`, so the first step is −x. fn_006901E0 walks
  `4·ceil(R/10)²` cells (0x6902A4) and fn_00690880 (storm) only `ceil(R/10)²` (0x690906, without ×4). An object only
  counts in the cell where its position is (fn_00604F40, 0x6903AE / 0x6909B4), so a multi-cell object does not enter
  twice. `SearchAround` (fn_006901E0 / fn_00690880) asks `IsAvailable` (vt +0x2C, 0x69038E / 0x690997) before the own
  cell and fn_00690090. It is `GameThing::IsAvailable` 0x401810 (not being deleted) and, for a villager,
  `Villager::IsAvailable` 0x751D50 (not DYING).
- **Fork tree** (`fn_00691F30(depth, data, fork, origin, list, scale)`, from 0x691DE6 with fork 0, depth 0, scale 1
  and the next free fork +0xA0 = 1):
  - centroid = sum of the tips of the active targets of the list / size of the list (0x692040..0x69210C);
  - with 2 or more targets the fork stops at `origin + (centroid − origin)·(0.2 + rand(0.4))` (0x69217C..0x6921F2);
    with one it reaches its tip;
  - joint i: `origin + (cut − origin)·i/(n−1)`; the inner ones are displaced `(rand(2·RandomFrac) −
    RandomFrac)·length` **in X and Z** (0x6924D5..0x692511; not Y and Z);
  - scale from `S/(depth+1)` at the root to `S/(depth+2)` at the tip, `S = scale · ForkScale · FP_ForkScale`
    (0x692366..0x69239E, 0x692515..0x692545): the trunk goes from S to S/2;
  - alpha `128 + PSysRand(127)` on all forks; 255 only if the branch's scale is > 1, which only happens in the
    collision of two lightnings (×3.0 of +0x60) (0x6923FF..0x692418, 0x692548..0x69257C);
  - with 2 or more targets it splits them by the sign of `dot((tip − cut).xz, (centroid − origin).xz)`: > 0 to the
    first list, the rest to the second (0x6925B9..0x6926F4). If one comes out empty, it keeps the last of the other
    (0x6926FA..0x69276A). Then it recurses with depth+1 from the last joint, first with one list and then with the
    other, each with the next free fork (0x69276D..0x6927DA, depth-first). It stops if there are no forks left. `2N−1`
    forks come out for N targets struck;
  - with one target, strike (0x6928F9..): +0x18 = `PSysRand(AverageLightmapLife/dt)`; **fn_00691E80** creates the
    `LightMapGroup` atom (`PCreatorLightMapAtom`) at the centroid of the list, which is the tip. fn_00691ED0 (it is
    not the light map) queues the target's object for its electric arcs (fn_00692FA0; the slot it asks, vt +0x1AC of
    the object's 3D object, is `IsAnimated`, not an impressive value, though earlier notes named it
    `GetImpressiveIntensity`), once per target when +0x5C > 0.2 (see
    [the arcs over what the bolt struck](#the-arcs-over-what-the-bolt-struck)). Then comes the `SpellEventInfo` type 3
    at the tip (fn_00691E00) with **strength 1**. It is 2 only if the lightning is linked with another (data +0x20,
    0x6929DE..0x6929EE). It is not 2 when the lightning comes out of a parent object. The same event also goes to the
    manager of the linked lightning (0x692AA0, fn_00690070).
  - On entry, the fork is re-hooked to the root (0x691F6D..0x691F7D) **before** the tests: if it then exits without
    placing it (it lacks the `DrawOffsetLT`, 0x692007, or the terrain, 0x692262) it is drawn with the joints of the
    last step that placed it.
  - Shield on the segment (0x692268..0x692361): `fn_006D0BC0(cut, 2.5)`; inside one, the entry point of the segment
    from the origin (vt 0xFC `FindIntersect`, margin 2.5), a `SpellEventInfo` 4 there {no movement, strength 1 (2
    linked), no shield test, target the shield's spell (fn_006D0B10)} to the lightning's spell; if it answers 0, the
    fork ends at that point (neither branches nor strike). The spark on the shield (fn_006D0AF0) always goes.
  - `NumTexturesToTile` (this +0x48, ctor −1): if it is not −1, the fork's chain is cut into `max(1, ftol(NTT ·
    length / data+0x60))` repetitions (0x6923D0..0x6923FC). data +0x60 **does** have a writer: fn_00690F50 sets it to
    the distance from the origin to the centroid of **all** the targets (+0x48 = sum of tips / N,
    0x6910BD..0x691177); the 1.0 of the ctor (0x68FE7F) only holds until the first search. Used by
    SF_LightningStrike / SF_LightningStormPush (15).
  - The tip position is `fn_00691E00`: the object in its map cell, height `GetAltitude + (+0x1c) + GetHeight`; a
    ground point, itself.
  - **Without a spell** (the manager +0xA8 == 0: the weather's fork strike, or a script's spot visual 35
    LIGHTNING_STRIKE, SF_LightningSingleStrike, started without a spell) the tip applies an effect itself instead of
    the event (checked in the disassembly, 0x6929FB..0x692A9B: `if (+0xA8) PSysManager::SpellEvent 0x6734C0 else if
    ([0xC029D0])`). The gate [0xC029D0] is 1 in .data and only read. A local `EffectValues` (vtable 0x8D8BA8, zeroed,
    fn_00525500: multiplier 1), `fn_005250A0(&info 0xCC9704, 0, 0)` (fn_005250D0 copies the 7 numbers and the radius;
    no applier, no player), then `fn_00525490(&tip)`: the tip through `MapCoords(LHPoint)` 0x603160 / `Set` 0x603340
    (the altitude is y − GetAltitude), then `EffectValues::ApplyEffectToMapPos` 0x525100. **0xCC9704 is row 11** of
    the GEffectInfo table (0xCC94C8, 12 rows of 0x34, ctor 0x524D40; 0xCC94C8 + 11·0x34), `EffectInfo::WeatherLightning`.
    Its values in info.dat: burn 10000, crush 0, hit 0, heal 0, fly-away 0, alignment 1, belief 0, radius 1 (mesh
    343), so a strike without a spell sets on fire whatever is within 1 m of each tip, and blames no player. The
    MapCoords is built once from the tip, so ApplyEffectToMapPos sees x and z rounded once.
- **Cut with the terrain**. In fn_00691F30 0x69220C..0x692262, after choosing the cut point and before the shields:
  - `LH3DIsland::RayCast` fn_00802550(origin, cut, &x, &z) converts both points to cells (x, z · 0.1 [0x8AC404], y /
    0.67 [0xC3720C]) and calls `RayCastInternal` fn_00802680. If it hits, the point in metres (× 10 [0x8AB414],
    globals 0xE9CD80 / 0xE9CD84).
  - `RayCastInternal` casts a **ray**, not a segment: it moves the end to `end + t·(end − start)`, with t the first
    map edge (0 or 512) in x or in z, that is, up to the edge plus one segment (0x802683..0x802765). The guard `t ==
    1e20` compares the float 1e20 (0x60AD78EC) with the double 1e20 (0x9A2BE0), which are not equal: the end moves
    **always**. Then it clips it to 0.1 .. 511.9 (Cohen-Sutherland, first x and then z; after clipping in z, the code
    of that end becomes 0 without looking at x again) and walks it cell by cell with the four DDA variants
    (0x802C80..0x803078: slopes `dx/dz`, `1/(dx/dz)`, `dy/dz`, `dy/dx`; the z edge is crossed if the x of that
    crossing stays in the column, otherwise the x one). A single column or a single row tests each cell with the
    whole segment.
  - The cell test fn_0083AE80 reads the four corners of the block itself (cells +4, +0xC, +0x8C, +0x94, also the edge
    row). If both ends are below the lowest corner or above the highest, there is no hit. Otherwise, it tests the
    plane of the two triangles (0.0)(1.0)(0.1) and (1.1)(1.0)(0.1):
    - u, v within −0.1 .. 1.1 [0x8C9B2C / 0x8AB230];
    - `u + v ≤ 1.1` in the first and `≥ 0.9` [0x8C5844] in the second;
    - the point in front of the start;
    - a denominator smaller than 1e-4 (qword 0x8C79D8) in the first triangle ends the test without looking at the
      second.
  - Without a hit, a descending ray (end.y ≤ start.y, |dy| ≥ 0.0001 [0x8BF518]) gives its crossing with y = 0. It only
    counts if it is ≤ 7500 m (`dx² + dz² ≤ 5.625e7` [0x9A2BD4]) from the camera (`g_camera` 0xEA1DB8;
    0x8025D9..0x802672).
  - If the terrain is closer to the origin (in x z) than the cut (`fcompp; test ah, 1`), it exits **the whole**
    function (0x692262 → 0x692ABE). That fork is not repositioned: it was already re-hooked on entry (0x691F6D) and is
    drawn as the previous step left it. There are no branches, strike or shield test either.
  - The other seven callers in the exe: `GCamera::Update` 0x442406, fn_0044EF60 0x44F046, `CameraModeNew3::Update`
    0x45DA4A, `GLandscape::Draw` 0x5E4848, fn_005E5620 0x5E5660, fn_00800C30 0x800D79 and fn_0086BD00 0x86BF1C.
- **Cooldown**: `PSysRand` 0x6729E0 is the function at [0xD4E0BC] (`GameRand` 0x672AF0 or `LocalRand` 0x672B40,
  depending on fn_00673340):
  - with n = 0 it gives 0 without rolling (0x510693 / 0x6DE574);
  - otherwise, `LHRand % n` **unsigned** (`div` 0x7DB62B);
  - the `ftol` of `LightmapSteps` reaches it as is (0x691091 / 0x69292F). A turn of 0 ms gives +inf, the ftol gives
    0x80000000 and the roll comes out in 0 .. 2³¹−1;
  - the earlier note ("no cooldown") was inferred and was wrong. The same draws are at 0x691CF9, 0x691DA0 and the
    alpha `PSysRand(0x7F) + 0x80` at 0x692551.
- **Numbers per event** (confirmed in the log): PU0 burn 800, crush 0.0006, hit 0.0017, radius 1 m; PU1
  1400/0.0011/0.003; PU2 2000/0.0018/0.0075.
- `LightningForkFlicker` 0x6B24D0 / 0x6A1000 (its property `FlickerFreq`): **unused**. None of the 132 spell files
  uses it.
- **Ribbon colour** (the chain creator). Creator default values: `FrameHeight` 64, `FrameWidth` 32 and
  `NumTexturesForWholeChain` −1 (ctor 0x6AA739..0x6AA747; with −1, `CreateChain` sets n−1 at 0x6AA8DF). With them,
  fn_006C8920 sets the **U across the width** `[(frame+FileOffset)·W, +W]/256`. The frame is `FrameOfHead` in the last
  repetition, `FrameOfTail` in the first and 0 in the others. The **V goes along the length**: `H/256 · local segment /
  segments of the repetition`. The lightning thus uses strip 0 of `S_lightning` (white core with a cyan halo) repeated
  4 times. The vertices are `joint ± side·scale` (half-width = scale, 0x67BA3B..0x67BB0D). The ends of neighbouring
  segments are joined at their midpoint (0x67BD2D..0x67BE82). The V offset (0x67BE91..0x67BED5, with `fmod` by
  `H/256`) is 0 because chain +0x4C = 0 (inferred: no other writer has been looked for).

The lightning's ribbons (`ParticleChainCreator`) and light maps (`ParticleLightMapCreator`, whose `ShiftX` / `ShiftZ`,
about 10, offset the stamp) are in
[chains and light maps](../physics/particle-creators.md#chains-and-light-maps).

##### DrawOffsetLT

When it is cast from the own hand (fn_00691B80 = `CastingFromHand` and `NetUnsafeIsMyInterfaceCasting` 0x673540: the
+0x44 of the spell, 1 without a spell), each step calls `SetRefPos` 0x6C7600 (vt 0x100) on each joint of the fork with
`(the step's origin = data +0x30, weight w − w·i/(n−1))`, w = 1 on the **trunk** (depth 0) and **0** on the branches
(0x691FC7..0x69203E), with the weight clipped to 0..1 (a NaN gives 0). Each frame, `GetOffset` 0x6C7690 gives `(my
interface's hand position now, GInterface +0x3A0 = CHand, +0x78, − the step's origin) · weight`, which fn_00679920
adds to the drawn position, interpolated or not (atom +0x124, 0x679B69..0x679BBF): the start of the lightning stays
stuck to the hand between steps and the tip does not move. `DrawOffsetDecay::ProcessList` 0x68F680 has nothing to do
with it: it is the clock of the other subclass (`DrawOffsetDecay`). Other rules that create `DrawOffsetLT`:
`UR_FollowCastPosn` 0x69FEAD (the origin sprite of SF_LightningBolt) and `UR_HandSprinkle` 0x6A046B.

##### The weather's fork strike

`UR_LightningStrike` 0x6937A0. Only `SF_LightningStrike` uses it: `SF_LightningSingleStrike` is `CreateRuleAnAtom` +
`UR_Lightning` with no `UR_LightningStrike`. It is the root rule (group 0, the only rule of that group) of the
always-on utility effect PSysUtilityPSys +0x14, stepped last of the per-turn utility slots with NET_GAME_TYPE 1, so its
draws are synced ([utility effects](hand-casting.md#utility-effects)). It makes one atom with its `NextGroups` (where
the strike's `UR_Lightning` lives) and starts its `SOUND_SPELL_LIGHTNING` once.

- **The queue.** The storm's fork callback [0xEEA384] = 0x68E8F0 (set by `PSysGlobal::InitializeOneTimeOnly`
  0x68F76D) is a 0x14-byte stdcall stub that only calls 0x693A80(&point, radius). 0x693A80 pushes {x, y, z, radius}
  (16 bytes) at the end of a growable array (data 0xD4EC30, capacity 0xD4EC34, count 0xD4EC38, grow step 0xD4EC3C =
  10) when the gate [0xC029E4] == 1 (1 in .data, only read). No draw, no sound. The radius is stored and never read.
  `PSysGlobal::OnClearMap` (0x68F839 → fn_00693770) sets the count to 0.
- **The step** (checked in the disassembly, 0x6937A0..0x6939CB): with no PCreator (+0x28) it returns 0 (detached) and
  leaves the queue. Else, while the count > 0, it takes the **last** record and decrements the count. If the live
  strike data count [0xD4EC88] is >= 50 the record is dropped (no atom, no draw). Otherwise `AtomCore::Create`,
  fn_00674DD0 (the atom joins the collection with its NextGroups {1}), `atom+0x128 =
  PCreator->CreateParticle(atom)` (0x69381A..0x693823: the rule's +0x28 is `AtomCreateRule::PCreator`, a
  `ParticleCreator`, whose slot 0x10 is `CreateParticle(AtomCore*)`, e.g. `ParticleSpriteCreator::CreateParticle`
  0x6AA010, `ParticleBaseChainCreator::CreateParticle` 0x6AA760; the atom's render particle, the step every create
  rule shares; a purecall in the base vtable 0x937C28), the 0x30-byte AtomData (fn_00693A00, which increments 0xD4EC88; its dtor 0x693A40 decrements
  it), `data+0x20 = point`, `data+0x2C = PSysFloatRand(0.4, 0.8)` (0x6729C0, `0.4 + rand(0.8f − 0.4f)`: **draw 1**),
  the atom's position (+0x80) = point, the rule's `SoundLightning` (+0x2C, 24 bytes) copied, `r = PSysFloatRand(1.0)`
  (**draw 2**), the size slot (+8) = 3 when `r < 0.3f`, 2 when `r < 0.75f`, else 1 (float constants 0x8AB23C /
  0x8AB274, `fcom; test ah,1`: a NaN counts as below), the flags byte (+0x14) `|= 0x22` (delayed and snapped to the
  ground) and `AtomCore::StartSound`. Then, for every atom of the collection, `GetAtomAge(atom) > data.life` deletes it
  (`test ah,0x41; jne` keeps it at age == life), with its sub-collections (the forks). It returns 1: the rule stays
  attached and the effect never finishes. With a turn of 0.1 s a strike lives 4 to 8 turns.
- So **an empty queue creates nothing and draws nothing**.
- What the strike atom then does is the strike's `UR_Lightning` (group 1) in parent mode around the cloud: the search
  radius is the default 20, since the queued radius is never read.
- `atom+0x128 = PCreator->vt+0x10(atom)` is not specific to this rule: `CreateRuleAnAtom::ModifyAtomCollection` makes
  the same call at the same point, right after fn_00674DD0 (0x69F43C..0x69F445, `mov [ebp+0x128], eax`).
- The ageing loop's other branch: an atom of the collection with no strike data gets one from fn_006939D0 (the counter
  incremented, the point (0, 0, 0)) with its life +0x2C left uninitialised. In the game's files only this rule makes
  the atoms of its group, so the branch is not reached.

##### The arcs over what the bolt struck

`UR_ObjectArcer` 0x6933E0, checked in the disassembly and pinned with the exe oracle. Only `SF_OnFire` uses the class
(the only one of the 132 spell files): group 2, `NextGroups {3}` (the ribbon, `ParticleChainCreator0`,
`S_Lightning.raw`, additive), `AverageTicksPerUpdate 2`, `JointsPerArc 8`, `RandomFrac 0.1`, `RandomTangents 0.2`,
`ScaleTangents 3.0531`, and a `RemoveRuleProb` on group 2 (`EventConditionAtomDelay` 3.12832 s, `RemoveFreq` 1.1681)
that takes the arcs away. `SF_OnFire` is the always-on utility slot PSysUtilityPSys +0xC (37), stepped once a turn on
the synced stream ([utility effects](hand-casting.md#utility-effects)).

- **The rule** (0x44 bytes, `AtomCreateRule` + +0x2C `AverageTicksPerUpdate`, +0x30 `RandomFrac`, +0x34
  `ScaleTangents`, +0x38 `RandomTangents`, +0x3C `JointsPerArc`, +0x40 `ChainCreator`; `DefineProperties` 0x6B2A90).
  The defaults before the file's properties are 10, 0.1, 1.0, 0.2 and 19 (the ctor); the property ranges (0..50,
  0..0.2, 0..5, 0..0.8, 5..20) are not clamps. Being an `AtomCreateRule` (flags |= 6) it counts as a creator: the
  effect never finishes.
- **The queue** (data 0xD4EC48, capacity 0xD4EC4C, count 0xD4EC50, grow step 0xD4EC54 = 10; 8-byte records {turn,
  object}). Records are pushed only by fn_00692FA0 (the count's other writers are its static init fn_00692EC0, the
  clear fn_00692F30 and the rule's pop), called only by fn_00691ED0, which the bolt calls once per target in
  `fn_00691F30` when the target's +0x16 is clear and the search is more than 0.2 s old (0x692951..0x692971; it sets
  +0x16 first). fn_00691ED0 queues the target's object (+4) when the target is an object (+0x14), the object has a 3D
  object (`Object::Game3dObject` +0x40) and that 3D object's `IsAnimated` (vt +0x1AC) is 0. fn_00692FA0 then pushes
  {the game turn, object} at the end when the gate [0xC029E8] is 1 (1 in .data, read by direct address only) and fewer
  than 100 arc data live ([0xD4EC84], incremented by the AtomData ctor fn_00693590, decremented by its dtor 0x6935E0).
  `IsAnimated` is 1 only for the 3D objects made as `ANIMATED` (`LH3DObject::Create` 0x80B4D0 type 2, vtable 0x9A32A0,
  slot 0x80B900 `mov eax, 1`); the static, morphable and complex ones give 0 (0x7F9BE0). An object's type is
  `ANIMATED` when its mesh has bones (`Object::Get3DType`: `(flags >> 7) & 2`, the L3D flag 0x100); the classes that
  override it (BigForest, CitadelPart, Creche, Flowers, Football: MORPHABLE; CitadelHeart: CITADEL) are never animated.
  `PSysGlobal::OnClearMap` (0x68F834 → fn_00692F30) sets the count to 0.
- **The step** (0x6933E0..0x693587): with no PCreator (+0x28) it returns 0 (detached), the queue left as it is. Else,
  while the count > 0: `AtomCore::Create`, fn_00674DD0 (the atom joins the collection with its NextGroups),
  `atom+0x128 = PCreator->vt+0x10(atom)`, the 0x84-byte AtomData (made when the atom has none), its +0x20/+0x24 = the
  **last** record (the count decremented), fn_00692F60 (the object's `IsAvailable` vt +0x2C: 1 refreshes +0x20 with
  the turn, else the object +0x24 is cleared; the turn is never read), fn_006930A0 (the points, below); with no points
  (+0x30 = 0) the atom is deleted (`DeleteFromAtomList`). Then for every atom of the collection, the new ones included,
  from the head: its data (made when missing, then with no object), fn_00692F60, ticks (+0x50) − 1; no object: the
  atom is deleted; else when the ticks are below 0, `ticks = PSysRand(AverageTicksPerUpdate)` and fn_00693350 picks
  the ends: `a = PSysRand(n)`, `b = PSysRand(n)` (in that order), from = points[a] (+0x54), to = points[b] (+0x60),
  their normals (+0x6C, +0x78). An atom with no sub-collection (+0x1C) returns 0 at once (the rest of the atoms not
  stepped; never reached, every atom has its NextGroups). Else fn_00692AD0 lays the ribbon in the first
  sub-collection. It returns 1.
- **The points** (fn_006930A0): the object's 3D object's mesh (`GetMesh` vt +0xF8); the vertex count of every
  primitive of every sub-mesh summed (fn_00693330; none: no point); `s = PSysRand(sub-mesh count)` (every sub-mesh,
  whatever its LOD or status bits), `p = PSysRand(that sub-mesh's primitive count)`, both vectors (+0x28 points, +0x3C
  normals, 12-byte entries) set to 5, then 5 × `v = PSysRand(primitive vertex count)` on the 32-byte vertices
  (position +0, normal +0x14). With the 3D object's matrix (+0x14 rows x, y, z, +0x38 translation): the point is
  `(z·M2C + y·M20) + x·M14 + T38`, `(z·M30 + y·M24) + x·M18 + T3C`, `(z·M34 + y·M28) + x·M1C + T40`; the normal goes
  through **the translation too** (0x693238..0x69328A), with its y and z summed in another order, `(x·M18 + z·M30) +
  y·M24 + T3C` and `(x·M1C + z·M34) + y·M28 + T40`, then is made unit length unless it is zero (squares summed z, y,
  x; `1 / sqrt`). Far from the world's origin the "normal" is nearly the direction from the origin to the object, so
  the arcs leave and arrive along that direction, not off the surface. A primitive picked with no vertex (or a
  sub-mesh with no primitive) reads past its array.
- **The ribbon** (fn_00692AD0): when the sub-collection has no atom, fn_00692B00 resolves `ChainCreator` (vt +0x24;
  none: no joints), clears the sub-collection's interpolation bit (`and byte [+0x38], 0xFD`), asks the creator for its
  chain data (vt +0x28, `JointsPerArc`) and makes `JointsPerArc` joints (`InitNewAtom`, -1). Then fn_00692B80 (checked
  bit for bit with the oracle, 4 cases, 0 mismatches): L = `sqrt((dz² + dy²) + dx²)`, d = from − to; then `PSysRandR3`
  (0x6729F0: three `rand(2) − 1` until `(z² + y²) + x² <= 1`; cdecl, the argument left on the stack) for the from
  tangent, `T1 = (n1 + R1·RandomTangents) · (L·ScaleTangents)`, then again for `T2` the same with n2; the cubic
  (fn_00693C80, also checked, 4 cases) `A = ((−2·to + 2·from) + T1) + T2`, `B = ((3·to + −3·from) + −2·T1) + −T2`, `C
  = ((from·0 + to·0) + T1·1) + 0`, `D = ((from·1 + to·0) + T1·0) + T2·0`; each joint i of n, in list order: `t = i ·
  (1 / (n − 1))`, one more `PSysRandR3` R, the joint (+0x80) = `(((A·t³ + B·t²) + C·t) + D) + (R·L) ·RandomFrac`, t² =
  t·t, t³ = t²·t. So the tangents' two draws are made before the joints' and even with no joint. Every step is at the
  game's 24-bit precision with no sine or cosine, so float arithmetic in that order is exact.
#### More addresses from the code

- `MagicFireBall::DeleteAndPutIntoSpellSeed` 0x682E10 is called from `SpellSeed::ApplyToMagicFireBall` 0x728AE0 on a
  held FIRE seed.

## openblack

Status: **done** for the fireball, its catch and feed, the lightning, the weather's fork strike and the arcs; not
ported: the fireball's draw offset from the hand and the reaction to a hand pick-up. Port
decisions: [port decisions: magic](../project/port-decisions-magic.md).

- **The size from the hand**: `PrepareCast` (`SpellSeed.cpp`, was `DoPreCastThings`) tests `GetSpellSeedOfMagicInfo`
  of the magic cast. From 2026-10-01 to 2026-10-08 it tested the seed's own type when the row left the field at −1
  (inferred then); with the vanilla data the two give the same seeds.
- **The fireball**: `Magic/Objects/MagicFireBall.cpp`; the rules in `Particles/Rules/Fireball.cpp`. Its
  ballistic cast (`CreateWithInitialDirection`, `Fireball.cpp:391-398`) does the original's origin fallback and the
  target at 0.8 (the comment at :379-382 calling them unverified is stale); only the < 0.1 sum is a float, x87 in the exe (a last-bit boundary).
  `Particles/Rules/Fireball.cpp` gives the heat row as the original (closes decision 7.7; raffclar's row 0 is not
  taken, see [port decisions: magic](../project/port-decisions-magic.md#the-miracles-service)).
- **The thrown ball's direction**: `fireball_rules::PitchedUp` keeps the FPU's arctangents unrounded into their
  cosines and sines, in fn_0069E6B0's order (`gutils::x87::Atan2`, `SinCos`, see
  [Floating point](../engine/floating-point.md#the-runtimes-arctangent-arccosine-power-and-logarithm)): 10 000 of
  10 000 directions equal fn_0069E6B0 run whole in the oracle (`test_fireball_direction`).
- **The hand and the ball**: the ball's `handTarget` and `radius` (`MagicFireBall` component) are set from its atom
  after the spells stepped (`particles::fireball_rules::UpdateHandTargets`); `HandSystem::PickObjectAlongRay` sends their
  sphere (not inside the temple); the hand taps (`hand_tap::Register<MagicFireBall>`), grabs (the big forest's 225 ms
  timer) and places a ball (`ApplyPlaceInHand`'s MagicFireBall branch: the catch, then `EndAction`) through
  `fireball::Catch`, which follows the original's order. The power-up pass feeds a FIRE seed
  (`gestures::FeedsFireBall`) and the hand then holds `_applyObjectState` (state 18) until a release, or until the
  turn finds the object under the cursor gone. The one-off seed follows the place-in-hand order (2026-10-11). openblack
  does not keep the status's +0x38.
- **`CreateWithInitialDirection`**: the port compared with 89129 (the 4 low bytes of the double read as a float, a
  disassembler error already corrected), so it almost never applied the 30° solution (corrected, `Fireball.cpp`).
- **The double constants**: of all those cited in `src/Magic`, `src/Particles`, `src/ECS/Fire`, `src/ECS/Weather`,
  `src/ECS/Effects` and `src/Worship`, only [0x8C7620] was wrong (corrected) and, borderline, [0x900C70] of
  `MapShield`. `Audio/Services/SpellSounds.cpp` compares the 0.6 / 0.3 of the sound size in double, as the original
  ([0x8CF7D8] and [0x9375E8] are doubles).
- **The lightning**: `Particles/Rules/Lightning.cpp`. The collection data is an entry keyed on the rule's slot, like
  the fireballs, so that nothing is left dangling. The interpolation bit is `Collection::flags`. The forks are released
  outside state 2 too (the port used to leave them as they were: that was an error, now they are not drawn). The
  "cooldown" is no longer used as a filter: that was the main cause of the "goes in jumps" (corrected). The +0xA4 pick
  is only rolled. The cooldown draw is `RandomBelow(effect, n)` (was `PSysRand`), which is already
  `game_random::particles::Rand` ✅ faithful. `NumTexturesToTile` is `Collection::chainTextures`. `IsAvailable` is
  `ecs::villager::IsAvailable` for a villager; the manager mode does not use it in the port. The creature case
  (a creature in sight takes every fork, its fizz read by fn_0047ACC0) is ported:
  `StrikeOnlyCreatures` ([above](#lightning)).
- **The clash**: faithful except the glow: the port put `CommonGlowGroup` at the tip of each fork (inferred, without
  basis), removed. **Approximate**: the port leaves the other lightning's atoms for the next step of the affected
  lightning (`ApplyGlow`), so the glow may arrive or leave one turn late. A data entry whose collection stops taking
  steps (none this turn or the previous one, or 10 s of clock) leaves the list with the dtor's semantics.
- **The strike without a spell**: `particles::lightning::StrikeTip` (the sink's event, or these values), `NoSpellTipValues`
  and `TipMapPosition` (`Particles/Rules/Lightning.h`; the raw x and z, which `ApplyEffectToMapPos` rounds itself, and
  the altitude above the land), applied in `StrikeTarget`. Outside the game (no info constants or no entities: a test)
  it applies nothing; in the game the debug Magic window's Spawn starts its effect without a spell too, so previewing
  a lightning file there burns as a script strike does.
- **The cut with the terrain**: `LandIslandInterface::RayCast`, common API in `3D/LandIslandInterface.h` +
  `3D/Implementations/LandIsland.cpp` (`GetCellCorners`); the comment of `RayCast` lists the other seven callers for
  whoever ports them. **Approximate**: float arithmetic where the original keeps extended x87 values between the
  `fstp`s; it only changes something right at the edge of a cell or a triangle. Port guard: a cap of 4·512 + 4 steps
  in the walk. With `OPENBLACK_SPELL_TRACE` the trace "fork at depth N ... cut by the land at (x, z)" comes out.
- **`DrawOffsetLT`**: `Atom::drawOffset` in `ParticleEffect.h`, added in `Effect::Collect`/`CollectChains`; the hand is that of
  the `HandSystem` (the same one from which `PSysProcessInfo` +0x0C comes). `UR_FollowCastPosn`
  (`Rules/HandFollow.cpp`) and `UR_HandSprinkle` create no `DrawOffsetLT` yet.
- **The weather's fork strike**: `Particles/Rules/LightningStrike.h`, the rule in `Rules/Lightning.cpp`. The queue is
  `particles::manager::State::strikeQueue`, `lightning_strike::Queue` / `Clear` (in `magic::OnLoadMap`, next to
  `explode_object::Clear`). The storms' fork callback (`weather::storms::SetForkCallback`) is set to
  `lightning_strike::Queue` in `Locator.cpp`, right after the weather system is made (the particle system is made
  before it), and cleared before the particle system is reset. The live count is `particles::manager::State::strikeAtoms`,
  counted in and out by the atom's data, so it follows the atoms through an effect reset or a new land.
  `lightning_strike::StrikeSoundSize` / `StrikeSound` are the size and flags (`SoundAction::k_Delayed |
  k_SnapToGround`). The earlier port made one atom at the effect's origin (0, 0, 0) on the first step and detached;
  that atom never went, and its `UR_Lightning` drew from the synced stream every turn (9771 of the 16927 synced draws
  of Land 1's 140-turn fidelity run, 20876 of 36967 in the cycle run). The port of the shared create-rule step,
  `Effect::NewAtom`, applies the creator's part (creator, colour, `InitAtom`) but sets no +0x128 field. The ageing
  loop's unreached branch: the port leaves such an atom alone.
- **The arcs**: `Particles/ParticleArcRules.{h,cpp}`, the maths in `Particles/LightningMaths.cpp`. Until this port the
  class was unregistered, so `SF_OnFire` had no creator and ended at its first step, and `utility::ProcessTurn` made it
  again every turn. `object_arcer::Queue` from `StrikeTarget` (`Rules/Lightning.cpp`), the gate
  `object_arcer::Queues` (a model, no bones, fewer than 100 live), `particles::manager::State::arcQueue` / `arcAtoms`,
  `object_arcer::Clear` in `magic::OnLoadMap`. A model is the object's mesh of the mesh pack
  (`explode_object::PackMeshOf`); an object without one (a creature's body is not in the pack) is not queued. The
  points: `object_arcer::PickPoints`, `maths::ArcSurfacePoint`, `ArcSurfaceNormal`; the matrix is the object's
  Transform (rotation × scale, position), as the exploded meshes take it. A primitive with no vertex gives no point (a
  port guard; no game mesh has one seen). The ribbon: `maths::ArcLength`, `ArcTangent`, `ArcJoints`.
- **The ribbon colour**: `Creators/Chain.cpp`, `Graphics/RendererChain.cpp` ✅ faithful.

### Test hooks

- `test_magic_object_players`: the fed seed's power, to the bit.
- Testbed: `miracles.fireball_catch`, `fireball_catch_held`, `fireball_absorb`, `fireball_absorb_own`.
- `test_lightning` (`TEST_F` cases): the registered classes, the properties of `ParticleChainCreator` and `ParticleLightMapCreator`,
  the UV per segment and, with `OPENBLACK_GAME_PATH`, the real files `SF_LightningBolt`, `SF_LightningBoltPUTwo` and
  `SF_LightningStrike` (groups, counters, `SplitAngle`, `ForkScale`, the size of the light map's `.raw`); also
  `drawOffsetLT` (the weight clipping and `GetOffset`), `trunkJointsFollowTheHand` (each trunk joint with the step
  origin and weight 1 − i/(n−1)), `chainTexturesOverride` (`NumTexturesToTile`) and `twoBoltsClash` (two effects: the
  new one stops at the collision point, the old one continues with an opaque fork of scale ×3 and its strike of
  strength 2 reaches both spells) and `landCutsTheFork` (with land at height 30, 20.1 m, between the hand at 30 m and
  the ground point, the fork is cut and there is no strike; with land at 0, there is).
- `test_land_raycast` (7 tests) covers `RayCast` / `RayCastCells`: the slope, the ray that goes past its end point,
  above, the fall onto flat ground, the y = 0 near or far from the camera, the diagonal walk and the clipping off the
  map.
- `test_lightning_strike`: the queue, the size, the regression "an empty queue makes no atom and no draw", the newest
  point first with the draws in the order life then size, the life bounds and age == life kept, the 50 cap and its
  order against the ageing, the cap shared by every effect.

## Pending

- **The fireball's `DrawOffset`** (fn_006C7840) that draws the locally cast ball from the hand to the start is not
  ported, so the ball appears at the gesture point.
- **Unverified**: the defaults of `UR_SideSpin` and `UR_Trail` (inferred; their constructors are not read). Verify
  with the rules' ctors. (The ballistic target at 0.8 and the origin fallback of
  `CreateWithInitialDirection::ModifyAtomCollection` 0x69E950 before 0x69EAF0 are settled, above.)
- **The fed FIRE seed's gesture loop sound**: whether the original stops an armed FIRE seed's gesture loop sound when
  the feed replaces its state (openblack leaves it, as that state's end does not run).
- **The place in the hand**: a successful place creates `REACT_TO_HAND_PICK_UP` (0x6E3D70), which openblack never
  creates (queued fix F-74); the status's +0x38 matters only when saved games are ported. `IsHandReadyForObject`
  also refuses while a creature is locked for the hand (the status's +0x24 & 0x20), which openblack tests too
  (`HandSystem::_statusCreatureLocked`, [each turn](../hand/states.md#each-turn)); openblack asks only the original's
  two conditions, nothing held and no creature locked (since 2026-10-22; it had also refused a caught ball while a
  tree was being pulled or a locked select on a pile, a field or a fish farm went on).
- **Lightning**: `LightningForkFlicker` 0x6B24D0 is unused (nothing to port); the creature case (a creature in sight
  takes every fork) is ported; the manager mode (fn_00690C70) is unread.
- **The arcs over what the bolt struck**:
  - a creature's 3D object (`Creature3d->Get3DObject()`) was not read for its `IsAnimated`: openblack queues no
    creature, since its body is not one of the mesh pack's;
  - the 3D object's matrix is taken as the object's Transform; that it holds the original's matrix bit for bit is not
    checked;
  - the arc's ribbon is drawn by the chain draw of the other ribbons; its look against the original (additive
    `S_Lightning.raw`) has not been compared on a screenshot.
- **The weather's fork strike**:
  - the gates [0xC029E4] (the queue) and [0xC029D0] (the no-spell strike effect) are 1 in .data and only read by
    direct address; a write through a pointer has not been ruled out;
  - the no-spell strike's fire (FireEffect, heat) after `ApplyEffectToMapPos` with burn 10000 has not been traced for
    this caller: which random stream its draws use under the PSys step is to be measured with a forked storm
    (`OPENBLACK_TEST_WEATHER`); no check has one.
- **The ribbon's V offset**: chain +0x4C = 0 is inferred; verify with `xref.py writers` on the chain's +0x4C.
