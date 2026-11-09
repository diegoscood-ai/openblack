# Info tables

Almost every number behind Black & White's objects, from a tree's wood to a miracle's cost or a creature's desires,
comes from one file of tables, `Scripts/info.dat`, loaded once when the game starts. The scripts' constant names come
from the same tables. A table is counted done here only when openblack reads it and its systems use it; how well each
system follows it is in that system's own domain.

**Progress: 7/36 done, 25 partial — 54%**

## Loading

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The tables are read once at start from the info file and shared by the whole game | done | `src/Parsers/InfoFile.cpp`, `Locator::infoConstants`, `Game::Initialize` |
| Each table's layout matches the game's, field by field | partial 60% | `src/InfoConstants.h`; many fields are still unnamed; re-measured 2026-10-21: many fields are still unnamed |
| Things are looked up in the tables by their type names, ignoring case; an unknown name is logged and skipped | done | the info tables' lookups by name, case-insensitive (`src/InfoConstants.cpp`), as CREATE_ABODE and CREATE_FEATURE use them |
| The script language's constant names for objects and types come from the tables | done | `components/lhvmlang` (`ChlConstants.cpp`); test `ChlLanguage.ConstantsFromInfoTables` |

## Magic

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each miracle's general table (cost, timers, gestures, cast type, power-ups, hold) | done | `src/Magic`; tests `test/test_magic_tables.cpp`, `test/test_spell_chants.cpp`; see ../miracles/ |
| Each miracle family's own table (heal, teleport, forest, food, storm and tornado, shields, wood, water, flocks, creature spells) | partial 80% | read; used miracle by miracle, see ../miracles/; re-measured 2026-10-21: read and used; the rest per the domain's own rows |
| Magic effects and spell seeds (the orbs in the hand and the one-shot ones) | done | test `test/test_magic_tables.cpp`; see ../miracles/ |
| Spot visuals and effects | partial 80% | the spot visuals' table is used by `ParticleSystemInterface::StartSpotVisual` (`src/Particles/ParticleManager.cpp`); see ../rendering/; re-measured 2026-10-21: read and used; the rest per the domain's own rows |
| Map shields and spell icons | partial 70% | the spell icons' table is used by `src/Worship/WorshipSpellIcon.cpp` and `TownCentreSpellIcon.cpp`, the map shields' by `src/ECS/Systems/Implementations/DynamicsSystem.cpp` only; re-measured 2026-10-21: read; part of the table is unused (see the notes) |

## Creatures

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each species' table and the differences between species | partial 70% | the species tables are used widely; the per-species differences table is unused; see ../creature/; re-measured 2026-10-21: read; part of the table is unused (see the notes) |
| Creature actions and the desire tables (desires, sources, dependencies, attributes, starting values) | partial 80% | used by the creature's mind (`src/Creature`); see ../creature/desires.md; re-measured 2026-10-21: read and used; the rest per the domain's own rows |
| Development phases and their lengths | partial 70% | the phases are used by `CreatureMindSystem`; the durations table is unused; see ../creature/; re-measured 2026-10-21: read; part of the table is unused (see the notes) |
| Actions the creature copies from the player | partial 80% | read by `src/Creature/CreatureMindTables.cpp`; see ../creature/; re-measured 2026-10-21: read and used; the rest per the domain's own rows |
| Creature pens | pending | read, not used |

## People and towns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Abodes, town centres and storage pits | partial 80% | used for building and placing them (`src/ECS/Abodes.cpp`, `src/ECS/Archetypes`); see ../building/; re-measured 2026-10-21: read and used; the rest per the domain's own rows |
| Villagers, their tribes and their state table (animations and flags per state) | partial 80% | the villagers' and tribes' tables and the state table are used (`src/ECS/VillagerAnimations.cpp`, `VillagerSpeed.cpp`, `MobileDrawing.cpp`); see ../villager/; re-measured 2026-10-21: read and used; the rest per the domain's own rows |
| Special villagers | pending | read, not used |
| Jobs | pending | read, not used; see ../villager/ |
| The town, its desires and belief | partial 80% | `src/ECS/Town` (desires, belief); see ../town/; re-measured 2026-10-21: read and used; the rest per the domain's own rows |
| Worship sites, prayer sites and icons, worship site upgrades | partial 40% | worship sites used (`src/Worship/WorshipSite.cpp`); prayer sites, icons and upgrades not; see ../worship/; re-measured 2026-10-21: worship sites used; prayer sites, icons and upgrades not |
| Alignment, influence, the player and reactions | partial 80% | used (`src/ECS/Effects/Alignment.cpp`, `src/ECS/Influence`, `src/ECS/Effects/Reactions.cpp`); see ../worship/; re-measured 2026-10-21: read and used; the rest per the domain's own rows |
| Dances | partial 80% | read; the dance system takes each row's file and whether it starts on its own (`DanceSystem::Create`); see ../scripts/dance_scripts.md; re-measured 2026-10-21: read and used; the rest per the domain's own rows |
| Totem statues | partial 80% | read by `src/ECS/Archetypes/AbodeArchetype.cpp` when the totem is made; the totem's own behaviour: see ../worship/; re-measured 2026-10-21: read and used; the rest per the domain's own rows |

## Nature and objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Trees, big forests and flowers | partial 70% | trees and big forests used (`src/ECS/Trees.cpp`, `BigForestArchetype.cpp`); the flowers' table is not; see ../nature/; re-measured 2026-10-21: read; part of the table is unused (see the notes) |
| Features, animated statics, mobile objects and mobile statics | partial 80% | used to create and draw them (`src/ECS/Archetypes`, `src/CHLApi.cpp`); see ../nature/ and ../physics/; re-measured 2026-10-21: read and used; the rest per the domain's own rows |
| Pots of food and wood | partial 90% | used widely (`src/ECS/PotResource.cpp`, `ObjectDelivery.cpp`); see ../resources/; re-measured 2026-10-21: used widely |
| Fields, field types and fish farms | partial 90% | fields (`src/ECS/Fields.cpp`) and fish farms (`src/ECS/FishFarms.cpp`) used; see ../resources/; re-measured 2026-10-21: fields and fish farms used |
| Animals and their state table | partial 80% | the animals' table and their state table used (`src/ECS/AnimalAI.cpp`, `AnimalFlee.cpp`); see ../animal/; re-measured 2026-10-21: read and used; the rest per the domain's own rows |
| Furniture, scaffolds and single-cell fixed objects | partial 40% | the scaffolds' table used (`src/ECS/Scaffolds.cpp`); furniture and single-cell fixed objects read, not used; re-measured 2026-10-21: the scaffolds' table used; furniture and single-cell fixed objects read, not used |
| Terrain materials (what each ground type does to objects and sounds) | partial 50% | used for the surface sounds (`src/ECS/SeaCells.cpp`), the forest miracle and the storm; see ../terrain/; re-measured 2026-10-21: used for surface sounds, the forest miracle and the storm only |
| Weather climates | done | `src/ECS/Weather/Climate.cpp`; see ../weather/ |
| Speed thresholds | partial 90% | used to pick the walking clips (`src/ECS/VillagerAnimations.cpp`, `AnimalAnimations.cpp`); re-measured 2026-10-21: used to pick the walking clips |

## Help, sound and play

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The help system, help spirits and help sprite guidance | partial 60% | the help system's table and the help sprite guidance used (`src/Help/HelpSystem.cpp`, `src/Game.cpp`); the help spirits' table is not; see ../interface/; re-measured 2026-10-21: the help system and help sprite guidance used; the help spirits' table not |
| The sound table | done | used by the audio (`src/Audio/SoundMap.cpp`, the town trigger distances in `src/Game.cpp`); see ../audio/ |
| Arrows, script highlights and showing a town's needs | partial 60% | script highlights (`src/ECS/ScriptHighlight.cpp`) and the town's needs (`src/ECS/ShowNeeds.cpp`) used; the arrows' table is not; re-measured 2026-10-21: highlights and town needs used; the arrows' table not |
| The ball, football and playtime tables for the creature's games | pending | read, not used; see ../creature/ |
