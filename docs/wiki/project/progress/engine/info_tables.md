# Info tables

Almost every number behind Black & White's objects, from a tree's wood to a miracle's cost or a creature's desires,
comes from one file of tables, `Scripts/info.dat`, loaded once when the game starts. The scripts' constant names come
from the same tables. A table is counted done here only when openblack reads it and its systems use it; how well each
system follows it is in that system's own domain.

**Progress: 11/35 done, 0 chosen, 23 partial, 0 inferred — 80%**

## Loading

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The tables are read once at start from the info file and shared by the whole game | done | `src/Parsers/InfoFile.cpp`, `Locator::infoConstants`, `Game::Initialize`; tests `CreatureInfo.realInfoDatSpeciesSpeedsBodyRatesAndCasting`, `CreatureInfo.ASpeciesRowsSpeedsBodyRatesAndCastingAreReadFromWhereTheFileKeepsThem` |
| Each table's layout matches the game's, field by field | partial 60% | `src/InfoConstants.h`; many fields are still unnamed |
| Things are looked up in the tables by their type names, ignoring case; an unknown name is logged and skipped | done | the info tables' lookups by name, case-insensitive (`src/InfoConstants.cpp`), as CREATE_ABODE and CREATE_FEATURE use them; tests `InfoNames.*` (`test/test_info_names.cpp`) |
| The script language's constant names for objects and types come from the tables | done | `components/lhvmlang` (`ChlConstants.cpp`); test `ChlLanguage.ConstantsFromInfoTables` |

## Magic

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each miracle's general table (cost, timers, gestures, cast type, power-ups, hold) | done | `src/Magic`; tests `test/test_magic_tables.cpp`, `test/test_spell_chants.cpp`; see ../miracles/ |
| Each miracle family's own table (heal, teleport, forest, food, storm and tornado, shields, wood, water, flocks, creature spells) | partial 87% | read; used miracle by miracle, see ../miracles/; follows the miracles it points at: [miracles](../INDEX.md#miracles) at 87% (2026-11-07) |
| Magic effects and spell seeds (the orbs in the hand and the one-shot ones) | done | test `test/test_magic_tables.cpp`; see ../miracles/ |
| Spot visuals and effects | partial 80% | the spot visuals' table is used by `ParticleSystemInterface::StartSpotVisual` (`src/Particles/ParticleManager.cpp`); see ../rendering/ |
| Map shields and spell icons | partial 70% | the spell icons' table is used by `src/Worship/WorshipSpellIcon.cpp` and `TownCentreSpellIcon.cpp`, the map shields' by `src/ECS/Systems/Implementations/DynamicsSystem.cpp` only |

## Creatures

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each species' table and the differences between species | partial 80% | the species tables are used widely; the per-species differences table gives the species' alignment change scale (`src/ECS/Effects/Alignment.cpp`; test `test/test_alignment.cpp`); see ../creature/ |
| Creature actions and the desire tables (desires, sources, dependencies, attributes, starting values) | partial 80% | used by the creature's mind (`src/Creature`); see ../creature/desires.md |
| Development phases and their lengths | partial 70% | the phases are used by `CreatureMindSystem`; the durations table is unused; see ../creature/ |
| Actions the creature copies from the player | partial 80% | read by `src/Creature/CreatureMindTables.cpp`; see ../creature/ |

## People and towns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Abodes, town centres and storage pits | partial 85% | used for building and placing them (`src/ECS/Abodes.cpp`, `src/ECS/Archetypes`); see ../building/; follows the buildings it points at: [building](../INDEX.md#building) at 85% (2026-11-07) |
| Villagers, their tribes and their state table (animations and flags per state) | partial 86% | the villagers' and tribes' tables and the state table are used (`src/ECS/VillagerAnimations.cpp`, `VillagerSpeed.cpp`, `MobileDrawing.cpp`); see ../villager/; follows the villagers it points at: [villager](../INDEX.md#villager) at 86% (2026-11-07) |
| Special villagers | partial 50% | the 48 named people's rows give a new villager its name, sex and age (`MakeSpecialVillager` and the name pick in `src/ECS/Villager/VillagerCore.cpp`; test `test/test_special_villagers.cpp`); the rows' job, marriage, tribe, pet and face are not read |
| Jobs | pending | read, not used; see ../villager/ |
| The town, its desires and belief | partial 80% | `src/ECS/Town` (desires, belief); see ../town/ |
| Worship sites, prayer sites and icons, worship site upgrades | partial 50% | worship sites used (`src/Worship/WorshipSite.cpp`); the prayer site's row gives its object type and impressiveness (`src/ECS/MapCells.cpp`, `src/ECS/ObjectImpressiveness.cpp`); the prayer icons' and the upgrades' tables are not used; see ../worship/ |
| Alignment, influence, the player and reactions | partial 92% | used (`src/ECS/Effects/Alignment.cpp`, `src/ECS/Influence`, `src/ECS/Effects/Reactions.cpp`); see ../worship/; follows the worship rows it points at: [worship](../INDEX.md#worship) at 92% (2026-11-07) |
| Dances | done | read; the dance system takes each row's file and whether it starts on its own (`DanceSystem::Create`, `SavedWorldLoad.cpp`), the reactions their duration (`VillagerReactions.cpp`), the creature's dances the fewest dancers and the beats (`CreatureMindDance.cpp`), the artefacts the most dancers (`ArtefactSystem.cpp`) and the creature's dance its impressiveness (`VillagerCreatureDance.cpp`); these are the frozen exe's only reads of the table (`xref.py readers 0xCC4B80-0xCC5CB0` and the users of `Dance::GetInfo` 0x50C460, 2026-11-07: rows' +0x10, +0x14, +0x98, +0xA4, +0xA8, +0xAC and the file name at +0x58); the area required (+0x9C, +0xA0) is read by neither: [not in the original](../../not-in-the-original.md#engineinfo_tables); see ../scripts/dance_scripts.md |
| Totem statues | partial 92% | read by `src/ECS/Archetypes/AbodeArchetype.cpp` when the totem is made; the totem's own behaviour: see ../worship/; follows the worship rows it points at: [worship](../INDEX.md#worship) at 92% (2026-11-07) |

## Nature and objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Trees, big forests and flowers | partial 80% | trees, big forests and flowers used (`src/ECS/Trees.cpp`, `BigForestArchetype.cpp`, `FlowersArchetype.cpp`); see ../nature/ |
| Features, animated statics, mobile objects and mobile statics | partial 80% | used to create and draw them (`src/ECS/Archetypes`, `src/CHLApi.cpp`); see ../nature/ and ../physics/ |
| Pots of food and wood | partial 90% | used widely (`src/ECS/PotResource.cpp`, `ObjectDelivery.cpp`); see ../resources/ |
| Fields, field types and fish farms | partial 90% | fields (`src/ECS/Fields.cpp`) and fish farms (`src/ECS/FishFarms.cpp`) used; see ../resources/ |
| Animals and their state table | partial 89% | the animals' table and their state table used (`src/ECS/AnimalAI.cpp`, `AnimalFlee.cpp`); see ../animal/; follows the animals it points at: [animal](../INDEX.md#animal) at 89% (2026-11-07) |
| Furniture, scaffolds and single-cell fixed objects | partial 40% | the scaffolds' table used (`src/ECS/Scaffolds.cpp`); furniture and single-cell fixed objects read, not used |
| Terrain materials (what each ground type does to objects and sounds) | partial 50% | used for the surface sounds (`src/ECS/SeaCells.cpp`), the forest miracle and the storm; see ../terrain/ |
| Weather climates | done | `src/ECS/Weather/Climate.cpp`; see ../weather/; test `WeatherTest.AClimateStartsWithTheSeasonsLeastRain` |
| Speed thresholds | done | used to pick the walking clips (`src/ECS/Systems/Implementations/VillagerAnimate.cpp` through `animation::SpeedLimitsIndex`, `src/ECS/AnimalAnimations.cpp`); the frozen exe's only readers of the table (`xref.py readers 0xD38348-0xD38438`, 2026-11-07) are the walking clips' choices, and openblack reads the same rows and fields there: `Villager::MoveToPosAnimation` 0x423484..0x4234FA both limits of rows 0 and 1, the cow's, sheep's and pig's `MoveAnimation` row 2's walk limit (+0x40), the horse's row 3's walk and run limits (+0x58, +0x5C), the lion's, tiger's, leopard's and wolf's rows 6 to 9's run limit (+0xA4, +0xBC, +0xD4, +0xEC); rows 4 and 5 (sheep, pig) are read by neither; tests `VillagerAnimation.VillagersWalkRunOrSprintBySpeedAndSex`, `VillagerAnimation.EachGaitGoesUpToItsLimitIncluded`, `VillagerAnimation.OnlyTheMensSexWalksByTheMensLimits` (`test/test_villager_animation.cpp`) |

## Help, sound and play

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The help system, help spirits and help sprite guidance | done | the help system's table and the help sprite guidance used (`src/Help/HelpSystem.cpp`, `src/Game.cpp`); the help spirits' table is read, not used, as in the original, where its only read, a row's object type as each of the two spirits is made, sets a map-list flag of an object that never goes on the map ([not in the original](../../not-in-the-original.md#engineinfo_tables), 2026-11-07); see ../interface/ |
| The sound table | done | used by the audio (`src/Audio/SoundMap.cpp`, the town trigger distances in `src/Game.cpp`); see ../audio/; test `AlignmentMusic.TownTriggerOfTheGameSynthetic` |
| Arrows, script highlights and showing a town's needs | done | script highlights (`src/ECS/ScriptHighlight.cpp`) and the town's needs (`src/ECS/ShowNeeds.cpp`) used; the arrows' table is read and never used, as in the original, where no arrow is ever made ([not in the original](../../not-in-the-original.md#engineinfo_tables), 2026-11-07) |
| The ball, football and playtime tables for the creature's games | partial 30% | the ball's table makes the football and gives its physics (`src/ECS/Archetypes/BallArchetype.cpp`, `DynamicsSystem.cpp`, `CreatureMindBeliefs.cpp`); the football positions', playtime and the creature's ball and football tables are read, not used; see ../creature/ |
