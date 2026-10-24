# The temple's leash posts

## Summary

Every temple has three leash posts beside its heart, one for each leash: the aggression leash, the learning leash and
the compassion leash. A post shows (a turning collar and a puff of smoke) once the temple is fully built and the
player's creature knows that leash. Tapping one of the local player's posts picks that leash: the collar then hangs on
the hand, the smoke turns orange, and a packet sets the creature's leash type at the next turn, which also sets the
leash's mood. Tapping the picked post again drops the pick.

openblack makes the posts with the heart (the same creation indices and CRT draws), draws them, lets the hand feel and
tap them, and applies the pick through the same packet, which wipes the local player's gesture path as the
original's. Left: the posts during the temple's destruction, and a few details of the hand and the culling. The leash itself is on
[The leash](leash.md#leash-and-the-hand).

## The original

### The temple's leash posts

Read in the 1.20 exe; bw1-decomp has the headers and symbols (`src/Black/CitadelHeart.h`), no bodies.

#### When and in what order

`CitadelHeart::Create` 0x464E20 builds the heart (its constructor 0x4649B0, an Object), then
`CallVirtualFunctionsForCreation` 0x4675A0: its 3D object (CITADEL type 8), the **CitadelEntrance** (an Object, ctor
fn_00468EB0 from 0x4676B9), `InsertMapObject`, a camera object (fn_00454960, not an Object), fn_008499C0, and at
0x4677DA `CitadelHeart::CreateLeashes` 0x464950. That does nothing when heart +0xE4 is set; else `new(0x10)`
`TempleLeash(heart 3D object +0x40, heart player +0xB5 = player number)` 0x464650 into +0xE4. A heart therefore takes
**five** creation indices: heart, entrance, post 0, post 1, post 2 (`TempleLeash` itself is not an Object). The
worship sites (fn_00464F50) are made after.

#### The posts and what they hold

**`TempleLeash`** {+0x0 the pick, +0x4..+0xC the three `LeashObj*`}. For i = 0, 1, 2 (one loop, 0x46466D..0x464818):

- **The point.** `GetExtraPos(i, &point)`, LH3DObject vt+0x1CC; for the citadel type fn_008831C0 (vtable 0x9A2BFC;
  the static type's is 0x80FF20). Metric i of the object's mesh (0x30 bytes each, translation p at +0x24) through the
  object's matrix, every step a float: x = ((p.y m3 + p.z m6) + p.x m0) + t.x, y = ((p.x m1 + p.y m4) + p.z m7) + t.y,
  z = ((p.x m2 + p.y m5) + p.z m8) + t.z (m0..m8 the rows at +0x14, t at +0x38; 0x883289..0x8832D0). When the object
  morphs with the land (vt+0x1F4; the citadel's is fn_0080BA90, always 1) y = (H(point) - H(origin)) + y, both heights
  `LH3DIsland::GetAltitude` at x and z taken as (v × 65536) × 0.1 truncated (0x8832E3..0x883377). With no metric i
  (or no metrics) it is the object's own position (0x883238, 0x883388). Then `MapCoords(point)`.
  `B_FIRST_TEMPLE` (and `b_temple00`) has 16 extra metrics; 0..2 are (-15.1034, 5.3527, -12.0655),
  (-17.5088, 5.38235, -14.5343) and (-19.6542, 5.35506, -18.2662), and 15 is the pen point.
- **The post**, `new(0x78)` `Object(MapCoords, GObjectInfo 0xD43368)` (0x636520, the next creation index), vtable
  `LeashObj` 0x8C84A0: +0x6C = the owner's player number, +0x54 = i, +0x58 = the TempleLeash.
  - +0x40 = `LH3DObject::Create(STATIC)`, mesh `MeshPack[0]`, `SetPosition(point, 0, 1)`. Never drawn itself: the draw
    copies its matrix.
  - +0x5C, **the collar** = `LH3DObject::Create(STATIC)` with collar mesh i, `SetPosition(point, 0, 1)`,
    `SetDynamicLighting(1)`, `SetAnimatedUV(0, v)` with v = 0.375 for i = 0, else i × 0.125, then `SetNeedSorting(1)`.
    v is the band of the leash texture for that type: the same v0 as the worn rope's
    ([The leash's rope](leash-rope.md#the-leashs-rope)).
  - The collar meshes (fn_004645D0, lazily into 0xC5E3D8..0xC5E3E0): **three loads** of `Data\MISC\leash.l3d`
    (`LH3DMesh::CreateFromHD`), each retextured by fn_0080B250(mesh, fn_008485E0()). fn_008485E0 returns
    `data\textures\leash.raw`, loaded once with flags 0x41 (ARGB4444) into [0xEF7540], the rope's texture;
    fn_0080B250 replaces the texture of every textured primitive of the mesh. The three copies are identical.
  - **Four CRT draws** (`Random` 0x81D180), in this order: +0x60 = Random(0, 1), +0x64 = Random(0, 2π),
    +0x68 = Random(0, 2π) (2π = 0x40C90FDB), +0x74 = Random(0, 15). 12 per temple, post by post
    ([Random streams](../engine/random.md#random-streams-between-turn-and-frame)).
  - +0x70, **the sprite** = `LH3DSprite::Create(1, 1)`, texture [0xEA1ABC] (smoke), its two extents × 2 / old size and
    size 2, colour 0x80FFFFFF, position = the point (never moved after).
- At the end the pick = **-1**.

**`LeashObj`**: `SaveObject` 0x464840 returns 0 (never saved; remade with the heart), `ValidAsInterfaceLeashTarget`
0x464850 is 0 (the leash cannot be tied to a post), `GetText` "Leash Obj". It keeps
`InterfaceMustBeInInfluenceForInteraction` 0x4028A0 = 1 (vt 0x714) and `Object::ValidForPlaceInHand` 0x402870 = 0
(vt 0x6FC). The three are deleted by the heart's **destructor** (0x464BF0, from the scalar deleting destructor
0x464BC0), not its `ToBeDeleted`: after its +0xA8 object, it calls fn_004648C0 on +0xE4 (0x464C1F; each `LeashObj`
deleted at once through vt+4 with 1, no `ToBeDeleted`), frees the `TempleLeash` and clears +0xE4. So the posts live
as long as the heart object, an unavailable heart's included.

**The pick.** Setter fn_004648E0 (`CitadelHeart::SetLeash`) and getter fn_00464920, as in
[Script enum and the temple's selection](leash.md#script-enum-and-the-temples-selection).

#### The collar's mesh

**`Data\Misc\leash.l3d`** (2788 bytes): flags 0x22000, two sub-meshes (flags 0xE0000800: LOD mask 7), each one
primitive of 34 vertices and 32 triangles, material type 9 (`TexturedChroma`), ALPHAREF 20, flags 4 (tiling,
one-sided, the object's UV offset added), colour 0xFF31798B, skin 0xFEA6C2F6. The file carries **no skin**: its skin
table starts at the end of the file, as `sun.l3d`'s and `mist.l3d`'s. The two sub-meshes are the same ring of 16 faces
round the z axis (radius about 1.06, z from -0.35 to 0.35), half a face apart: u runs round the ring (0..1, with the
seam doubled), v from 0 to 0.125, one eighth of the texture: the `SetAnimatedUV(u, v(i))` of the draw moves it onto
the leash's band and scrolls it round. (🔍 inferred) the skin id stays in the primitive's texture slot (never
resolved, as the mist's and moon's), so fn_0080B250's test for a texture sees it and replaces it: every primitive of
the file takes `leash.raw`.

#### The draw

Frame side, fn_00466730 from `CitadelHeart::DrawNow` 0x46733D (which runs for every heart from the landscape draw,
fn_00467360, whatever is on screen, and not in the citadel's draw). Nothing unless the heart's player has a creature
(+0xA4C) and the heart 3D object's +0x9C == 1.0. +0x9C is the percent built, clamped to 0..1 by the citadel's vt+0x200
fn_00883120, set each turn by `Citadel::Process` 0x462E33 from the heart's `GetPercentBuilt`; reaching 1 also sets the
player's flag [0xEB9A1C + 4 × player]. For each i (t = i + 1):

- **Gate**: post i is skipped (no draw, no collision) unless Creature +0x1108 + 4t is set (+0x110C EVIL, +0x1110 ROPE,
  +0x1114 GOOD: the creature knows that leash).
- The point is read again with `GetExtraPos(i)`; dt = float(`g_game_time_inc`) × 0.001.
- **Spin and scroll**, each a float stored, then wrapped by x − float(trunc(x × 1/P)) × P:
  +0x64 += 0.1 dt and +0x68 += 1.0 dt (P = 2π, 1/P = 0x3E22F983); +0x60 += 0.5 dt wrapped by x − float(trunc(x)).
- The collar: `SetAnimatedUV(+0x60, v(i))` (u scrolls at 0.5 a second); its matrix = the post object's matrix, then
  `MulPre` (fn_007FAE60) by `SetYXZMatrixOnly(0, +0x64, +0x68)` with no translation: it turns in its own frame.
- **The sprite's frame**: +0x74 += 10 dt, wrapped with P = 15 (1/P = 0x3D888889); `ftol(+0x74) & 0x3F` into the
  sprite's flags (low 6 bits): frames 0..14 at 10 a second.
- **Brightness** b = trunc(float(B + G + R of [0xEDDD08]) × 0.16666667), [0xEDDD08] = the terrain light table's entry
  255 ([The terrain light table](../world/terrain.md#terrain-light-table)).
- **The picked post** (pick == i) of the local player's temple:
  - if the hand's 3D object (`GInterface +0x3A0` = `CHand`, `+0x482C`) has +0xAC set, the post is not drawn and gets no
    collision this frame. +0xAC is "the hand is hidden": 0 by `CHand::Show` 0x46C1B0, 1 by fn_0046C2E0 (from
    `OperateInteractionCamera` 0x46CBDC / 0x46CBEF and others);
  - otherwise the sprite is (b << 24) | 0xC18119 in the additive smoke [0xEA1AC4], and **the collar hangs on the hand**:
    at the translation of the hand object's +0x80 matrix (`Morphable::TransformedMatrices`, CHand +0x47F0, set at
    0x46CB10), with that matrix's `GetYXZ` angles, then `MulPre` by diag(0.5 s) with translation (0.05 s, 0.25 s, 0),
    s = the hand object's scale (+0x44, from CHand +0x94) × 173.52948.
- Any other post: the sprite is (b << 24) | 0xFFFFFF in [0xEA1ABC].
- Each post drawn: `LH3DSprite::AddDrawing`, the collar's draw (vt+0x100), then
  **`GInterface::SendInvisibleDrawCollision(post, point, 1.0)`** 0x519960: the hand's pick of it, a sphere of radius 1
  at the point just read (the picked one too); its depth is the projected w.
- The smoke sprite is culled only when its quad is wholly off one side or its centre is behind the near plane, with no
  far test. `DrawNow` skips the posts while the heart's destruction sequence runs (+0xB8, +0xBC > 21.8 s).
  `SetDynamicLighting(1)` of the collar sets +4 |= 0x20 only at the "Light" detail 2-6.

#### The tap

- `InterfaceValidToTap` 0x464450 ignores its argument: post +0x6C == `players[GGame::PlayerIndex]` +0xB5 (g_game
  +0x205A59 the index, the players at g_game + 0x18, 0xA60 each). Only the local player's own posts.
- `InterfaceTap` 0x464490: not the local player's post: 1, nothing done. The picked post: pick = -1, 1, no sound and no
  packet. Otherwise sound 42 `G_ClickOnSpell_01` when the status is `MyInterfaceStatus`, pick = i at once (frame
  side), and `SetPacket(0x65, i + 1, 0)`. The leash keys V and B send the same packet
  ([The leash keys](leash.md#the-leash-keys)); openblack's do too (`LeashSystem::PressKey`).
- A post needs the hand in the influence and never goes into the hand, so a press on one is a tap at once, and the
  hand state over it is 18 Over Object
  ([The text of each state](../interface/tooltips.md#the-text-of-each-state)).

#### The tooltip over a post

`GetOverwriteInteractableToolTip` 0x464580 (vt 0x194: i = 0 → 0xEC8, 1 → 0xECA, 2 → 0xEC9) is read only in hand state
13 Can Select Lock (fn_005D77C0, 0x5D787E: its text, else 0xE85; the only call through vt 0x194 in `.text`). A post
never reaches 13: fn_005D7F20 gives 13 only through `ValidForLockedSelectProcess` (vt 0x6CC), which LeashObj keeps
from Object (0x419330 = 0), and 9 only through `ValidForPlaceInHand`, also 0, so over a post the state is 18 (its
IsInteractable, vt 0x190, is GameThingWithPos's 0x5701B0, IsAvailable). State 18 shows, in the influence and valid to
tap, `GetOverwriteTapToolTip` (vt 0x19C, LeashObj keeps GameThingWithPos 0x5705C0 = 0) or 0xE7A: **over a post the
hand shows 0xE7A "Tap"**, and over another player's post (not valid to tap) the land texts.

#### Packet 0x65

Turn side, `GPacket::ProcessPacket` → 0x63DCD7: when the player has a creature, the packet is dropped while it is
under the creature spell COMPASSIONATE (8) or ANGRY (9) (`IsCreatureSpellActive` 0x4F51E0, creature +0x370: the slot's
+8 is its state, set to 1, waiting, by `ActualReceiveSpell` 0x4F509F, so a spell waiting to start counts). Otherwise
(also with no creature, and with a leash the creature does not know) `GLeashStatus` (interface status +0x39C →
+0x12C) +0x1C = type (0x5E6AEA), then fn_005E6B00: the heart's `SetLeash(type)` again (the pick, synchronised),
`Creature::UpdateMoodsFromLeash(type)` when the player has a creature, and for the local player it clears a block of
g_game +0x25006C (+8, 0x320 dwords; bytes +0xC88, +0xC90, dword +0xC8C). That block is the `GestureSystem` (0xC98
bytes, ctor 0x54BB40): its stroke buffer, the same wipe as `ProcessPowerUpSystem`'s exit (0x5E6B59..0x5E6B81), which
`SetOn` does too (on: through fn_005E6B00 at 0x5E7001; off: 0x5E7102), read in the frozen v1.42 exe
([the leash picker](../magic/gestures.md#the-leash-picker)).

**`Creature::UpdateMoodsFromLeash`** 0x4CEFC0, once at the change and only while a leash is on
(`GetInterfaceStatusLeashOn` 0x4CF060, any of the player's interface statuses): EVIL →
`SetCheatDominantDesire(2 ANGER, 36000, 0)`, GOOD → (1 COMPASSION, 36000, 0), ROPE → fn_004DCF20 (the clear,
[The timed dominant desire](leash.md#the-timed-dominant-desire)).

#### Land 1

With the start-up answer 3, `LOAD_MY_CREATURE(1850, 1300)` at turn 7 loads the profile's creature (`C4ba71b36.erc`),
and `LandControl1` at turn 30 calls `DEV_FUNCTION(2)` then `DEV_FUNCTION(3)`
([Changing land](../scripts/land-change-and-saving.md)): from then on the creature knows all three leashes, and the
temple, built by the skip's `SET_PROPERTY(22, citadel, 1.0)`, shows its three posts. The test runs' locked land view
(camera at (1752, 52, 2650) looking at (1786, 26, 2800)) faces away from the temple at (1915.05, 2508.89), so the posts
are off its screen.

### Sources

- bw1-decomp `src/Black/CitadelHeart.h` (the layout and the symbols); the bodies read in the exe; `Data\Misc\leash.l3d`
  and `leasha.raw` read from the frozen game data.

## openblack

**Done**: the posts, their draw, the hand's feel and tap, packet 0x65 and the moods it reaches. **Partial**: see
Pending.

- **The posts.** The creation index skip in `CitadelArchetype.cpp` (5, as above); the pure rules in
  `Worship/LeashPosts` (`worship::leash_posts`: the point, the four draws' ranges, the spin step, the sprite cell, the
  collar band, the smoke's brightness and colour, the pick's two mappings, the scripts' reading of the pick and the
  tooltips; `test_leash_posts`); and the posts themselves, `Worship/TempleLeash` (`worship::temple_leash`, tested in
  `test_citadel_plan`). `CitadelArchetype::CreateHeart` calls `CreatePosts` right after the heart goes into the map
  cells, with `game_random::crt::Random`: three entities, post 0 first, each with a `Transform` at its point (unturned,
  scale 1: `LH3DObject::SetPosition(point, y_angle 0, scale 1)`) and a `LeashPost` {index, owner, heart, spin}, the
  spin seeded by its four draws; the heart gets a `TempleLeash` {posts, pick -1}. The posts are not in the map cells
  and not saved, and they go when the heart's entity is destroyed (an `on_destroy<TempleLeash>` listener;
  `Game::LoadMap` disconnects it before the registry's clear, which takes the posts with everything else).
- **The collar** is one mesh in the cache for every post (`resources::shared_assets::LoadLeashCollar`, from the game's
  start, `misc/leash`), its textured primitives retextured (`L3DMesh::SetSkinOfTextured`) with `misc/leash_collar`:
  `leash.raw` with `leasha.raw` as its alpha, cut to ARGB4444 (`Texture2DLoader::FromColourAlphaTag`, `Argb4444`), the
  game's one texture of the rope's two files (`test_leash_collar`).
- **The draw** (frame side, `Graphics/RendererLeashPosts.cpp`, the pure `graphics::leash_post_draw::Build`,
  `test_leash_post_draw`). `Renderer::PreDraw` calls `CollectLeashPosts` once a frame outside the temple. For each
  heart, `worship::temple_leash::ShownPosts` (`leash_posts::Shown`, tested in `test_citadel_plan`): the heart's
  `CitadelHeart::drawPercent` (the original's +0x9C, clamped by `SetHeartDrawPercent`) exactly 1, its player's
  creature (`LeashSystemInterface::PlayersCreature`) and that creature knowing the post's leash (`Knows`). For each
  post shown: `leash_posts::Step` on its `LeashPost::spin` with `FrameSeconds(game_clock::FrameGameMs())` (0 while
  paused, as the mists); the spin is a field of `LeashPost`, whose contents no state hash part reads (`pools` hashes
  only the storage's entities). Then `Build`: the collar's matrix `leash_posts::CollarMatrix` (the post's
  `Transform`, fixed at creation, then `affine::MultiplyReversed` by `RotationYXZ(0, x turn, z turn)`), its texture
  offset (scroll, `CollarBand`) and its smoke, a `billboard::Sprite` of half width 2 at the post's point, cell
  `SpriteCell`, colour `SmokeColour(SmokeBrightness(land_light::FullLight))`. The picked post of the local player's
  temple (`creature::LocalPlayer`): nothing while the hand entity has `components::NotDrawn`; else the orange smoke,
  additive, and the collar `leash_posts::CollarOnHand` on the hand's root bone
  (`HandSystemInterface::GetHandMatrix()` × the first of `GetBoneMatrices()`) with the hand entity's scale (its
  `Transform::scale.y`, the model scale the original's +0x44 is). `DrawPass` queues each post as two Z objects when on
  screen, its smoke first (key (x² + y²) + z² at the point, the screen test a sphere of the half width), then its
  collar (key (x² + z²) + y² at its origin, `BoxInView` of the mesh). The drain draws the smoke through
  `world_triangles::SubmitRaw` (smoke.raw and smokea.raw, `materials::k_Smoke` or `k_SmokeAdditive`) and the collar
  with `DrawMesh` on a transient instance row (its matrix, no fade, `frame_anim::PackUvOffset(u, v)`, the land light
  alone for its colour).
- **The hand** (`HandSystem::PickObjectAlongRay`, HandPlacement.cpp). Every invisible draw collision is one test,
  `ecs::hand_pick::InvisibleSphereAlong` (`test_hand_pick_reject`): the teleport stones' (radius 3, unchanged) and the
  posts'. Outside the temple, `worship::temple_leash::HandPosts` gives every temple's shown posts (`ShownPosts`), each
  at its point read again this frame (`PointNow`: the special point through the heart's `Transform`, on the land, as
  at creation), but the local player's picked post while the hand entity has `NotDrawn`; each is a sphere of
  `leash_posts::k_HandCollisionRadius` 1.0, after the stones and before the creature, the nearer hit taking the cursor.
  `hand_tap::Register<LeashPost>` (HandSystem.cpp): valid to tap = `temple_leash::InterfaceValidToTap`, the post's
  owner the local player (`leash_posts::ValidToTap`); `HandSystem::ValidForPlaceInHand` refuses a post, so a press on
  one is a tap at once (the hand's TapOnly branch, as an abode's).
- **The tap and packet 0x65.** The tap (`temple_leash::InterfaceTap`, from the tap packet at the turn as every tap;
  `leash_posts::Tap`): nothing for a post that is not the local player's; the picked post: pick -1, no sound, nothing
  sent; any other: sound 42 (the in-game bank, 2D, no owner) for the local interface, the pick = the post at once, then
  `game_packets::Type::LeashType` 0x65 with the leash (i + 1) and the sending player (`Packet::player`). Its handler
  (`temple_leash::ApplyLeashType`, registered by `temple_leash::RegisterPacketHandler` in `Game::LoadMap` right after
  the hand's): refused when the player's creature is under the compassionate or angry spell
  (`Spells::IsActive` in `creature_spells`, of `SpellOf(CREATURE_RECEIVE_SPELL 8 / 9)`, `leash_posts::LeashRefused`; tested
  only with a creature); otherwise the creature's leash (`LeashSystemInterface::SetType` (`LeashSystem::SetType`), openblack's home of the
  original's `GLeashStatus` +0x1C, whether or not the creature knows the leash) and the temple's pick (`SetPick`).
  Before, it went through `ChangeType`, which also refused a leash the creature did not know; the original's
  handler has no such test (0x63DCD7..0x63DD1F: the two spells, then fn_005E6AE0). The test is the senders' own: the
  frozen exe has exactly four `SetPacket(0x65, ...)` calls (every call to 0x550C50 pushes a constant type, and only
  these four push 0x65): `LeashObj::InterfaceTap` 0x464564, whose post is felt and drawn only while the creature
  knows its leash (the draw's gate, Creature +0x1108 + 4t non-zero) and which tests nothing itself;
  `DoLeashSelectionSystem` 0x5D03B4, after +0x110C + 4 (i - 1) non-zero and the leash not the current one
  (0x5D0344..0x5D0360); and V / B in `DoAction` 0x63FE63 / 0x63FEEF, after +0x1108 + 4·type == 1. Ours keep the
  same tests (`ShownPosts` for the feel, the picker's known test, `PressKey`'s `CommandFor`). So the two differ only
  when the creature stops knowing the leash, or another creature becomes the player's, between the frame that sends
  the packet and the turn that applies it (a mind loaded over the creature, a new creature, the debug switches):
  the original then still sets it, and so does ours now. The moods are not the handler's: `SetType`
  takes them once at the change while the leash is on (`UpdateMoodsFromLeash`), the one place that sets them at a type
  change, as the original reaches them from the type set. Tested in `test_leash_posts`, `test_citadel_plan` and
  `test_leash_system`.
- **The leashes known after a load.** A creature made from its mind file knows the leashes the file keeps (a version
  27 or later file's four flags, `creature_mind_body::LeashesFromFlags`, set by `player_creature::RestoreLeashes`
  known or not over what it knew), so the posts it knows come with it to the next land; the save writes them back
  (`creature_mind_body::ToMindFile`). A file with no flags leaves the leashes as the species starts them.
- **The tooltip.** State 18 through the same rules (`HandSystem::SubmitToolTips`); state 13's own-text lookup takes the
  post's (`leash_posts::ToolTipOf`), which no post reaches there either.

### Test hooks

- The Citadel debug window's "Leash posts" node reads out a player's posts (point, spin, smoke cell), which of them
  show now and the pick, sets the pick, reads out what the hand feels of the player's posts, has Tap buttons that tap
  a post as the local interface, and a "Draw the posts" switch (`EngineConfig::drawLeashPosts`; the spin moves on
  either way).

## Pending

- **The temple leash** ([The temple's leash posts](#the-temples-leash-posts)): openblack's heart makes its posts and
  keeps the pick, the frame draws them, the hand feels and taps them, and packet 0x65 sets the leash. Still open:
  - the post's game position: the original's `Object` takes `MapCoords(point)`; openblack keeps only the point, and
    nothing in openblack reads a post's map position;
  - the land under a post: `GetHeightAt` is the exact `GetAltitude`, but whether it re-quantises through `ToFixed`
    (openblack's `GroundUnder` goes fixed → metres → fixed) is not read;
  - the creation index of the worship sites (fn_00464F50), made after, waits on the worship sites;
  - which matrix the hand's +0x80 is (the first of `Morphable::TransformedMatrices`; 🔍 inferred the root bone);
  - packet 0x65 with no creature still sets the player's leash type (`GLeashStatus` +0x1C, the interface status's):
    openblack keeps the leash type on the creature, so with no creature there is none to set. No sender sends it
    without a creature (all four test the creature first), so it is reached only when the creature goes between the
    frame that sends it and the turn. A leash the creature does not know is set, as the original (2026-10-22);
  - the mood "while a leash is on" asks any of the player's interface statuses (`GetInterfaceStatusLeashOn`
    0x4CF060); queued as a fix;
  - whether the profile's mind file holds non-zero leash flags is not read (ciphered);
  - `DrawNow` skips the posts while the heart's destruction sequence runs (+0xB8, +0xBC > 21.8 s); queued with the
    temple's destruction sequence;
  - the smoke sprite: the original culls only a quad wholly off one side or with its centre behind the near plane,
    with no far test; inside the far plane openblack draws the same, and whether a post can lie beyond it is not
    checked;
  - `SetDynamicLighting(1)` of the collar sets +4 |= 0x20 only at the "Light" detail 2-6; what the static draw does
    with that flag is not followed;
  - the hand's hide (+0xAC = 1, fn_0046C2E0) from `OperateInteractionCamera` 0x46CBDC / 0x46CBEF and the others:
    queued with the interaction camera's fix;
  - the order of the hand's invisible spheres on an exact tie (the frame's draw order of the heart, the stones and the
    creature) is not read;
  - the post's sphere depth is the projected w in the original; queued as a fix (`hand_pick::InvisibleSphereAlong`).
