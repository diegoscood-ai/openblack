# Challenge natives: miracles, influence and weather

The challenge scripts' functions for miracles (casting, giving, finding and changing them), rewards, prayer power, influence, the vortex and storms and climates. The game has 464 of these functions in all; the language statement each comes from is shown in italics, and "called" counts are calls in the shipped `challenge.chl`. How the virtual machine runs them is in [../engine/script_vm.md](../engine/script_vm.md); what each challenge is about is in [../story/](../story/).

**Progress: 28/38 done, 1 partial — 75%**

## Used by the shipped scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Changes the inner and outer radius (and calm) of a shield, storm or influence: *set ‹obj› properties inner ‹inner› outer ‹outer› [calm ‹calm›]* (called 43 times in 25 scripts) | partial | `ChangeInnerOuterProperties`: `ecs::script_containers::ChangeInnerOuter` (`src/ECS/ScriptContainers.cpp`) sets a flock's domain radius, flock distance and calm; the weather thing's version is not ported. Our wiki differs: the original changes a flock's radii and calm, or a storm's ([villagers](../../bw1-notes/villagers.md#script-flocks-and-containers)) |
| Puts a ring of influence around an object for a player: *create influence on ‹target› [radius ‹radius›] ‹zero› ‹anti›* (called 19 times in 9 scripts) | done | `InfluenceObject`: `magic::script::InfluenceObject` (`src/Magic/Script/CHLInfluence.cpp`), a ring on the object through `influence::CreateRingOnObject`; `test/test_influence.cpp` |
| Puts a ring of influence at a position for a player: *create influence at ‹position› [radius ‹radius›] ‹zero› ‹anti›* (called 29 times in 23 scripts) | done | `InfluencePosition`: `influence::CreateRing` at the position |
| Gives a player's influence at a position: *influence ‹player› ‹raw› at ‹position›* (called 10 times in 7 scripts) | done | `GetInfluence`: `influence::CalculatePlayerInfluence`, with allies unless raw |
| Changes a storm's temperature, rain, snow, cloud cover and how fast its rain falls: *set ‹storm› properties degrees ‹temperature› rainfall ‹rainfall› snowfall ‹snowfall› overcast ‹overcast› fallspeed ‹fallspeed›* (called 15 times in 8 scripts) | done | `ChangeWeatherProperties`: `weather_thing::SetWeatherProperties` (`src/Magic/Script/CHLWeather.cpp`); `test/test_weather.cpp` |
| Changes how often a storm throws sheet and forked lightning: *set ‹storm› properties sheetmin ‹sheetmin› sheetmax ‹sheetmax› forkmin ‹forkmin› forkmax ‹forkmax›* (called 18 times in 8 scripts) | done | `ChangeLightningProperties`: `weather_thing::SetLightningProperties` |
| Changes how long a storm lasts and how it fades: *set ‹storm› properties time ‹duration› fade ‹fade time›* (called 16 times in 8 scripts) | done | `ChangeTimeFadeProperties`: `weather_thing::SetTimeFadeProperties` |
| Changes a storm's clouds, their shade and their height: *set ‹storm› properties clouds ‹num clouds› shade ‹blackness› height ‹elevation›* (called 15 times in 8 scripts) | done | `ChangeCloudProperties`: `weather_thing::SetCloudProperties` |
| Casts a miracle on an object, from a place, with a radius, time and curl: *cast ‹spell› spell on ‹target› from ‹from› radius ‹radius› time ‹duration› curl ‹curl›* (called 4 times in 3 scripts) | done | `SpellAtThing`: `magic::script::SpellAtThing` (`src/Magic/Script/CHLSpells.cpp`), cast by the neutral player on the object |
| Casts a miracle at a position, from a place, with a radius, time and curl: *cast ‹spell› spell at ‹target› from ‹from› radius ‹radius› time ‹duration› curl ‹curl›* (called 51 times in 16 scripts) | done | `SpellAtPos`: `magic::script::CastSpellAtPos`, the neutral player with no cast check |
| Finds a miracle of a kind already at a position, within a radius: *get spell ‹spell› at ‹position› radius ‹radius›* (called 9 times in 3 scripts) | done | `SpellAtPoint`: the first spell of that kind within the radius (shields are not looked up) |
| Changes a miracle's radius: *set ‹object› radius ‹radius›* (called once in 1 script) | todo | `SetMagicRadius` logs "not implemented" |
| Gives the player a reward (a miracle, a creature or an object), dropping it from the sky or not: *reward ‹reward› at ‹position› [from sky]* (called 8 times in 5 scripts) | todo | `CreateReward` logs "not implemented" and pushes 0 |
| Gives the player a reward in a town: *reward ‹reward› in ‹town› at ‹position› [from sky]* (called 6 times in 3 scripts) | todo | `CreateRewardInTown` logs "not implemented" and pushes 0 |
| Whether a player has a miracle: *spell ‹spell› for player ‹player›* (called 41 times in 1 script) | done | `HasPlayerMagic`: `players::HasMagicTypeEverBeenEnabled` |
| Gives a player influence everywhere (or takes it back): *enable/disable player ‹player› virtual influence* (called 4 times in 4 scripts) | todo | `SetVirtualInfluence` logs "not implemented" |
| Fades a vortex out: *start ‹vortex› fade out* (called 4 times in 4 scripts) | done | `VortexFadeOut`: `ecs::vortex::StartFadeOut` (`src/ECS/Vortex.cpp`); `test/test_vortex.cpp` |
| Sets a vortex's town, the flock it gathers, the flock's position, distance and radius: *set ‹vortex› properties town ‹town› flock position ‹position› distance ‹distance› radius ‹radius› flock ‹flock›* (called 2 times in 2 scripts) | done | `VortexParameters`: `ecs::vortex::SetParameters`, the vortex's town and flock parameters |
| Sets how much prayer power a store or player has: *set ‹object› mana ‹mana›* (called 9 times in 4 scripts) | done | `GameSetMana`: `worship::site::SetMana` on the thing's worship site (`src/Magic/Script/CHLWorship.cpp`) |
| Sets a miracle's properties for a time: *set ‹object› magic properties ‹magic type› [time ‹duration›]* (called once in 1 script) | done | `SetMagicProperties`: `worship::dispenser::SetMagicProperties` |
| Whether an object is under a miracle: *‹object› affected by spell ‹spell›* (called 2 times in 2 scripts) | todo | `IsAffectedBySpell` logs "not implemented" and pushes false |
| Puts a miracle inside an object (a dispenser, a chest) or takes it out: *enable/disable spell ‹magic type› in ‹object›* (called 14 times in 8 scripts) | done | `SetMagicInObject` (`src/Magic/Script/CHLWorship.cpp`): a town's held miracles through `worship::town::AddMagicTypesHeld` or `RemoveMagicTypesHeld`; anything that is not a town gets the original's "Object should be town" |
| Pauses or restarts the climates' weather: *enable/disable climate weather* (called 4 times in 2 scripts) | done | `PauseUnpauseClimateSystem`: `climate::SetClimateSystemEnabled` |
| Gives the prayer power a miracle costs: *get mana for spell ‹spell›* (called 4 times in 3 scripts) | done | `GetManaForSpell`: the miracle's cost to create from the info tables |
| Ends every storm within a radius of a position: *delete all weather at ‹position› radius ‹radius›* (called 2 times in 2 scripts) | done | `KillStormsInArea`: `storms::KillStormsInArea`; `test/test_storm.cpp` |
| Gives a player's prayer power: *get ‹worship site› mana total* (called 2 times in 2 scripts) | done | `GetMana`: the worship site's prayer power |
| Stops a player charging a miracle: *clear player ‹player› spell charging* (called 2 times in 2 scripts) | done | `ClearPlayerSpellCharging`: `worship::player::CancelAllSpellsCharging` |

## Not used by the shipped scripts

The game has these but no shipped script calls them; mods and fan-made challenges can.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Gives a player a miracle or takes it away: *enable/disable spell ‹value› for player ‹value›* (not called by the shipped scripts) | done | `SetPlayerMagic`: `players::SetMagicTypeEnabled` |
| Whether an object was cast by another: *‹spell instance› cast by ‹caster›* (not called by the shipped scripts) | todo | `ObjectCastByObject` logs "not implemented" and pushes false |
| Whether there is wind magic at a position (unconfirmed): *wind magic at a position (no statement in the language)* (not called by the shipped scripts) | todo | `IsWindMagicAtPos` logs "not implemented" and pushes false |
| Gives the time since a player last cast a miracle: *get player ‹player› time since last spell cast* (not called by the shipped scripts) | done | `PlayerSpellCastTime`: the seconds since the player's last cast, the largest float without one |
| Gives the last miracle a player cast: *get player ‹player› last spell cast* (not called by the shipped scripts) | done | `PlayerSpellLastCast`: the player's last cast's miracle |
| Gives where a player last cast a miracle: *last player ‹player› spell cast position* (not called by the shipped scripts) | done | `GetLastSpellCastPos`: the world point of the player's last cast |
| Whether a player is charging a miracle: *player ‹value› spell charging* (not called by the shipped scripts) | done | `IsSpellCharging`: `worship::player::AnySpellCharging` |
| Whether a player is charging a given miracle: *player ‹value› spell ‹value› charging* (not called by the shipped scripts) | done | `IsThatSpellCharging`: `worship::player::IsThatSpellCharging` |
| Sets whether a player's miracles resist the wind: *enable/disable player ‹value› wind resistance* (not called by the shipped scripts) | todo | `SetPlayerWindResistance` logs "not implemented" |
| Gives whether a player's miracles resist the wind: *a player's wind resistance (no statement in the language)* (not called by the shipped scripts) | todo | `GetPlayerWindResistance` logs "not implemented" and pushes nought |
| Pauses or restarts the climates making storms: *enable/disable climate create storms* (not called by the shipped scripts) | done | `PauseUnpauseStormCreationInClimateSystem`: `climate::SetStormCreationEnabled` |
