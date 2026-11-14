# Particles: the PSys core

## Summary

The particle systems (PSys) draw almost every effect of the game: miracles, the hand's glow, smoke, splashes, belief
sparkles and the vortexes. An effect is read from a spell file and made of atoms that rules move and creators turn into
sprites, meshes, chains or light maps; it is stepped on the synchronised or the local random stream depending on
whether it belongs to the game logic or only to the picture.

openblack has its own engine for the same files: the types, the class registry, the effects linked to spells, the
hierarchies, the step and the drawing of the world effects, with the rules and creators ported from the original
(their pages: [Particle creators](particle-creators.md) and [Particle rules](particle-rules.md)).

## The original

The original's particle engine as the miracles use it: the particle types, the class registry, the effects linked to
a spell, the hierarchies, and an index of the rules with the page where each one is. The drawing of the world
particles is in [The PSys in the world](#the-psys-in-the-world-format-step-drawing-and-water-rules); the miracles and
the core of the magic in [The spell core](../magic/spell-core.md); the sound of the particles in
[Magic and weather sounds](../audio/magic-and-weather-sounds.md#sound-of-the-particles).

### Particle types

`fn_0068EA00` fills once the table of 150 names `0xD4EBB0` with `.\Data\Spells\ZSpellFiles\<name>.txt`; 122
have a file and the rest stay NULL (NONE, TORNADO, FIREWORK_SINGLE, FOOD_IN_HAND...). FIREWORK (2) has `SF_FireWorks`
(0x68ED69, `mov [eax + 8]`; read 2026-10-31, where this page had it NULL). Some names are stored through a
register (`mov eax, str; mov [ecx+off], eax`), which an older name table lost: FOOD and FOOD_POISONED →
SF_Food, HEAL and HEAL_FX → SF_HealChakra, LANDSCAPE_VORTEX_OUT_BEFORE → SF_LandscapeVortexInBefore (sic).

### PSys class registry

Class name → factory for the modifiers (rules, emitters) and the creators. `ParticleEffect.cpp` registers the classes that
already existed (`RegisterCoreModifiers`); each new rules or creators file (`src/Particles/Rules/`, `src/Particles/Creators/`)
will have its `RegisterXxx()`, called from the explicit list `RegisterAll()` in `ParticleRegistry.cpp`. Whatever is not
registered remains "not ported yet" (no effect). The registered creators derive from `Creator` and call
`ReadCreatorProperties` first.

- The original resolves the class names of a spell file through its persistence registry in `PSysFileData::OnLoaded`
  0x672620; the file is parsed by fn_006B4C40.
- Creator base classes: `ParticleCreator` (common properties fn_006A85E0) and `ParticleSpriteCreator` 0x6AA0D0; the
  mesh creators' DefineProperties are `ParticleBaseMeshCreator` 0x6B37A0, `ParticleMeshCreator` 0x6B38B0,
  `ParticleMeshCreatorAnimTextured` 0x6B3970 and `ParticleAnimCreator` 0x6B3D70; `ParticleChainCreator` props
  0x6B4760; `ParticleLightMapCreator` props 0x6B45E0.

### PSys linked to the spell

- The spell owns its effect and advances it itself with its PSysProcessInfo (`manager::StartForSpell` /
  `ProcessForSpell`); `ProcessTurn` does not touch it.
- `StrengthFloatProvider` = `info.power`; `EventConditionTrueWhenEnabled` = `info.enabled`.
- The float providers (each times its `ScaleBy`): `MagnitudeFloatProvider` 0x69DA90 the magnitude (manager +0xA0),
  `StrengthFloatProvider` 0x69DAF0 the strength (manager +0x54), `MagnitudeTimesStrengthFloatProvider` 0x69DB50 both,
  each held to its `Minimum`..`Maximum` (-1000000..1000000 when the file gives none, 0..100 for the last; every
  one of the frozen spell files' five `MagnitudeTimesStrengthFloatProvider`s gives both, so the 0..100 is never
  used, and openblack keeps the one default); `RenderHandScaleFloatProvider` 0x69A750 the magnitude and
  `RenderHandScaleTimesStrengthFloatProvider` 0x69A770 the magnitude times the strength, with no limits. openblack:
  `Effect::Step`'s providers (test `VolumeGlowEffectTest.TheFloatProvidersReadTheMagnitudeAndTheStrength`).
- `LandscapeCollide` with `SendEvent` sends event 3 (global position, movement of the step, strength 1) before deleting
  the atom. Event 1 is sent at the start (fn_00673070).
- `LandscapeCollide` is 0x67D6E0: its `SendEvent` flag is +0x20 and the event 3 carries no shield check.
- `EventAlways::ModifyAtomCore` 0x69DBB0: every step a type 2 event at the atom (global position, last global
  movement, strength 1, no shield check).
- `AtomCore::GetLastGlobalMovement` 0x674420: the drawn position minus the previous drawn one (0 until the atom has
  been drawn twice).
- `PSysManager::SpellEvent` 0x6734C0 forwards the event to `Spell::SpellEvent` (vtable) and returns its result (1
  applied, 0 not); 0 when the effect has no spell.
- `PSysManager::GetPowerUpLevel` 0x673510: the spell's level (−1 base, 0, 1); −1 without a spell.
- The spell queries a rule can make: `Spell::NetUnsafeIsMyInterfaceCasting` 0x7201E0 (spell +0x44, this computer's
  interface casts it), `Spell::IsHumanPlayerCasting` 0x720230 (+0x4C), `Spell::IsScriptCasting` 0x720270 (its creator
  is the script's player, g_game +0x205A5B, the neutral one), `Spell::GetPlayer` 0x55CDF0 (+0xA4).
- `PSysManager::IsInState(1)` 0x673050 is "closing"; `IsInState(2)` is "enabled" (`PSysProcessInfo` +0x38).
- `SpellTargets` points: the `AddTarget` that takes an LHPoint is 0x670CF0 (12 bytes per point at +0x14); fn_006710B0
  counts points plus objects; fn_00670E30 is the cyclic read cursor.
- (inf) While a PSys class is not ported, a spell's effect counts it as a creator until it is closed.
  Otherwise, an effect with no ported classes would end in the first turn and cut the spell's cycle short.

- PSysManager::SpellEvent 0x6734C0: to the spell, 0 without one
- PSysManager::GetPowerUpLevel 0x673510: the spell's level, -1 without one
- SpellTargets' points (+0x14, 12 bytes each: the AddTarget 0x670CF0 that takes an LHPoint), e.g. a shield's impacts (fn_006D0AF0) for its sparks. fn_006710B0 counts points + objects; fn_00670F00 takes a point first.
- PSysManager::GetPowerUpLevel 0x673510: -1 base, 0, 1
- Spell::NetUnsafeIsMyInterfaceCasting 0x7201E0 (+0x44): this computer's interface casts it
- Spell::IsScriptCasting 0x720270: its creator is the script's player (g_game +0x205A5B; the neutral one)

### Fix in the PSys core: the hierarchies

- In the original (ctor of `AtomCollection` 0x6748B0, `LocalToGlobal` 0x6751D0 / fn_006752D0, fn_00673DB0,
  `CommonInitNewAtom` 0x674C60): a collection is in a hierarchy if **any** ancestor belongs to a group marked in
  `Hierarchies` (not only the parent); its frame is the product of the local matrices of **the ancestor atoms whose
  group is marked** (the others do not count), and that matrix carries the atom's **scale** (baseScale × ruleScale, the Y
  × the stretch). A new atom is born at 0 if its parent belongs to a marked group and otherwise at the parent's position.
- Ported in `Effect::CreateCollection`, `LocalToGlobal` / `GlobalToLocal` (new), `GlobalPosition`,
  `SpawnPosition` and `PostUpdate` (the frame and its scale pass down through the unmarked atoms; the drawn scale of an
  atom in a hierarchy is also multiplied). The dome needs it: the patches (group 2) and the sparks (group 4) are in the
  frame of the root, which grows from 0.1 to 1 in 2 s (`UR_ChangeScale`) and shrinks when it closes. It also changes,
  towards the original, SF_Bonfire, SF_Smoke/Steam (root 0.8) and the effects in the hand and on the pedestal that scale
  their root (`SetScale`). No rule with its own `GlobalToLocal` (HandFollow, Fireball) is in an affected collection.
- `AtomCollection::GlobalToLocal` 0x675410 is the inverse of the `LocalToGlobal` frame (outermost ancestor first);
  `AtomCore::GlobalToLocal` 0x673D40 brings a point into the parent atom's frame (atom +0x80) in a hierarchy and leaves
  it global otherwise; `CalculateGlobalPos` 0x673D80.
- `CommonInitNewAtom` 0x674C60: with no parent the new atom starts at the effect's origin; with a parent of an
  unmarked group, at the parent's position (fn_006744B0).
- `GetCurrentParentPos` 0x674AF0 (the parent atom's +0x80, or the manager's origin) and `GetCurrentParentVel` 0x674BB0.
- `AtomCore::AddSubCollections` 0x673B70 adds new sub-collections of the given groups under an atom.
- fn_006731B0 makes a new atom in the first root collection of a group (the lightning's light maps go into their
  InitiallyCreated group, not under the fork); nothing when the group has no root collection.
- `AtomCore::MoveToBaseGroup` 0x673BD0: fn_00673180 finds the first root collection of the group and fn_00673C00
  unlinks the atom and links it at the head of that one; the atom keeps its position, velocity and data (the tornado's
  flung objects, 0x6D33F1). With no such collection the original leaves the atom in none (never updated nor drawn
  again);
- Per-collection rule data: the `BaseCollectionModifierData` list at collection +0x24 (each with its modifier at
  +0x1C), found by modifier (UR_CloudGather 0x6D4AB0, UR_Tornado 0x6D18C3, its sub-collections 0x6D2B10 / 0x6D2EA6) and
  destroyed with the collection.
- `PSysManager::SetOrigin` 0x672E10.

- AtomCollection::GlobalToLocal 0x675410: the inverse of LocalToGlobal's frame (outermost first)
- CommonInitNewAtom 0x674C60: no parent -> the origin; a parent whose group is flagged -> 0 (the collection is in its frame); else the parent's position (fn_006744B0), in the same frame as this collection
- 0x673BD0: fn_00673180 finds the first root collection of the group, fn_00673C00 unlinks the atom and links it at the head of that one (the order of a collection's atoms does not change what the rules do to each one)
- PSysManager::fn_006731B0: a new atom in the first root collection of that group (the lightning's light maps go into their InitiallyCreated group, not under the fork). nullptr when that group has no root collection.
- AtomCollection::LocalToGlobal 0x6751D0 / GlobalToLocal 0x675410: a point of the collection's frame. In a hierarchy the frame is the product of the local matrices (fn_00673DB0: rotation, scale, position) of the ancestor atoms whose group is flagged in Hierarchies (fn_006752D0); outside one, the world.

### The PSys in the world: format, step, drawing and water rules

Comes from rendering.md (the part of the PSys that the render had). **Faithful** except for what is stated as not ported.

- Files: `Data\Spells\ZSpellFiles\SF_X_txt.zzz` (u32 size + zlib) with editor text: header
  `BEGINPROPERTIES` (DeleteOnCloseDown, Hierarchies[25], InitiallyCreated[25], MaxSpellAge) and blocks
  `BEGINCLASS <Class> <Name>`. A loose `.txt` with the same name takes priority (`LHLoadData`).
- Spell file cache: `PSysFileData` 0xD4EBC0; the loose `.txt` is read first through `LHLoadData` 0x7BCDF0, else
  `<name>_txt.zzz`.
- Model: each modifier has a `Group` (0..24) and a `Condition`; a *collection* is a live instance of a group;
  `InitiallyCreated` creates the roots at the origin; `NextGroups` gives each new atom its subcollections; `Hierarchies`
  puts the child atoms in the parent's local frame. Nothing moves by itself: only the rules.
- Per-turn step (dt = 0.1 s) with the previous and current draw state, interpolated when drawing with the turn fraction.
  The atom's draw (fn_00679920) blends the atom's two drawn matrices themselves, the last step's (+0x98) and this
  step's (+0xD0): `LHMatrix::SetBlend` 0x679C30, every cell (this - last) x t + last with t kept in 0..5 ([0x9357B8]),
  the scale the same way, the alpha byte the last one's when both agree, else the same blend kept in 0..255 and
  truncated, then through the effect's alpha byte, (alpha x byte) >> 8 unless 255 (PSysAlpha, fn_00679860); only for
  a collection with the flag 0x02, else this step's matrix as it is. openblack (2026-11-11): `Effect::DrawnOf` from
  `Atom::savedDraw`, `saved_draw::Blend` (test `ParticleSavedDraw.TheTwoKeptMatricesAreBlendedAsTheOriginalBlendsThem`,
  pinned to the exe's own SetBlend through the oracle); the drawn rows carry the scale and the stretch, which the mesh,
  mist, surface and exploded-piece draws now take as they are. A thing a tornado or a vortex carries
  (`RenderParticleGameObject::DrawAt` 0x67B170: the 3D object takes the rows and the drawn scale as they are) gets the
  rows as they are: openblack keeps them, at the atom's drawn place, in its `ParticleDrawnMatrix`, which the
  renderer draws it with (`ecs::DrawnModel`), and the release takes; its Transform's turn and scale are left alone,
  as the original's draw touches only the 3D object, and the state hash does not read the drawn matrix. Its game
  place follows the particle's step: `RenderParticleGameObject::GameUpdate` 0x6CA170 (render particle vt+0x108,
  called last in the atom's post-update fn_00673EA0) copies this step's drawn info (+0xD0) and, while the object is
  available and out of the map cells (Object +0x24 bit 0 clear), sets its MapCoords (+0x14..+0x1C) from the
  translation through `MapCoords(LHPoint)` 0x603160 (x, z x 6553.6 truncated, the altitude above the ground).
  openblack: `particle_carried_objects::FollowParticle` from the carriers' `AfterAtomStep`, on `Atom::savedDraw[1]`.
  Before, openblack blended an unscaled turn and the scale
  apart, at a fraction kept in 0..1, and took the effect's alpha as a float over 255.
  The atom's frame is stored by fn_00673EA0 in [0, 2N) together with the previous one, and it is only moved with PlayAnim
  (`Atom::playAnim`, +0x118; without it neither step nor wrap-around); fn_00679920 interpolates the frame **number**
  and truncates it (a step, without blending two frames): `frame_anim::ParticleFrameAdvance` /
  `ParticleFrameLerp` / `ParticleFrameIndex` (were `PSysFrame*`), see [Frame-animated textures](../rendering/textures.md#frame-animated-textures).
  End: without atoms nor creation rules, or age > MaxSpellAge; `CloseDown` activates `TrueOnCloseDown`, releases the
  `RemoveOnCloseDown` rules and deletes immediately if `DeleteOnCloseDown`.
- Per-atom conditions: `Modifier::ModifyCollection` 0x675B10 applies an atom rule to each atom whose atom-level
  condition holds; the alpha condition 0x67DC80 is "alpha > AlphaValue".
- fn_00673290: a modifier with flag 2 (+0x10) and without 4 keeps an effect without atoms alive while it is attached
  and the effect is not closing.
- `AtomCore::Create` 0x6737F0; `SetAtomAge` fn_00673CE0 (birth = time − x).
- The common creator part fn_006A85E0 sets the atom's +0x74 = InitialScale (0x6A8761). Only `CreateParticle3DSprite`
  draws a random scale: `RandomiseScale` (+0x7A) ? (PSysFloatRand(0.7) + 0.3) × InitialScale (0x6AA1B4..0x6AA1D6); the
  other creators do not (the mist and the animated meshes draw their own).
- Sprite frame: `RandomiseInitFrame` (+0x7B) ? PSysRand(NumFrames +0x4C) : InitFrame (+0x70) (0x6AA1D9..0x6AA1F5);
  `RandomiseFrameDirection` (+0x7F) reverses the rate. The SetAngleY(PSysFloatRand(2π)) of +0x80 (0x6AA1FA..0x6AA213)
  never runs: +0x80 is 0 from the ctor 0x6A9F66 and no property sets it (DefineProperties 0x6B4380).
- `DrawOffsetLT` (0x28 bytes, ctor 0x6C75A0) is set with `AtomCore::SetDrawOffset` 0x673AF0 into the atom's +0x124;
  another kind, `DrawOffsetDecay` (0x40 bytes, ctor 0x6C7840), draws an atom from the hand until it decays (used by
  UR_WillowWisp's `DoDrawOffsets` +0x80 and the fireball's local cast).
  - Made by `CreateWithInitialDirection::ModifyAtomCollection` for each new ball while
    `NetUnsafeIsMyInterfaceCasting` (0x69F186; no owner spell counts as true), right after the ball's place (+0x80) is
    set: offset = `GetNetUnsafePos` (this computer's hand, interface +0x3A0 → +0x78) − the place (0x69F1B5..0x69F1DD).
  - The ctor stores it twice (+0x20 the first, +0x2C the current), the age +0x38 = 0, the rate +0x3C = 0.5, and puts
    the object at the head of the list [0xD4EDF0] (fn_006C7760). `GetOffset` (vt +0xFC, 0x6C7960) gives +0x2C.
  - Once a frame, in `PSysGlobal::DrawLoop` after the frame's draws, fn_006C7990 walks the list and fn_006C78E0 steps
    each: age += g_game_time_inc × 0.001 (stored as a float), k = 1 − age × rate bound to [0, 1], current = k × first
    one. So the first frame draws the whole offset and it is gone after 2 s of game time.
  - Ported for the fireball: `Atom::DecayingDrawOffset`, made in `Rules/Fireball.cpp` from the hand system's matrix,
    stepped by `manager::StepDrawOffsets` in `magic::Update` with `game_clock::FrameGameMs` (each step takes the
    previous frame's ms, the draw coming between), read by the draw (`DrawOffsetOf`); tests `FireballDrawOffset.*`.
  - A trail with UseParentsDrawOffset (the three fireball throw files) shares the ball's offset
    (`UR_Trail::MakeTrail` 0x6A41B0; [the trail](../magic/fireball-and-lightning.md)); openblack copies it into each
    trail atom (`Atom::ShareDrawOffset`).
  - UR_WillowWisp's `DoDrawOffsets` (+0x80, 0 from the ctor 0x6A6C78; set in SF_Food, SF_Wood and SF_FoodPoisoned):
    `NetUnsafeIsMyInterfaceCasting` once per step (0x6A6F00..0x6A6F23), then for each new atom, right after its place
    (+0x80, 0x6A72A0), the same `DrawOffsetDecay(hand − place)` (0x6A72AC..0x6A7312), before its random orientation.
    Ported in `WillowWisp` (`Particles/ParticleEffect.cpp`, `doDrawOffsets`); test
    `FoodWood.theGrainsOfACastFromThisComputersHandAreDrawnFromTheHandWithDoDrawOffsets`. These effects are stepped
    in the turn, so the frame step's order (the previous frame's ms) holds for them as for the fireball.
- DrawData +0 is the atom it was taken from (0x679C0C); `PSysManager::AddDrawing` keeps the draw fraction at +0xB0
  (0x6797D4) and passes it as DrawData +0x14.
- The three draw entries are in `GJPSysInterface`'s vtable 0x8FA8A0.
- `Particle3DSprite::DrawAt`: the sprite's size is the scale, at least 0.0001 (0x67AEA4..0x67AEBE).
- Every owner list of effects (GParticleContainer fn_0063E0F0 0x63E151, Spell 0x71FC4C, MapShield 0x72C0AC, the seed
  graphics 0x726E70, the vortices 0x5FEA49) puts a new effect at the head and walks from the head: the newest is
  stepped and drawn first.
- The spell dispensers' effect and the flying flock's cast are stepped by their owner's Draw with the frame's ms
  (`SpellDispenser::Draw` 0x722940, 0x7229FE; `FlockFlying::Draw` 0x724100, 0x7241D1);
- Drawing: **three paths** depending on who draws the effect (`particles::DrawPath`, `particles::manager::SetDrawPath`). **Sorted** = `Draw_(t, 1)`
  (fn_00679840, +0xAE = 1 at 0x67984E): the effect has no Z-sorter object; each sprite (0x840C70), each mesh,
  the opaque ones too (fn_00679F60, key its translation), each mist (fn_007FA7F0, key mist+0x38) and each chain
  (fn_0067B380, key the joint n/2) enter the queue on their own; the ZR_SurfRevol surfaces are drawn
  immediately (0x67CBA0). It is used by `Spell::Draw` 0x720441, the storm 0x72DCB7, the shield 0x72D160, the teleport
  0x5FCDC5, the dispenser 0x722A13, the flock 0x72420D, the hand utilities, `TownCentre::DrawPSys` 0x69BF19:
  it is the default path. **Queued** = `AddDrawing` 0x6797D0 (+0xAE = 0 at 0x6797DE): **one** object at `GetOrigin`
  and inside it all the atoms in the order of fn_006798B0 (atoms of the collection, its chain, child collections); only
  the seed in the ball or the icon (0x51A2CA) and the containers with `GSpotVisualInfo` +0x4C `SingleZSort` = 1
  (0x63E190, 0x63E26A; all the info.dat entries with a file have it). **Immediate** = `Draw_(t, 0)`: everything
  right where the call is; the effect in the hand (`CHand::DrawSpellInHand` 0x46E76A, inside the hand
  object). Sprites from
  `S_SpriteSheet{1,2,3}` (8×8 cells of 32 px, cell = (FileOffset + frame) & 63), screen-aligned quad
  with rotation atan2(M[0][2], M[0][0]) or XZ plane (`SetHorozontal`); additive mode 13 (102 of 137) or 6, no Z write
  except `MaterialUpdateZBuffer`; no light nor haze except `UseLandscapeColor` (below).
- Scripts: `SPECIAL_EFFECT_POSITION` / `_OBJECT` (CHL 52/53) → `GParticleContainer` with the `GSpotVisualInfo` table
  (50 entries → PARTICLE_TYPE → file); duration in seconds, made turns by the natives (a negative count forever; 0
  closes `_OBJECT`'s at its first turn: the table's lifetime is only `CreateSpotVisual` 0x63E540's); it stays where the
  object was when it was made and closes if the object disappears; returns an object that the script can delete.
- `SPECIAL_EFFECT_POSITION` (`GScript::SpecialEffectPosition` 0x70C330) →
  `GParticleContainer::CreateSpotVisualWithSpecifiedDuration` 0x63E580. `SPECIAL_EFFECT_OBJECT` (0x70C460):
  `CreateSpotVisualWithSpecifiedDuration(obj +0x14, type, 1.0, duration, obj)` (0x70C50B).
  Both then give the new container the local player, `g_game` +0x18 + 0xA60 × `PlayerIndex` (+0x205A59), through its
  vt +0x20 (`GParticleContainer::SetPlayer` 0x63E3C0, which hands it to its effect), before
  `AddScriptGameThing(visual, 1)` and the push (0x70C3ED..0x70C427); no container is "Spell not created" (0xC20674)
  and 0. openblack: `SpecialEffectPosition` (`src/CHLApi.cpp`) gives its visual the local player through
  `ParticleSystemInterface::SetSpotVisualPlayer` (test `SpotVisualPositionNative.*`,
  `test/test_spot_visual_position_native.cpp`), so the rules that read the player (the player-colour tint, the camera
  scale rule) run for it, and adds it to the scripts' things as one the script created (`ScriptObjects().Register(visual,
  true)`); `SpecialEffectObject` does the same (below).
  `SPECIAL_EFFECT_OBJECT` (`GScript::SpecialEffectOnId` 0x70C460, read in the v1.42 exe 2026-10-28): an invalid thing
  logs "Thing invalid" (0xC0D7B8) then "Spell not created" and pushes 0; a container made is given the local player,
  `g_game` +0x18 + 0xA60 × `PlayerIndex` (+0x205A59), through its vt +0x20 (`GParticleContainer::SetPlayer` 0x63E3C0)
  (0x70C519..0x70C53C). openblack: `SpecialEffectObject` (`src/CHLApi.cpp`) gives its visual the local player through
  `ParticleSystemInterface::SetSpotVisualPlayer` (test `SpotVisualObjectNative.*`,
  `test/test_spot_visual_object_native.cpp`), so the rules that read the player (the player-colour tint, the camera
  scale rule) run for it. Then `AddScriptGameThing(visual, 1)` (0x70C53F..0x70C542) and its id is pushed; openblack
  adds it to the scripts' things as one the script created (`ScriptObjects().Register(visual, true)`), only when made.
  Its turns are worked out before the thing is checked, as `SPECIAL_EFFECT_POSITION`'s (0x70C4A6..0x70C4C7:
  `ftol(float(u64(1000 div ms)) × seconds)`); openblack takes `game_clock::TicksForSeconds` (0 seconds 0 turns).
- `ADD_SPOT_VISUAL_TARGET_OBJECT` (CHL 297, `GScript::AddTargetObject` 0x6FDD80, read in the v1.42 exe 2026-10-27):
  POP the target, POP the visual, each through `GetScriptGameThing`; no visual: "Thing not valid"; a target and a
  visual that is a particle container (vt `IsParticleContainer`): `GParticleContainer::AddTarget(GameThing*)`
  0x63E3A0, which hands the target to its effect (psys vt+0x114) when it has one; a target that is not there is
  skipped without a message. Used by Lethys's beam on the creature (refs/chl/lethysvortex.txt:84277) and the
  workshop's (refs/chl/theworkshop.txt:98912). openblack: `AddSpotVisualTargetObject` in `src/CHLApi.cpp`, through
  `ParticleSystemInterface::AddSpotVisualTargetObject` (a visual that is no spot visual's object finds no container
  and is left alone); tests `SpotVisualTargetNative.*` (`test/test_spot_visual_target_native.cpp`).
- `ADD_SPOT_VISUAL_TARGET_POS` (CHL 296, `GScript::AddTargetPos` 0x6FDCE0, read in the v1.42 exe 2026-10-28): POP
  z, y, x, then the visual through `GetScriptGameThing`; no visual: "Thing not valid"; a particle container:
  `GParticleContainer::AddTarget(const LHPoint&)` 0x63E380, which hands the point to its effect (psys vt+0x110) when it
  has one. Used by the shield's stone rays (refs/chl/throwthroughshield.txt:138226) and the dust's
  (refs/chl/landcontrol5.txt:146242). openblack: `AddSpotVisualTargetPos` in `src/CHLApi.cpp`, through
  `ParticleSystemInterface::AddSpotVisualTarget`; tests `SpotVisualTargetNative.*Point*`.
- The container does not follow its owner: `Process` 0x63E33D..0x63E349 puts the effect at `GetLHPoint(+0x14)`, the
  container's own MapCoords with the y read from the land again, and nothing moves +0x14; the owner (+0x28) is only
  tested for availability (its loss still ends the effect).
- The container's strength (+0x34, 1.0 from `SetToZero` 0x63E1B0, saved with it) is the only value it gives its effect:
  `Process` 0x63E280 builds a `PSysProcessInfo` of zeros with `Strength` = +0x34 and the enabled byte 1, and the
  manager keeps it (`Strength` at manager +0x54, read by `StrengthFloatProvider`, `MagnitudeTimesStrengthFloatProvider`
  and `UR_VolFXOnObject`; the magnitude is +0xA0). `SetParticleStrength` 0x63E3E0 sets it: a town artefact's worth
  (0x426820), the citadel heart's destruction glow (0x4662DC) and the scripts' strength property (0x70EC29).
  openblack: `manager::Container::strength`, given to the effect as `ProcessInfo::power` every turn
  (`ParticleSystemInterface::SetSpotVisualStrength`; test
  `ParticleSystemTest.ASpotVisualsStrengthIsItsEffectsEveryTurnFromItsContainer`).
- Spot visuals: the `GSpotVisualInfo` table is 0xD44470. `GParticleContainer::Create` 0x63E508 → fn_0063E410 creates the
  effect with NET_GAME_TYPE 1 (push 1 at 0x63E436), synchronised, at the LHPoint of the MapCoords it keeps at +0x14
  (0x63E42F..0x63E474).
- The owner as the effect's target: once fn_0063E410 returns 1, `Create` passes the owner to the effect's AddTarget
  (fn_0063E3A0 → psys vt+0x114, 0x63E522..0x63E534) when the owner is set and the entry's `TargetOwnerObject` (+0x50)
  is not 0. info.dat sets it for 33 HEAL_FX, 34 HIGHLIGHT_ON_OBJECT, 38 BUTTERFLIES_ON_OBJECT, 40 FLIES_ON_OBJECT and
  46 PILEFOOD_SPEEDUP only (0 for the beams 13, 20, 21, 41).
- The container's draw flag +0x38 = (`SingleZSort` == 1) (0x63E190..0x63E198); fn_0063E240 reads it to draw with
  `AddDrawing` (0x63E26A) or `Draw_(1)` (0x63E277).
- Durations: `CreateSpotVisual` 0x63E540 passes the entry's own life (+0x44); `CreateSpotVisualWithSpecifiedDuration`
  0x63E580 passes its int straight to `Create` 0x63E4B0 → fn_0063E410 (+0x30, 0x63E489). `Process` 0x63E2A0..0x63E2B1:
  below 0 forever, else decremented and closed at ≤ 0, so 0 closes it on its first Process.
- Container list: g_game +0x205BCC (next +0x3C), a new one at the head (fn_0063E0F0 0x63E151..0x63E168).
  `ProcessParticleContainers` 0x63E090 walks it from the head (the newest), taking the next one first (0x63E0A3); each
  `Process` 0x63E280 closes its effect when its owner has gone or its turns are over, sets its origin and steps it
  (`Process_` 0x6736B0); a result of 5 deletes the container (`ToBeDeleted` 0x63E1D0) with its effect.
- **`UpdateRuleGravityWithFloor`** (`Particles/Rules/Fireball.cpp`, a single class shared with the miracles; ctor 0x6A1510, `ModifyAtomCollection` 0x6A1880).
  Default values of the ctor: MaxSpeed 100, Gravity 10, Damping 0, WindMagnification 100, UseWind 1, bounces 0.5/0.5,
  GroundDrag 0, ImpactSpeed 5/20/40, MinAlphaForImpactSoundOrRipple 60, ripple distance 2 (+0x40) and ripple enabled
  (+0x71 = 1), with no property. Per atom, without the per-atom `Condition`:
  - damp = UseDamping and (not DisableDampingForNonHuman or `IsHumanPlayerCasting` 0x673580, which is 0 without a `Spell`);
    wind likewise with UseWind / DisableWindForNonHuman. With wind: v += (wind·WindMagnification·0.1 − v)·Damping·dt
    (wind = `fn_00771B10` = `GClimate::GetWeather(p, 1)`: (int8 x/8, 0, int8 z/8); here `weather::GetWindAt(p, true)` from ECS/Weather); otherwise,
    with damping: v ·= 1 − dt·Damping.
  - v is stored and the atom moves **before** gravity. Ground = `GetAltitude(x, z)`; lowest point = global y
    (`RenderParticle::GetLowestPoint` 0x6C79B0; the mesh one, `Particle3DObj` 0x6C7AE0, is not there because there are no
    mesh particles). Above: v.y −= clamp(v.y + MaxSpeed, 0, 1)·Gravity·atom gravity·dt.
  - Below: it is raised to the ground; d = n·v with the terrain normal; if d < 0, impact (`fn_006A1630`) and bounce:
    vn = n·d, vt = v − vn, drag m = min(dt·GroundDrag, |vt|) in the direction of vt (if |vt|² < 1e-4 the direction
    is +x), v = vt·DampingHorozontalBounce·surface − vn·DampingVerticalBounce. Surface (UseSurfaceForBounce):
    table 0x937574 by `GetSurfaceType` = 1,1,1,1,1,1,**0.2** (deep water),**0.2** (shallow),1,1,25.
  - Impact `fn_006A1630`: nothing if alpha < MinAlpha, if ImpactSound is NO_SOUND (−1), if |d| < ImpactSpeedSmall, if the
    atom's sound is still playing or if ImpactSoundCondition fails; level 3/2/1 according to ImpactSpeedMedium/Large; and a
    ripple on the water. Consequence: the pieces of `SF_ExplodeObject` (NO_SOUND) **never** make a ripple; only the fireball
    (`SF_FireBallThrow*`, SOUND_SPELL_FIREBALL_HIT) makes one. The atoms of this engine do not play sounds yet
    (`AtomCore::StartSound` 0x6745D0), so the "while it is playing" wait does not apply. `CheckShieldDeflections`
    (creature shields) is not there. Unit test `test_particle_water`.
  - Property layout (UpdateRuleGravity DefineProperties 0x6AC1B0): +0x20 MaxSpeed, +0x24 Gravity, +0x28 Damping, +0x2C
    WindMagnification, +0x30 UseDamping, +0x31 UseWind, +0x32 / +0x33 Disable{Wind,Damping}ForNonHuman; its own +0x34
    DampingHorozontalBounce, +0x38 DampingVerticalBounce, +0x3C GroundDrag, +0x44 ImpactSound, +0x5C its condition,
    +0x60..+0x68 ImpactSpeed{Small,Medium,Large}, +0x6C MinAlphaForImpactSoundOrRipple, +0x70 UseSurfaceForBounce, +0x72
    CheckShieldDeflections.
  - The ripple (`AtomDataRipple`, fn_006A1630 from 0x6A1CC2): only where `MapCoords::IsWater`, and only when the atom is
    farther than the rule's +0x40 in x, z from its last ripple (`minDistance² < dx² + dz²`, 0x6A176F..0x6A179B), which
    then moves there (AtomDataRipple +0x20, (0, 0, 0) at creation). The ring: at the atom, growth 4 × the atom's
    radius (0x6A17CE: the virtual `Object::GetRadius` slot, which for an atom is `AtomCore::GetRadius`; with
    InitialScale 1.5 the growth is 6), cell 0x30, colour 0xFFFFFFFF, angle 0, rate and aspect 1; nothing when the pool is
    full.
- **`UR_Explosion`** (`Particles/Rules/Explosion.cpp`, [Lightning explosion](../magic/lightning-explosion.md); the rings here are
  `Particles/ParticleWaterRings` `AddExplosionRings`, the only implementation; ctor 0x67E090, `ModifyAtomCollection` 0x67ECE0, `InitCollection`
  0x67E200), in `SF_BeamExplosionSingle/Many/Loads`. By default InitialDelay 3.5, SmokeDelay 3, BeamDelay 0. Point =
  `GetCurrentParentPos` (the +0x80 of the parent atom or the origin) with y = ground altitude. If the effect closes, it
  closes the lightning container. With collection age > InitialDelay: water rings (above) or scorching; > BeamDelay:
  visual spot BEAM_EXPLOSION_FX (magnitude 1, 60 turns); > SmokeDelay: **SMOKE on dry land, STEAM over water**
  (`IsDryLand`), magnitude 8, 4 s. The 3rd argument of `CreateSpotVisualWithSpecifiedDuration` is the effect's
  magnitude (`GJPSysInterface::Create` 0x68F3A1 `SetScale`). The damage to objects, the `SpellEvent` 2 and the shield are
  done by the miracle's side ([The spell core](../magic/spell-core.md)); the ground mark (`fn_008251C0`: a morphable mesh 0x251 of scale 8, not a shadow; `TemporaryShadow` is
  `fn_00825090`) is in `ecs/GroundMarks` ([Meshes stuck to the ground (land_morph)](../rendering/meshes-and-lod.md#meshes-stuck-to-the-ground-land_morph));
  the mesh debris (five rock meshes queued to shatter) is `Rules/ExplodeObject.cpp`'s ([Exploding objects](particle-rules.md#exploding-objects)).
  The ring growth constant is [0x9357D8] = 10 (× 0.5 at 0x67E39F, × 0.7 at 0x67E43A, × 1 at 0x67E4D1);
  `MapCoords::IsDryLand` there means altitude ≥ 4, and each ring is skipped when the 1024 slots are full.
  Test: `OPENBLACK_TEST_PSYS="SF_BeamExplosionSingle,1464,2016,0,1"`, camera `1452,14,2002,1464,0,2016`, capture at
  frame 272 of 300 (rings) or 360 of 400 (steam); `OPENBLACK_PSYS_TRACE=1` writes the explosion.
- Test: `OPENBLACK_TEST_PSYS="SF_Bonfire,1790,2630,0,1"` with the camera `1775,45,2600,1790,30,2630`, `-n 5000`
  (bonfire with flames and smoke); `OPENBLACK_PSYS_TRACE=1` writes the atoms and age of each effect every 20 turns.
- **Beliefs over the town centre** (`src/Particles/TownBelief.cpp`): each functional centre has TOWN_BELIEF (SF_TownBelief, `UR_TownCentreBelief` 0x69BF30), which advances once per frame with dt = 0.1 s. One symbol per active player: the first one (rank 0) still over the totem; the others orbit (radius and speed by belief, at 2.5 per height rank) and the second one fights, with the file's fight sparkles. A symbol of no belief has no size. It is drawn with two glows from S_SpriteSheet3 (player colour and white spinning) and the symbol. The symbol is the player's cell of the players' symbol sheet (`fn_0069D630`: cell = the player's number, material `fn_00590420` = `CreateMaterial(0xD, [0xD19C3C])`, PlayerSymbol::OpenOnce 0x5DE2F0's sheet): the human's cell is the "player symbol" cell of the profile (registry; 0 without it, as in this installation) copied from ChooseSymbol, and a computer god's its `.cps` image when the computer played it at the game's start, else empty. openblack draws the local player's straight from ChooseSymbol and every other player's from its cell of `player_symbols/sheet` (2026-11-03). The stand-in is exact: `PlayerSymbol::RebuildLocalTattoo` 0x5DEA60 (from CreateFinalTextureSymbols at the game's start, and its other callers) copies the profile's cell of the OriginalChooseSymbol texture [0xD19C40] into the player's cell texel for texel (`fn_005DE980`); `PlayerSymbol::OpenOnce` 0x5DE2F0 copies OriginalChooseSymbol(a).raw over ChooseSymbol(a).raw and makes both textures with the same flags (0x41), and the frozen data's two pairs are byte-identical (sha1 265ff09a… / 0ad40244…), both on openblack's 4444 list. The one other source is a symbol replaced by the player's own picture (`PlayerSymbol::Open` 0x5DE5E0: Symbol<n>.bmp in the user's folder, then `fn_005DE900`), which openblack does not read anywhere. Base: the totem's icon's drawn place, raised by the totem's height (its icon's mesh at the totem's scale) and HeightAt1 (below). The owner's SpellColumn column is missing.
  - AtomData +0x24 / +0x28 / +0x2C are a1, a2 and the phase; a1 and a2 start at PSysFloatRand(2π) each, a1 first
    (0x69C0B8..0x69C0D5), the phase at 0. +0x30 is the wait before the next fight, +0x34 the fight timer, +0x38 its
    length, all 0 from the ctor (0x69C6C4..0x69C6D6).
  - The fight: only the second symbol fights (0x69C388..0x69C3A7); its wait counts down by dt (0x69C473); then the
    timer += dt and, past the length, the next wait and fight length are drawn together, wait first
    (0x69C3C3..0x69C424), timer back to 0. f = timer / length (0 for ≤ 0 or NaN, at most 1), fight = 1 − (2f − 1)²,
    which lowers the symbol in the position of the same step (0x69C537); speed × ((SpeedUpDuringFight − 1)·fight + 1)
    (0x69C47F..0x69C496).
  - The motion (0x69C4A5..0x69C510): phase += dt·PhaseSpeed, a1 += ((cos(phase) + 1)·0.25 + 0.5)·speed·dt·0.846,
    a2 += speed·dt, each fmod 2π. The exe keeps each sum on the x87 stack into `__CIfmod` and takes fcos of the
    unrounded result (0x69C4BF);
  - The ranks (fn_0073BB10): a player ranks above another with more belief, or as much and a higher number; the belief
    is the town's GBelief +0x8. The candidates are slots 0..6 of `GetNextActivePlayer` 0x5508D0 (0x69BFF4; the neutral
    one has no symbol), but the rank counts all 8 slots, the neutral one included (0x73BC20..0x73BC24).
  - The symbols: `PlayerSymbol::CreateFinalTextureSymbols` 0x5DEF00 makes them; ChooseSymbol is 4 × 4 cells of 64 × 64;
    the computer players get the Lethis / Kazarr / Nemesis `.cps` greyscale images. Each symbol is its own Z object
    (`PlayerSymbolSprite::AddDrawing` 0x69D790).
  - Removal: a centre leaves the list g_game +0x205CFC in its `ToBeDeleted` (0x743B68..0x743BAD); fn_0069BCA0 (from
    `TownCentre::ToBeDeleted` 0x743B45) deletes the centre's effect (+0xC8) and clears +0xC8.
  - ✅ The effect (`TownCentre::CreatePSys` 0x69BC10): `PSysInterface::Create` of type 25 at the origin, magnitude 1,
    local, the centre added as its target (vt +0x114), then processed at once. The rule's first step with a target
    (collection data +0x24 = 1 from fn_0069C680) takes it (`SpellTargets::TakeTargetObject`, +0x20) and makes one atom
    per `GetNextActivePlayer` (0x69C022..0x69C0D5): `AtomCore::Create` (its PSysRand(0x100)), +0x74 = 1.5 (the base
    scale), `InitNewAtom` with NextGroups (the fight sparkle group 3), the atom data's player (+0x20), a
    `ParticlePlayerSymbol` at +0x128 (fn_006C9D70: a `PlayerSymbolSprite` of the player's colour | 0xFF000000 and
    symbol cell), then a1 and a2. The atom's colour stays white.
  - Every step (0x69C118..0x69C661), while the centre has a town (+0x98) and a totem (+0xCC): the base is the totem's
    icon's drawn place (the totem's own 3D object, Object +0x40, the translation of its matrix at +0x38..+0x40), else
    the totem's map point at its land height plus its altitude; its y += (the totem's `GetHeight` (vt +0x42C) +
    HeightAt1) + y. The Object +0x40 object is the icon: `TotemStatue::SetPlayersCreature` 0x7381C0 re-meshes it to
    `GetPlayerTotemForTown` (0x7381F0), and `TotemStatue::Draw` 0x738960 places it 2.729 above the TotemStatue's
    other 3D object at +0x8C, the plinth (0x738BCF..0x738C13); the TotemStatue's vtable keeps `Object::GetHeight`
    0x638120 at +0x42C, which reads the +0x40 object's mesh, so the height is the icon mesh's at the totem's scale
    (+0x50). Oracle (the rule run with a fake centre, totem and icon, the rank-0 symbol placed at the base itself,
    2026-11-04): the hand icon (mesh 250, box half height 3.954256, 0x407D1288) at
    (10, 24, 20), scale 1, HeightAt1 2 gives (10, 33.908512, 20) (y 0x4207A251); Land 1's Norse centre's totem
    (scale 0.99, about its script's size 990) with an icon at (1805.22, 30, 2691.31) (the centre's script point at a stand-in
    height: the base is linear in it) gives y 39.82943 (0x421F5156). openblack: the icon's
    Transform raised by `Height(icon mesh half extents, the plinth's scale field)` + HeightAt1
    (`town_belief::SymbolBase`, test `TownBeliefSymbols.TheSymbolsTurnOverTheTotemsIconRaisedByTheIconsHeight`;
    ported from raffclar/openblack PR #406, which reads the same place). The distance scale is |camera − base| × FixedScale, 1..10 (with
    UseFixedScale). Each atom: belief clamped 0..1 (0 for NaN), rank; rank 0 radius and speed 0; else radius and speed
    by belief, speed ÷ radius; scale 0 at no belief, else ScaleAt0..1. Position x = cos a2 cos a1 r s, y =
    ((1 − fight) rank HeightPerLevel + sin a1 r) s, z = sin a2 cos a1 r s, each plus the base; +0x78 the scale. Then
    each child collection of `GroupFightSparkle`: +0x50 (its alpha) = ftol(255 (1 − (2f − 1)²)) of the fight f
    clamped to 0..1, and +0x38 bit 2 = fight > `FightFracWhenEmitting` (0.2: the ctor's, 0x6C0442; the file does not
    give it). `EC_CollectionShouldBeEmitting` (0x67D830) is that bit; only SF_TownBelief uses it.
  - The draw: `ParticlePlayerSymbol::DrawAt` 0x67AE20 (alpha byte not 0): `SetScale` (glow 1.5 × the drawn scale,
    symbol the drawn scale), `SetAlphaColor` (0x69D690: the symbol = player colour × particle colour, (c p) >> 8 in four
    channels; the glow that RGB at alpha (symbol alpha × 100) >> 8), `SetPos`, then `PlayerSymbolSprite::Draw`
    0x69D7E0 or `AddDrawing`. Oracle: `SetAlphaColor`'s five cases of `TownBeliefSymbols.ASymbolsColoursAreThePlayersTimesTheParticles`.
  - openblack: `particles::RegisterTownBeliefRules` (UR_TownCentreBelief, EC_CollectionShouldBeEmitting), the
    motion in `src/Particles/TownBeliefMaths.h` (raffclar's names and layout, the original's order of operations), the
    symbol's draw in `src/Particles/PlayerSymbol.h`, each centre's effect id on its entity
    (`components::TownBeliefSymbols`), stepped by `manager::ProcessByFrame`. A symbol is an atom with an
    `Atom::playerSymbol`, drawn by the effects' collects as its glows and symbol, its glows stepped once a frame
    (`manager::StepPlayerSymbols`). Tests `test/test_town_belief_symbols.cpp`. Debug: the
    Magic window's "Belief sprites" section reads out the centres, their symbols, those sparking and the sparkles.
  - When: the original makes the effect last in `TownCentre::MakeFunctional` (0x743F18, after the totem, the town's
    centre, the spell icons and the worship site's spells, only with a town) and in `ResolveLoad` (0x7448D8) for a
    loaded centre; `CreatePSys` deletes an earlier one, and its `ProcessPSys` steps twice at once (the first process),
    the first step making the symbols and drawing their local random numbers there. Then one step a rendered frame
    (`TownCentre::DrawAll`). openblack: `town_belief::MakeEffect`, last in `AbodeArchetype::MakeTownCentreFunctional`
    (also reached by a loaded game's centres, made in the original's link order), two `ProcessByFrame` steps; then
    `town_belief::Step` once a frame.

- 0x67DC80: alpha > AlphaValue
- A spell's effect with a class not ported yet lives until the spell closes it (inf: that class may create atoms, and ending the spell at once would cut its lifecycle short) fn_00673290: a flag 2 modifier keeps it too until it closes (KeepsAlive)
- fn_00679C30: the whole 3x4 frame, rotation included, is blended element by element; the frame between the steps is fn_00679920's own lerp (0x679A79..0x679B03, frame_anim::PSysFrameLerp, t' up to 5 when looped), done whatever the +0x38 bit 2
- fn_006798B0: the collection's atoms through fn_00679920 (0x6798B9..0x6798CD), then its chain (0x6798CF..0x6798E6: fn_0067B380 / fn_0067B370 by [0xC0215D]), then every atom's child collections (0x6798EB..0x67990D). The atoms are interpolated as CollectCollection does (its alpha cut-off) and the joints as CollectChainsOf does (none) fn_00679920 0x67999E: interpolated only when the collection's +0x38 bit 2 is set, else the current PSR
- How the original draws an effect: which of GJPSysInterface's three draw entries (vtable 0x8FA8A0) its owner calls. The byte PSysManager +0xAE that fn_00679860 copies to [0xC0215D] (0x67987E..0x679884) is what each atom's DrawAt reads to queue itself or draw at once. Decided where the effect is started (manager::SetDrawPath). Report: dev\tmp_dis\miracles\polish\psys_draw_paths_verdict.md
- +0x124 its DrawOffset (AtomCore::SetDrawOffset 0x673AF0). Only DrawOffsetLT (0x28 bytes, ctor 0x6C75A0) is ported; UR_Lightning's CreateForkStructure gives one to every fork joint (0x69131C..0x69134C). fn_00679920 adds GetOffset to the atom's drawn position every frame, interpolated between the steps or not (0x679B69..0x679BBF)
- Modifier flag 2 (+0x10) without 4: an effect with no atoms is not finished while it is attached and the effect is not closing (fn_00673290; UR_HealSpellChakra's ctor 0x6A0810 sets 6 and clears 4: it waits for targets)
- PSysFileData cache 0xD4EBC0
- GParticleContainer::Create 0x63E508 -> fn_0063E410: Create(..., NET_GAME_TYPE 1) (push 1 at 0x63E436), synced, at the LHPoint of the MapCoords it keeps (+0x14)
- The container draws its effect by the entry's SingleZSort (fn_0063E0F0 0x63E190, fn_0063E240 0x63E26A / 0x63E277). 🔍 inferred without the info block, 1: every entry of info.dat that has a spell file has SingleZSort 1 (tmp_dis\psys\psys_report.md, the SPOT_VISUAL table)
- GParticleContainer::ProcessParticleContainers 0x63E090: from the head (the newest, fn_0063E0F0 0x63E151..0x63E168), the next taken first (0x63E0A3); each container's Process 0x63E280 closes its effect when its owner has gone or its turns are over, sets its origin and steps it (Process_, 0x6736B0); 5 (finished) deletes the container (ToBeDeleted 0x63E1D0) with its effect
- fn_0063E0F0 0x63E190..0x63E198: a container's +0x38 = (GSpotVisualInfo +0x4C SingleZSort == 1), which fn_0063E240 reads to draw its effect with AddDrawing (0x63E26A) or Draw_(1) (0x63E277)
- The same with the duration in game turns, as CreateSpotVisualWithSpecifiedDuration 0x63E580 takes it (its int argument; < 0: forever, else that many Process turns, 0 closing it at the first one: 0x63E2A0..0x63E2B1): UR_Explosion's 60 (0x67EE92) and TicksForSeconds(4) & 0xFFFF (0x67EF2C)
- One game turn (GParticleContainer::Process 0x63E280, Process_ with the turn length)
- The effect's draw path (SetDrawPath). A DrawableSource's drawable is Queued unless it says otherwise: one Z object at its origin (the fire's FireGraphic, fn_007325D0 0x73261D); the town belief is Sorted (TownCentre::DrawPSys 0x69BF19 Draw_(1))
- Kind::Sprite atoms (ParticleSpriteCreator, the town belief's symbols): key = the LH3DSprite's +0/+4/+8 that Particle3DSprite::DrawAt fills, the atom's position raised by height x size x 0.5 with CentreAtBase (0x67AFAB..0x67AFD6), then LH3DSprite::AddDrawing 0x840C70 (0x67B0D2). The town belief's ParticlePlayerSymbol: PlayerSymbolSprite::AddDrawing 0x69D790 at its position
- Kind::Mesh atoms (Particle3DObj / AnimTextured / Particle3DAnim, opaque or not, cut or not): key = the object's +0x38..+0x40 (fn_00679F60 0x679F7E..0x679F9F), its translation = the atom's position. Only after CheckRegionOnScreen (0x679F75): the renderer's test. Drawn as instances: RenderContext::particleAtoms
- 0x6A176F..0x6A179B: minDistance^2 < dx^2 + dz^2
- 0x6A17CE
- UR_Explosion::InitCollection 0x67E200 (0x67E347..0x67E55B): MapCoords(point).IsDryLand() (altitude >= 4) -> false, and the caller puts the scorch sprite 0x251 of size 8 at a random angle PSysFloatRand(2 pi) (fn_008251C0), no ring. Otherwise (water or altitude < 4) three rings at the point, growth 10 x 0.5, x 0.7 and x 1, cell 0x30, colour 0xFFFFFFFF, angle 0, rate and aspect 1; each is skipped when the 1024 slots are full. Returns true then.
- UpdateRuleGravityWithFloor::AtomDataRipple (fn_006A1630, from ModifyAtomCollection 0x6A1CC2), the ripple of a particle that touches the floor, once the rule's own tests passed (alpha >= rule+0x6C, an ImpactSound, the speed against rule+0x60, ImpactSoundCondition, the ripple flag rule+0x71 = 1): only where MapCoords(position).IsWater(), and only when the particle is farther than minDistance (rule+0x40) in x, z from its last ripple, which is then moved there (lastRipple: the atom's AtomDataRipple +0x20, (0, 0, 0) when it is made). The ring: at the particle, growth 4 x Object::GetRadius of the atom, cell 0x30, colour 0xFFFFFFFF, angle 0, rate and aspect 1 (nothing when the pool is full). Returns true when a ring was added.
- PlayersSymbols (PlayerSymbol::CreateFinalTextureSymbols 0x5DEF00): the local human's cell is a copy of the ChooseSymbol cell of the profile's "player symbol" (registry, 0 when missing, as on this install), so it is drawn straight from ChooseSymbol (4 x 4 cells of 64 x 64). Computer players get Lethis/Kazarr/Nemesis .cps greyscale images there (`player_symbol_sheet::MakeComputerSymbols`); every symbol other than the local player's is drawn from that sheet (`player_symbol::SourceOf`).
- The players with some belief in the town and their rank (fn_0073BB10): the players with more belief, or as much and a higher number. The belief is the town's GBelief +0x8 (Town::belief.belief, by player number). The candidates are GetNextActivePlayer 0x5508D0's slots 0..6 (UR_TownCentreBelief 0x69BFF4; the neutral one has no symbol); the rank counts all 8 slots, the neutral one included (fn_0073BB10 0x73BC20..0x73BC24: inc esi; cmp esi, 8)
- The town centres (an Abode whose number is a TownCentre's, with a Mesh) and their town's beliefs, in the order of TownCentre::DrawAll 0x7447F0: the list g_game +0x205CFC, which the TownCentre ctor pushes at the head (0x743AC3.. 0x743ACA), so the newest first (here by the object creation index, Object +0x3C); its IsAvailable test (vt+0x2C == 1, 0x744808): components::Unavailable left out
- The list g_game +0x205CFC: a centre leaves it in its ToBeDeleted (0x743B68..0x743BAD)
- 0x69C0B8..0x69C0D5: the new symbol's two angles, a1 then a2
- 0x69C388..0x69C3A7: the second symbol only. While its wait (+0x30) is > 0 it counts down by dt (0x69C473)
- 0x69C3AD..0x69C3C1: the fight's timer (+0x34) += dt; past the fight's length (+0x38), the next wait and fight are drawn together, wait first (0x69C3C3..0x69C424), and the timer is back to 0
- 0x69C47F..0x69C496: speed x ((SpeedUpDuringFight - 1) x fight + 1)
- 0x69C4A5..0x69C510: phase += dt x PhaseSpeed, a1 += ((cos(phase) + 1) x 0.25 + 0.5) x speed x dt x 0.846, a2 += speed x dt, each one fmod 2 pi. (aproximado) the exe keeps each sum on the x87 stack (extended) into __CIfmod and takes fcos of the unrounded fmod result (0x69C4BF); here float sums and cos of the stored float
- The belief symbols over every town centre (TOWN_BELIEF, SF_TownBelief: TownCentre::CreatePSys 0x69BC10, UR_TownCentreBelief 0x69BF30, ParticlePlayerSymbol / PlayerSymbolSprite::Draw 0x69D7E0). TownCentre::DrawAll 0x7447F0 (Process3dEngine 0x54E032, every rendered frame, paused too): ProcessPSys, one step of each centre's effect with dt = [0xD01A38] ms x 0.001 (0.1 s, not the frame's time), and the glows' frame
- fn_0069BCA0 (TownCentre::ToBeDeleted 0x743B45): the centre's effect +0xC8 deleted (vt +4(1)) and +0xC8 = 0

#### The PSys sprites' landscape colour

- ✅ A sprite creator's `UseLandscapeColor` (creator +0x79) is the sprite's flag 8 (`CreateParticle3DSprite`
  0x6AA0D0, 0x6AA19B). `Particle3DSprite::DrawAt` 0x67AE80 then, at the sprite's drawn position (after the
  `CentreAtBase` raise, 0x67AFC2..0x67AFD6), takes the land's light with `LH3DIsland::GetColorAndSpecular` 0x801C90
  (its first output, the diffuse; the land's specular is not used, 0x67AFE8..0x67AFF3), multiplies the atom's colour
  (DrawData +8, its alpha already scaled by the creator's `ScaleAlpha`, 0x67AF30..0x67AF67) by it as (l c) >> 8 in
  the four channels, the alpha too (0x67AFF8..0x67B063), then hazes it as an object there: `GetFogValue` 0x7FEB30
  (0x67B073) with the atom's specular (DrawData +0xC) scales the colour's R, G and B and returns the sprite's specular
  (sprite +0x20 the colour, +0x24 the specular, 0x67B078..0x67B089). Without the flag the sprite takes DrawData +8 and
  +0xC as they are (0x67B08F..0x67B09F), with no light and no haze. `LH3DSprite::Draw` 0x840530 writes the sprite's
  colour and specular into every vertex (0x840AE0, 0x840AE6), and the device adds the specular's red, green and blue
  to the textured colour (D3DRS_SPECULARENABLE), never its alpha.
- Oracle: `DrawAt` run with `GetColorAndSpecular` and `GetFogValue` stubbed (the land light read from the slot the
  call writes, the haze's arguments captured): the four cases of
  `ParticleDrawFrame.LandscapeLitSpritesTakeTheLandsLightInEveryChannel`. The same four without the flag: no call to
  either, the sprite's colour the atom's (its alpha scaled) and its specular the atom's, bit for bit.
- The files with the flag: `SF_FlockGroundDust`, `SF_LightningStormInHand` (and PU1, PU2), the debris of
  `SF_LightningStormPush`, `SF_MultiPickUpFood` (and Fish, Poisoned), `SF_MultiPutDownFood` (and Poisoned),
  `SF_WaterInHand` (and PU1).
- openblack: `Creator::useLandscapeColour`, read for the sprite creators; `particles::draw::LandscapeLitSprite`
  (`land_light::At`'s diffuse, `argb_colour::MultiplyArgbShift8`, `graphics::haze::ApplyObject` at the pass camera's
  depth), drawn by `Renderer::DrawParticleSprites` in their own calls with the `WorldTexturedLit` program
  (`vs_sprite_lit` / `fs_world_textured_lit`: the texture times the colour plus the specular, saturated). Tests
  `ParticleDrawFrame.SpriteCreatorsReadUseLandscapeColorOffByDefault`,
  `ParticleDrawFrame.LandscapeLitSpritesTakeTheLandsLightInEveryChannel`,
  `ParticleDrawFrame.LandscapeLitSpritesAreHazedAsAnObjectThere`. Debug: the Magic window's particle files play any of
  them.
- ✅ The other sprites' own specular: the atom's (the creator's `SpecColor`, alpha 0, or what a rule writes), added as
  it is. The only shipped sprite creator with a `SpecColor` the game loads is `SF_DefenseSphere`'s sparks (30, 30, 30):
  the shield's. `SF_FireWorksNew`'s (124, 70, 52) is in a file the game never loads
  ([not in the original](../project/not-in-the-original.md#renderingparticles)). openblack: `particles::draw::SpriteAddsSpecular` (a
  specular with some red, green or blue, or a sprite lit by the land) sends a sprite to the `WorldTexturedLit` program
  with its specular; the others keep the plain program. Test `ParticleDrawFrame.SpritesAddTheirOwnSpecularOnlyWhenItHasAColour`.

### The scripts' special effects, one by one

✅ v1.42 by xref and the frozen data (2026-10-31). Every effect id the shipped scripts start through
`SPECIAL_EFFECT_POSITION` (CHL 52) and `SPECIAL_EFFECT_OBJECT` (CHL 53), found by simulating every statement of
`challenge.chl` (no other native starts a spot visual; `ADD_SPOT_VISUAL_TARGET_POS` / `_OBJECT` only aim one). The id is
the `GSpotVisualInfo` row (`CreateSpotVisualWithSpecifiedDuration` 0x63E580 takes ids below 50); its PARTICLE_TYPE and
life are the frozen `Scripts\info.dat`'s (the 0x44-byte rows from the one named NONE), the file the particle type
table's ([Particle types](#particle-types)); a type with no file makes no effect (`GJPSysInterface::Create` 0x68F2F0
returns 0, so `GParticleContainer::Create` 0x63E4B0 deletes its container) and the script gets "Spell not created"
and 0. For every id both natives pass the magnitude 1.0, the script's own duration in turns (the row's life is only
`CreateSpotVisual`'s) and the local player, and the effect is drawn queued (`SingleZSort` 1 in every row with a file);
the container stays where it was made, and the object of `_OBJECT` is its effect's target only where the row's
`TargetOwnerObject` is set (34 here). openblack's table (`k_SpotVisuals` in `src/Particles/ParticleManager.cpp`) was
compared row by row with info.dat and the game's file table for all 50 rows: the same file and life in each.

| Id | Spot visual | Particle type → file | Life | Native: scripts | openblack |
|---|---|---|---|---|---|
| 6 | FIREWORKS | 2 → SF_FireWorks | 200 | position: landcontrol5, theslavers | ✅ the same |
| 10 | COMMAND_SUCCEED | 33 → SF_CreatureTarget | 100 | position: BlindWoman, CreatureGuide, LandControlT, Shaolin, SwapCreatures, SwapToApe, land4ogre, lostbrother, spiritualhealer, swaptobrownbear, swaptocow, thelostflock, theslavers; object: SingingStonesSongs, singingstonecircle | ✅ the same |
| 12 | CREATURE_TARGET | 33 → SF_CreatureTarget | -1 | position: CreatureBreeder, CreatureDevelopment, thehermit | ✅ the same |
| 20 | MAGIC_BEAM | 119 → SF_SimpleBeam | 30 | object: FreeTheCreature, lethysvortex, spiritualhealer, theslavers, theworkshop; position: landcontrol5, throwthroughshield | ✅ the same |
| 22 | STEAM | 114 → SF_Steam | 30 | position: SingingStonesSongs | ✅ the same |
| 23 | SMOKE | 115 → SF_Smoke | 30 | position: Crusaders, FreeTheCreature, LandControlAll, Shaolin, SwapCreatures, land4meteorites, thebigfight | ✅ the same |
| 24 | DUST | 118 → none | 30 | position: CreatureCurse | ✅ the same: no effect, 0 to the script. The original makes no dust cloud here either: info.dat's row 24 (file offset 0x48D88) is `DUST`, type 0x76 = 118, life 0x1E, reaction 0x29, SingleZSort 1, TargetOwnerObject 0; fn_0068EA00 fills all 150 names with the empty string 0xC58370 (0x68EAC0..0x68EAC8) and stores no name at +0x1D8 (118 × 4), and no other function writes the table (xref: its only writers are fn_0068EA00 and the clear fn_0068E9B0); `LoadFileData` 0x68F240 loads nothing for an empty name (0x68F262..0x68F268: the length 0 jumps to the end), and nothing else fills the file data table [0xD4EBC0], so `GJPSysInterface::Create` returns 0 (0x68F325..0x68F32E), `fn_0063E410` returns 0 (0x63E49E..0x63E4A8), `GParticleContainer::Create` deletes its container and returns 0 (0x63E512..0x63E51E), and `SpecialEffectOnPos` logs "Spell not created" and pushes 0 (0x70C3EB, 0x70C432..0x70C449). The script's `8 of Dust = 7` then sets nothing. No file of the game's is a dust for 118 (the only dust file, SF_FlockGroundDust, is type 126). The oracle can't run fn_0068EA00 (it allocates its tables on the heap, which the oracle traps) |
| 25 | BONFIRE | 116 → SF_Bonfire | 30 | position: fireonhigh, land4meteorites, land4ogre, landcontrol5 | ✅ the same |
| 27 | OBJECT_APPEAR | 131 → SF_MagicObjectCreated2 | 30 | position: Crusaders, FreeTheCreature, LandControlT, land4meteorites, thebigfight, throwthroughshield | ✅ the same |
| 29 | BANG | 135 → SF_SmokeExplode | 30 | position: idolpyre, land4meteorites, land4ogre | ✅ the same |
| 34 | HIGHLIGHT_ON_OBJECT | 137 → SF_HighlightOnObject | -1 | object: lostbrother, protectgatekeys | ✅ the same, the object its target |
| 35 | LIGHTNING_STRIKE | 62 → SF_LightningSingleStrike | 40 | position: throwthroughshield | ✅ the same |
| 36 | BEAM_EXPLOSION_FX | 138 → SF_BeamExplosionFX | 60 | position: land4meteorites, thebigfight | ✅ the same |
| 37 | BUTTERFLIES | 127 → SF_Butterflies | 200 | position: Shaolin | ✅ the same |
| 39 | FLIES | 129 → SF_Flies | 200 | object: swaptobrownbear | ✅ the same |
| 42 | FLASH | 139 → SF_Flash | 50 | position: idolpyre, land4meteorites, land4ogre | ✅ the same |
| 45 | SINGING_STONES_HEAL | 142 → SF_SingingStonesHeal | 100 | position: SingingStonesSongs | ✅ the same since 2026-10-31: its ring rule `UR_LightSheetOnObject` was not ported, so no ring stood round the stones ([the ring of light round an atom](particle-rules.md#the-ring-of-light-round-an-atom)) |
| 47 | SEE_THIS_BEAM | 145 → SF_SeeThisBeam | 100 | position: leavethroughvortexl1 | ✅ the same |

Every class of these 16 files is ported (each file's `BEGINCLASS` names against the classes openblack knows); how
each rule behaves is on its own page ([Rule index](#rule-index)).

### The camera-distance scale

Read in the exe (`ModifyAtomCollection` 0x6A7BE0, `DefineProperties` 0x6B3240, factory 0x6C09A0) and run in the
oracle.

- Properties: +0x20 `CameraDistMin`, +0x24 `CameraDistMax`, +0x28 `ScaleAtCameraDistMin`, +0x2C
  `ScaleAtCameraDistMax`; defaults 10, 100, 0.1, 1 (factory 0x6C09D1..0x6C09E6). Modifier flags |= 8 (0x6C09C7): not a
  creator.
- Each step of its effect, for its collection, it returns 0 (detached) when the two distances compare equal, a NaN
  included (`fcomp; test ah,0x40`, 0x6A7BE7..0x6A7BF2), or when the effect has no player
  (`collection +0x30` = the `PSysManager`, vt+0x1C = `GetPlayer` 0x6735C0, which returns +0x18; 0x6A7C02..0x6A7C0C).
  The neutral player is a player, so only an effect no one owns is left out.
- Otherwise, for each atom of the list (+0x40, next +0x18): the camera is `MyInterface` +0x39C (the interface
  status) +0xB0 ([The sky alignment](../magic/influence-and-alignment.md#the-sky-alignment)), read again per atom; the
  atom's place is `CalculateGlobalPos` 0x673D80. d = sqrt((dz² + dy²) + dx²), each difference camera − atom, stored
  as a float (0x6A7C60..0x6A7C8A). Then the rule scale +0x78 =
  - `ScaleAtCameraDistMin` when d < `CameraDistMin` or either is a NaN (`test ah,1`, 0x6A7C98..0x6A7CA2);
  - else `ScaleAtCameraDistMax` when d > `CameraDistMax` (`test ah,0x41`, 0x6A7CAB..0x6A7CB5);
  - else (d − min) / (max − min) × (sMax − sMin) + sMin in that order (0x6A7CBA..0x6A7CD1).
  It returns 1. No random draw. It runs at its effect's step (per turn or per frame, as the effect is stepped).
- **No effect of the game uses it.** The only file with the class is `SF_FailedCast` (group 0, the root point atom
  under which the sparks of group 1 are made; `Hierarchies` flags only group 1, so the sparks' collection is not in a
  hierarchy and does not take the root's scale). No string of the exe names `SF_FailedCast` (`xref str FailedCast`:
  none; the particle type table fn_0068EA00 names `SF_FailedApply` for the failed cast), so the game never loads it.

### Rule index

Each PSys class that the miracles use, with its address and where it is described. Those in `src/Particles/Rules/` are
registered in `ParticleRegistry.cpp`; the rest remain "not ported yet".

| Class | Address | File | Where |
|---|---|---|---|
| `CreateRuleAnAtom`, `CreateRuleSphere`, `EmitterRuleConical` | ctor 0x69F350 and MAC 0x69F410 (CreateRuleAnAtom: one atom at the spawn point + offset, once, with its SoundOfCreate); 0x69E160 (CreateRuleSphere: NumAtoms atoms inside a ball, once); 0x6A6A60 (EmitterRuleConical) | — | [Sound of the particles](../audio/magic-and-weather-sounds.md#sound-of-the-particles) |
| `StartStopSoundOnCondition`, `AddSoundToAtom`, `RemoveSoundFromAtom` | 0x69DC40, 0x69DCA0, 0x69DDD0 | `Rules/Sound.cpp` | [Sound of the particles](../audio/magic-and-weather-sounds.md#sound-of-the-particles) |
| `StrengthFloatProvider`, `EventConditionTrueWhenEnabled`, `LandscapeCollide` (SendEvent) | `LandscapeCollide` 0x67D6E0 | `SpellLink.h` | [PSys linked to the spell](#psys-linked-to-the-spell) |
| `MagnitudeFloatProvider` | UpdateParams 0x69DA90 | — | [The size of the fireball thrown with the hand](../magic/fireball-and-lightning.md#the-size-of-the-fireball-cast-with-the-hand) |
| `UR_FollowLocalHand`, `UR_FollowCastPosn` | 0x69A6A0, 0x69FE30; `UR_FollowLocalHand` DefineProperties 0x6B1B90 (`UseGraspPos` +0x20 is read and not used) | `Rules/HandFollow.cpp` | [The hand](../magic/hand-casting.md#the-hand) |
| `ZR_ChainGesture`, `CreateRuleMakeChain` | 0x68A080, 0x69FD10; DefineProperties 0x6B0170, NumAtoms 0x6B0B60 | `Rules/Gesture.cpp` | [Utility effects](../magic/hand-casting.md#utility-effects) |
| `UR_GesturingRecognised` | 0x6884F0 / 0x688910; DefineProperties 0x6B0560 | — | [Utility effects](../magic/hand-casting.md#utility-effects) |
| `UR_HandSprinkle`, `UR_WillowWisp`, `AppearanceRuleTumble` | 0x6A0220, 0x6A6D20, 0x6A6200; DefineProperties 0x6B19D0 (UR_HandSprinkle: `FracToCloseDownOn` +0x38 and `KeyPoints` +0x44 are read and not used by the rule), 0x6ABC10 (AppearanceRuleTumble) | `Rules/Sprinkle.cpp` (UR_HandSprinkle) | [Food and wood](../magic/food-wood-water-heal.md#food-and-wood) |
| `UR_HealSpellChakra` | 0x6A0B20 | `Rules/Heal.cpp` | [`UR_HealSpellChakra` 0x6A0B20](../magic/food-wood-water-heal.md#ur_healspellchakra) |
| `CreateRuleFusedSphericalExplode` | 0x69F610 | `Rules/Heal.cpp` | [`CreateRuleFusedSphericalExplode` 0x69F610](../magic/food-wood-water-heal.md#createrulefusedsphericalexplode) |
| `UR_HealInHand` | 0x6A0F40 | `Rules/Heal.cpp` | [`UR_HealInHand` 0x6A0F40](../magic/food-wood-water-heal.md#ur_healinhand) |
| `UR_KPStretchHeight`, `UR_KPMoveAtoms` | 0x6A50C0, 0x6A60B0; ctors 0x6A4F50, 0x6A5F40 | `Rules/KeyPoints.cpp` | [The other missing classes](../magic/lightning-explosion.md#the-other-missing-classes) |
| `UR_FollowTargets`, `UR_Flocking`, `EventConditionAtomNearVillagers` | 0x6A04B0, 0x683580, 0x67D8E0; `UR_FollowTargets` AtomData 0x28 bytes, vtable 0x8FD3EC, ctor 0x560210 (+0x24 the target, +0x20 the game turn it was last seen, unused) | `Rules/Flock.cpp` | [The particles](../magic/flocks.md#the-particles) |
| `UR_ForestPath`, `ParticleGoodEvilCreator` | 0x6A3770, 0x6AAA00; `UR_ForestPath` ctor 0x6A35D0, DefineProperties 0x6AEE60, AtomData 0x30 bytes ctor 0x55FAC0 | `Rules/Forest.cpp`, `ParticleEffect.h` | [The other missing classes](../magic/lightning-explosion.md#the-other-missing-classes) |
| `ParticleAnimWithCameraCreator` | vtable 0x937C54, ctor 0x6A9920, DefineProperties 0x6B42A0, CreateParticle 0x6A9B00; particle ctor 0x6C8630, GameUpdate 0x6C86F0, DrawAt 0x67AA30, StopCamera 0x6C86C0, dtor 0x6C86A0 | `Rules/Forest.cpp`, `Rules/Forest.h` | [The particle meshes](particle-creators.md#the-particle-meshes-and-the-shield-dome), [Forest](../magic/forest.md#forest) |
| `ParticleAnimCreator` | fn_006A97F0, DrawAt 0x67A8E0 | `Creators/Mesh.cpp` | [The particle meshes](particle-creators.md#the-particle-meshes-and-the-shield-dome), [Forest](../magic/forest.md#forest) |
| `UR_BeliefSprite` | 0x69B750; DefineProperties 0x6B2180, factory 0x6C01C0, AtomData 0x30 bytes ctor 0x55F4F0 | `Rules/Belief.cpp`, `BeliefSpriteMaths` | [The belief sprites](particle-rules.md#the-belief-sprites) |
| `UR_ManaPathNew` | 0x69ACD0 (its step fn_0069B2A0, its draws fn_0069AC70); DefineProperties 0x6B20B0, AtomData 0x88 bytes | `Rules/ManaPath.cpp`, `ManaPathMaths` | [The mana path](particle-rules.md#the-mana-path) |
| `UpdateRuleGravityWithFloor`, `CreateWithInitialDirection`, `AttatchFireBallToAtom`, `SetAtomHasBeenDeflected`, `UR_SideSpin`, `AR_FadeAlphaWithHeightAboveLandscape`, `AddSubCollectionsToAtom`, `UR_Trail` | 0x6A1880, 0x69E950, 0x682FD0, 0x6A26C0; `UR_SideSpin` 0x69E300, `AR_FadeAlphaWithHeightAboveLandscape` 0x6A4DE0, `AddSubCollectionsToAtom` 0x69F220, `UR_Trail` MAC 0x6A40F0; `CreateWithInitialDirection` DefineProperties 0x6B1230 | `Rules/Fireball.cpp` | [Fireball](../magic/fireball-and-lightning.md#fireball-magic_type-1-3-seed-2-fire) |
| `UR_OrientSpriteWithVelocity` | 0x69A790; DefineProperties 0x6AC610 (+0x20 SmoothFactor, +0x24 ProportionDefault); AtomData 0x38 bytes, ctor 0x560DC0 (+0x20 first, +0x24 smoothed velocity, +0x30 its rate) | `Rules/Orient.cpp` | [The other missing classes](../magic/lightning-explosion.md#the-other-missing-classes) |
| `UR_Lightning`, `UR_LightningStrike` | 0x6914C0, 0x6937A0; `UR_Lightning` props 0x6B2500, ctor defaults 0x6900B0, CollectionData ctor 0x68FE20; file PSysLightning.cpp 0x68FD50-0x6941B0 | `Rules/Lightning.cpp` | [Lightning bolt](../magic/fireball-and-lightning.md#lightning) |
| `LightningForkFlicker` | 0x6B24D0 | not ported: no spell file uses it, nothing to port | [Lightning bolt](../magic/fireball-and-lightning.md#lightning) |
| `UR_AddDefensiveSphere`, `UpdateRuleShieldSpark`, `UR_InitialSpin`, `UR_VapourEndEffect`, `SetCollectionAlpha`, `UR_AtomsAtEPTarget`, `CheckShieldDeflections` | 0x6A2A60, 0x6A2BF0, 0x69E490, 0x6A39E0, 0x6A2720, 0x69A960, 0x6A2570; DefineProperties: `UR_InitialSpin` 0x6B1520, `UR_AddDefensiveSphere` 0x6AE8C0, `UpdateRuleShieldSpark` 0x6AEA00, `CheckShieldDeflections` 0x6AF540 | `Rules/Shield.cpp` | [The particles](../magic/shields.md#the-particles) |
| `UR_SphereSurfaceTracer` | 0x6A32B0; DefineProperties 0x6AEBB0: +0x20 ThetaSpeed, +0x24 PhiSpeed, +0x28 SphereRadius, +0x2C ScaleSphereRadius, +0x30 ScaleAlpha, +0x34..0x3C ScaleX/Y/Z, +0x40 Alpha, +0x44 OrientToSurface | `ParticleEffect.cpp` | [The particles](../magic/shields.md#the-particles) |
| `ZR_SurfRevol`, `UR_ChangeScale` | 0x686370; profiles TestDisk 0x685860, TestFunnel 0x6868E0, TestFunnelSpout 0x686940, TestFunnelParab 0x686910; ctor 0x686200; DefineProperties 0x6AF6F0 | `Rules/SurfRevol` | [SF_TeleportVortex and ZR_SurfRevol](../magic/teleport.md#sf_teleportvortex-and-zr_surfrevol) |
| `UR_Explosion` | 0x67E200, 0x67ECE0, 0x67E900 | `Rules/Explosion.cpp` | [`UR_Explosion`](../magic/lightning-explosion.md#ur_explosion) |
| `SpreadingDiskEmitter`, `DiskEmitter`, `SetPSysCloseDown`, `EventConditionCollectionDelay` | 0x6A6610, 0x6A64D0, 0x6A26D0; `SetPSysCloseDown` DefineProperties 0x6ACC60 (base properties only: it closes the effect for every atom that passes the condition) | — | [The files](../magic/lightning-explosion.md#the-files) |
| `UR_MoveAtom`, `UR_ChangeScaleXYZ` | 0x6A5E50, 0x6A5240 | — | [The visible lightning bolt](../magic/lightning-explosion.md#the-visible-beam-sf_beamexplosionfx-pt-138) |
| `UR_ExplodeObject`, `UR_ExplodeObject2` | 0x6814E0, 0x681560 | `Rules/ExplodeObject.cpp` | [Exploding objects](particle-rules.md#exploding-objects) |
| `UR_SimpleBeam` | ctor 0x675C50, DefineProperties 0x6B28E0, MAC 0x6760E0, beam layout fn_00675D00 | `ParticleBeamRules.cpp`, `BeamMaths` | [The simple beam](particle-rules.md#the-simple-beam) |
| `ER_GlintsOnTarget` | MAC 0x6A53A0, glints fn_006A5520 | `ParticleGlintRules.cpp` | [The glints on a target](particle-rules.md#the-glints-on-a-target) |
| `CreateRule_GameObjectRef`, `ER_EmitFromParentAtom` | 0x69DED0, 0x6A5BA0 | `ParticleObjectRules.cpp` | [The object rules](particle-rules.md#the-object-rules) |
| `UR_CloudMoverNew`, `UR_CloudGather` | 0x6D41C0, 0x6D4A70; props 0x6ACD10, 0x6ACD70; file PSysTornado.cpp 0x6D15D0-0x6D6160 | `Rules/Storm` | [The cores and the clouds](../magic/storm-and-tornado.md#the-cores-and-the-clouds) |
| `UR_Tornado`, `UR_FollowParent` | 0x6D18B0; props 0x6AD550 | `Rules/Storm` | [The tornado](../magic/storm-and-tornado.md#the-tornado) |
| `UR_StormCast` | 0x6D59B0; props 0x6AD3E0, ctor 0x6BCFB0 | `Rules/Storm` | [The whirlwind](../magic/storm-and-tornado.md#the-whirlwind) |
| `ParticleMeshCreator`, `ParticleMeshCreatorAnimTextured` | ctor 0x6A8960; 0x6A8B00, 0x6A8DA0 | `Creators/Mesh.cpp` | [The particle meshes](particle-creators.md#the-particle-meshes-and-the-shield-dome) |
| `ParticleChainCreator`, `ParticleLightMapCreator` | 0x6AA900, 0x6A9D80 | `Creators/{Chain,LightMap}.cpp` | [Chains and light maps](particle-creators.md#chains-and-light-maps) |
| `ParticleMistCreator` | ctor 0x6AA380 | `Creators/Mist.cpp` | [`ParticleMistCreator`](particle-creators.md#particlemistcreator) |
| `ParticlePointCreator`, `ParticleSpriteCreator` | — | already there | [Not ported / not verified](../magic/food-wood-water-heal.md#pending) |
| `UR_LightSheetOnObject` | 0x69C940; ctor 0x69C8B0, DefineProperties 0x6B0300, CollectionData ctor 0x55F570, its sheets' draw fn_0069C890 | `Rules/LightSheetOnObject.cpp` | [The ring of light round an atom](particle-rules.md#the-ring-of-light-round-an-atom) |
| `UR_ScaleByCameraDist` | 0x6A7BE0; DefineProperties 0x6B3240; factory 0x6C09A0 | `Rules/CameraScale.cpp` | [The camera-distance scale](#the-camera-distance-scale) |
| `EmitterRuleLightningSprite` | 0x6A6910; emission test fn_006A6860; DefineProperties 0x6AF9C0; factory 0x6BDF90; CollectionData 0x34 bytes, ctor 0x55FDC0 | `Rules/StormFlash.cpp` | [The storm in the hand's flashes](particle-rules.md#the-storm-in-the-hands-flashes) |
| `EmitterRule::ShouldEmit`, `EmitterRuleSimple` | 0x6A63A0, 0x6A6700 | — | Simple and Disk make one atom per step; Conical loops only with `AllowMultipleEmits` (0x6A6BF4); SpreadingDiskEmitter always loops (0x6A66DC). Every shape with no PCreator returns false before `ShouldEmit` (bw1-decomp PSysModifiers.cpp:2698, 2721, 2822 and DiskEmitter's): it comes off the collection having made no atom nor its data; no shipped file has such an emitter (40 emitters read), openblack the same since 2026-11-11 (test `ParticleEmitterCreator.*`) |
| `EventAlways` | 0x69DBB0 | `Rules/Fireball.cpp` | [PSys linked to the spell](#psys-linked-to-the-spell) |
| `AR_FadeAlpha`, `UR_ChangeScale` | 0x6A4C40, 0x6A4EE0 | — | window rules: lerp inside [start, stop], the stop value on the first step past stop, nothing otherwise |
| `AR_FadeOutOnceConditionTrue` | 0x6A7CF0 | — | once ConditionStartFadeOut (+0x24) holds (or there is none) age, alpha and scale are kept and it fades out over TimeToFadeOut from there |
| `UpdateRuleGravity` | 0x6A1410 | — | — |
| `UR_GustyWind` | 0x6A7500 (noise `VLNoise3To1` 0x590CA0: the lattice [0xD066D8] through the permutation [0xBEFDBC] as perm[perm[perm[z] + y] + x] (fn_00590E30), blended linearly along x, y, z; 0 until `PSysGlobal::InitializeOneTimeOnly` fills the lattice, which it does at start-up) | `ParticleEffect.cpp` (`noise::ValueNoise3`) | — |
| `UR_OrientSpriteWithRandomAngle` | 0x6A2100 (DefineProperties 0x6AC660: +0x20 RandomAngle, +0x24 DefaultAngle) | — | [Model lighting and colour](../rendering/model-lighting.md) |
| `EC_DeflectionInAtomsHierarchy`, `EC_DeflectionInCollectionsHierarchy`, `EventConditionAtomHasBeenDeflected` | 0x67DAF0, 0x67DB20, 0x67DB70 | `Rules/Fireball.cpp` | the atom or a parent deflected / a collection's parent atom and theirs / the atom's flag bit 3 |
| `EventConditionAtomCloseWater`, `EventConditionFireBallSteam` | 0x67DCA0, 0x67DDD0 | `Rules/Fireball.cpp` | not deflected, below CutOffHeight (+0x10) above the land and over water (or off the land) / not deflected and raining or snowing there (`GClimate::GetMaxRainingOrSnowing`) |

- `UR_WillowWisp` 0x6A6D20 emits atoms along the path of the parent atom (the origin without one). CollectionData 0x38
  bytes: +0x20 the last position, +0x2C the amount emitted so far, +0x30 the atoms made, +0x34 "first". The rate is
  `MaxAtoms / DieAge` (fild +0x4C, fdiv +0x40, 0x6A6F08, no guard); `MaxAtoms` is no cap. `SmoothingValue` and
  `AccelerationForCast` are not read in the exe. `EmitConditionOfParent` is tested on the parent atom (none: nothing is
  emitted, 0x6A6F28..0x6A6F46). Each atom is placed along last → P (0x6A7075) and aged by its fraction of dt with
  fn_00673CE0 (0x6A70CC..0x6A70F7). 🔍 inferred `EmitDueToMovingMaxRate` 0 means no cap (0x6A6FB6).
- `UR_WillowWisp`'s random draws per atom, in this order: with RandomRadiusMax > 0 the angle `PSysFloatRand(2π)`
  (0x6A7132) then the radius `PSysFloatRand(RandomRadiusMin, RandomRadiusMax)` (0x6A7153); then the point in the ball
  `PSysRandR3` (0x6A71A8: x, y, z) and only after it the speed `PSysFloatRand(RandomSpeed)` (0x6A71B1), which scales
  x, y, z in turn (0x6A71B8..0x6A71D6).
- `UR_WillowWisp`'s `AddCastVelToInitPos` (+0x82, 0x6A7240) puts the atom at the parent's x and z plus velocity × step,
  and at its own y plus velocity.y × step (0x6A724A..0x6A728E: [esp+0x38] and [esp+0x40] are the parent's place read
  at 0x6A6DEE): the place along the path and the random radius are dropped across the ground, their draws still made.
- `UR_SphereSurfaceTracer` 0x6A32B0 draws three angles on an atom's first update, `PSysFloatRand(2π)` each, into theta
  (+0x24, 0x6A3330), phi (+0x28, 0x6A333D) and a third (+0x2C, 0x6A334A) that nothing reads.
- `AppearanceRuleTumble` turns by |v|·TumbleSpeed·dt (at most MaxTumbleSpeed with RestrictMaxRotation) about z when
  |v.z| < |v.x| (0x6A6293..0x6A62AC), else about x (0x6A6317..0x6A6330).

### Particles (PSys)

The PSys engine (file format, per-turn step, drawing, script effects, the rules ported for water and the town's
beliefs) is in [The PSys in the world: format, step, drawing and water rules](#the-psys-in-the-world-format-step-drawing-and-water-rules).

### The noise grid

- The lattice values 0xD066D8: fn_00590DF0 (PSysGlobal::InitializeOneTimeOnly 0x68F75C, from GGame::InitOneTimeOnly 0x54F0F4) fills them once with 1 - GameFloatRand(2) (0x590DFC..0x590E1D). That runs before GGame::Init sets the seeds (0x54F4AF), on the seed g_game was made with: 0 (inferred: the `new GGame` site is not read; the LH operator new 0x7DB790 is calloc and GData::GData 0x510500 does not set +8). The same values every game, and Init's 0x88F89F is not moved by these 256 draws
- Noise::VLattice 0x590EB0: the lattice value of i (the 256 values through the permutation table 0xBEFDBC)

### Effect core details

- AtomCore::RandomiseOrientation 0x6743E0: three PSysFloatRand(2 pi [0x40C90FDB]), each stored in the slot of the argument it pushed (fstp [esp], 0x6743ED / 0x6743FA / 0x674407): the first draw is z, the second y, the third x; then SetAngleXYZ(x, y, z) 0x674200 = Rz(-z) Ry(-y) Rx(-x)
- OrientToSurface (+0x44, 0x6A351D; set by SF_DefenseSphereInHand and SF_DefenseSphereOnHolder): each patch turns to face out of the sphere at its (theta, phi), so the 15 MSH_S_SPELLBALLSURFACE02 patches tile one ball instead of all keeping the same orientation. fn_006743A0(pi/2 - phi) [0x8C78D8]: rows (ca, -sa, 0), (sa, ca, 0), (0, 0, 1) (the cos and sin stored as floats) and the 4th row (+0x68..+0x70) zeroed (not kept here); then every row's (x, z) -> (c x - s z, c z + s x) with c = cos theta, s = sin theta (the floats stored at [esp + 0x18] / [esp + 0x14], 0x6A33B3 / 0x6A33D3), 0x6A353D..0x6A35C4
- 0x6B3566, ctor 0 (0x6A91CA)
- The collections' data go with the effect: UR_AddDefensiveSphere's DefensiveSphere (fn_006A2A30 -> fn_006D0B70)
- By index: a sub-collection's rule may add a sibling to the atom it hangs from (UR_CloudGather's AddSubCollection of the tornado group on its core, 0x6D4BBB), which the original links at the head of the list, past the iteration; (aproximado) here it is appended and also updated this step, one step earlier than the original
- SetRefPos 0x6C7600: the point, and the weight clamped to 0..1 (0x6C7604..0x6C7680: a NaN gives 0)
- The step of an effect (fn_00673340) copies the process info it is given into the manager (+0x24, 15 words), then sets the camera forward's y (+0x40) to 0 and, when x² + y² + z² (+0x3C, +0x40, +0x44) is below 1e-4 (0x6733B8), its x to 1, the z kept (0x673389..0x6733CB): every rule reads the forward across the land only, not made one long, and a camera looking straight down gives the rules a heading along x

### Town belief effects

- PSysFloatRand(a, b) 0x6729C0 of the rule (UR_TownCentreBelief 0x69C3E1 / 0x69C40E): the effect of a town centre is local (TownCentre::CreatePSys 0x69BC31, push 0), so the local stream (Step opens its step scope)
- 0x69C427..0x69C46B: f = timer / length, 0 for <= 0 or NaN, at most 1; fight = 1 - (2f - 1)^2
- A new effect: CreatePSys 0x69BC10 calls ProcessPSys at once (0x69BC95) and every effect's first Process steps twice (the flag +0xAD that the PSysManager constructor sets, fn_00672B50 0x672BF7, called by the constructor 0x672C77 and fn_00672CE0 0x672D4B; `GJPSysInterface::Process` calls fn_00673300 at 0x6736F4, which steps fn_00673340 once more and clears +0xAD at 0x673308..0x67331E, before the normal step at 0x673329) [V]; then one step each rendered frame (`TownCentre::DrawAll`). The original makes it in TownCentre::MakeFunctional (0x743F18) / ResolveLoad (0x7448D8), openblack in the same place (`town_belief::MakeEffect`, last in `AbodeArchetype::MakeTownCentreFunctional`)

### Utility effects

- PSysUtilityPSys (0xD4E0E8, 0x40 bytes made by PSysGlobal::InitializeOneTimeOnly 0x68F779): +0 the trail (48), +4 its "active", +8 the recognised sparkles (35), +0xC SF_OnFire (37), +0x14 SF_LightningStrike (61), +0x18 SF_ManaPathNew (22), +0x1C SF_BeliefSprite (24), +0x20 the selection (28), +0x24 its "active". (+0x10, the exploded meshes' SF_ExplodeObject, is PSys/Rules/ExplodeObject.cpp's own)
- The trail's magnitude table (0xC0213C / 0xC0214C): camera distance {0, 50, 500, 1500} -> {0.2, 1, 1, 1.5}
- The gesture trail's condition (fn_00671DA0): the game expects a gesture
- M_Buttons bit 0x02 (no setter found) and byte [0xD17D10] (unknown) are not ported
- fn_005CF200: a seed in the hand with m_Held & 8
- fn_005CF1C0: the selection open with the hand ready
- A circle-sized seed in the hand (fn_00729AC0 == 4)
- fn_006717F0 / fn_00671740 / fn_00671B40 / fn_00671CD0 / fn_00672100 / fn_006719E0: the slot made when missing, Process_(info, [0xD01A38]) (vt 0xFC) with every field 0 but power 1.0 (+0x30) and enabled (+0x38); 5 deletes it (vt 4) and clears the slot, made again on the next turn
- The gates [0xC029FC] (+0x0C), [0xC02A00] (+0x18, +0x1C) and [0xC02A04] (+0x08) are 1 in .data and never written (refs: reads only); the slots' NET_GAME_TYPE: 1 for +0x0C (0x6716EE) and +0x14 (0x67198E), 0 for the others
- The pixel box of the matched samples (0xD4EB00)
- fn_00689790 0x68989E..0x6898C2 compares fabs(x), fabs(y), fabs(z) with the double at 0x8C79D8 = 9.9999997473787516e-05, the float 1e-4f widened (so the float compare is exact)
- fn_0068C650 (the gesture's shape) and fn_0068C140: the box keeps its centre, its half sizes divided by the shape's
- fn_0074CAF0
- The shape is squashed vertically on the screen (x 0.75 each time) until its depth on the land is no more than twice its width, at most 15 times (fn_00689790: 0.75 [0xC02650] at 0x689C20, 15 [0xC0264C] at 0x689C26, the ratio against 2 [0x8AB478] at 0x689C37)
- PSysUtilityPSys: SF_Gesture (35) is stepped once a turn (ProcessTurn, fn_00672100) and drawn every frame with the turn's fraction (fn_00671DA0 0x671DD3); its rule takes the record
- The recognised sparkles (35) are stepped once a turn (ProcessTurn) and drawn here with Draw_(1) (0x671DD3). (pending) the original draws them only while MyInterface +0x3A0 != 0
- The records waiting for UR_GesturingRecognised (the list 0xD4EB10 / count 0xD4EB18; it takes the newest one per step)
- fn_00671DA0 (misnamed GInterface::InterfaceActionHandObjectApplyToPos), every frame: the trail on while the game expects a gesture, following the hand with magnitude handScale x f(camera distance); the selection effect while the selection is open. seconds = the frame's game time.
- fn_006721B0 (PSysGlobal::GameLoopEnd 0x68F5C9, after the exploded meshes' slot +0x10, which explode_object::GameLoopEnd keeps), once a turn: the slots +0x0C SF_OnFire, +0x18 SF_ManaPathNew, +0x1C SF_BeliefSprite, +0x08 SF_Gesture (the recognised sparkles) and +0x14 SF_LightningStrike, in that order, each made when missing (PSysInterface::Create(NULL, type, 0, 0, 1.0, NET)), stepped with an empty ProcessInfo (power 1, enabled) and the turn's ms ([0xD01A38], vt 0xFC Process_), and dropped when it returns 5

### Addresses

The code's comments describe what it does in plain words; the original's addresses and function names they used to quote are kept here, next to the openblack symbol each one corresponds to.

| Address or name | What it is | openblack |
|---|---|---|
| `0x67C175..0x67C6B2` | RenderParticleGJMesh::DrawAt: the CPU lighting of the mesh's world triangles (land light + model light) | `vs_world_triangles.sc` |


### Sources

- runblack.exe (the addresses above) and, for the sound bank, `LHaudiodllR.dll`.

## openblack

Status: **partial**: 25 of 34 rows done and 4 partial in [the particles progress page](../project/progress/rendering/particles.md)
(84%).

- `src/Particles/` (`ParticleEffect.cpp`, `ParticleManager.cpp`, `ParticleSpellFile.cpp`, `ParticleRegistry.h`, `SpellLink.h`, `Noise.cpp`,
  `Utility.cpp`, `TownBelief.cpp`, `ParticleWaterRings.cpp`): the engine, reached through Locator services; the spell
  files, textures and meshes load once through the resource caches;
- `src/Particles/Creators/` and `src/Particles/Rules/`: the creators and the rules;
- an effect keeps the forward of the process info its spell gives it across the land only, along x when almost none is left (`particles::FlatHeading` in `SpellLink.h`, from `Effect::SetProcessInfo`; tests `LightningMiracleHeading.AnEffectKeepsTheForwardAcrossTheLand`, `LightningMiracle.TheBoltsAimAlongTheCamerasForward`): a bolt or a storm cast looking straight down heads along x, as the original's;
- the formulas (frame steps, chain UVs, sound sizes) are tested with fakes in `test/`, for example `test_spell_sounds`
  and `test_lightning`.
- Every effect's first process steps twice, as the original's: `Effect::Process` in `src/Particles/ParticleEffect.cpp`
  (its first-process flag), which every process of `src/Particles/ParticleManager.cpp` goes through; the town centre's
  effect is processed once when it is made (`town_belief::MakeEffect`), so it steps three times on that frame.

Where it is in the code:

- `ParticleEffect.h` / `ParticleEffect.cpp`, the effect:
  - `Effect::AnyCreatorLeft`: whether the effect still lives: a creating modifier, a keep-alive one until it closes,
    or a class not ported yet until the spell closes it;
  - `Effect::CollectOrderedOf`: the draw order of a collection: its atoms, its chain, then every atom's children;
  - `Effect::ConditionForAtom`: the event conditions on an atom (for example alpha above AlphaValue);
  - `Effect::GlobalToLocal`: a world point into the collection's frame, the inverse of LocalToGlobal;
  - `Effect::MoveToBaseGroup`: moves an atom to the first root collection of a group;
  - `Effect::SpawnPosition`: a new atom's place: the origin, 0 in a flagged parent's frame, or the parent's position;
  - `NewAtomInGroup`: a new atom in the first root collection of a group (none when the group has no root);
  - `PowerUpLevel`: the spell's level, -1 without a spell; `SendSpellEvent`: an event to the spell, 0 without one;
  - the emitters (`Shape::Conical` loops with AllowMultipleEmits, the spreading disk always; the others make one atom
    a step).
- `ParticleSpellFile.cpp`: `File::Load`, a spell file read once and kept in a cache by name.
- `SpellLink.h`: `IsMyInterfaceCasting`, whether this computer's interface casts the spell; `PowerUpLevel` there is
  the spell's side (-1 base, 0, 1).
- `ParticleManager.h`: `SpotVisualDrawPath`, the queued or the direct draw of a spot visual from its SingleZSort flag.
- `ParticleWaterRings.h`: `AddParticleRipple`, the ripple of a particle that touches the floor over water, farther than
  the minimum distance from the last one, growing at 4 times the atom's radius.
- `Rules/SurfRevol.h`: the test shapes `FunnelSpout` (r = 1.5 t, y = 3 (sqrt 2t - 1)) and `FunnelParab`
  (r = t, y = 3 (t^2 - 1)).
- `TownBelief.cpp`: `ForEachCentre` (the available town centres), `RemoveCentre` (a deleted centre's effect goes),
  `ShownSymbols` (the players with belief in the town and their rank), `StepCentre` (the symbols' angles, waits and
  fight timers), `SymbolCreator` (the creator of a player's symbol: the local player's from its profile's ChooseSymbol cell, every other player's from its cell of the players' symbol sheet, `player_symbol::SourceOf`).
- `Utility.h` / `Utility.cpp`: `PendingRecognised` (the recognised gestures waiting for their sparkles, the newest one
  per step), `StepTurn` (a utility slot made when missing and stepped with power 1; type 5 deletes it), `TrailScale`
  (the trail's magnitude by camera distance, {0, 50, 500, 1500} m -> {0.2, 1, 1, 1.5}).

Porting decisions taken from raffclar's tree (his particle rules on our engine, the draw list, the ribbons):
[Port decisions: engine and world](../project/port-decisions-engine-world.md#particles).

#### Particle types

- openblack takes raffclar's table (`particles::ParticleTypeFile`, plus `ParticleTypeName` for the debug windows): its 150 file names are the ones the game's table fills in (fn_0068EA00, 122 entries, compared one by one 2026-10-31). Type 2 (FIREWORK) had no file in it, where the game's names `SF_FireWorks` (`SpellFileNames + 8`); it now does (test `MagicTables.particleTypeFiles`). The spot visuals' own table already named it for the fireworks 6, 7 and 8; an effect started by particle type 2 now makes the fireworks, as the game's does.

#### Fix in the PSys core: the hierarchies

- An atom moved to a group with no root collection: openblack deletes it (`Effect::MoveToBaseGroup`), where the original leaves it in none ⚠️ approximate.

#### The PSys in the world: format, step, drawing and water rules

- openblack reads the format with raffclar's `components/psys` (`ParticleFile`, `SplitCompressed`; `EnumHeader` for `Data\AllMeshes.h` and `Data\SoundAction.h`; `StackedBitmap` for the light maps). `test_particle_file_parity` keeps the reader openblack had before as a reference: on all 132 spell files, the 4 enum headers and every light map layout of the install both give the same data. The component is stricter on malformed input (a number with trailing characters, a `+` sign, a missing `SIZE`, a negative count or a misspelt sound switch make the file unreadable, where the old reader read on); no file of the game has any of these. Its enum reader takes every enum of a header (the old one only the first: `AllMeshes.h` adds its `ANM_` names, which no `MeshEnum` names) and knows no `/* */` comments or hexadecimal values (none in the game's headers).
- openblack steps them once a turn (pending).
- openblack: `manager::CollectSorted` / `CollectQueued` / `HandEffects`, which the renderer draws by path (`manager::k_DrawByPath` is on, b02ffeb75: `RenderingSystem` puts every atom in `particleAtomDrawDescs`).
- In openblack: `particles::manager::StartSpotVisual`, reading the info block's flag (the same five entries without one).
- openblack: `components/psys` (the file format) and `src/Particles/ParticleSpellFile` (the file's name and its cache), `ParticleEffect` (collections, atoms, rules: CreateRuleAnAtom/Sphere, emitters Simple/Disk/Conical, UR_WillowWisp, deletion rules, AR_FadeAlpha/FadeCollectionAlpha/FadeOutOnceConditionTrue, UR_ChangeScale, SetScale, SetAtomAlpha, UpdateRuleGravity, UR_UpdatePosnFromVelocity, UR_GustyWind (with the original's 3D lattice noise, `noise::ValueNoise3`), UpdateRuleRotatePrincipalAxis, FollowOrigin, UR_FollowParent, ForceConstant*, UR_SphereSurfaceTracer, UR_OrientSpriteWithRandomAngle; conditions and float providers), `ParticleManager` (effects, script containers, test hook) and `Graphics/RendererParticles.cpp`. The unported classes are logged once ("not ported yet") and do nothing.
- openblack uses float sums ⚠️ approximate.
- The random draws of `UR_WillowWisp` (`ParticleEffect.cpp`) are made one per statement, the point in the ball before
  the speed, so no compiler can take them in another order (raffclar's #952, refitted; `test_particle_draw_order`).
- `UR_SphereSurfaceTracer` draws its three angles one per statement, theta first; it drew only two before
  (`test_particle_draw_order`).
- `UR_WillowWisp` with `AddCastVelToInitPos` starts from the parent's x and z, as the original; it kept the place along
  the path and the random radius before (`test_particle_draw_order`).

#### The camera-distance scale

- openblack: `camera_scale::Runs`, `CameraDistance`, `ScaleAtDistance`, pinned to the oracle's bits in `test/test_camera_scale.cpp`, and the rule `ScaleByCameraDist` (`RegisterCameraScaleRules`): "owned" is an effect player ≥ 0, the camera is `Locator::camera` `GetOrigin()` at the step, the place `Effect::GlobalPosition`. Without a camera (tests only) it leaves the atoms as they are (port guard). Since the game loads no file with it, it changes nothing in a game; the debug menu's particle player can show it with `SF_FailedCast`.

### Test hooks

- `OPENBLACK_TEST_PSYS="SF_Name,x,z[,height[,magnitude[,seconds]]]"`: that spell file at (x, ground + height, z).
- `OPENBLACK_PSYS_TRACE=<turns>`: the effects' log; `OPENBLACK_TEST_PSYS_RAND_OUTSIDE=1`: each site that draws a
  particle random number outside an effect's step, logged the first time, with the totals at exit.
- `OPENBLACK_TEST_PSYS`, `OPENBLACK_PSYS_SOUND_TRACE` and `OPENBLACK_PSYS_CHAIN_TRACE`, in
  [Debug environment variables](../project/debugging.md#debug-environment-variables); `test_spell_sounds` and
  `test_lightning`.

## Pending

- **Verify with xref/oracle:** the 3 **🔍 inferred** and 0 **⚠️ approximate** marks in The original above. Each is settled by reading the function named next to it (`python tools/xref/xref.py show <function>`) and, where a number is in doubt, by an oracle run of that function (`tools/oracle/GUIDE.md`).

- Measuring the double first process (`Effect::Process`, 2026-11-08): it moves every effect's atoms from its first
  turn on, so it moves nearly every reference (scenes, fidelity draws, hand demos); it goes alone, first, in a joint of
  its own.
- Chains: `UseDynamicLighting` (the V scroll is 0 in all the data).
- The spell dispensers' effect and the flying flock's cast are stepped once a turn in openblack, not by their owner's
  Draw with the frame's ms.
- The rule files that delete atoms themselves (Flock, Gesture, Heal, Lightning, Shield, Storm) do not tell the
  creators yet; none of them deletes SF_Forest's camera atom.
- World PSys: the mesh creators, mist, chains, light maps, town rules and sounds are on this engine now
  (`src/Particles/Creators/`, `Rules/`, [Particle creators](particle-creators.md#openblack)). Left: the hand's
  multi pick-up effects (`src/ECS/Systems/Implementations/HandEffects.cpp`) are still hand-written, with the
  SF_MultiPickUp files' values as constants and the grains and fish tinted by a mean colour of their cells where the
  original uses the landscape light; to be moved onto this engine.
- The belief sprites: an atom of the rule's collection that it did not make has an AtomData the original leaves at the
  heap's fill (`PSysBase::new`), where openblack starts it at 0; no file has such an atom. The sound's owner
  alignment is the one every particle sound lacks (above).
- The testbed scenario
  `particles.highlight_glints` sets out a silver and a gold scroll (types 100 and 99) close up for checking their
  glints; it has no reference run yet.
- Unknown: whether `PSysManager::IsInState(1)`, which makes `Process_` 0x6736B0 return without stepping when the file's
  +0x1C flag is set, is the pause state; so whether the original skips the step on paused frames is not known.
- `magic::CubicSpline` (Magic/HandMotion.cpp, the hand's pour curve, built by fn_005B3760 for HandStateGrain) is a
  third copy of the key-point spline build with divisions, so its float steps are not the game's; its reader in the
  original is not located. To be routed through `key_points` once the reader is known.
- The key-point rules with keys at the same t (h = 0 or not a number), which no file has:
  - `UR_KPStretchHeight`'s inline reader stores the bits of the atom pointer argument into the stretch (0x6A51BB):
    a heap address, which cannot be reproduced; openblack keeps the stretch.
  - `UR_KPMoveAtoms`' output starts at 0 once per step (0x6A6104) and carries the previous atom's value; openblack
    starts each atom at 0.
  - `UR_ForestPath`'s two outputs are uninitialised locals; openblack uses 0.
  - `UR_KPStretchHeight` skips an atom whose age is not a number (the `test ah,1` on StartTime); openblack does not.
- The simple beam:
  - On a creature the original ends the beam at one of its bones (`Creature::GetPSysBeamTargetPos` 0x4F8B30,
    fn_004813B0, bone index LH3DCreature +0x51B8); which bone, and where +0x51B8 is set, is not found. openblack ends it
    at half the creature's height above its place, like any object.
  - An object's end: the original starts from `MapCoords::GetLHPoint` of the object's +0x14 (the fixed-point map
    coordinates) and adds GetHeight x 0.5 to y; openblack starts from its Transform position. That the two are the same
    float for every object a beam can target is not shown.
  - When an unset target (0xCDCDCDCD) is laid, the MinHeight test asks the land at x = z = -4.3e8, out of the int32
    range of the map coordinates: openblack's `GetHeightAt` gives 0 there (FtoL pins it to INT32_MIN, a negative
    cell), while the original's float-to-int keeps the low 32 bits of the int64, so its cell there is not known. It
    does not matter with the files: with MinHeight -4.3e8 the test y - ground < MinHeight fails for any land height
    (the inner key points have y >= about -3.45e8), so the clamp can never fire there.
  - NumSplinePoints below 2 crashes the original (the u array of n - 1 entries is written through a null pointer at
    0x677647) and makes no curve in openblack; no file gives it.
- raffclar's names still to take for the particle files (with the particle system interface): `particles::Object` is an
  alias of the component's `ParticleObject` and `particles::File` adds the name to `ParticleFile`; the loaders keep the names
  `ParticleFileLoader` and `LightBitmapLoader` (his `ParticleFileLoader` and `ParticleBitmapLoader`); the namespace sweep
  `psys::` → `particles::` is its own step.
- A loose `.txt` that the original reads with a malformed number: whether the original reads on as the old reader did
  (fn_006B4C40) is not checked; the component rejects the file.
- Unverified: `Particles/Creators/Mesh.h` `drawWithLandscapeColour`: "///< +0x5E: the particle's +0x24 bit 2 (CreateParticle 0x6A8B82)" versus `Mesh.cpp` `MakeMeshCreator` and miracles.md: "puts it in the particle's +0x24 bit 1". Bit 1 or bit 2: unverified.
- The script natives' spot visuals (`SPECIAL_EFFECT_POSITION`, `SPECIAL_EFFECT_OBJECT`): both readings hold. The native takes seconds and turns them into turns itself, `ftol(seconds × (1000 div ms per turn))` with the integer division first (`GScript::SpecialEffectOnPos` 0x70C330), and `CreateSpotVisualWithSpecifiedDuration` 0x63E580 then takes turns ([Turns, not seconds](../magic/lightning-explosion.md#ur_explosion)). Both natives now take `game_clock::TicksForSeconds` (the integer division first, 0 kept as 0 turns, a negative truncated, as 0x70C349..0x70C36A and 0x70C4A6..0x70C4C7). `GParticleContainer::Create` stores any count as it is (fn_0063E410 +0x30) and its `Process` keeps a negative one for ever (0x63E2A0..0x63E2B1); openblack's `StartSpotVisual` keeps -1 for every negative, the same in play. (Every spot visual is now started through the particle system's `StartSpotVisual`, in turns.)
- Unverified: `Particles/ParticleEffect.cpp` `LandscapeCollide` / `EventAlways` use `GetLastGlobalMovement 0x674420 (inf: the atom's velocity over this step)`, while `Rules/Shield.cpp` says it is "the drawn position minus the one before (0 until it was drawn twice)". The Shield reading is kept above; unverified.
- Not recorded: what the original does with an effect that has no player. Ours keeps -1 as "no player": the player-colour tint, the player colour on the surface-of-revolution effects, the evil-forest alignment test and the fireball and shield spin rules are all skipped until a player is set. raffclar's testbed gives every scenario effect player 0 when none is named, and ours now does the same; the -1 behaviour itself is our port's, still to be checked against the exe.

