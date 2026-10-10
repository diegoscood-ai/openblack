# How a computer god uses miracles

A computer god casts the miracles its towns and temple give it, as a person does: it picks the best miracle of a kind
for the job, makes sure it has the prayer power, charges the icon if needed and casts it at the right place. The land
scripts also cast miracles in a god's name, from where its hand is, for staged attacks.

**Progress: 3/38 done, 9 partial — 20%**

See [../miracles/](../miracles/) for each miracle, and [ai.md](ai.md) for when a god wants to cast.

## Which miracles it has

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A god has the miracles of the towns that believe in it, as the person does | partial | towns hold their miracles (`magic::script::CreateNewTownSpell`) and enable them for their owner (`PlayerMagic::remainder`, `src/ECS/Components/PlayerMagic.h`); no god uses them; see [../worship/](../worship/) |
| Its miracles sit as icons in its temple, charged from its prayer power | todo | worship sites are made for every player (`magic::script::CreateWorshipSite`), but nothing charges or takes a god's icons; see [../miracles/casting_and_globes.md](../miracles/casting_and_globes.md) |
| It needs prayer power like any player | todo | each worship site keeps its prayer power, but nothing spends a god's |
| It looks for an icon needing charge and charges it up when its worshippers can | todo | no computer god mind in our tree |
| Scripts can top up an icon's prayer power, as Khazar's is for six wood miracles | done | `GAME_SET_MANA` (`magic::script::GameSetMana`, `src/Magic/Script/CHLWorship.cpp`) sets the worship site's prayer power |
| Scripts ask how much prayer power a miracle costs | done | `GET_MANA_FOR_SPELL` (`magic::script::GetManaForSpell`): the miracle's cost to create |

## Choosing a miracle

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It picks its best aggressive miracle for hurting a town or people | todo | no computer god mind in our tree |
| It picks its best aggressive miracle against a creature | todo | no computer god mind in our tree |
| It picks its best compassionate miracle | todo | no computer god mind in our tree |
| It picks its best impressive miracle, and judges how well one would impress where it is cast | todo | no computer god mind in our tree |
| It picks its best shield, judging whether a shield is possible at all | todo | no computer god mind in our tree |
| For each kind (food, heal, water, wood, shield, physical shield, aggressive, compassionate, impressive) it asks whether it has enough prayer power | todo | no computer god mind in our tree |
| It weighs how well a miracle would work at a place before casting it there | todo | no computer god mind in our tree |
| A desire to cast is weighed with how long until the miracle is ready | todo | no computer god mind in our tree |

## Casting

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It casts a miracle either from an icon or as if by gesture | todo | no computer god mind in our tree |
| Miracles can be cast in another player's name, belonging to them | done | `magic::CastAtPos` and `magic::CastAtObject` take the casting player (`magic::creator::OfPlayer`); the debug miracle caster casts as any player (`src/Debug/MiraclesCaster.cpp`) |
| A miracle a computer god casts is timed by its own timer from the miracle tables, not the person's | partial | the timer is read (`magic::GetTimerWhenComputerPlayerCasting`, `src/Magic/MagicTables.cpp`; shown in `src/Debug/Magic.cpp`), but nothing casts as a computer god |
| Food on a town's storehouse, on a worship site, or on its starving creature | todo | no computer god mind in our tree |
| Wood on a building site, on the workshop, or on a storehouse | todo | no computer god mind in our tree |
| Water on a burning building | todo | no computer god mind in our tree |
| Heal at its worship site, and on its hurt creature | todo | no computer god mind in our tree |
| A shield over an unprotected building, a town object, or against a belief attack | todo | no computer god mind in our tree |
| A physical shield against things thrown at it | todo | no computer god mind in our tree |
| Aggressive miracles at a town it wants to destroy | todo | no computer god mind in our tree |
| Aggressive miracles at a creature it is attacking | todo | no computer god mind in our tree |
| Impressive miracles at a town it wants to win | todo | no computer god mind in our tree |
| Miracles to meet a town's needs (food, wood) to win it | todo | no computer god mind in our tree |
| A miracle on its own creature for a laugh (starts at weight 0) | todo | no computer god mind in our tree |
| A creature that sees a computer god's ordinary miracle reacts to it but doesn't learn it, unlike a person's | todo | no computer god mind in our tree; even the person's casts are not watched by creatures yet: `CreatureMindSystem::SeeMiracle` is only called by the debug spawner (see [../creature/learning_by_observation.md](../creature/learning_by_observation.md)) |

## Miracles the scripts cast for a god

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts cast a miracle from a god's hand position at a target, with radius, time and curl | partial | `SPELL_AT_POS` (`magic::script::SpellAtPos`) casts it as the neutral player from the point given; the god's hand position is a stub that gives the origin, and the miracle belongs to no one |
| Lethys's fake attacks on the second land: three level-two fireballs | partial | dormant: the scripts that do it never run, as the story never reaches that land (`LOAD_MAP` is empty); `SPELL_AT_POS` (`magic::script::SpellAtPos`) casts it as the neutral player from the point given; see [lethys.md](lethys.md) |
| Nemesis's fireball volleys on Khazar's land from the vortex, curling left, straight and right | partial | dormant: the scripts that do it never run, as the story never reaches that land (`LOAD_MAP` is empty); `SPELL_AT_POS` (`magic::script::SpellAtPos`) casts it as the neutral player from the point given; see [khazar.md](khazar.md) |
| Explosions on temples and wonders from a point above them | partial | dormant: the scripts that do it never run, as the story never reaches that land (`LOAD_MAP` is empty); `SPELL_AT_POS` (`magic::script::SpellAtPos`) casts it as the neutral player from the point given |
| Nemesis's physical shield over his last town every 30 seconds | partial | dormant: the scripts that do it never run, as the story never reaches that land (`LOAD_MAP` is empty); `SPELL_AT_POS` (`magic::script::SpellAtPos`) casts it as the neutral player from the point given (a physical shield); see [nemesis.md](nemesis.md) |
| A miracle on a creature (strength on Lethys's creature, invisibility on Nemesis's) | partial | dormant: the scripts that do it never run, as the story never reaches that land (`LOAD_MAP` is empty); `SPELL_AT_THING` casts on an object and the creature miracles exist ([../miracles/creature_spells.md](../miracles/creature_spells.md)), but the gods' creatures are never loaded (`LOAD_CREATURE` is a stub) |

## What it remembers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It records the miracles other players cast near it, by kind | partial | each player keeps its last cast and a count of casts by type (`PlayerMagic::lastCast`, `castCount`); no god reads them |
| It reacts to a player casting at it by using its creature as a diversion | todo | no computer god mind in our tree |
| It records the damage each player's miracles did to what is its own | todo | no record of the damage done by each player's miracles in our tree |
