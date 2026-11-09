# Prayer power

The magic the worshippers make. Each worship site stores it in a battery that its dancers fill, and the miracles draw on
it to be summoned, kept going and powered up.

**Progress: 14/25 done, 0 chosen, 9 partial, 0 inferred — 85%**

How the original does it, in our wiki: [Magic: the core of the miracles](../../../magic/gestures.md).

## Making it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each dancer chants a set amount a turn, from its tribe's worship site table | done | `worship::site::Capacity` (`src/Worship/WorshipSite.cpp`): dancers times the tribe's chants per villager; test `WorshipTest.CapacityAndBattery` |
| The player's tribal power multiplies what is chanted; all sites use the same multiplier | done | the capacity multiplies by the player's tribal power (`players::MagicOf(player).tribalPower`, the same entry for every site, as the original's index 2); nothing raises a tribal power, and in the original every tribal power stays 1 ([the hand](../../../magic/hand-casting.md#the-tribal-power-column)); test `WorshipBattery.capacityAndSize` |
| The battery's size is its tribe's base plus a share for each dancer | done | `site::MaxBattery`: the tribe's battery plus a share per dancer; test `WorshipTest.CapacityAndBattery` |
| An idle site fills its battery up to its size | done | the end of the site's turn refills it with the boost (`site::ProcessSpellIcons`, `src/Worship/WorshipSite.cpp`, through the rule `magic::EndWorshipTurn`, `src/Magic/WorshipBattery.cpp`); tests `WorshipTest.EndOfTurnDanceIntensityAndBattery`, `WorshipBattery.idleSiteFillsItsBattery` |
| The dance speeds up as more is drawn and slows as the battery fills | partial 50% | the intensity is worked out each turn (`magic::EndWorshipTurn`, `src/Magic/WorshipBattery.cpp`, then `site::SetDanceIntensity`; test `WorshipBattery.drawingSpeedsTheDanceAndDrainsTheBattery`), but nothing plays the dance faster (`danceSpeed` is not read) and the dancers stand |
| Demand beyond what the dancers can give is strain, which stops the icons charging | done | `site::ProcessSpellIcons`: with strain above 0 the icons get nothing; test `WorshipTest.StrainIsDemandOverCapacity` |

## Spending it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Summoning a miracle from an icon charges its cost to create from the site | done | `icon::StartCharge` and the share each turn (`src/Worship/WorshipSpellIcon.cpp`), charged to the site (`site::UseChants`); `test/test_worship.cpp` |
| A running miracle draws its upkeep each turn, topping itself up to its safety level | done | a spell whose creator is an icon is kept going by its site (`creator::MaintainSpell`, `src/Magic/Core/SpellCreator.cpp`, to `icon::MaintainSpell`); tests `SpellChants.shieldIsMaintained`, `SpellChantRules.maintainedShield` |
| The icons charge between uses, sharing evenly what the site can give | done | `site::ProcessSpellIcons`: the lesser of available and needed, split between the charging icons; test `WorshipBattery.strainAndIconShare` |
| A reserve is kept for miracles already cast while any icon's seed is out | done | `site::AvailableForIcons`; the reserve is the info.dat integer read as a float, so about 0, as the original; test `WorshipTest.ReserveForMaintainingIsTheInfoDatBug` |
| A seed dropped or shaken off gives back what it holds to its site | partial 90% | `worship::ReturnSeedToItsSite` and `ApplySeedToObject` (`src/Worship/Worship.cpp`): the seed's chants back into the battery; no test pins it |
| Powering a miracle up costs more; any surplus goes back to the site | partial 90% | `seed::SetPowerUp` (`src/Magic/Core/SpellSeed.cpp`): a charge above the level's cost goes back to the site; no test pins it |
| A miracle drawing on a site shows a trail of light from the site (the mana path) | pending | the site's mana path sprite (`WorshipSite::CreateManaPathSprite` 0x77B2C0) is not ported; see [../rendering/light_beams.md](../rendering/light_beams.md) |
| Scripts' miracles (the neutral player's) cost nothing | done | the neutral player gives what is asked (`creator::MaintainSpell`, `src/Magic/Core/SpellCreator.cpp`); test `SpellChants.lightningNeutralPlayerRefills` |
| The prayer power a site holds shows in its altar's tooltip, and a charging icon shows its charge | done | the altar's tooltip with its site's battery (`hand_tooltips::PrayerEnergyToolTip`, test in `test/hand/test_hand_tool_tips.cpp`; [tooltips](../../../interface/tooltips.md)); the charge ring over a charging icon (`icon::UpdateChargingVisual`, `src/Worship/WorshipSpellIcon.cpp`); a display of it in the temple was not found in the exe |
| The player's statistics count the prayer power used | done | `site::UseChants` adds to the player's `chantsUsed`, written by the statistics (`src/Game/GameStats.cpp`); [../interface/statistics_counted.md](../interface/statistics_counted.md); `test/test_player_system.cpp` |
| The virtual influence past the border uses up the worship sites' spare prayer power | pending | the chants the virtual influence reads and uses are passed as 0 ([virtual influence](../../../magic/influence-and-alignment.md#virtual-influence)); the land scripts switch it off on Land 1 and on on Lands 2, 3 and 5; see [influence](influence.md) |

## Sacrifice

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A living thing dropped on the site's altar is sacrificed for prayer power | partial 80% | ported from raffclar PR #551: a villager, an animal, a tree or a dead tree let go on a worship site's altar (its worship totem) is offered (`altar_offering::Offer`, `src/ECS/AltarOffering.cpp`, from `HandApplyToObject.cpp`); its prayer power goes into the site's store and its sacrificed total; a villager is refused when the site is not built or it is indestructible; Land 2's sacrifice quest: [the_sacrifice.md](../story/silver_scrolls/the_sacrifice.md); the villager's offering is read in the exe ([the sacrifice](../../../magic/worship.md#a-sacrifice-at-the-altar)); the animal's and the tree's are raffclar's, not read there; tests `AltarSacrifice.*` |
| What a sacrifice gives is its kind's sacrifice value, from half to all of it by how much life it has left | partial 80% | ported from raffclar PR #551: `altar_sacrifice::SacrificeValue` = (life x 0.5 + 0.5) x the kind's `sacrificeValue` (`src/ECS/AltarSacrifice.cpp`), with a child's bonus for a villager; an animal's or a tree's is that times its life again; the villager's value is the original's `Object::GetSacrificeValue` 0x63AAF0 ((life × 0.5 + 0.5) × the info's +0xE4, read in the v1.42 bytes) and its child bonus 1.25 (0x752CD3); an animal's or a tree's extra life factor is not read in the exe; test `AltarSacrifice.ValueGoesFromHalfAtNoLifeToAllAtFullLife` |
| Sacrifice is an evil deed and moves the player's alignment | partial 80% | ported from raffclar PR #551: the death with reason Sacrifice (a villager) or half an animal's death (`alignment::UpdateForDeath`), and the creature is told the deed (`creature_mimic::Consider`); see [alignment](alignment.md); the villager's death and the deed are read in the exe; the animal's half is not |
| A sacrifice given by the hand sounds at the hand, and a villager then cries out as it dies (a man one of eight screams, a woman one of six; a child's sound and cry are never heard, a vanilla bug kept) | done | `altar_sacrifice::PlaySounds` (`src/ECS/AltarSacrifice.cpp`), called by the altar offering only when a hand gives the thing (`src/ECS/AltarOffering.cpp`); test `test/test_altar_sacrifice.cpp` |
| A villager sacrificed raises a light from the top of the altar | partial 90% | the object-appear spot visual at full magnitude, for the site's player, from the altar's point raised by its height (`src/ECS/AltarOffering.cpp`), as read in the exe (0x752D86); no test pins it |
| The prayer power a villager, an animal or a living tree gives floats up in red, for the player at this machine | partial 80% | a whole number at least three wide, rising from the land under the pointer (`src/ECS/AltarOffering.cpp`); the land point it rises from is inferred (the interface's altitude taken as on the ground, [our wiki](../../../magic/worship.md#the-sacrifice-at-the-altar)); no test pins it |
| A tree or a dead tree sacrificed leaves its fading ghost | done | the same 500 ms dissolving ghost a store's taken object leaves, at the pose the hand draws it in (`src/ECS/AltarOffering.cpp`); test `AltarSacrifice.TreesFadeAsGhostsAndAreNoDeed` |
| Scripts can read a player's sacrifice total | partial 90% | ported from raffclar PR #551: `GET_SACRIFICE_TOTAL` answers a worship site's sacrificed prayer power (`script::GetSacrificeTotal`, `src/Magic/Script/CHLWorship.cpp`); used by [The Sacrifice](../story/silver_scrolls/the_sacrifice.md); as the exe's `GScript::GetSacrificeTotal` 0x6FE960 (a site's +0xF4, else an error and 0); no test pins it |

## Cheats

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
