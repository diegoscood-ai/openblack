# Casting and globes

How the player takes a miracle into the hand, holds it, powers it up, casts it with the mouse and drops it, and how
the one-shot globes look and are taken. Gesture recognition itself is in [../gesture/](../gesture/); the prayer power
side is in [prayer_cost.md](prayer_cost.md); dispensers, icons and where globes come from are in
[dispensers_and_seeds.md](dispensers_and_seeds.md).

**Progress: 41/54 done, 9 partial — 84%**

How the original does it, in our wiki: [Miracles one by one](../../bw1-notes/miracles.md).

## Buttons and pressing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The right button casts the held miracle; the left button taps globes and grabs as usual | done | The action button (right) both casts the held seed and taps globes; `SeedActionPressed` in `src/ECS/Systems/Implementations/HandSpellSeed.cpp`, the press branches in `src/ECS/HandPressChain.cpp` |
| Gesture miracles (fireball, storm, shields, flocks) arm on press and cast on release | done | Armed on press (`BeginApplyOnRelease`), cast on release (`UpdateSeedAction`), `src/ECS/Systems/Implementations/HandSpellSeed.cpp`; no unit test |
| Held miracles (lightning, food, wood, water) lock on press, cast at once, then apply once a game turn where the hand points | done | Locked apply on press, then one apply packet a turn while held; `src/ECS/Systems/Implementations/HandSpellSeed.cpp` (`LockedApplyMap`, `SendSeedApplyToMapCoord`) |
| Placed miracles (forest, heal, teleport, blast, creature spells) cast at once on press | partial | Forest, heal, teleport and blast cast on press (`src/ECS/Systems/Implementations/HandSpellSeed.cpp`); creature spells can't be cast from the hand at all (`cast_rules::CanCastOn` in `src/Magic/CastRules.cpp` refuses the creature class, TODO) |
| A creature spell can only be cast on a creature | partial | Never at a point (`cast_rules::CanCastAt` refuses the creature class); but the hand can't cast one on a creature either, only scripts can (`SPELL_AT_THING`, `src/Magic/Script/CHLSpells.cpp`) |
| An object under the hand is tried before the land | done | `SeedActionPressed` tries `ValidToApplyThisToObject` first, `src/ECS/Systems/Implementations/HandSpellSeed.cpp` |
| A press outside the player's influence does nothing at all: no puff, no sound, even over a creature | done | `SeedActionPressed` returns with no fail puff when the hand is out of influence, `src/ECS/Systems/Implementations/HandSpellSeed.cpp` |
| A press before the seed is ready fails with the failure puff and sound | done | `ValidToApplyThisToMapCoord` needs the seed ready, else `FailApply` (spot visual 4 and `G_SpellCastFailure`), `src/ECS/Systems/Implementations/HandSpellSeed.cpp` |
| A press where the miracle's cast rule refuses (needs land, needs influence, out of the map) fails with the puff and the failure sound | done | `magic::seed::CanCast` (`src/Magic/Core/SpellSeed.cpp`, `src/Magic/CastRules.cpp`) then `FailApply` |
| Land for a cast rule means a map cell without water, whatever its height | done | `cast_rules::IsLand` uses `sea_cells::IsLand` (`src/Magic/CastRules.cpp`); no unit test |
| The failure puff is drawn at full size (the camera-distance scaling applies only to effects a player's miracle owns) | done | `psys::manager::CreateSpotVisual` at magnitude 1 (`src/Particles/PSysManager.cpp`) |
| A second press while a gesture seed is armed is ignored | done | While armed the button is still down; the release casts before any new press, `src/ECS/Systems/Implementations/HandSpellSeed.cpp` |
| While a gesture seed is armed a humming loop plays at the hand, following it, and stops hard on release or cancel | done | `BeginApplyOnRelease` / `EndApplyOnRelease`: the hand's sound tag, track 3, looping, `src/ECS/Systems/Implementations/HandSpellSeed.cpp` |
| Storm and shields are cast at the circle drawn while the button is held; a circle is remembered 5 s; with no circle the cast fails | done | Circle kept 5 s (`k_CircleLife`, `src/Magic/Gestures/PowerUpSystem.cpp`); `GestureAllowsCast` fails a cast without it, `src/ECS/Systems/Implementations/HandSpellSeed.cpp` |
| A locked miracle moved over a place it can't apply skips that turn but stays locked; only letting go ends it | done | `UpdateSeedAction` skips the apply when the point is not valid and stays locked, `src/ECS/Systems/Implementations/HandSpellSeed.cpp` |
| Letting go of a held miracle stores its remaining prayer power and age back in the seed, so it can be pressed again until spent | done | `seed::ApplyUnlockProcess` and `StoreChantsAndAgeFromSpell`, `src/Magic/Core/SpellSeed.cpp` |
| After a cast the seed is deleted, kept in the hand, or follows its miracle, as its record says; a success sparkle shows as it leaves the hand | done | `ApplySeedToMapCoord` keeps or removes the seed by `isKeptInHand`, success spot visual 3; `seed::FollowsSpell`, `seed::DrawSpells` (`src/ECS/Systems/Implementations/HandSpellSeed.cpp`, `src/Magic/Core/SpellSeed.cpp`) |
| A seed is deleted together with its miracle | done | `seed::ProcessInHand` deletes the seed when its spell closed; the spell's deletion deletes its seed (`src/Magic/Core/Spell.cpp`) |
| A held miracle carried outside its cast rule (over water, out of influence) is dropped from the hand | partial | The spell's cast point follows the hand (`src/Magic/Core/Spell.cpp`), but the "can't cast here" end of the action is pending; the apply only skips that turn |
| A hand-cast miracle is placed at the hand's reported point | done | The apply packet's point and `FinishCast` in `src/Magic/Core/SpellSeed.cpp` |
| Every hand cast lasts the miracle's player timer times the seed's multiplier | done | `PrepareCast` in `src/Magic/Core/SpellSeed.cpp` (player timer and initial chants times `castMultiplier`) |
| Force-feedback effects on recognising and casting | n/a | openblack has no force-feedback devices |

## Holding a seed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A seed from an icon needs 1.5 s before it is ready; a globe or dispenser seed is ready at once | done | `seed::ProcessInHand` (`delayBeforeSeedActive`), `src/Magic/Core/SpellSeed.cpp`; a globe's seed is ready at once (`one_off::CreateSpellIntoHand`, `src/Magic/Core/OneOffSpellSeed.cpp`) |
| Becoming ready cancels a press made too early | done | `seed::ProcessInHand` calls the hand's `EndAction` when the seed becomes ready |
| The hand holds a seed as a not-ready miracle until it is ready, then by the seed's hold (above, side or magic), refreshed each game turn | done | `ComputeHoldParameters` (MAGIC until ready, then the seed's hold type), every frame from `UpdateSeedInHand`; `src/ECS/Systems/Implementations/HandHolding.cpp` |
| Each hold shows a still frame of its hand animation chosen by the seed's size, with no leaning | done | `Cwiggle`, `Chold_above`, `Chold_side` held at a frame by the hold radius, `src/ECS/Systems/Implementations/HandSystem.cpp` |
| The hand rises by how it holds (a little above, more for magic, by the seed's height at the side) | done | The hold height by hold type in `src/ECS/Systems/Implementations/HandPlacement.cpp` |
| The seed's model sits below the hand, turned with it (forest half round, ground flock a quarter) | partial | The seed sits at the hold point turned with the hand (`HandPlacement.cpp`); the seed's own turn from its record (forest, ground flock) is not applied |
| The held seed and hand sway as the cursor runs ahead, and roll about the line to the camera | done | `HeldSway` and the grain state's tilt, `src/ECS/Systems/Implementations/HandPlacement.cpp`, `HandHolding.cpp` |
| Taking or losing a seed fades the hand's pose over 0.13 s; becoming ready or powering up does not | done | The 0.13 s state blend only on a change of the hand's state, `src/ECS/Systems/Implementations/HandSystem.cpp` |
| The seed's model shows only once the seed is ready | partial | The model is shown in the hand by the magic's "drawn in hand" flag (`ShowSeedMesh`, `src/ECS/Systems/Implementations/HandSpellSeed.cpp`), not tied to readiness |
| The in-hand effect sits among the fingertips (at the hand point for water), starts as the seed enters, is drawn only once ready and is sized by the hand's distance and the seed's power | partial | `hand_fx::CreateInHandEffect` / `UpdateInHandEffect` (`src/Magic/Hand/HandMagicFX.cpp`): drawn once ready, strength from the seed; it always sits at the hand's origin (the bone is not applied) |
| Creature potions (phials) are held at the side of the hand | done | The seed record's hold type in `ComputeHoldParameters` (`HandHolding.cpp`) |
| A holder effect (fire, lightning, heal, shield, storm, water, teleport) plays round a globe or a held seed of that kind | partial | Around globes and icons (`src/Worship/SpellSeedGraphic.cpp`); a held seed shows its in-hand effect instead |

## Globes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Tapping a globe puts its miracle in the hand, fully charged and ready, with the bubble-pop sound at the hand; the globe goes | done | `one_off::InterfaceTap` and `CreateSpellIntoHand`, `src/Magic/Core/OneOffSpellSeed.cpp` (bubble-pop sample at the hand) |
| A globe shows its miracle's model or holder effect inside a transparent spinning sphere with a running glint | done | The orb faces the camera with its 4 by 4 glint (`one_off::UpdateFrames`) and the seed graphic inside (`src/Worship/SpellSeedGraphic.cpp`) |
| An extreme globe has one coloured ring per power-up level, turning, in the player's colour (the local player's for a neutral globe) | done | The seed graphic's power-up bands in the owner's (or local player's) colour, `src/Worship/SpellSeedGraphic.cpp` |
| A globe's seed is made at the player's icon for that miracle when they have one (so it can be powered up and refunds there) | done | `worship::player::FindBestSpellIconForSpellSeed` in `one_off::CreateSpellIntoHand` (`src/Magic/Core/OneOffSpellSeed.cpp`); a loose seed without an icon |
| A creature judges a globe it finds by its owner: one belonging to another player can be stolen | todo | Nothing in the creature mind looks at globes |
| A creature weighs a globe by its miracle: aggressive, compassionate, playful or health-restoring | todo | Nothing in the creature mind looks at globes |
| Globes are saved and loaded with the game | todo | No game save system |

## Power-ups (extreme miracles)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Drawing a power-up gesture over a ready, uncast icon seed raises it to the next level (fire, lightning, storm, heal, water, food, blast) | done | `PowerUpGestures` in `src/Magic/Gestures/PowerUpSystem.cpp` with the worship icon provider (`src/Worship/GestureIconProvider.cpp`) |
| A power-up waits for the icon to charge the extra; a scribble cancels the pending level | done | `icon::SetChargingPowerUp` (`src/Worship/WorshipSpellIcon.cpp`); a scribble sets the pending level back (`PowerUpGestures`) |
| Extreme versions use their own cast effect, in-hand effect, cost and strength | done | `MagicInfoForPowerUpLevel` in `src/Magic/MagicTables.cpp`, used by `seed::MagicTypeOf` |
| Taking or powering up a seed flies five bands onto the hand, plays the band sound and the announcer names the level | done | `seed::SetPowerUp` (`src/Magic/Core/SpellSeed.cpp`): `hand_fx::AddSpellToHandVisuals` with `G_SpellPowerUpBand` and the SpellDialogue level voice 10 to 12 |
| Bands spin round the hand, faster further up the arm; bracelets stay for each power-up after a delay; the hand glows in the player's colour | partial | Bands and delayed bracelets in `src/Magic/Hand/HandMagicFX.cpp`; the hand's glow is computed (`hand_fx::GetGlow`) but not drawn |
| A tribe's power spins the tribe's name round the hand, rises as a column on casting, with the tribe's voice | done | `src/Magic/TribalPowerSpin.cpp` run by `hand_fx` (`src/Magic/Hand/HandMagicFX.cpp`), started from the seed in the hand and let go or raised as a column by `FinishCast` (`src/Magic/Core/SpellSeed.cpp`) with the tribe's voice, drawn by `Renderer::DrawTribalPower`; tests `test/test_tribal_power_spin.cpp`, `test/test_hand_fx_tribal_power.cpp`. Only for a tribe whose power is above 1, which no tribe reaches in the vanilla game; the step runs in the magic loop rather than the frame's draw, and nothing is drawn inside the temple |

## Dropping and cancelling

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A scribble drops the held seed with the shake sound (quiet) and the bands flying off; outside the power-up path it needs the hand in influence | done | `ProcessPowerUpSystem` scribble (in influence) calls `RemoveHandSpellVisuals` and `ForceDropHeld` (`src/Magic/Gestures/PowerUpSystem.cpp`, `src/ECS/Systems/Implementations/HandSpellSeed.cpp`) |
| A dropped seed gives its prayer power back to its worship site; a globe seed with no icon just vanishes | done | `worship::ReturnSeedToItsSite` (`src/Worship/Worship.cpp`) from `ApplyForceDropHeld`; a seed without an icon is just deleted |
| Putting a seed down on a dispenser, worship site or icon returns it there | done | `worship::IsSeedReturnPoint` / `ApplySeedToObject` (`src/Worship/Worship.cpp`), `dispenser::ApplySeed` (`src/Worship/SpellDispenser.cpp`) |
| Leaving the hand records the last miracle (for the repeat gesture), cancels the icon's charge and removes the bracelets | done | `SeedLeftHand` in `src/ECS/Systems/Implementations/HandSpellSeed.cpp`: the last seed type, `worship::OnSeedOutOfHand` (the charge cancelled) and the bands removed |

## Choosing a miracle and moving with it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Choosing a miracle by gesture (spiral, or reverse spiral for creature spells, then the miracle's gesture, 30 s to finish) and repeating the last one (40 s) | partial | Spiral and reverse spiral open the selection (30 s timeout) and the R gesture repeats the last seed (`src/Magic/Gestures/PowerUpSystem.cpp`); the 40 s cap on the repeat is not found |
| A thrown miracle leaves along the hand's smoothed velocity (four fifths of the way in a tenth of a second, speed mapped up to 200) | done | The grain state's smoothed hand velocity (`src/ECS/Systems/Implementations/HandHolding.cpp`) sent with the apply |
| A miracle spins by how the hand's path turns sideways as it is cast | done | The grain state's sideways angular velocity (`HandHolding.cpp`), sent with the throw data |
| The held miracle and globes are saved and restored with the game | todo | No game save system |
