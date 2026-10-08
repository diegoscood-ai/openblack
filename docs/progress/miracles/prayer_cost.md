# Prayer power cost

What each miracle costs in prayer power: charging the seed, the prayer it starts with, the upkeep each turn and per
event, refills from whoever cast it, and how tribal power changes the cost and strength. How worshippers make prayer
power is in [../worship/](../worship/).

**Progress: 23/25 done, 0 partial — 92%**

How the original does it, in our wiki: [Miracles one by one](../../bw1-notes/miracles.md).

## Who pays

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A miracle from an icon is paid and topped up from its worship site's store | done | `creator::MaintainSpell` (`src/Magic/Core/SpellCreator.cpp`) asks `worship::icon::MaintainSpell`, which takes from the icon's site (`site::MaintainSpell`, `src/Worship/WorshipSite.cpp`) or, without a site, from the icon's own store. Only on lands whose scripts make worship sites (none on Land 1) |
| A player's miracle cast from a globe or dispenser seed with no icon gets no refill at all | done | `creator::MaintainSpell`: a player other than the neutral one gives 0; test `SpellChants.lightningPlayerLivesOnItsInitialChants` |
| The neutral player (scripts) gives all a miracle needs; a player with no store gives nothing | done | `creator::MaintainSpell` (the neutral player gives the whole amount, any other player 0); test `SpellChants.lightningNeutralPlayerRefills` |
| A creature's miracle uses its player's tribal power | done | `magic::AllocSpell` (`src/Magic/Core/Spell.cpp`) gives the spell its creator's player, and `GetTribalPower` uses that player's. The creature's own energy cost is in [creature_spells.md](creature_spells.md) |

## Charging a seed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A globe's seed comes fully charged for free | done | `one_off::CreateSpellIntoHand` (`src/Magic/Core/OneOffSpellSeed.cpp`) fills the seed's store with its whole need |
| An icon charges its seed up to the miracle's cost to create (base and each power-up level have their own) | done | `site::ProcessSpellIcons` shares the site's spare prayer among the charging icons, `icon::AddToChantStore` caps it at the level's cost (`src/Worship/WorshipSite.cpp`, `WorshipSpellIcon.cpp`); test `WorshipTest.ChantStoreKeepsTheExcessQuirk`. Only where worship sites exist |
| A seed's power is how much of its need it holds, at most full | done | `seed::GetPower` (`src/Magic/Core/SpellSeed.cpp`) |
| A power-up's extra cost is charged by the icon and any surplus is refunded to the site | done | `seed::SetPowerUp` gives a charge above the new level's cost back to the icon's site battery (`src/Magic/Core/SpellSeed.cpp`); the icon charges the new level through `icon::SetChargingPowerUp` |
| Only a seed made at an icon is topped up and refunds to its icon | done | Only a seed whose creator is a worship icon is maintained by it (`creator::MaintainSpell`) and refunds to it (to the icon's site battery: `worship::ReturnSeedToItsSite`, `src/Worship/Worship.cpp`); a loose seed's charge is lost |
| A dropped or scribbled-away seed returns what it holds to its worship site | done | `HandSystem::ApplyForceDropHeld` (`src/ECS/Systems/Implementations/HandSpellSeed.cpp`) calls `worship::ReturnSeedToItsSite`, which adds the store to the site's battery and deletes the seed |
| Scripts ask how much prayer power a miracle needs | done | `GET_MANA_FOR_SPELL`: `magic::script::GetManaForSpell` (`src/Magic/Script/CHLSpells.cpp`), the magic's cost to create |

## A miracle's prayer while it lives

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A cast miracle starts with its initial prayer times the seed's multiplier | done | `PrepareCast` in `src/Magic/Core/SpellSeed.cpp` (initial chants and the player timer both times the seed's multiplier) |
| Its safe level is five seconds of upkeep, at most its initial prayer and at least one event's cost | done | `chants::GetChantSafetyLevel` (`src/Magic/Core/Chants.cpp`) over the rule `magic::GetChantSafetyLevel` (`src/Magic/SpellChants.cpp`); tests `SpellChants.lightningSafetyLevel`, `SpellChantRules.safetyLevelIsFiveSecondsOfUpkeep` |
| Forest and the shields keep their initial prayer as the safe level | done | `chants::GetChantSafetyLevel` with `IsMaintainedSpell`, over `src/Magic/SpellChants.cpp`; tests `SpellChants.shieldIsMaintained`, `SpellChantRules.maintainedShield` |
| A recharged miracle refills its shortfall from its caster each turn | done | `chants::Recharge` in `base::CoreProcess` (`src/Magic/Core/Spell.cpp`) and the refill inside `chants::PayFor`, over `magic::Recharge` and `magic::PayFor` (`src/Magic/SpellChants.cpp`); tests `SpellChants.lightningNeutralPlayerRefills`, `SpellChantRules.refillingCasterHoldsTheSafetyLevel` |
| Each turn it pays its upkeep (at least 1), and each event its event cost | done | `chants::PayForOneTurn` (an upkeep of 0 pays nothing) and `chants::PayForOneEvent` (`src/Magic/Core/Chants.cpp`), over the rules in `src/Magic/SpellChants.cpp`; tests `SpellChants.lightningEventCost`, `SpellChantRules.eventCost` |
| Teleport and the shields divide their costs by the tribal power | done | `chants::PayFor` with `divideCostsByTribalPower`, over `src/Magic/SpellChants.cpp`; tests `SpellChants.shieldDividesByTribalPower`, `SpellChantRules.tribalPowerDividesCost` |
| Its strength is how full it is times tribal power, the seed's power and its multiplier; at nothing it ends | done | `chants::GetSpellStrength` over `magic::GetSpellStrength` (`src/Magic/SpellChants.cpp`), and `base::CoreProcess` closes the spell at strength 0; tests `SpellChants.strengthEdges`, `SpellChantRules.strengthEdges` |
| A held miracle's prayer runs down with its upkeep while held | done | `ProcessMaintainRequest` (`src/Magic/Core/Spell.cpp`) pays every live spell's upkeep, the ones held in the hand included |
| Food and wood pay per unit dropped (the first drop brings more), even when it falls in water, and are never refilled | done | `magic::ResourceEvent` and the per-event `chants::PayFor` in `src/Magic/Spells/SpellResource.cpp`; see [food.md](food.md), [wood.md](wood.md) |
| Pressing food or wood again needs enough prayer left for a first drop | done | `magic::HasEnoughChantsForResourceRecast` (`src/Magic/Spells/SpellResource.cpp`) |
| The miracle table's costs per miracle and level (initial, create, per event, per turn, per shield hit) come from the game's data | done | `src/Magic/MagicTables.cpp`; tests `SpellChants.realInfoDat`, `MagicTables.realInfoDat` (with the game's data) |

## Tribal power and display

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Tribal power is the product of the miracle's tribes' powers, kept between 0.5 and 100 | done | `magic::GetTribalPower` (`src/Magic/MagicTables.cpp`); test `MagicTables.tribalPower` |
| A player's tribal powers grow from the tribes worshipping them | todo | Nothing writes `PlayerMagic::tribalPower` (`src/ECS/Components/PlayerMagic.h`): every tribe counts 1.0. Our wiki differs: it notes (inferred) that nothing in the original writes them either ([villagers](../../bw1-notes/villagers.md#home-food-sleep-homeless-and-age)) |
| Spending prayer sends motes of light from the caster to the miracle, one per 100 prayer power, when the caster has a worship site | todo | the chant rules report each payment to the caster (`SpellCasterInterface::OnChantsSpent`, `src/Magic/SpellChants.h`) and `chants::Context::createSpellPoint` takes it, but `ChantContextOf` (`src/Magic/Core/Spell.cpp`) never sets it and nothing draws the motes; see [../rendering/light_beams.md](../rendering/light_beams.md) |
