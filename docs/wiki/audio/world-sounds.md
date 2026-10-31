# World sounds

## Summary

What the world sounds like: the animation clips' sounds (SmallSounds.SAS) and the villagers' banter, the villagers',
buildings' and trees' own sounds, the objects' sounds (picking up, dropping, piles, collisions, pitch and its
deviation), the water (the sea, the coast, splashes, waterfalls), and the sound tags that keep a loop on a thing.
openblack plays them through its anim-effects and sound tag system at the original's sites, and a tag ends by the
rule of its thing's kind; what is left (the distance of `DynamicsSystem::PlayAnimEffect`'s callers, a few
inferred points) is in Pending.

## The original

The addresses are the v1.42 exe's and `LHaudiodllR.dll`'s (0x1000xxxx), read with the 1.20 symbols; the patch did not
change the audio. The engine every sound goes through is on [the audio engine](engine.md).

### Clip sounds

`Audio/AnimationSounds`: `Data\SmallSounds.SAS` gives 115 clips a sound group (1 people,
18 cow, 36 pig, 39 sheep, 40 horse...) and events `ms soundId action`.

- When crossing an event, the key {voice (1 man, 2 woman, 3 child), 2, group, surface, soundId} chooses a row of the
  `LHAudioAnimArrayTable` of editor.sad (the one with the most exact columns; tie: the last one) and a random sample
  from its `LHAudioWaveNumTable` list.
- It only sounds within the sample's `maxDist` from the camera (footsteps: 20 m; the padding NULL.wav have 0 and
  never sound).
- Surface: 7 under water; otherwise the info.dat `surfaceSound` of the cell's second material at its altitude
  (1 grass, 2 gravel, 3 hard, 4 mud, 5 snow, 8 leaf litter).
- The THROWN screams only in the first 15 turns (10 in the vortex).
- Also done: those of VillagersBanter.sad (0x92-0x94; 0x92 sounds in the villager's house), stopping the
  saw (action 1) and the sound following the object.
- The tribe plays no part ✅: column 1 of the key is the constant 2 (0x51661F), the voice comes only from child,
  woman or man (0x5165E3..0x51660C), and the banter always goes to the one VillagersBanter bank, GAudio+0x3C8
  (0x516762..0x516768, 0x516786..0x51678C). Every tribe's villagers babble from the same samples.
- They go through `audio::PlayAnimationEffect`
  0x42A4B0 ([The audio engine](engine.md)) on the 16 channels, like fn_00516510: the distance and the surface are always those of the villager (also
  for the banter that sounds in its house; without a house, at the camera), the maximum distance 800 and the GAudio
  filters (the banter and the tree whispers are userParam 1: they go silent with the script's widescreen), and a
  footstep (4) with that widescreen discards the rest of the clip's events.

### Villagers', buildings' and trees' own sounds

Every site in the v1.42 exe where a villager, a building or a tree starts or stops a sound of its own, found from the
callers of the play, tag and anim-effect entry points (0x429D60, 0x429DA0, 0x429E30, 0x42A000, 0x42A040, 0x42A330,
0x42A4B0, 0x71E840, 0x71E8C0, 0x71EA40, 0x71EB60, 0x71EBE0, 0x71EC30, 0x71ED40); `LHSamplePlay` itself is called only
by 0x429E30 and `HelpDude::PlaySample` 0x5BB544. Banks: GAudio+0x3A8 + 4·type, so +0x3AC InGame, +0x3B0 editor, +0x3BC
Scriptsfx. Options fields as in "Channels, priorities and loops" (+0x08 is3D, +0x0C track, +0x20 owner, +0x24 sample,
+0x30 point, +0x48 pitch). `SoundTag::GetRandomSample` 0x71ED40 = first + `LocalRand(n)`, the **local** stream (oracle:
(0xDD, 8) on the local seed 0x88F89F gives 227 and leaves 0xE6F4C8D6; on 0x12345 221, on 0xDEADBEEF 227; (0x218, 6)
gives 536, 538, 536). The local seed is in the state hash's `random` part, so a local draw added or removed moves that
part, and not the synced trace.

| source | original | event and rule | in openblack |
|---|---|---|---|
| Birth | `Villager::HousewifeGivingBirth` 0x762430 (0x76246F) | the turn the countdown (+0x58, a word) reaches 0 and `ChildBorn` 0x762220 gives a child: `SoundTag::Create(the child's MapCoords, 20 + LocalRand(10) G_BabyCry, track 0, mode 2, loops 0, +0x40 0, 3D, InGame, 0)` 0x71EB60 | `VillagerBirth.cpp:220` (done; test `VillagerBirthTest.ABirthCriesOnceAtTheChild`) |
| Creche | `Villager::ChildAtCreche` 0x757E33 | each turn of the state, sky type ≤ 1.2 and the creche functional: `SamplePlayAnimEffect(creche, d, {0, 0, 19, 0, 82}, 0, editor, track 0, 0, 0)`, the owner the creche (edi, the `IsTouching` vt+0x6B8 caller at 0x757D5A, pushed last at 0x757E32), d = √((dz² + dy²) + dx²) from `Get3DSoundPos(creche)` (vt+0x10 at 0x757DBD; Creche's slot is `GameThingWithPos::Get3DSoundPos` 0x56FE20: x, z = MapCoords · 10/65536, y = `LH3DIsland::GetAltitude` + the MapCoords' altitude) to `LH3DTech::g_camera` (0x757DCF); row 82 is 116 NULL and child_laugh_01..09 | `VillagerChild.cpp` (done): the sound position is `CrecheSoundPoint` = `map_coords::ToWorld(MapCoordsOf(creche))`. The distance is `DynamicsSystem::PlayAnimEffect`'s, `glm::distance` in floats (x, y, z order) and not the x87 sum (dz² + dy²) + dx² **(approximate**, shared with the physics' callers; see Pending**)** |
| Work, idle, states | `Object::CheckSounds` fn_00516510 (the clips' sound events) | see "Anim-effects and SoundTag in openblack" | `Animations.cpp:144..177` → `AnimationSounds::Fire` (done) |
| Picked up | `GInterface::GenericPickup` 0x5D2800 | screams 180 / 194 / 187 + rand 7 (confirmed: xref immediates) | `HandHolding.cpp:289..301` (done) |
| Sacrifice of a villager | `Villager::ApplyThisToObject` 0x752C40 (0x752E26, 0x752E4E, 0x752EA6) | only with an interface status: options InGame 179 G_Sacrifice_01, owner the villager, 3D, track 0, loops 0, at the status's reported hand point (+0xC8). Then a child (vt+0xAF8): the **same options** with the bank switched to Scriptsfx, which has no 179 (nothing plays), and then `GetRandomSample(13, 10)` (Scriptsfx 13..22 are child_cry_normal) written to the options and never played. Else the editor bank: a woman ([info +0x1F8] = 1) 536 + LocalRand(6) H_WomanScream, a man 221 + LocalRand(8) H_ManScream | `altar_offering::Offer` (`ECS/AltarOffering.cpp`), when the hand gives it (the hand's point of the turn), right after the villager's death: `altar_sacrifice::PlaySounds` (`ECS/AltarSacrifice.cpp`) plays in this order, the cry picked after the first play and the child's cry picked after both, then dropped (test `test_altar_sacrifice`) |
| Sacrifice of the others | `Living::ApplyThisToObject` 0x5ED6C1, `Tree::ApplyThisToObject` 0x74BEA9, `DeadTree::ApplyThisToObject` 0x510FCF | the same InGame 179 play, nothing more (`Living::ApplyThisToObject` likewise only with an interface status, at its hand point +0xC8) | `altar_offering::Offer` through `altar_sacrifice::PlaySounds`, as for a villager |
| Windmill | `Windmill::CallVirtualFunctionsForCreation` 0x405950 (0x405974) | after the abode's, unless UNAVAILABLE (+0xA bit 0): `SoundTag::Create(windmill, 13 G_windmill, 0, 2, −1, 0, 1, InGame, 0)`. No Remove: the tag goes the turn its thing is no longer functional (`ProcessSoundTags`, `Abode::IsFunctional` 0x406200; Windmill has no IsFunctional of its own). The windmill is the class `Abode::Create` 0x402E20 makes for ABODE_TYPE 0xA exactly (byte table 0x4030B0, jump table 0x403098); `Windmill::Create` 0x405890 is its only caller (0x4058E2, the only reference to the slot +0x624 at 0x8AAF3C), called only by `Abode::Create` (0x402EC9), whose callers are `PlannedAbode::CreatePlannedNoFixedCheck` 0x4057AC (a building site's abode, not built: its tag goes on the first turn, unheard, and nothing makes it again once built. Read: tag 13 is made only at 0x405974 (the twelve callers of `SoundTag::Create` 0x71E840 pass 0xAB, 0xA8, 3, 0x4A and others, never 13 but here); the windmill's creation slot is reached only from `Abode::Create` and `Object::ResolveLoad` 0x63A02E; the site's finishing, `MultiMapFixed::Built` 0x52EBB0, calls no creation slot (its callees: vt +0xC, +0x8C4, +0x8C0, +0x48, +0x1C, `CreateReaction`, +0x210, +0x80, `RequestChangeTexture`)), sub_715180 0x715750 / 0x71584C (the land's feature script commands, registered by `GSetup::LoadMapFeatures` 0x718126) and `Town::CreateFootballPitch` 0x73A707. `GameThing`'s constructor clears the flags (bw1-decomp), so a windmill just made is never UNAVAILABLE; a loaded one gets its tag again from `Object::ResolveLoad` 0x63A02E, which calls the creation slot first (fix row, with the save loading) | `AbodeArchetype.cpp`, right after the chimney smoke (as `Abode::CallVirtualFunctionsForCreation` makes it), through `AbodeSounds::StartWindmillHum`; every path that makes an abode goes through it (the land script, building sites, the editor). Its end through `GameQueries::thingFunctional` |
| Building ambience | `Abode::Draw` 0x515F70 (0x516137..0x516283) | **every frame the abode is drawn**: n = ftol(80000.0f [0x8D8704] / g_game+0x205D48, the frame's game ms); `LocalRand(n) == 1`, drawn first; d = √((dz² + dy²) + dx²) from `Get3DSoundPos` (vt+0x10) to `LH3DTech::g_camera`; d < 200.0f [0x8C7B34]; s = `Time2SkyType(GetVisualTime())`: s < 0.7f [0x8AB238] → action 71; s < 1.3f [0x8C7A18] → visual time < 12.0f [0x8CF1C0] ? 73 : 72; else 74; `SamplePlayAnimEffect(abode, d, {0, 0, 19, 0, action}, 0, editor, track 1, 0, 0)` 0x516283. editor.sad rows: 71 = 42 NULL and 16 G_DogBark, 72 = 50 NULL and the same barks, 73 = 5 NULL, G_Rooster_01..04 and G_pigeon_01 twice, 74 = an empty list (nothing plays). The bank's editor text has the labels ABODE_DAY / DUSK / NIGHT / DAWN in that order; by when they play and what they hold, 71 is day, 72 dusk, 73 dawn and 74 night. `Abode::Draw` is the slot +0x610 of Abode, Creche, Wonder and Graveyard, and is called on every path by `Windmill::Draw` 0x516326, `TownCentre::Draw` 0x516457, `StoragePit::Draw` 0x519350 (a tail jump) and `Workshop::Draw` 0x51CC6E. Totem 0x51ABA0, Football 0x531610, SpellDispenser 0x722940 and Field 0x528570 draw their own way without it; the citadel parts draw through `MultiMapFixed::Draw` and `CitadelHeart::Draw` | wired (raffclar's lane, unverified against v1.42): `AbodeSoundSystem::Draw` (`Locator::abodeSoundSystem`), from the object draw list's consumer after each object's draw, on screen or not; the sound at `ToWorld(MapCoordsOf)`, played by `audio::PlayAnimationEffect` with its own distance and track on. Pure rule `AbodeSounds` (the chance, the reach, the distance's order, the key, `Ambience`) |
| Knock | `Abode::InterfaceTap` 0x40694A | 110 + counter, at the hand | `AbodeKnockSystem.cpp:67` (done) |
| Hit, collapse | `Abode::ReactToPhysicsImpact` 0x406610, `ApplyEffectsDueToPhysicalDestruction` 0x40671D | see "The world's callers on the channels" | `BuildingDamageSystem` (done) |
| Workshop | fn_007798A0 0x7798FC and fn_00779910 0x779A1B / 0x779A39, from `Workshop::Process` | a scaffold started: tag (workshop, 74 G_Workshop, 0, 2, −1, 0, 1, InGame, 0); finished: options 150 G_ScaffoldReady_01, owner the workshop, 3D, track 0, then `Remove(workshop, 74, InGame)` | `Workshops.cpp:168`, `:193..200`, `:208` (done) |
| Worship totem | `TotemStatue::SetWorshipPercentage` 0x738449 / 0x73845B, `Process` 0x738099 / 0x73810B | the loop of 30 G_VillageBell released, tag 11 G_TotumMove (0, 2, 0, 0, 1, InGame, 0); arrived (the gap +0x9C − +0xA0 below [0x999A90] with +0xCC set): Remove(11, stop), then 30 at the totem, 3D, track 0 | `WorshipPercentage.cpp:171..172`, `:534..542` (done) |
| Scaffold planted, combined, tapped | `Scaffold::BuildBuilding` 0x6E9470, `ApplyThisToObject` 0x6E9AC9, `InterfaceTap` 0x6E9E7E | 174, 201 + counter (owner the held scaffold, already deleted by the join; 3D, track 0, at the hand: it plays to its end there, see [Combining and splitting](../world/building-sites.md#combining-and-splitting)), 151 + counter | `Scaffolds.cpp:833..841`, `:1240`, `:1350` (done) |
| Scaffold phantom | `Scaffold::DrawInHand` 0x5181A0 (0x518292, 0x5182E3) | every frame, only when the plan is valid (`IsPlannedValid(0)`) and the status is the local interface's: f = +0x78 − min(g_game+0x25053C % 100, +0x78), the min **unsigned** (`jb` 0x5181FA). If f > 0: fading in (+0x88 bit 0) with the static `appearSoundPlayed` [0xCC631C] clear → `disappearSoundPlayed` [0xCC6320] cleared, options InGame 51 G_ScaffoldAppear_01, 2D, track 0, no owner, pitch +0x48 = abode number (`GAbodeInfo` vt+0x44) < 0 ? 100 : 110 − 3·number, then the appear mark set; fading out with the disappear mark clear → the appear mark cleared, `PlaySoundEffect(0, 50 G_ScaffoldDisappear_01, mode 2, 0, 0, 2D, InGame)` 0x429D60, the disappear mark set. The marks are function statics: one pair for every scaffold | `scaffolds::PlayHeldPhantomSound` (`ECS/Scaffolds.cpp`), each drawn frame from `Game.cpp` for the status's held object, through the pure `scaffolds::PhantomSoundOf` (test `test_scaffold_phantom_sounds`), with the marks in `Locator::audioState`. Checked in the disassembly 0x5181A0..0x518300: `IsPlannedValid(0)` 0x6EA950 is `HasPlanOf(c, false)`, +0x78 the fade timer, +0x88 bit 0 `k_FadePhase`, the pitch +0x48 set and the caller's mask +0x1C left at its default. The phantom's draw is `graphics::scaffold_ghosts` ([the phantom in the hand](../world/building-sites.md#the-phantom-in-the-hand)) |
| Tree: wind | `Tree::Draw` 0x74B009 / 0x74B25C | the bend and the rustle, see trees.md | `Trees.cpp:1257`, `:1343` (done) |
| Tree: tug, uproot | `Tree::InterfaceSetInMagicHand` 0x74B758 | tag 32 + rand 3 (confirmed: xref immediates; oracle: GetRandomSample(32, 3) at local seed 0xE6F4C8D6 gives 33) | `HandHolding.cpp:237` (done) |
| Tree: falling | `PhysicsObject::GameTurnUpdate` 0x6460F0 | a felled tree that toppled, taller than 10: tag 31 + LocalRand(1) at its MapCoords | `DynamicsSystem.cpp:1260` (done) |
| Tree: dropped, watered | `Tree::DropSfx` 0x74BD03, `ApplyWaterSpell` 0x74C500 | see "The world's callers on the channels" | `HandPhysics.cpp:111`, `Trees.cpp:321` (done; the watered tree's growth is owned by the tree, test `ForestSystemTest.waterSpellSoundIsOwnedByTheTree`) |

Other areas' sites found on the way: the citadel heart fn_00465C70 (tag 171 G_CitadelPlasma looping at 0x465F46;
Remove 171, then tag 168 G_CitadelExplode_01, at 0x466316 / 0x466331; openblack's heart, the temple itself, has its hurt beam and spark sounds ([the temple interior](../interface/temple-interior.md), test `test_temple_heart`) but does not make these two tags yet), the creed
(`Creed::CallVirtualFunctionsForCreation` 0x50B3C1 and `ToBeDeleted` 0x50B3FA, 166, the creature's), the vortex
(`LandscapeVortex` 0x5FEE5A, the volcano's G_Volcano_02: [Creation](../world/vortex.md#creation)), fn_006E63C0 0x6E6416 (174 in 2D, the reward code) and the falling-spell film's
sub_526E00 (from `GGame` fn_00553A60).

The windmill's hum, the sacrifice's sounds and the buildings' ambience are wired (`AbodeArchetype.cpp`,
`ECS/AltarOffering.cpp`, `AbodeSoundSystem` through the draw list), and so is the held scaffold's phantom
(`scaffolds::PlayHeldPhantomSound` from `Game.cpp`).

- 0x71E69A: GameThing::IsFunctional (vtable +0xD4) false -> ToBeDeleted; 0x71E6B8: Get3DSoundPos != 1 -> the same (in the code: `Process`)

**The crowd samples are never played** ✅ (read 2026-10-21). InGame.sad has H_ImpressedGroup_01..03 (14..16) and
H_LoseBeliefGroup_01..03 (17..19), but nothing in the v1.42 exe or its scripts plays them:
- no call of the play, tag and anim-effect entry points above (0x429D60, 0x429DA0, 0x429E30, 0x42A000, 0x42A040,
  0x42A100, 0x42A310, 0x42A330, 0x42A4B0, 0x71E840, 0x71E8C0, 0x71EA40, 0x71EB60, 0x71EBE0, 0x71EC30, 0x71ED40) has
  14..19 as its sample in the 30 instructions before it. The only such numbers near one are something else: the
  spell bank's samples of `WorshipSpellIcon::PlayFullyChargedSoundFX` (0x77F530..), a column of the spirits' anim-effect
  key in `HelpDude::PlaySoundFX` (0x5C2994) and the force feedback effect 0xE of `OneOffSpellSeed::InterfaceTap`
  (0x72A673; its sample is 0x6D);
- the villagers' impressed states `Villager::InitialiseImpressedReaction` 0x766D00 and `PerformImpressedReaction`
  0x766D60 only look at the impressive thing (`LookAtObject`) and count turns; neither plays a sound, and the clips'
  sounds go to editor.sad and VillagersBanter.sad, not InGame.sad (`Object::CheckSounds` 0x5166E0, 0x516768);
- no script of `Scripts\Quests\challenge.chl` (decompiled whole with lhvmtool) starts 14..19 in the in-game bank:
  its in-game sounds are 40, 41, 166..170, 174 and 178.
So a crowd that is impressed or loses belief makes no sound of its own; what is heard is the spirits' remarks
(GGuidance's belief and losing-belief texts, [voices](voices.md)).

### Sounds

- LHAudio's pitch is a **percentage** of the wav's frequency (100 = normal). On start (0x1001278B, unsigned
  integers): d = deviation·p/100, p = p − d + rand·2d/32767 (0 → 100), frequency = rate·p/100 (integer division; the same
  in `LHSampleSetPitch` 0x10013520, which does nothing if the channel already has that p). The pitch, the volume, the loops
  (+0x248) and the .sad mode only count if their bit is in the flags at +0x244 (0x1, 0x20, 0x40, 0x400) and the
  caller has not set them (mask +0x1C of the options); otherwise, 100, 127, 0 and 3. openblack passed the raw number.
  In the sample header (read at 0x10011420) the pitch is at +0x260 (flag 0x1) and the volume at +0x25C (flag 0x20).
- **Volume** (verified with Unicorn): LHaudio sends to `QSWaveMixSetVolume`
  floor(master·v/127)·258 (0x100133C1; master = BWSetup's `AudioSampleMasterVolume` = 127 → v·258, 0..32766) and
  QMixer stores it as vol/32767 (0x18007AE5) and **multiplies** it by the distance gain (0x1800AE20):
  linear gain v·258/32767 (`sample_play::QMixerGain`, `Sound::volume`). The .sad's "user param"
  (`LHSampleGetUserParam` 0x10014230) is the high half of that same u32 (+0x25C >> 16).
- Picking up from a pile, field or fish farm: **a single looping channel** (G_PICKUPWOOD 98 for wood; G_PICKUPFOOD 44 for the rest) whose pitch rises to ftol(60 + 180·t²) (confirmed: oracle) % per turn; it stops on release or when it runs out. Putting down on a pile: G_PileFood/Wood(Small) depending on the amount (< 200 small ones).
- Nothing sets AL_PITCH = 1 every frame, so the .sad's pitch stays: the pitch is set when the source is created and
  with `device::SetSourcePitch`.
- **Distances** (`QSWaveMixSetDistanceMapping {min, max, scale}`, LHaudiodllR 0x10012159): .sad +0x268 / +0x26C / +0x270 if the flags 0x80 / 0x100 / 0x200 are set; otherwise, 1 / 9999 / 0.3 (`LH_SamplePlayOptions` 0x10010E90). QMixer (0x1800ACDF, 0x1802CE50; channel flags 0x103/0x111 from 0x10012065: neither 0x800 "cap at max" nor 0x1000 "linear"; verified with Unicorn): gain 1 up to min (or with scale 0), `min / (min + scale·(d − min))` up to max (min/d with scale 1), **0 beyond max** (the channel keeps playing, muted). The scale is per channel because LHaudio does not use the hardware mixer (option `UseHardware` of `HKCU\Software\Lionhead Studios Ltd\Audio\Override`, which does not exist; with it it would call `QSWaveMixSetListenerRolloff(4)`).
- `GAudio::PlaySoundEffect` 0x429E30 with a position: **does not start** if the camera is farther than the .sad's max
  (raw +0x26C, `LHSampleGetMaxDistance` 0x10014170; the mapping's max if it is 0) → `audio::PlaySoundEffect` (`Audio.h`).
  In addition (0x429F36..0x429FD9, and the same in `SamplePlayAnimEffect` 0x42A4B0): with the widescreen (panoramic) view set by a script
  (+0x45E8 and +0x45EC) no sample with user param 1 plays (InGame has 60, editor 82, e.g. `G_WaterFlow`);
  inside the citadel (`g_game+0x205A28 == 1`) only those with user param 2; after `SET_GAME_SOUND false` (GScript+0x90,
  0x7100B0, which also does `LHSampleStopAll`) only the dialogue banks HelpSprites and Villagers. The collisions of
  physics go through `SamplePlayAnimEffect` → `LHSamplePlayAnimEffect` 0x100146F0: they do not start beyond the .sad's max nor beyond
  800 (0x426E6B) and the channel's owner is the object; what the physics passes is in [Collision sounds](../physics/physics.md#collision-sounds) and [Sounds, dust and the look of impacts](../physics/physics-impacts.md#sounds-dust-and-the-look-of-impacts). All in `Audio/SamplePlay` (`PlaySoundEffect`, `PlayAnimEffect`).
- **Channels and modes** (`LHSamplePlay` 0x100113B0: allocation 0x10011020, start 0x10011420; `Audio/SamplePlay`): 16
  channels (+0xCC, constructor 0x1001535E). Each channel remembers bank, owner (+0x20 of the options: 0 none, −1 the
  ambience's, or the object), sample, clone group (.sad +0x118) and priority (.sad +0x240). Mode 1: a free channel.
  Mode 2: **nothing** (not even a new position) if a channel of the same bank and owner plays the same sample or one of the same group
  (> 0); otherwise, a free one. Mode 3: the channel of the same bank, owner and sample (it restarts), otherwise that of the same group
  (> 0), otherwise a free one. With no free one: the one with the lowest priority if it is lower than the sample's (it restarts); otherwise, it does not play.
  `G_BigSplash` (mode 2) does not play again while the same sample of the same object is playing; the ten `G_HandInWater`
  (group 4, mode 3, no owner) restart one another; the ambience one-shots (mode 3, owner −1) restart their
  channel if they come up again while playing. Only the samples that go through `sample_play` count towards the 16: the
  physics hits, the hand's water and dust, the pick-up loop, the ambience, the `SoundTags` (waterfall) and those
  of the boat. Still on their own: `AnimationSounds`, the rocks, the camera whistle,
  `G_RockPast` and the hand's pick-up/plant/break `PlaySample` calls.
- **Tracking and listener, once per turn** (`fn_004270D0` from `ProcessAudioGameTurn`): `LHSampleUpdate3DChannels`
  0x10014310 moves each 3D channel with +0x0C (1 by default) and without AtmosInfo to its owner's position
  (`Get3DSoundPos`, game function 0x427200): without an owner, **the camera**; owner −1 or no longer present, it stops; beyond
  its max (+0x6C) from the camera, it stops. The collisions set +0x0C = (code A of the table ≠ 0x16, 0x64689F); the hand in
  the water and the pick-up one, 0. Then `LHListenerUpdate`: the QMixer listener = the camera (position, forward and up,
  velocity 0) **only once per turn** (and not while paused nor in the first 5 turns); openblack the same
  (`sample_play` calls `device::SetListener` once per turn).
- The .sad files are RIFF wavs (`QSWaveMixOpenWaveEx`); openblack used to try MPEG first and dr_mp3 found frames inside
  some of them (`G_BigSplash_03` lasted a fraction of a second): now a RIFF goes straight to the wav reader.
- **Listener axes**: openblack is left-handed and OpenAL right-handed; positions and velocities already went with x ↔ z, the
  orientation (at, up) did not, and left and right came out reversed. Now it goes through the same swap.
- **Hand in the water / grabbing land** (`StartLandscapeGrip` fn_005D1AB0): a single branch chosen by the **water bit
  of the cell** (`InBounds && IsLand` 0x5D1F94; outside the map = water), not by the height. Water: ring,
  `G_HandInWater_01..10` = InGame 99 + counter (0xD18228, 0..9, advances even if discarded; confirmed: xref immediates), **3D at (x; 0.2; z)** (0.2: confirmed: xref immediates),
  vol 127, pitch 100 ±5 %, min 40 / max 150 / scale 4, and the scare of the fish. Land: dust and
  `G_HandGrabLand_01..06` = InGame 4 + LocalRand(6) (0x5D1FC4, confirmed: xref immediates; a `SoundTag::Create` 0x71EB60 tag without an object, 2D: is3D 0, vol 10, pitch
  60 ±15 %). The ten water ones are in **clone group 4** and are played in mode 3 without an object: `LHSamplePlay`
  (0x10011146..0x100111BC) restarts the previous one's channel → **one at a time**, the rotation of the 10 samples
  (`sample_play`, HandFish.cpp; openblack's `SplashWater` sets the point the fish shoals flee from).
  Trace: `OPENBLACK_AUDIO_TRACE=1`.
  - Conditions (resolved): the land branch (0x5D1FA8) makes no dust nor sound if `g_game+0x25005C` (the **HelpSystem**) has the **widescreen** set (+0x45E8) **by a script** (+0x45EC = the script's task number, `GScript::SetWideScreen` 0x6F7BF0 → `HelpSystem::SetWideScreen` 0x5C6AD0; the videos and the playback pass 0); the water one (0x5D1FF0) does nothing with the **game paused** (`g_game+0x14` bit 4, toggled by `PauseGame` 0x54AE20) nor if the hand's 3D object (`CHand+0x482C`) draws something held (+0x8C, set by its `SetHeldG3D` vt+0x234 = 0x816830; empty also with a `SpellSeed` that is not drawn in the hand).
- **Ambience (atmos)** (`Audio/SoundMap`, `Audio/Services/AtmosBanks`):
  - Cell zone: `Terrain::GetAtmosType` 0x7352B0 = `(flags >> 2) & 0xF` (bits 2..5 of byte 7; **1 = SEA** outside
    the map or without a block). Table 0x9CB048 of 14 types {name, bank, daytime}: 1 SEA `ocean.sad`, 2 STILL_FRESH_WATER
    `lake.sad`, 3 COASTAL `shore.sad`, 4 JUNGLE*, 5 ARCTIC, 6 DESERT*, 7 COUNTRYSIDE*, 8 SWAMP*, 9 RUNNING_WATER
    `stream.sad` (no base map uses it), 10 STRATOSPHERE `high.sad`, 11 NIGHT* `night.sad`, 12 RAIN, 13 WIND
    (* = daytime). The codes of Daniels118's editor are `tipo << 1` (bit 1 is the water cell that is not drawn).
    Byte 7 is openblack's `LNDCell::flags`; bit 1 is set in about 12-20 % of the cells of the base maps and bits 6..7
    are always 0. No base map has RUNNING_WATER or DESERT cells: `GStream::CreateRiver` 0x7341E0 writes only footprints,
    never the ambience bits, so `stream.sad` never plays in the base game.
  - The distances and heights are info.dat's `GSoundInfo` (`InfoConstants::sound`): `load_variables` 0x42CF93 reads
    DETAIL_SOUND_INFO, 0x3C bytes, into 0xD9A908 (file offset 0x4A2B4). weatherPercentageForMaxFade 0xD9A908 = 40,
    weatherPercentageForMaxWeatherVolume 0xD9A90C = 70, radiusForMaxAtmosVolume 0xD9A910 = 20,
    radiusForMinAtmosVolume 0xD9A914 = 50, fadeOutStart 0xD9A918 = 20, normalAtmosFadeStartHeight 0xD9A91C = 120,
    normalAtmosFadeEndHeight 0xD9A920 = 250, atmosphereHeight 0xD9A924 = 200, atmosphereMaxVolHeight 0xD9A928 = 1500,
    spaceHeight 0xD9A92C = 44444, atmosVolumeStep 0xD9A930 = 0.01, townTriggerDistance / OffDistance 300 / 400,
    noBlocksForFullAtmos 0xD9A93C = 40 and distanceFromNearestAtmosForFadeStart 0xD9A940 = 20. The ambience does not
    use fadeOutStart, atmosVolumeStep (its steps are fixed, below), noBlocksForFullAtmos nor the last one.
  - `GSoundMap::Update` 0x71D6F0, every turn from `GGame::EndTurn` (before `ProcessSoundTags`): receiver = camera;
    11 × 11 cells around (radius 50); per type, number of cells and the nearest corner. Volume = radial (1 up to
    20, 0 at 50) × fade by the camera's height above the ground at that corner (1 below 120, 0 at 250); the daytime ones
    also (radial × weatherFade) × (1 − night), in that order (0x71DFC8..0x71DFDA; `sound_map::DayOnlyShare`, test `SoundMap.ADayOnlyAmbienceTakesTheWeatherThenTheNight`; until 2026-10-22 ours took weatherFade × (1 − night) first, one bit apart now and then), night = max(0, sky type − 1). The coast is computed without that and the **sea =
    min(radial, 1 − coast)**: within 20 of a COASTAL cell the sea goes quiet (the point `1464, 2016` of Land1 gives SEA 0,
    COASTAL 1; `1300, 2016` gives SEA 1). STRATOSPHERE with the absolute height: 0 below 200, rises up to 1500, falls up to
    44444 (fn_0071DB80 0x71DB80; its two other outputs, the "normal" ones, are only printed by the Dump). NIGHT = fade(camera) × (1 − fraction of sea cells) × weatherFade × night. RAIN = rain/70, WIND =
    (|wind| − 15)/30. `CameraWeather()` reads the weather with `ecs::weather::atmos::GetWeatherSmooth(cámara, true)`
    (`LH3DAtmos::GetWeatherSmooth` 0x835180; byte 3 is the cloudiness).
  - `InitAtmos` 0x428EF0 (from `GGame::FinishInitialisation`) registers the 14 banks of the table with
    `LHBankRegister(path, 0)` (fn_00428F30; type 0 stays empty) and zeroes the targets and current volumes;
    `ReleaseAtmosSoundBanks` 0x428F90 lets them go in `UnfinishInitialisation` and `ToBeDeleted`. Each bank's line of
    the Dump is `"%s Vol=%3.3f Sent=%d Step=%d"` (0x9CB380). When paused or in the first 5 turns `GGame::EndTurn` calls
    `GAudio::AtmosProcess(0)` instead (below, in openblack).
    Otherwise `ProcessAudioGameTurn` 0x427080 runs the music, fn_00429100 (the targets), `ProcessAtmosBanks`,
    fn_004270D0 (the listener), `LHAtmosProcess(1)` when no video plays, and fn_00429700.
  - `GAudio`: targets = the map's volumes, **all 0 inside the citadel**: `GAudio::IsInsideCitadel` 0x4282F0 (once misnamed `HelpSystem::GetWideScreenControl`) is `g_game+0x205A28 == 1` (`GoInsideCitadel` 0x554004, 2 during the falling spell video); the widescreen does not touch the ambience (without a citadel interior: never). The `fn_00429100` copies 15 floats, so the 15th (the camera's x) falls into `current[0]` (NONE, without a bank: only visible in the trace). `ProcessAtmosBanks` 0x428FE0: step 0.04 between 0.1 and 0.8 and 0.02 outside (confirmed: xref immediates; the bounds 0.1 < cur ≤ 0.8 confirmed: xref constants) (0 → 1 in 33 turns, 3.3 s), group 1 if GAudio+0x190 > −0.6 (double at 0x8C4A08) otherwise 2, `LHAtmosSetBankVolume(trunc(cur·127))` (confirmed: xref constants). GAudio+0x190 is set by `fn_005E2240(a)` = 2a − 1 with a = clamp(a, 0, 1), from `fn_0064AC30` in `GPlayer::ProcessPlayers` every turn: a = (alignment of the **player with the most influence at the camera's position**, `MapCoords::CalculateMostInfluentialPlayer` + `GPlayer::GetAlignmentValue`, + 1) / 2; `GAudio::Reset` (on clearing the map) leaves it at 0.
  - The DLL's mixer (`fn_10001610`, `LHAtmosProcess` 0x100018B0): samples with +0x27C = 0 are 2D **loops** (vol of the
    .sad or 127, fade-in +5 per turn without a cap until reaching its volume, gain bank·fade/127; they are cut
    abruptly when the bank reaches 0); +0x27C = f > 0 are **one-shots** in **a single queue** for all the banks,
    next = counter + 4f + rand·12f/32767, and when registering a bank counter = that bank's earliest − 20, the queued ones due before it drawn again ([banks-and-channels.md](banks-and-channels.md#the-lhatmos-mixer)). **At most one per
    turn** (the head, if due): it plays if its bank is not at 0, relative to the listener in QMixer (x, y, 0) with x, y =
    2 − rand·4/32767; if |x| + |y| ≤ 1 they are multiplied by 4 and if they end up at (0, 0) they go to 5·(a, b), with a, b = ±1 that
    rotate (a' = −a, b' = −a·b). It is put back in the queue even if it has not played. The one-shots of a group other than
    their bank's go down 5 per turn. In Land1 there are 15 loops and 400 one-shots.
  - The water banks are group 0, so they play with any alignment: `ocean.sad` a stereo loop (vol 20) and the 14 waves
    twice (every 20 turns at vol 40, every 40 at vol 20 with ±20 % pitch), `shore.sad` a loop (30) and 18 waves,
    `stream.sad` a loop (50). `lake.sad` has no loop: only the ocean's waves at **160 %** pitch, every 60 turns.
  - **Axes of the relative mode** (verified with Unicorn): LHaudio converts the relative position (+0x14) to polar (0x10012269): azimuth = atan2(x, y) in degrees (with π taken as 1/0.318471 and |ftol| of the negatives), elevation = atan(z/|xy|), range |xyz|; QMixer (0x1800AA85) goes back to right = r·cos(el)·sin(az), up = r·sin(el), forward = r·cos(el)·cos(az). That is, **x right, y forward, z up**: the one-shots (x, y, 0) lie in the horizontal plane around the listener, at 2..7.1 (e.g. (2, 2) ahead to the right, (0, −4) behind), attenuated with their mapping (min 1, scale 2 or 4: at (2, 2) with scale 2, 0.215).
- All the world's sounds (picking up, uprooting, planting, crushing, rocks, whistles, piles) go through the 16
  channels as in the original, 2D or 3D according to their place
  ([The world's callers on the channels](script-audio.md#the-worlds-callers-on-the-channels)).

### Water audio

What sounds with the water and when. The engine (channels, modes, distances, banks) is in [The audio engine](engine.md); the
ambience and the hand in the water are, for now, in
[Objects](../world/objects.md) ("Hand in the water / gripping
the land" and "Ambience (atmos)"). All ✅ faithful.

- **Hand in the water**: when starting to grip the terrain over a water cell (`StartLandscapeGrip` fn_005D1AB0), a
  `G_HandInWater_01..10` (InGame 99 + counter) in 3D at (x; 0.2; z), one at a time (clone group 4, mode 3), with the
  splash ring and the fish scare; it does not sound when paused or with something in the hand.
- **Impacts against the water**: with altitude < 3 in the rounded cell the collision type is WATER and the
  `editor.sad` sample from the collision table plays ([Sounds, dust and the look of impacts](../physics/physics-impacts.md#sounds-dust-and-the-look-of-impacts)); `G_BigSplash`
  (mode 2) does not repeat while the same sample of the same object is playing.
- **Drowning**: the events of clip 252 `P_DROWNING` (`M_P_Drowning`: splash 157 and voice 134,
  [above](../world/water.md#sinking-drowning-and-being-deleted)).
- **The missionaries' boat**: three 2D sounds from `Scriptsfx.sad` at the launch (62, 61, 60;
  [above](../world/sea-life-and-scenery.md#the-missionaries-boat-petitnavire)).
- **Land 3 waterfall and Land 4 ark**: `G_WaterFlow` (InGame 12) looping through a `SoundTag`
  ([above](../world/sea-life-and-scenery.md#fixed-scenery-per-land-land-3-waterfall-land-4-ark-and-dinosaur)). The
  script's `CREATE_WATERFALL` makes a `GWaterfall` that does nothing; openblack's
  `FeatureScriptCommands::CreateWaterfall` makes an empty object too, with no sound.
- **Particles that fall into the water**: the bounce and the ripple of `UpdateRuleGravityWithFloor`
  ([The PSys in the world: format, step, drawing and water rules](../physics/particles.md#the-psys-in-the-world-format-step-drawing-and-water-rules)).
- **Ambience**: by the ATMOS type of the cells near the camera: 1 SEA `ocean.sad`, 3 COASTAL `shore.sad`, 2
  STILL_FRESH_WATER `lake.sad` (and 9 RUNNING_WATER `stream.sad`, which no base map uses); the sea goes quiet within 20
  of a COASTAL cell. No music is tied to the sea: `G_WaterCreatureCave_01` (InGame 177) belongs to the citadel's
  creature room (`CreatureRoom::DrawAdditional`). Rivers have no sound of their own (`ATMOS_TYPE_RUNNING_WATER` not analysed, see
  [Rivers](../world/sea-coast-and-rivers.md#rivers)).

The sound tag engine used by the waterfall and the ark (it is not in [The audio engine](engine.md)):

- **SoundTags** (`Audio/SoundTags`, SoundTag.cpp 0x71E300..0x71ED90): tag {thing or fixed point, offset,
  sample, loop, active}. `ProcessTurn` = `SoundTag::ProcessSoundTags` 0x71E5F0 once per turn (GGame::EndTurn):
  if the thing no longer exists → ToBeDeleted; if it is active, `fn_0071E680` calls `GAudio::PlaySoundEffect` 0x42A100 with
  **the tag itself as owner** of the channel (+0x20), the point (`Get3DSoundPos` of the thing), the offset +0x1C,
  sample +0x28, follow +0x30 (only with a thing; `false` for the scenery ones), mode +0x38, loops +0x3C, is3D +0x44 and its
  bank (`SoundTag::Set` 0x71E4F0); 0x429E30 only starts it with the camera ≤ maxDist (.sad +0x26C) from the point and
  mode 2 leaves the channel that is already playing. In openblack it goes through `sample_play` (owner `Owner::Tag`), so it counts towards the 16
  LHaudio channels; `IsPlaying` = `LHSampleIsPlaying` (fn_0042A2D0) and `ReleaseLoop` = 0x42A310. `SetActive(0)` cuts
  abruptly (LHSampleStop); `Delete` releases the loop (LHSampleReleaseLoop) and the tag dies when the pass finishes.
  `Clear` from `audio::ClearMap` in `Game::PrepareNewLand`, before the registry reset. It is the
  complete SoundTag (`audio::tags`, [The audio engine](engine.md)): `Delete` = ToBeDeleted 0x71ECB0 →
  CreateSoundTagForDeadObject 0x71ECD0 (releases the loop only if it is playing with loops), and the street lanterns (`LanternSounds`)
  are tags of this module.

- 0x71ECD3 (in the code: `TagForDeadObject`)

## openblack

### Counterparts in the code

What openblack does at each step of the original described above, by section.

**Villagers', buildings' and trees' own sounds**

- the thing's position is gone, inferred to be both

**Sounds**

- `HandSystem::UpdatePickupSound` with `audio::PlaySoundEffect` (.sad mode 2, owner 0, `audio::SetPitch`, `audio::StopSoundEffect`); traced with `OPENBLACK_HAND_TEST_FISH=1`: a single start and pitch 0.60 → 1.02 over the hook's 3 s (the original's `LHSampleSetPitch(InGame, 0, sample, ftol(60 + 180 t²))` takes the rate from 60 % to 240 %). The original makes it 3D (+0x0C 0, it does not follow anyone) at the point +0xC8 of the interface state, which is **the hand**: `GInterface::Process` → `fn_005D2250` sends in packet 0x15 the hand's position (`CHand`+0x78, `Morphable::position`, 0x5D2350); `GPacket` 0x63CA9E → 0x5DBFB0 stores it at +0xA4 (and the camera at +0xB0/+0xBC); `GInterfaceStatus::Process` 0x5DC4E7 → `fn_005DBC60` computes the hand's velocity with +0xA4 − +0xC8 and copies +0xA4 to +0xC8 (0x5DBF1F) **before** `ProcessInInteract` (0x5DC574), which reaches `UpdateMultiPickup`. So it sounds where the hand is at the start (mode 2 does not move it afterwards) and does not start with the camera more than 180 from the hand. openblack: the position of the hand's `Transform`.
- The hand's water, dust and pick-up code, by function (the behaviour is on [the hand's states](../hand/states.md) and
  [Water](../world/water.md)):
  - `HandSystem::SplashHand` (`HandFish.cpp`): the ring and `G_HandInWater_01..10` (above); the ring's angle is drawn
    even when the ring pool is full. On land the grip plays `G_HandGrabLand` instead.
  - `OPENBLACK_PRINT_ALTITUDE` ("x,z", `HandDebugHooks.cpp`) logs the land's height at a point, to check the cells of
    an ambience zone.
  - `HandSystem::TryPickUpFish`: a fish farm's pot, as `FishFarm::NetworkFriendlyStartLockedSelect` 0x52D770 (the farm's
    stock is not taken). `HandSystem::TryPickUpField` and `HandSystem::ProcessInInteractField`: a field's first scoop
    (0x529900) and the scoop per turn (0x529730), which start and keep `G_PickUpFood`.
  - `HandSystem::UpdatePickupParticles`: the pick-up atoms (a fish's random scale, first frame and direction, and the
    tumble turn of `AppearanceRuleTumble`); `ParticleSpriteUv` picks a looped sprite's cell.
- `GetAtmosType` (`src/Audio/SoundMap.h`): a cell's ambience zone as `Terrain::GetAtmosType`, 1 off the map or
  without a block.
- Rivers: type 9 RUNNING_WATER plays `stream.sad` from the cells' own ambience bits like every other zone
  (`k_AtmosTypes` and the sound map's loop up to `AtmosType::RunningWater`, `src/Audio/SoundMap.cpp`); as in the
  original, a river writes no ambience bits and no base map has such a cell, so it never plays in the base game.
- `AL_INVERSE_DISTANCE_CLAMPED` with reference = min and `AL_ROLLOFF_FACTOR` = scale (`device::SetSourceDistance`, `src/Audio/Device/Device.cpp`; a source starts with `PlaySource`), and the channel beyond max is muted in `AlSampleOutput` (the 16 channels of `audio::sample_play`; there are no `AudioEmitter` emitters any more). `Sound` also stores `cloneGroup` (+0x118), `playMode` (+0x274 with 0x400, otherwise 3), `atmosGroup` (+0x11A) and `atmosFrequency` (i32 at +0x27C, −1 = not an atmosphere one; checked with `offsetof` against the .sad's `AudioBankSampleHeader` in
  `components/pack/include/PackFile.h`, whose loop kind is the `AudioBankLoop` enum). `Sound::playType` is openblack's
  own repeat kind (`PlayType`), not the .sad mode, which is `playMode`.
- `CinematicDirectorSystemInterface::IsWideScreenOn` (only `SET_WIDESCREEN`), `Game::IsPaused` and `!_held` (`HandPlacement.cpp`).
- `atmos_banks::Alignment()` reads `GameQueries::cameraAlignment`, which `ecs::audio_queries` takes from `ecs::effects::alignment::GetInterfaceAlignment()` (fn_0064AC30, once per turn) with the formula of fn_005E2240; the same x as the target of the sky alignment, so the sky test hook and the debug slider also reach it (see [The audio engine](engine.md)). Only from turn 6 and not paused (`g_game+0x14 & 4`); otherwise, `AtmosProcess(0)`: the loops stop and **only the channels with AtmosInfo** (0x10001EBF: loops = 1, one-shots = their entry), not the other samples. With a video (`g_game+0x250188`) `LHAtmosProcess(1)` does not run: openblack's films (our Bink player) set
`GameQueries::videoPlaying` (`video::IsPlaying`, `src/ECS/AudioQueries.cpp`) and `ProcessAudioGameTurn` skips the ambience mix
while one plays. On changing map (`GAudio::Reset`) ambience and samples stop, but the bank volumes are kept.
- `sample_play::PolarRelative` and the relative emitter in OpenAL's listener space (right, up, −forward). Traces: `OPENBLACK_AUDIO_TRACE=1` (channels) and `OPENBLACK_ATMOS_TRACE=<n>`. Checked in Land1 (camera at 2120, 40, 2400): the lake waves (lake.sad, pitch 160 of the .sad) play at AL_PITCH 1.600 at (−1, 1) and (1, 2).

### Anim-effects and SoundTag in openblack

#### The anim-effects in the core

| piece | original | where |
|---|---|---|
| Tables of each bank, read **once** when it is registered | `LHBankRegister` 0x10002778..0x100029AB (`LHFileSegmentAnimArray` → bank +0x124 / +0x12C width / +0x130 rows; `LHAudioWaveNumTable`) | `AnimEffects.cpp` `anim_effects::RegisterTables` from the bank loop in `Game.cpp` |
| `AnimEffectTable` (the magic code's `AnimEffectBank`, unchanged: same members, `Load`, `FindList`, `SoundId`, `FindSample`) | `LHFindAttribRow` 0x10014420 (row with the most exact columns; on a tie, the last one, fn_10014610) | `AnimEffects.h`; `AnimEffectBank.h` remains as an alias |
| `anim_effects::Number` | `LHSampleGetAnimEffectNumber` 0x10014670: list of 1 → that sample; otherwise, `list[LH_AudioSystem::Rand(n)]` (0x10015710) | `AnimEffects.cpp`, `sample_play::Random` |
| `anim_effects::Play` | `LHSamplePlayAnimEffect(owner, dist, n, track, bank, min, max)` 0x10014A20: nothing with the audio off; the **caller's** distance ≤ 800 (+0x44, `LHSampleRegister3DObjectFunction` 0x426E6B) and ≤ the sample's maxDist (.sad +0x26C, 0x10014ABD); is3D 1, +0x10 0, min/max with their bits 0x80/0x100 only if > 0; the point is given by the game's 3D function for the owner (fn_00427200: without an owner, the camera; unavailable owner or the ambience one, nothing) | `AnimEffects.cpp`, `audio::OwnerSoundPoint` |
| `anim_effects::PlayKey` | 0x100146F0: same gates (800 also for stopping); action 1 = `LHSampleStop` of the first channel of (bank, owner, sample) for each sample in the list (0x1001491C); other = `LHSampleReleaseLoop` (0x10014990) | `AnimEffects.cpp` |
| `audio::PlayAnimationEffect(owner, dist, key, action, bank, track, min, max)` | GAudio 0x42A4B0: action ≠ 0 goes straight to 0x100146F0 without filters or min/max (0x42A4BC); action 0: number (0x42A4F9; 0 = nothing), userParam filters (1 with the script widescreen, only 2 in the citadel, banks after SET_GAME_SOUND, 4 in states 0x10/0x16/0x17), unavailable owner if track (0x42A5A1), and 0x10014A20 | `Audio.h`, `AudioSystem.cpp` |
| `AnimationSounds::Fire` (signature intact) | fn_00516510: the events with from ≤ t < to; without `Get3DSoundPos` nothing; the distance and the surface are **the villager's** (0x51655F, `GSoundMap::GetSurfaceType` 0x51662B → `ecs::sea_cells::GetSurfaceType`); group 1: not alive (`Object::IsAlive` 0x402610, life > 0 and available, the slot of every class) → nothing more (0x5165BC), on any thing (`AnimatedThing::alive` from `ecs::life::LifeOf`, `AnimationSounds::EventVoice`, tests `ClipEventVoice.*` and `AnimationSoundsTest.ADeadAnimalIsQuietOnlyInThePeoplesClips`, since issue 22), voice 3 child / 1 + (+0x1F8 ≠ 0); 0x92 → banter with the house as owner (`GetAbode` 0x51675D; without a house, owner 0 = the camera); 0x93/0x94 → banter on the villager; step (4) of a villager with the script widescreen → **the rest of the list is discarded** (0x5166A4 jumps to the end); THROWN 399/401 only with < 15 / < 10 turns; always track 1, min/max 0 (0x5167A8) | `AnimationSounds.cpp` |
| `AnimationSounds::PlayFromTable` (signature intact, trees) | Tree::Draw 0x74B009 (bend, track 0) / 0x74B25C (rustle, track 1), editor bank (GAudio+0x3B0), the tree as owner (`push ebx` = this, 0x74B008 / 0x74B25B) | `AnimationSounds.cpp`; test `AnimEffectsSyntheticTest.ATreesSoundsAreOwnedByTheTree` |

`AnimEffectBank` (from the magic code) remains as an alias of `AnimEffectTable` for its tests; its `Load(path)` copies the tables
that the core read when registering spells.sad (`test_anim_effects` `MiraclesBankCopiesTheCoresTables`: same rows, lists and
samples). `SpellSounds` no longer uses it: it plays through `audio::PlayAnimationEffect`.

Audible changes (all because of the original):
- The sounds of the clips and of the trees go through the 16 channels (priority, mode, stealing) and follow their owner once per
  turn (`LHSampleUpdate3DChannels`), not every frame.
- GAudio's filters: in the Land 1 intro (the script widescreen lasts until a click) the userParam 1 samples do not
  sound: the tree rustles 307..319 and **all** of VillagersBanter.sad; the creaks 320/321 do. Nor do the villagers'
  footsteps.
- The 0x92 banter is heard at the house but is cut by the villager's distance: with the camera 4 m from him it now sounds
  (before it measured from the house and almost never did). Without a house it sounds at the camera (before nothing).
- Stopping (the saw) stops the first channel of the triple (before all of the owner's emitters).

`OPENBLACK_ANIM_TRACE` trace before/after (Land 1; camera 4 m from villager 0 on turn 100; forced clips
437 yawn, 354 saw, 369 sitting; WATER miracle; the "after" with `OPENBLACK_AUDIO_TEST_NO_WIDESCREEN=1` to remove
the intro filter): same lines and same cases — tree rows with the 15 samples 307..321, footsteps 259..268 on
surfaces 2/3, saw 229/230 on 2/3, `banter N too far (D > 8)` and the 4 lines of the water PSys the same (spells.sad/51).
Difference: the yawn gives 4 banter that sound (`VillagersBanter.sad/N`) and before 0, because of the villager's distance. Without the
hook ✅ faithful only the creaks 320/321 sound. The core's rejections (800, maxDist) go to `OPENBLACK_AUDIO_TRACE`.

- `AnimEffectTable::FindList`: LHFindAttribRow 0x10014420: a row matches when each of its first width - 1 columns is the wildcard or the key's (0x100145A1..0x100145B6); between two matching rows fn_10014610 keeps the earlier one only when it has more columns equal to the key (0x10014662), so the later wins a tie
- `anim_effects::Number`: 0x10014674..0x1001468F: the audio system's +8 / +0x14 / +4 and a bank
- `anim_effects::Play`: 0x10014A48..0x10014A7D: +8, +0x14 (active), +4 and a bank
- `anim_effects::Play`: 0x10014A83 (the global 800) and 0x10014AB0 (the sample's .sad +0x26C): the caller's distance must not be more
- `anim_effects::Play`: +0x08 = 1 (0x10014AF3)
- `anim_effects::Play`: +0x10 = 0 (0x10014AFD)
- `anim_effects::Play`: +0x0C (0x10014B05)
- `anim_effects::Play`: +0x1C (0x10014B0C)
- `anim_effects::Play`: 0x10014ACC: fcomp 0
- `anim_effects::Play`: +0x30 (0x10014B9F..0x10014BBA)
- `anim_effects::PlayKey`: 0x1001471E..0x10014762: +8, +0x14, +4, a bank, and the distance within the global 800
- `anim_effects::PlayKey`: 0x100147D0: one sample of the list, then the same as 0x10014A20
- `anim_effects::PlayKey`: 0x1001491C..0x10014989: for each sample of the list the first channel of (bank, owner, sample) stops
- `anim_effects::PlayKey`: 0x10014990..0x100149F5: the same with LHSampleReleaseLoop
- `AnimAction`: The 4th argument of GAudio::SamplePlayAnimEffect 0x42A4B0: 0 plays one sample of the row (0x42A4EC), any other value goes to the key variant 0x100146F0: 1 stops the row's samples (0x1001491C), the others release their loops (0x10014990)
- `k_MaxDistance`: LHSampleRegister3DObjectFunction(fn_00427200, 800.0) at 0x426E6B: LH_AudioSystem+0x44, the farthest an anim effect plays, stops or releases (0x1001475A, 0x10014A8A: a distance, compared with the caller's)
- `Play`: LHSamplePlayAnimEffect(owner, dist, sample, track, bank, min, max) 0x10014A20: nothing while the audio is off (+0x14), for no bank, farther than k_MaxDistance (0x10014A8A) or than the sample's max distance (.sad +0x26C raw, 0x10014ABD); options is3D 1 (+0x08), +0x10 0, track +0x0C, owner +0x20, the sample +0x24, min +0x54 / max +0x58 with their caller bits 0x80 / 0x100 only when > 0 (0x10014ACC..0x10014B4F); the channel allocated (0x10011020) and the point of the game's 3D function for the owner (fn_00427200, the camera for none; nothing for the atmos owner or an unavailable thing, 0x10014B91..0x10014B9D); then LHSamplePlay. `distance` is the caller's (the camera's distance to the object), not measured here.
- `PlayKey`: LHSamplePlayAnimEffect(owner, dist, key, action, track, bank, min, max) 0x100146F0: the same gates (+0x14, bank, k_MaxDistance 0x1001475A), the key's row (none: nothing); action 0 plays one of its samples as Play (0x100147D0; GAudio never calls it with 0); 1 stops, for each sample of the list, the first channel of (bank, owner, sample) (0x1001491C..0x10014989: LHSampleStop); any other value releases its loop (0x10014990..0x100149F5: LHSampleReleaseLoop)

#### SoundTag

The tags are kept by the sound tag system (`Locator::soundTagSystem`, `SoundTagSystemInterface`, raffclar's slot
and names, with our own signatures; the tag record is `SoundTagSystem::Tag`, kept in the system's list and not in the
registry, so no land gets a storage for it). Its implementation `ECS/Systems/Implementations/SoundTagSystem.cpp` is the port of SoundTag.cpp
0x71E300..0x71ED90; `src/Audio/Services/SoundTags.{h,cpp}` keep the API `audio::tags`, each function handing its call to
the system:

| function | original |
|---|---|
| `Create(thing, sample, track, mode, loops, extra3DFlag, is3D, SfxBank, delay)` | 0x71E840 (offset 0) |
| `Create(thing, offset, …)` | fn_0071E8C0 → ctor 0x71E300: the point is the thing's MapCoords (x, GetAltitude + y, z) |
| `Create(point, …)` | fn_0071EA40: sounds **right away** through 0x429E30 (owner the tag, track 0, mode +0x50, without +0x10) except 3D with delay |
| `CreateAtMapCoords(x, z, height, …)` | 0x71EB60 (`GameQueries::landAltitude` = LH3DIsland::GetAltitude 0x803090) |
| `SetActive(tag, b)` | fn_0071E640: an active tag that is switched off cuts its sample (0x42A210) |
| `Remove(thing, sample, SfxBank)`, `Remove(…, stop)` | 0x71EBE0 / 0x71EC30: each tag from fn_0071ED60 (thing, sample, type) → ToBeDeleted; with stop, 0x42A210 first |
| `Delete(tag)` | ToBeDeleted 0x71ECB0 → CreateSoundTagForDeadObject 0x71ECD0: forgets the thing; if the sample is playing with loops (fn_0042A460 +0x40 and fn_0042A2D0) it releases the loop (0x42A310) and the tag lives until it ends; otherwise, it is deleted (whatever is playing continues) |
| `RandomSample(first, n)` | 0x71ED40 |
| (internal) `ProcessSoundTags` | 0x71E5F0 from `GGame::EndTurn` 0x54E989, from newest to oldest; fn_0071E680: with a thing, not functional / no point → ToBeDeleted, inactive → nothing, otherwise 0x42A100 (point, +0x1C, sample, +0x30, mode, loops, +0x40, is3D, bank) every turn; without a thing: the delay (`CheckDelay` 0x71E760: sounds when 347 ([0x980530]) · turns · [0xD01A38] ms ≥ the distance, if it is within maxDist and active; the delay runs out anyway) or it is deleted when its sample stops |
| (internal) `TagSoundPoint` (was `Get3DSoundPos`) | 0x71EC90: its thing's; without a thing it answers 1 without writing, so the channel keeps its point (`SamplePlay::UpdateChannels`) |

`SoundTag::Set` 0x71E4F0: active 1, track only with a thing (0x71E55D), delay only if is3D (0x71E56B), +0x34 = 0 in the
three effect-bank forms (bank = GAudio+0x3A8 + 4·type, GetBank 0x71E610). The fourth form, fn_0071E920 (ctor fn_0071E460,
+0x34 = 1 → ambience bank GAudio+0x194 + 4·type, the ATMOS_TYPE banks), is fn_0071EA40 with that bank and the same
arguments (point, sample, track, mode, loops, +0x40, is3D, type, delay; checked in the disassembly 0x71E920..0x71EA3B):
a point tag that sounds right away through 0x429E30 except 3D with delay, and GetBank gives every later use of the tag
(the delay's max distance and play, the end-of-sample test, the stops) the ambience bank. It is ported as
`SoundTagSystemInterface::CreatePointSound(point, …, AtmosType, delay)`: the tag keeps the ambient type
(`SoundTag::ambientBank`) and its bank is the registered atmos bank of that type (`rain.sad` for 12), found by file name
as the atmos mixer finds it. Its only caller is the thunder in `GWeather::Update` 0x83FC62 via the callback [0xEEA388] =
0x429CE0 (pushes: sample 2 + GetTickCount() % 11, track 0, mode 2, loops 0, +0x40 0, is3D 1, type 12, delay 1). It is
ported as `audio::tags::Thunder` (`Services/SoundTags`), the storms' sheet callback that `audio::Init` sets (tests
`AmbientPointSoundWaitsAndPlaysFromItsBank`, `ThunderIsAClapOfTheRainBankPickedFromTheTicks`,
`TheAudiosStartSetsTheStormsThunder`). `CreateAtMapCoords`
receives x/z in world units: the original's MapCoords stores them as integers and scales them by 1/6553.6 ([0x8AA3A4],
0x71EB8A/0x71EBA6). The older names (`sound_tags::Create(TagDesc)`, the waterfall of
`DesignedScenery`) remain on top: a `TagDesc` without a thing is the waterfall's ScriptMarker (a GameThingWithPos that does not
go away), which is repeated as a thing tag.

`LanternSounds` is now a street lantern with its tag (API intact: `SetOn`, `ProcessTurn`, `Clear`):
`CallVirtualFunctionsForCreation` 0x734810 → fn_0071E8C0(lantern, (0, Object::GetHeight, 0), 0x93, 0, 2, −1, 0, 1, InGame, 0)
and `SetActive([0xDA0A10])` 0x734965; `SetOn` = fn_007349E0 (SetActive of each lantern on change). A lantern that goes away
leaves its tag like that of a dead object (it releases the loop). It sounds the same as before: at the tip, when approaching to less than
5 (0x429E30), looping, and it is not cut when moving away (channel without track); now it counts in the 16 channels and respects GAudio's
filters.

Tests: `test_sound_tag_channels` (12: the ambient point tag that waits and plays from its bank, mode 2 only re-triggers if not playing, mode 3 restarts, out of range and inactive, the thing that
goes away releases the loop and the tag waits for it to end, one without loops goes away immediately, point once, delay at 347/s, Remove with and
without stop and by bank type, lantern at its tip) and `test_anim_effects` (7). In game (`OPENBLACK_TIME_OF_DAY=22`,
`OPENBLACK_AUDIO_TEST_LANTERN=100`): 12 lanterns with a tag, `G_Lantern_01.wav mode 2 owner sound tag`, looping, with the
camera 3.2 from the tip.

Original facts:
- The tag list is g_game+0x205C1C; new tags go at its head (0x71E31F..0x71E339). ProcessSoundTags reads the next tag
  before processing one, because a tag may delete itself (0x71E600).
- The channel's owner is the tag itself (0x71E6F1).
- Tag fields: +0x10 the point, +0x4C active (1 from Set, 0x71E4FA). +0x30 = 0 without a thing (0x71E568). A turn
  counter goes up on each pass (0x71E686). The dtor is fn_0071E5A0. CreateSoundTagForDeadObject clears the thing at
  0x71ECD3.
- fn_0071E680, one pass of a tag: IsFunctional (vt +0xD4) false (0x71E69A), or Get3DSoundPos ≠ 1 (0x71E6B8), →
  ToBeDeleted; inactive (+0x4C, 0x71E6A4) → nothing; otherwise it plays at 0x71E6F2; a tag without a thing goes to its
  delay (0x71E71B), and otherwise is deleted once its sample stops (fn_0042A2D0).
- CheckDelay: it measures `GCamera::GetDistanceSq` of +0x10 (0x71E7A6); while reach² < dist², the sound has not reached
  the camera yet (0x71E7D4); it plays through 0x42A040 when dist² < max² and the tag is active (0x71E7E1..0x71E7F9); the
  delay is cleared at 0x71E825.
- The point form fn_0071EA40 builds the tag with fn_0071E3E0 (no thing, no offset).
- **Whose tag ends when** (✅ faithful, 2026-10-20; read from each class's slot +0xD4): `GameThing` 0x405240 is
  `IsAvailable` (not UNAVAILABLE, 0x401810), and so are `MobileStatic` 0x4396A0 (the landscape vortex), `FieldCrop`
  0x608270, the citadel (fn_00465C70's thing is the citadel part's citadel), the interface (sub_5D2730, the hand's
  gesture) and the script markers; `Living` 0x4172D0 is available and life ≠ 0 (a NaN life does not work: the
  creature of `Creature::SetCreedPower` 0x4F8ABD, the animals); `Villager` 0x751CF0 is available and the top state not
  13 or 14; `MultiMapFixed` 0x52EF70 is available, built (vt +0x890; `TotemStatue::IsBuilt` 0x5610B0 for the totem of
  `TotemStatue::SetWorshipPercentage` 0x73845B) and not (percent repaired, the life, ≤ the non-functional mark,
  vt +0x894, 0.75 at 0x52EFC0; a NaN life works); `Abode` 0x406200 its own (the workshop of fn_007798A0, the
  windmill). The creed object's (0x50B3C1) is `GameThing`'s; its own `ToBeDeleted` 0x50B3FA removes the tag anyway. A
  script's ATTACH_SOUND_TAG (0x7101B9) takes the rule of its thing's class. The order of a pass (0x71E69A..0x71E6BE):
  not functional → ToBeDeleted; inactive → nothing, its position not asked; no position → ToBeDeleted.
  openblack: `ecs::thing_functional::OfThing` (the abodes through `abode_queries::IsFunctional`, the village totem as a
  fixed object, creatures and animals as living, villagers, everything else available), the pure rule
  `thing_functional::IsFunctional` (tests `ThingFunctional.*`), in `SoundTagSystem`'s pass in the original's order
  (test `SoundTagTest.AnInactiveTagOnAWorkingThingWithNoPlaceStays`). The debug bar's Audio window shows each village
  totem's rising tag and whether the totem works. Not read: a forest's (0x539A60, its two counts) and a spell's
  (0x55CDC0) own rules, reachable only through a script's ATTACH_SOUND_TAG; openblack takes them as available.
- Street lanterns: `GStreetLantern::ToBeDeleted` 0x734AB0 only clears +0x60 and unlinks the lantern, so its tag finds
  the thing gone by itself. `InitStaticsValues` 0x54A84F clears the lanterns' tags and the on flag for a new map.
- Original callers: `Windmill::CallVirtualFunctionsForCreation` 0x405974 → SoundTag::Create(windmill, 13 G_windmill, 0,
  2, −1, 0, 1, InGame, 0); `Creature::SetCreedPower` 0x4F8ABD / 0x4F8AD2 → SoundTag::Create(creature, 166 G_Creed_01, 0,
  2, −1, …), then Remove.

**Audit.** Checked in the disassembly 0x42A4B0, fn_00516510 (0x5165BC, 0x5166A4, 0x51675D, 0x5167A8), Tree::Draw
0x74AFE1/0x74B1FA, 0x10014A20, 0x100146F0 (0x1001491C/0x10014990), 0x10015710, fn_10014610, 0x71E300, 0x71E4F0, 0x71E5F0,
0x71E640, fn_0071E680, 0x71E760, 0x71EA40, 0x71EB60, 0x71EBE0/0x71EC30, 0x71EC90, 0x71ECB0/0x71ECD0, 0x71ED40,
0x734920/0x734965: they add up. Only comments fixed (the ambience form fn_0071E920 that was missing, the MapCoords
scale, the stale point of 0x427209). The trees' keys pass column 1 = 0, as the original (ebp = 0, 0x74AB6A); openblack
passed 2 until issue 22, with no audible effect, since all 201 rows of editor.sad have the wildcard in that column
(test `AnimEffectsTest.TreeKeysAlignmentIsAWildcardInTheEditorBank`).

### Test hooks

The audio hooks and traces are listed together on [the audio engine](engine.md#test-hooks).

## Pending

- Villagers, buildings and trees ([their own sounds](#villagers-buildings-and-trees-own-sounds)): the birth, the
  creche, the workshop, the windmill, the worship totem and its bell, the scaffolds and their phantom's appear and
  disappear, the trees, the sacrifice and the buildings' ambient sound of the time of day sound already. A building
  under construction rolls as a built one does (`Abode::Draw` has no percent-built test); a planned one makes none
  (`PlannedMultiMapFixed::Draw` is a bare return). The anim effects that `DynamicsSystem::PlayAnimEffect` plays (the creche, the physics'
  collisions) measure their distance with
  `glm::distance`, not as the original sums it ((dz² + dy²) + dx² on the x87 at 0x757E0C..0x757E27 for the creche)
  **⚠️ approximate**; each caller's order is to be read before it changes. The creed's 166 is the creature's.

### From the audio engine's list

- `IsInScript` (vt +0x448) is taken as false by the footstep filter (`AnimationSounds.cpp`) **🔍 inferred**. openblack
  now has a "held by a script" test (`ecs::script_held::IsInScript`, used by the CHL functions), but the filter does
  not ask it, and whether it is the same test as vt +0x448 has not been checked.
- A tag's thing: `Get3DSoundPos` ≠ 1 = the entity no longer has a position **🔍 inferred**.
- The point of a tag without a thing on a new start of an anim-effect: the original reads the +0x50 of the newly
  allocated channel (0x427209, before LHSamplePlay writes the point), a stale value; openblack gives the tag's point
  **⚠️ approximate** (no caller starts an anim-effect with a tag as owner).
- The street lanterns receive their tag on the `ProcessTurn` following their creation (there is no
  `CallVirtualFunctionsForCreation` hook) and none has the UNAVAILABLE mark **⚠️ approximate**.
- `PlayFromTable` has no track argument: the site (bend 0 / rustle 1) is distinguished by the key's soundId.
- `Key` owners with track do not move (the voices pass track 0); `Tag` owners follow their thing.
- `OnThingDeleted` is called only when a creature leaves the game (`ecs::creature_removal`, `src/ECS/CreatureRemoval.cpp`:
  its sounds lose their owner); for every other thing it is not called: the entities carry a version, so
  `thingPosition` already gives empty for a destroyed one on the next turn.
- `AnimationSounds::PlayFromTable`: the bend key is {size, 0, 0, 10, 75} with size 1 / 2 / 3 (0x74AF5D..0x74AF64), the
  rustle {0, 0, 20, 0, 70} (ebp = 0 from 0x74AB6A, never written again in `Tree::Draw`). `TreeRustle` passes the same
  0 in column 1 (2 until issue 22, inaudible: every editor.sad row has a wildcard there).
