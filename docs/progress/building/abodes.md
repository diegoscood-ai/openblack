# Abodes

Abodes are the homes of a town: each tribe has its own set, from tents and huts to shacks and large houses, and the
land scripts place them with food and wood in them. Villagers live in them with their families, and they light up and
smoke at night while people are in.

**Progress: 15/25 done, 2 partial — 64%**

How the original does it, in our wiki: [Buildings, building sites and towns (the building side)](../../bw1-notes/buildings.md).

## Placing and looks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each tribe has its own abodes (several sizes of home) and civic buildings, found by name | done | the abode info table by tribe (`src/InfoConstants.h`, `src/Enums.h`) |
| The land script places an abode in a town with its angle, size, food and wood | done | CREATE_ABODE in `src/LHScriptX/FeatureScriptCommands.cpp`, `src/ECS/Archetypes/AbodeArchetype.cpp` |
| An abode given an unknown town joins the nearest town | done | the nearest town in the list (`map_cells::FindNearestTownInList`, `src/ECS/Archetypes/AbodeArchetype.cpp`) |
| Abodes sit into the slope of the land; large civic buildings bend with it | done | an abode that does not follow the land sinks to the lowest ground under its corners, at most max(0.2 x its radius, 0.8) (`src/ECS/Archetypes/AbodeArchetype.cpp`); the big civic buildings morph with it |
| Abodes block the way of walkers with a round footprint | done | the building's bounding circle (`components::Fixed`, `src/ECS/Archetypes/AbodeArchetype.cpp`) and its map cells |
| Windows light up at night while someone is home | done | the window colour at night while someone is home (`src/3D/NightLights.cpp`), drawn by the rendering system; see ../sky/ for night |
| Chimneys smoke while someone is home, blown by the wind and the hand | done | `src/ECS/ChimneySmoke.cpp`, with the hand's wind (`chimney_smoke::UpdateHandWind`) |
| Abodes and lanterns light the ground round the town at night | done | the village lights stamped into the land cells (`src/3D/NightLights.cpp`, `Locator::villageLightSystem`) |
| Street lanterns of the town and the country, flickering and crackling | done | `src/ECS/Archetypes/StreetLanternArchetype.cpp`; the crackle in `src/Audio/Services/LanternSounds.cpp` |
| Snow settles on roofs in cold lands | todo | the weather keeps no snow cover (`src/ECS/Weather/Atmos.cpp`); See ../weather/ |
| Abodes cast a shadow at night by lantern light | todo | see ../rendering/shadows.md |
| A full abode shows how full it is (food and people) when the hand is over it | todo | the hand's building texts are not ported; See ../interface/ |
| Abodes are highlighted under the hand | todo | See ../hand/ |
| Footpaths go round abodes | done | an object the creature must avoid, put in the map cells, reroutes the footpaths round it (`src/ECS/MapCells.cpp`, `footpaths::RerouteFootpathsAroundObstacle`); `test/test_footpath_obstacles.cpp`; See ../terrain/ |
| Low-detail versions of buildings far away | n/a | openblack always draws the most detailed meshes. Our wiki differs: in this version of the original the level of detail loading is disabled, so models always draw their first level ([rendering-objects](../../bw1-notes/rendering-objects.md#villager-blobs-object-reflections-and-lod)); See ../rendering/ |

## Living in them

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An abode holds a set number of adults and children | done | the abode info's room for adults and children (`abode_villagers::GetRoomLeftForAdults`, `GetRoomLeftForChildren`, `src/ECS/Town/AbodeVillagers.cpp`), every villager housed through `AddVillagerToAbode` |
| Villagers go in by the door and are counted as present | done | arriving home and leaving (`abode_villagers::ArriveHome`, `LeaveHome`, `src/ECS/Villager/VillagerHome.cpp`) |
| Abodes keep food for dinner and wood | done | `Abode::foodAmount` and `woodAmount`, eaten from at home (`src/ECS/Villager/VillagerFood.cpp`) |
| The abode works out the food its family needs for dinner | todo | the housewife's dinner states are TODO rows of the state table |
| An abode knows its nearest drinking water | todo |  |
| Tapping an abode's roof calls its people out or sends them in | done | tapping an abode brings out the villagers inside (`abodes::InterfaceTap`, `villager::SetStateWhenTappedOnAbode`, `src/ECS/Villager/VillagerEmergency.cpp`); `test/test_villager_repair.cpp`. Our wiki differs: the tap only brings out those inside; it sends nobody in ([villagers](../../bw1-notes/villagers.md#repairs-the-tap-on-a-home-and-the-town-emergency)) |
| A disciple dropped by an abode moves into it | todo | nothing takes a dropped disciple into an abode; See ../villager/disciples.md |
| Villagers can hide in buildings in an emergency | todo | `GO_AND_HIDE_IN_NEARBY_BUILDING` is a TODO row of the state table; See ../town/emergencies_and_aggression.md |
| An abode is part of its player's influence through its town | done | `src/ECS/Influence/InfluenceSources.cpp`; See ../worship/ |
| Food and wood can be given to an abode by hand | partial | a held pot, tree or fence goes into an abode only as wood for its building site (`resource_stores::IsResourceStore`, `src/ECS/ResourceStores.cpp`); See ../resources/ |
| The creature can look at, kick or stomp on abodes, and learns from it | partial | the creature's stomp and kick give a home the blow a thrown thing gives (`src/ECS/Systems/Implementations/CreatureObjectActionSystem.cpp`); looking and learning: See ../creature/ |
