# Dispensers and one-off seeds

Where miracles come from other than the player's own worship: miracle dispensers that grow a globe every so often,
one-shot globes placed by the lands' scripts, and the miracle icons at the worship site and village centres that
charge a miracle for the hand. Taking a globe and holding it is in [casting_and_globes.md](casting_and_globes.md).
Debug and testbed tools that make dispensers and globes don't count here.

Silver scrolls that give a dispenser are listed in [../story/rewards.md](../story/rewards.md) (among them the flying-flock
dispenser of [The Magic Dragon](../story/silver_scrolls/the_magic_dragon.md), the food dispenser of
[The Singing Stones](../story/silver_scrolls/the_singing_stones.md), the big dispenser of [The Sea](../story/silver_scrolls/the_sea.md) and the wolf-pack dispenser of
[The Slavers](../story/silver_scrolls/the_slavers.md)), with the reward script's refill-time quirk. Scrolls that give seeds: the itchy
creature seed of [The Heavenly Fire](../story/silver_scrolls/the_heavenly_fire.md), the heal chest of
[Swap To Brown Bear](../story/silver_scrolls/swap_to_brown_bear.md) and the water seeds of [Throwing Stones](../story/silver_scrolls/throwing_stones.md).
Gold scrolls that hand out seeds or dispensers: the three fire seeds of [Khazar's Fireball Challenge](../story/gold_scrolls/khazars_fireball_challenge.md),
the three shield seeds of [Khazar's Shield Challenge](../story/gold_scrolls/khazars_shield_challenge.md), the monk's two water one-shot
miracles in [Fire! Fire! I'm on Fire!](../story/gold_scrolls/fire_fire_im_on_fire.md), the half-built shield dispenser of the fourth
land's arrival ([The Defending Ogres](../story/gold_scrolls/the_defending_ogres.md)) and the heal dispenser of
[I have a surprise for you.](../story/gold_scrolls/i_have_a_surprise_for_you.md#the-reward)

**Progress: 10/28 done, 0 chosen, 15 partial, 0 inferred — 74%**

How the original does it, in our wiki: [Miracles one by one](../../../magic/fireball-and-lightning.md).

## Dispensers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A dispenser is a building with the miracle-creator model, holding one miracle, which may be an extreme version | done | `dispenser::Create` (an abode with a `SpellDispenser` component) and its magic's power-up level in `dispenser::CreateOneOffSpellSeed`, `src/Worship/SpellDispenser.cpp`; checks: scenarios `miracles.dispensers`, `miracles.dispenser_vortex` |
| It counts game turns only while active, holding a miracle, built and repaired, and makes its first globe after its full period (300 turns) | done | `dispenser::StepTurn` counts only while active and holding a magic, from the town's abode pass (`src/ECS/Town/TownProcess.cpp`); scripts make the first globe at once when they activate it (`SetMagicAndPeriod`), as our wiki reads ([dispensers](../../../magic/worship.md#dispensers-and-fireflies)); test `DispenserRules.ADispenserCountsToItsPeriodWhileItsBubbleIsGoneAndAsksAgainUntilOneIsMade`; built and repaired are the abode's own tests (`abodes::IsBuilt`, `abodes::IsRepaired`); test `DispenserRules.ADispenserCountsOnlyWhileItsBuildingIsBuiltAndRepaired` |
| The globe floats at 1.2 times the dispenser's height and appears with a sparkle | done | `OrbPosition` and the orb spot visual in `dispenser::CreateOneOffSpellSeed`, `src/Worship/SpellDispenser.cpp`; test `DispenserRules.TheOrbFloatsAboveItsDispenserAndStaysOnItWhileTheyTouch` |
| Once the globe is taken or moved off, the count starts again | done | `dispenser::StepTurn` (orb gone or no longer touching: tick back to 0), `src/Worship/SpellDispenser.cpp`: the orb touches while it is available and their distance across the land less both their radii is within 0.001 m, the original's object touch test, and a globe that could not be made is asked for again on the next pass with the count left as it is ([wiki](../../../magic/worship.md#dispensers-and-fireflies)); tests `DispenserRules.ADispenserCountsToItsPeriodWhileItsBubbleIsGoneAndAsksAgainUntilOneIsMade`, `DispenserRules.TheOrbFloatsAboveItsDispenserAndStaysOnItWhileTheyTouch` |
| A faint starry disk turns on the ground under every dispenser, always on, the same for every miracle | done | The dispenser's ground effect (particle type 0x90) started in `dispenser::Create`, `src/Worship/SpellDispenser.cpp`; check: scenario `miracles.dispenser_vortex` |
| Dispensers and globes make no sound of their own apart from the pop when taken | done | No dispenser sound; the pop is in `one_off::InterfaceTap` (`src/Magic/Core/OneOffSpellSeed.cpp`); check: scenario `miracles.dispenser_vortex` |
| The lands' scripts place dispensers with their tribe, miracle, angle, size and period | partial 90% | `CREATE_SPELL_DISPENSER` in `src/LHScriptX/FeatureScriptCommands.cpp` to `magic::script::CreateSpellDispenser` (`src/Magic/Script/MapScriptMagic.cpp`); tests `LandScriptMagicObjectsTest.CreateSpellDispenserGivesItsMiracleAnOrbAtOnceAndItsPeriod`, `LandScriptMagicObjectsTest.CreateSpellDispenserWithNoPeriodMakesItsOrbAndIsSwitchedOff` pin the town, the miracle, the orb and the period; the angle and size are not pinned (Land 1 places none; Lands 3 and 5 do) |
| A dispenser belongs to a town | done | A map script's: the town of its number, else the nearest town (`script::DispenserTownFor`, `src/Magic/Script/MapScriptMagic.cpp`); with no town, as the CHL's, the first player's oldest town (`dispenser::TownIdFor`, `src/Worship/SpellDispenser.cpp`), as the original ([page](../../../beings/towns.md)); tests `test/test_town_script_id.cpp` `TownScriptIdTest.AMapScriptsDispenserJoinsTheTownOfItsNumberElseTheNearestTown`, `TownScriptIdTest.ADispenserWithNoTownJoinsTheFirstPlayersOldestTown` |
| Challenge scripts put a miracle into a dispenser or object or take it out | done | `SET_MAGIC_PROPERTIES`, `SET_ACTIVE`, `SET_TIMER_TIME` on a dispenser and `SET_MAGIC_IN_OBJECT` on a town (`src/Magic/Script/CHLWorship.cpp`, `src/Worship/SpellDispenser.cpp`); CHL `CREATE` of a spell dispenser. A timer under one turn leaves the period and never touches the switch, as the original ([wiki](../../../magic/worship.md#script)); tests `MagicSystemTest.AScriptTurnsOnlyADispenserOffAndSetsItsTurnsUnlessNone`, the `SET_MAGIC_PROPERTIES` and `SET_MAGIC_IN_OBJECT` rows of `test/test_miracle_core_rows.cpp` |
| The globe's land light and haze are taken at its point moved towards the camera | partial 70% | The orb's light and haze are taken at its position plus the facing offset, not at the point moved towards the camera ([spell core, pending](../../../magic/spell-core.md#pending), issue #60) |
| A dispenser hit by a thrown rock: how it is drawn broken and what becomes of its globe | pending | openblack breaks it into pieces like a house; the original draws it its own way and what happens to its globe is still to read ([spell core, pending](../../../magic/spell-core.md#pending), issue #60) |
| Dispensers are saved and loaded with the game | pending | No game save system |

## One-shot globes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The lands' scripts place one-shot globes, plain or powered up | partial 90% | `CREATE_ONE_SHOT_SPELL` / `CREATE_ONE_SHOT_SPELL_PU` to `magic::script::CreateOneShotSpell` / `CreateOneShotSpellPu` (`src/Magic/Script/MapScriptMagic.cpp`); CHL `CREATE` of a one-shot spell too; tests `LandScriptMagicObjectsTest.CreateOneShotSpellMakesTheNamedSeedAtItsBaseLevel`, `LandScriptMagicObjectsTest.CreateOneShotSpellPuMakesTheMiraclesFirstSeedAtItsLevel`, `LandScriptMagicObjectsTest.CreateOneShotSpellPuNeedsAMiracle`; no check runs a land script that places one |
| A globe a challenge script makes drops onto the land and settles there, even when the script lifts it straight after (Khazar's gifts at the start of the second land) | done | CHL `CREATE` of a one-shot spell: `magic::script::CreateOneShotSpell` (`src/Magic/Script/CHLWorship.cpp`) makes it on the land and `ecs::physics::script_create::PutIntoPhysics` (`src/ECS/Physics/ScriptCreate.cpp`) puts it into the physics standing still; test `ParticleCarriedObjectsTest.AScriptsNewObjectGoesIntoThePhysicsStillAndSettlesOnTheLand`; [the common step](../../../scripts/map-script-reader.md#the-common-step-after-the-switch) |
| A globe is made for a miracle at its seed's power-up level | done | `one_off::Create` (`src/Magic/Core/OneOffSpellSeed.cpp`) with the level from `GetPowerUpFromMagicType`; test `MagicSystemTest.AOneShotOfAMagicTypeHoldsTheFirstSeedWithItAtItsLevel` |
| Lifting a tree or rock that a firefly hides in leaves a one-shot globe in its place, drawn by weight from the land's firefly reward table | done | `fire_fly::OnPlacedInMagicHand` and `fire_fly::Reward` (`src/Worship/FireFlyReward.cpp`), from the hand's pick-up; fireflies in `src/ECS/FireFlies.cpp`; `FIRE_FLY_SPELL_REWARD_PROB` is wired (`CREATE_FIRE_FLY` itself is empty); test `WorshipTest.FireFlyRewardProbabilities` |
| The scripted "falling miracle" sequence: a challenge script starts it with the same command that plays the intro film; it runs only when the player has a creature, and the screen fades back to normal as it starts | done | `SET_AVI_SEQUENCE` sequence 2 in `src/CHLApi.cpp` to `Video/FallingSpellVideo.h` (nothing without the player's creature), with the black fade cleared; tests in `test/video/test_falling_spell_video.cpp` |
| Skirmish set-up chooses the one-shot miracles and miracles on offer | pending | No skirmish set-up; see [../multiplayer/](../multiplayer/) |

## Seeds hidden in trees and rocks

No land hides a miracle seed under a tree, and no tree carries one: the one-shot globes the land scripts place all lie
in the open (none of the 244 placed on the story lands and skirmish maps is within 3 m of a tree), and shaking, burning
or felling a tree, or the creature lifting one, never reveals anything. The seed players remember finding under a tree
or rock comes from a firefly: by day fireflies hide in the trees and rocks nearest the houses and street lights they
hovered by, and when the player's hand uproots or lifts the one a firefly hides in, the firefly is gone and a one-shot
globe lies where it stood (the row above; [../nature/fireflies.md](../nature/fireflies.md)). Reward chests can also hold
a one-shot seed ([../hand/clicking_and_activating.md](../hand/clicking_and_activating.md)).

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The Hermit's rock on Land 1: a strong creature miracle seed appears where the rock lay once the hand lifts it, the creature holds it or it is moved more than 5 m, and the advisors explain that fireflies hide under rocks at dawn and become seeds | partial 30% | Scripts can now make the seed (CHL `CREATE` of a one-shot spell, `src/Magic/Script/CHLWorship.cpp`); the rest is the Hermit's challenge script, not checked here |

## Worship site and village-centre icons

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player's worship site shows one icon per miracle they have, in a fixed placement | partial 80% | `icon::Create` in the site's slots (`src/Worship/WorshipSpellIcon.cpp`, `src/Worship/WorshipSite.cpp`); on lands where the player has a worship site; no test or check covers it (re-measure 2026-10-21) (Land 1 has no worship site) |
| Converting a town adds its village-centre miracle (and power-ups) to the player's icons | partial 50% | A town's magic and village-centre icons are kept (`src/Worship/TownMagic.cpp`, `src/Worship/TownCentreSpellIcon.cpp`) and its worship site added at load or when it gets people; a town changing player is not followed |
| Tapping an icon starts it charging, with a click whose pitch rises with the icon's place | done | `icon::InterfaceTap` / `HandleValidatedTap` (an empty hand starts the charge for the tapping player) and `PlayTapSound` by placement (`icon::TapSoundPitch`, `src/Worship/WorshipSpellIcon.cpp`, [page](../../../magic/worship.md)); tests `test/test_dispensers_seeds_miracle.cpp` `DispenserSeedsIcons.TappingAnIconWithAnEmptyHandStartsItChargingForThatPlayer`, `DispenserSeedsIconTap.TheTapsClickRisesWithTheIconsPlaceUpToTheSixth` |
| Each charging icon takes an equal share of what the site can give each turn until it holds the miracle's cost to create | done | The site's turn shares what is left among the charging icons (`src/Worship/WorshipSite.cpp`); tests `WorshipBattery.strainAndIconShare`, `WorshipTest.ChantStoreKeepsTheExcessQuirk` |
| A charging icon shows a ring of light filling with the charge, a pulsing shine and a flash as it starts | partial 40% | The pulse ring while charging, raised over the icon in its player's colour, and the icon's tint in its player's colour (two flashes as the charge starts, then dim; a pulse when full) and the sheet of light round it that rises with the charge (`icon::UpdateChargingVisual`, `charge_visual::Tint`, `charge_visual::SheetPoints`, `src/Worship/`; tests `IconChargeVisual.*`); still to show through the real input path, and the town centre's slot icons do not show it yet ([the charge visual](../../../magic/worship.md#the-charge-visual)) |
| A fully charged icon puts the seed into an empty hand, with a voice naming the miracle | done | `icon::PutFullyChargedSeedInHand` and the SpellDialogue voice in `icon::Process` (`src/Worship/WorshipSpellIcon.cpp`); tests `MagicSystemTest.ASummonedIconThatIsNotFullStartsChargingAndGivesNoSeedYet`, `MagicSystemTest.AFullIconSummonedWithTheHandBusyStartsChargingInstead` |
| Tapping a charging icon cancels it and gives the prayer power back | done | `icon::CancelCharge` / `ReturnAllChantsToWorshipSite` (`src/Worship/WorshipSpellIcon.cpp`): only the hand that charges it cancels, back at the plain miracle, the store into the site's battery (lost without a site); test `test/test_dispensers_seeds_miracle.cpp` `DispenserSeedsIcons.TappingAChargingIconCancelsItAndGivesThePrayerPowerBackToTheSite` |
| Scripts enable or disable a player's miracles and ask whether they have one | done | `SET_PLAYER_MAGIC`, `HAS_PLAYER_MAGIC` in `src/Magic/Script/CHLSpells.cpp`; test `test/test_miracle_core_rows.cpp` `MiracleCoreScript.AScriptGivesOrTakesAPlayersMiracleAndAsksWhetherThePlayerHasIt` |
| Scripts ask whether a miracle is charging and clear all charging | done | `IS_SPELL_CHARGING`, `IS_THAT_SPELL_CHARGING`, `CLEAR_PLAYER_SPELL_CHARGING` in `src/Magic/Script/CHLWorship.cpp`; tests `test/test_script_hand_natives.cpp` `ScriptHandNatives.ASpellIsChargingWhileAnIconOfThePlayersTempleHoldsChants`, `ScriptHandNatives.ThatSpellIsChargingWhenTheIconOfItsSeedHoldsChants`, `ScriptHandNatives.ClearingThePlayersChargingGivesTheChantsBackToTheSite` |
