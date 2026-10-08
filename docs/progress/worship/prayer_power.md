# Prayer power

The magic the worshippers make. Each worship site stores it in a battery that its dancers fill, and the miracles draw on
it to be summoned, kept going and powered up.

**Progress: 12/22 done, 5 partial — 66%**

How the original does it, in our wiki: [Magic: the core of the miracles](../../bw1-notes/magic.md).

## Making it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each dancer chants a set amount a turn, from its tribe's worship site table | done | `worship::site::Capacity` (`src/Worship/WorshipSite.cpp`): dancers times the tribe's chants per villager; test `WorshipTest.CapacityAndBattery` |
| The player's tribal power multiplies what is chanted; all sites use the same multiplier | partial | the capacity multiplies by the player's tribal power (`players::MagicOf(player).tribalPower`, the same entry for every site), but nothing works the tribal powers out: they stay 1 |
| The battery's size is its tribe's base plus a share for each dancer | done | `site::MaxBattery`: the tribe's battery plus a share per dancer; test `WorshipTest.CapacityAndBattery` |
| An idle site fills its battery up to its size | done | the end of the site's turn refills it with the boost (`site::ProcessSpellIcons`, `src/Worship/WorshipSite.cpp`, through the rule `magic::EndWorshipTurn`, `src/Magic/WorshipBattery.cpp`); tests `WorshipTest.EndOfTurnDanceIntensityAndBattery`, `WorshipBattery.idleSiteFillsItsBattery` |
| The dance speeds up as more is drawn and slows as the battery fills | partial | the intensity is worked out each turn (`magic::EndWorshipTurn`, `src/Magic/WorshipBattery.cpp`, then `site::SetDanceIntensity`; test `WorshipBattery.drawingSpeedsTheDanceAndDrainsTheBattery`), but nothing plays the dance faster (`danceSpeed` is not read) and the dancers stand |
| Demand beyond what the dancers can give is strain, which stops the icons charging | done | `site::ProcessSpellIcons`: with strain above 0 the icons get nothing; test `WorshipTest.StrainIsDemandOverCapacity` |

## Spending it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Summoning a miracle from an icon charges its cost to create from the site | done | `icon::StartCharge` and the share each turn (`src/Worship/WorshipSpellIcon.cpp`), charged to the site (`site::UseChants`) |
| A running miracle draws its upkeep each turn, topping itself up to its safety level | done | a spell whose creator is an icon is kept going by its site (`creator::MaintainSpell`, `src/Magic/Core/SpellCreator.cpp`, to `icon::MaintainSpell`) |
| The icons charge between uses, sharing evenly what the site can give | done | `site::ProcessSpellIcons`: the lesser of available and needed, split between the charging icons |
| A reserve is kept for miracles already cast while any icon's seed is out | done | `site::AvailableForIcons`; the reserve is the info.dat integer read as a float, so about 0, as the original; test `WorshipTest.ReserveForMaintainingIsTheInfoDatBug` |
| A seed dropped or shaken off gives back what it holds to its site | done | `worship::ReturnSeedToItsSite` and `ApplySeedToObject` (`src/Worship/Worship.cpp`): the seed's chants back into the battery |
| Powering a miracle up costs more; any surplus goes back to the site | done | `seed::SetPowerUp` (`src/Magic/Core/SpellSeed.cpp`): a charge above the level's cost goes back to the site |
| Scripts' miracles (the neutral player's) cost nothing | done | the neutral player gives what is asked (`creator::MaintainSpell`, `src/Magic/Core/SpellCreator.cpp`) |
| The prayer power the player has is shown (in the temple and on the icons) | partial | the charge ring over a charging icon (`icon::UpdateChargingVisual`, `src/Worship/WorshipSpellIcon.cpp`); nothing shows it in the temple; see `../interface/` |
| The player's statistics count the prayer power used | done | `site::UseChants` adds to the player's `chantsUsed`, written by the statistics (`src/Game/GameStats.cpp`); [../interface/statistics_counted.md](../interface/statistics_counted.md) |
| Prayer power can pay for influence away from the citadel (unconfirmed how it's used in the story) | todo | the virtual influence is not ported (`SET_VIRTUAL_INFLUENCE` is a stub) |

## Sacrifice

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A living thing dropped on the site's altar is sacrificed for prayer power | todo | a worship totem does not take a held villager or animal yet (`src/ECS/HeldApply.cpp`, `src/ECS/Systems/Implementations/HandApplyToObject.cpp`); Land 2's sacrifice quest: [the_sacrifice.md](../story/silver_scrolls/the_sacrifice.md) |
| What a sacrifice gives is its kind's sacrifice value, from half to all of it by how much life it has left (unconfirmed that the scale is its life) | todo |  |
| Sacrifice is an evil deed and moves the player's alignment | todo | see [alignment](alignment.md) |
| Scripts can read a player's sacrifice total | todo | `GET_SACRIFICE_TOTAL` is a stub in `src/CHLApi.cpp`; used by [The Sacrifice](../story/silver_scrolls/the_sacrifice.md) |

## Cheats

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Infinite prayer power | partial | each site's infinite flag (`WorshipSite::infiniteChants`, read by `site::Available`), set from the debug Miracles window (`src/Debug/Miracles.cpp`); test `WorshipTest.InfiniteChantsCheat` |
| Free upkeep for running miracles | partial | each site's free maintenance flag (`site::MaintainSpell`), set from the debug Miracles window |
