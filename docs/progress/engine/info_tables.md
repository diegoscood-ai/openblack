# Info tables

Almost every number behind Black & White's objects, from a tree's wood to a miracle's cost or a creature's desires,
comes from one file of tables, `Scripts/info.dat`, loaded once when the game starts. The scripts' constant names come
from the same tables. A table is counted done here only when openblack reads it and its systems use it; how well each
system follows it is in that system's own domain.

**Progress: 7/36 done, 24 partial — 53%**

## Loading

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The tables are read once at start from the info file and shared by the whole game | done | `src/Parsers/InfoFile.cpp`, `Locator::infoConstants`, `Game::Initialize` |
| Each table's layout matches the game's, field by field | partial | `src/InfoConstants.h`; many fields are still unnamed |
| Things are looked up in the tables by their type names, ignoring case; an unknown name is logged and skipped | done | the info tables' lookups by name, case-insensitive (`src/InfoConstants.cpp`), as CREATE_ABODE and CREATE_FEATURE use them |
| The script language's constant names for objects and types come from the tables | done | `components/lhvmlang` (`ChlConstants.cpp`); test `ChlLanguage.ConstantsFromInfoTables` |

## Magic

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each miracle's general table (cost, timers, gestures, cast type, power-ups, hold) | done | `src/Magic`; tests `test/test_magic_tables.cpp`, `test/test_spell_chants.cpp`; see ../miracles/ |
| Each miracle family's own table (heal, teleport, forest, food, storm and tornado, shields, wood, water, flocks, creature spells) | partial | read; used miracle by miracle, see ../miracles/ |
| Magic effects and spell seeds (the orbs in the hand and the one-shot ones) | done | test `test/test_magic_tables.cpp`; see ../miracles/ |
| Spot visuals and effects | partial | the spot visuals' table is used by `psys::manager::CreateSpotVisual` (`src/Particles/PSysManager.cpp`); see ../rendering/ |
| Map shields and spell icons | partial | the spell icons' table is used by `src/Worship/WorshipSpellIcon.cpp` and `TownCentreSpellIcon.cpp`, the map shields' by `src/ECS/Physics/PhysicsObjects.cpp` only |

## Creatures

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each species' table and the differences between species | partial | the species tables are used widely; the per-species differences table is unused; see ../creature/ |
| Creature actions and the desire tables (desires, sources, dependencies, attributes, starting values) | partial | used by the creature's mind (`src/Creature`); see ../creature/desires.md |
| Development phases and their lengths | partial | the phases are used by `CreatureMindSystem`; the durations table is unused; see ../creature/ |
| Actions the creature copies from the player | partial | read by `src/Creature/CreatureMindTables.cpp`; see ../creature/ |
| Creature pens | todo | read, not used |

## People and towns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Abodes, town centres and storage pits | partial | used for building and placing them (`src/ECS/Abodes.cpp`, `src/ECS/Archetypes`); see ../building/ |
| Villagers, their tribes and their state table (animations and flags per state) | partial | the villagers' and tribes' tables and the state table are used (`src/ECS/VillagerAnimations.cpp`, `VillagerSpeed.cpp`, `MobileDrawing.cpp`); see ../villager/ |
| Special villagers | todo | read, not used |
| Jobs | todo | read, not used; see ../villager/ |
| The town, its desires and belief | partial | `src/ECS/Town` (desires, belief); see ../town/ |
| Worship sites, prayer sites and icons, worship site upgrades | partial | worship sites used (`src/Worship/WorshipSite.cpp`); prayer sites, icons and upgrades not; see ../worship/ |
| Alignment, influence, the player and reactions | partial | used (`src/ECS/Effects/Alignment.cpp`, `src/ECS/Influence`, `src/ECS/Effects/Reactions.cpp`); see ../worship/ |
| Dances | todo | read, not used |
| Totem statues | partial | read by `src/ECS/Archetypes/AbodeArchetype.cpp` when the totem is made; the totem's own behaviour: see ../worship/ |

## Nature and objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Trees, big forests and flowers | partial | trees and big forests used (`src/ECS/Trees.cpp`, `BigForestArchetype.cpp`); the flowers' table is not; see ../nature/ |
| Features, animated statics, mobile objects and mobile statics | partial | used to create and draw them (`src/ECS/Archetypes`, `src/CHLApi.cpp`); see ../nature/ and ../physics/ |
| Pots of food and wood | partial | used widely (`src/ECS/PotResource.cpp`, `ObjectDelivery.cpp`); see ../resources/ |
| Fields, field types and fish farms | partial | fields (`src/ECS/Fields.cpp`) and fish farms (`src/ECS/FishFarms.cpp`) used; see ../resources/ |
| Animals and their state table | partial | the animals' table and their state table used (`src/ECS/AnimalAI.cpp`, `AnimalFlee.cpp`); see ../animal/ |
| Furniture, scaffolds and single-cell fixed objects | partial | the scaffolds' table used (`src/ECS/Scaffolds.cpp`); furniture and single-cell fixed objects read, not used |
| Terrain materials (what each ground type does to objects and sounds) | partial | used for the surface sounds (`src/ECS/SeaCells.cpp`), the forest miracle and the storm; see ../terrain/ |
| Weather climates | done | `src/ECS/Weather/Climate.cpp`; see ../weather/ |
| Speed thresholds | partial | used to pick the walking clips (`src/ECS/VillagerAnimations.cpp`, `AnimalAnimations.cpp`) |

## Help, sound and play

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The help system, help spirits and help sprite guidance | partial | the help system's table and the help sprite guidance used (`src/Help/HelpSystem.cpp`, `src/Game.cpp`); the help spirits' table is not; see ../interface/ |
| The sound table | done | used by the audio (`src/Audio/Services/SoundMap.cpp`, the town trigger distances in `src/Game.cpp`); see ../audio/ |
| Arrows, script highlights and showing a town's needs | partial | script highlights (`src/ECS/ScriptHighlight.cpp`) and the town's needs (`src/ECS/ShowNeeds.cpp`) used; the arrows' table is not |
| The ball, football and playtime tables for the creature's games | todo | read, not used; see ../creature/ |
