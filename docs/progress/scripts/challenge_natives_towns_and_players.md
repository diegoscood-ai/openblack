# Challenge natives: towns, players and rival gods

The challenge scripts' functions for towns (belief, desires, stores, buildings, worship), players (alignment, alliances, land balance) and the rival gods the computer plays. The game has 464 of these functions in all; the language statement each comes from is shown in italics, and "called" counts are calls in the shipped `challenge.chl`. How the virtual machine runs them is in [../engine/script_vm.md](../engine/script_vm.md); what each challenge is about is in [../story/](../story/).

**Progress: 8/49 done, 1 partial — 17%**

## Used by the shipped scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Gives a player's good or evil alignment: *alignment of player ‹zero›* (called 2 times in 2 scripts) | done | `GetAlignment` in `src/CHLApi.cpp`: `ecs::effects::alignment::Get` for the player (`src/ECS/Effects/Alignment.cpp`); `test/test_alignment_system.cpp` |
| Has a town build a building at a position, with a desire: *build building at ‹position› desire ‹desire›* (called 28 times in 9 scripts) | done | `BuildBuilding`: `ecs::building_sites::ForceBuildingOfPlannedAtPos` (`src/ECS/Town/BuildingSites.cpp`), the town's planned building at the point, with the desire times five; `test/test_building_sites.cpp` |
| Gives how much food or wood a store holds: *get resource ‹resource› in ‹container›* (called 11 times in 7 scripts) | todo | `GetResource` logs "not implemented" and pushes nought |
| Puts food or wood into a store: *add resource ‹resource› ‹quantity› to ‹container›* (called 5 times in 3 scripts) | partial | `AddResource`: `ecs::object_resources::AddResource` on an object, pushing what it took; a villager's own resources (Land 1's builders) are not ported |
| Takes food or wood out of a store: *remove resource ‹resource› ‹quantity› from ‹container›* (called 9 times in 4 scripts) | todo | `RemoveResource` logs "not implemented" and pushes nought |
| Gives a town's or villager's belief in a player: *get ‹object› belief for player ‹player›* (called 21 times in 9 scripts) | todo | `BeliefForPlayer` logs "not implemented" and pushes nought |
| Sets an object's belief in a player relative to others: *set ‹object› player ‹player› relative belief ‹belief›* (called once in 1 script) | todo | `ObjectRelativeBelief` logs "not implemented" |
| Moves a rival god's hand to a position at a speed: *move computer player ‹player› to ‹position› speed ‹speed› [with fixed height]* (called 68 times in 26 scripts) | todo | `MoveComputerPlayerPosition` logs "not implemented": there are no computer players |
| Turns a rival god (computer player) on or off: *enable/disable computer player ‹player›* (called 3 times in 3 scripts) | todo | `EnableDisableComputerPlayer311` logs "not implemented" |
| Gives where a rival god's hand is: *computer player ‹player› position* (called 8 times in 6 scripts) | todo | `GetComputerPlayerPosition` logs "not implemented" and pushes a zero position |
| Puts a rival god's hand at a position at once: *set computer player ‹player› position to ‹position› [with fixed height]* (called 10 times in 7 scripts) | todo | `SetComputerPlayerPosition` logs "not implemented" |
| Sets a town's or villager's belief in a player: *set ‹object› player ‹player› belief ‹belief›* (called 5 times in 4 scripts) | todo | `SetPlayerBelief` logs "not implemented" |
| Boosts or lowers one of a town's desires: *set ‹object› desire boost ‹desire› ‹boost›* (called 10 times in 4 scripts) | done | `SetTownDesireBoost`: `ecs::town_desire::ScriptSetTownDesireBoost` (`src/ECS/Town/TownDesire.cpp`), with the original's range checks and re-sort; `test/test_town_desire.cpp` |
| Whether a rival god is ready: *computer player ‹player› ready* (called 23 times in 12 scripts) | todo | `ComputerPlayerReady` logs "not implemented" and pushes false |
| Turns a rival god on or off (second form): *enable/disable computer player ‹player›* (called 9 times in 7 scripts) | todo | `EnableDisableComputerPlayer345` logs "not implemented" |
| Creates a random villager of a tribe at a position: *create random villager of tribe ‹tribe› at ‹position›* (called once in 1 script) | todo | `CreateRandomVillagerOfTribe` logs "not implemented" and pushes 0 |
| Sets what a scaffold builds, its size and whether it destroys what is under it: *set ‹object› building properties ‹type› size ‹size› [destroys when placed]* (called 8 times in 5 scripts) | done | `SetScaffoldProperties`: `ecs::scaffolds::SetScaffoldProperties` (`src/ECS/Scaffolds.cpp`), its type, size and destroy flag |
| Sets a rival god's personality (a named set of behaviours) on or off: *set computer player ‹player› personality ‹aspect› ‹probability›* (called 25 times in 5 scripts) | todo | `SetComputerPlayerPersonality` logs "not implemented" |
| Makes a rival god do an action at once: *force computer player ‹player› action ‹action› [‹obj1›] [‹obj2›]* (called 11 times in 6 scripts) | todo | `ForceComputerPlayerAction` logs "not implemented" |
| Adds an action to a rival god's queue: *queue computer player ‹player› action ‹action› [‹obj1›] [‹obj2›]* (called 4 times in 1 script) | todo | `QueueComputerPlayerAction` logs "not implemented" |
| Gives the town with a number from the land script: *get town with id ‹id›* (called 28 times in 14 scripts) | todo | `GetTownWithId` logs "not implemented" and pushes 0 |
| Makes a villager a disciple of a job, with or without a sound: *set ‹object› disciple ‹disciple type› [with sound]* (called 3 times in 3 scripts) | todo | `SetDisciple` logs "not implemented" |
| Hands a rival god back to its own thinking: *release computer player ‹player›* (called 26 times in 18 scripts) | todo | `ReleaseComputerPlayer` logs "not implemented" |
| Sets how fast a rival god acts: *set computer player ‹player› speed ‹speed›* (called 4 times in 3 scripts) | todo | `SetComputerPlayerSpeed` logs "not implemented" |
| Gives a rival god as an object: *get computer player ‹player›* (called 2 times in 2 scripts) | todo | `CallComputerPlayer` logs "not implemented" and pushes 0 |
| Whether a town may build a worship site: *enable/disable ‹object› build worship site* (called 6 times in 2 scripts) | done | `SetCanBuildWorshipsite`: `magic::script::SetCanBuildWorshipsite` (`src/Magic/Script/CHLWorship.cpp`), for a town or the temple's heart |
| Sets a rival god's attitude to a player: *set computer player ‹player1› attitude to player ‹player2› to ‹attitude›* (called 4 times in 2 scripts) | todo | `SetComputerPlayerAttitude` logs "not implemented" |
| Sets how allied two players are: *set player ‹player1› ally with player ‹player2› percentage ‹percentage›* (called 5 times in 5 scripts) | todo | `SetPlayerAlly` logs "not implemented" |
| Gives how many adults a town or container holds: *adult size of ‹container›* (called once in 1 script) | todo | `IdAdultSize` logs "not implemented" and pushes nought |
| Gives how many adults a building can hold: *adult capacity of ‹container›* (called 2 times in 1 script) | todo | `ObjectAdultCapacity` logs "not implemented" and pushes nought |
| Gives how many worshippers have died in a town: *get worship deaths in ‹town›* (called 2 times in 1 script) | done | `GetTownWorshipDeaths`: the town's deaths from worship (`TownDeaths`, written when a villager dies) |
| Gives how many towns a player has: *get player ‹player› town total* (called 4 times in 1 script) | todo | `GetPlayerTownTotal` logs "not implemented" and pushes nought |
| Gives a town's totem: *get totem statue in ‹town›* (called 2 times in 2 scripts) | todo | `GetTotemStatue` logs "not implemented" and pushes 0 |
| Gives the time since a player last attacked a town: *get time since player ‹player› attacked ‹town›* (called 2 times in 1 script) | todo | `GetTimeSinceObjectAttacked` logs "not implemented" and pushes nought |
| Gives the total health of a town's buildings and villagers: *get building and villager health total in ‹town›* (called 2 times in 1 script) | todo | `GetTownAndVillagerHealthTotal` logs "not implemented" and pushes nought |
| Gives how much has been sacrificed at a worship site: *get ‹worship site› sacrifice total* (called 2 times in 1 script) | todo | `GetSacrificeTotal` logs "not implemented" and pushes nought |
| Clears a rival god's actions: *clear computer player ‹player› actions* (called once in 1 script) | todo | `GameClearComputerPlayerActions` logs "not implemented" |

## Not used by the shipped scripts

The game has these but no shipped script calls them; mods and fan-made challenges can.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Sets a player's alignment: *set the player's alignment (no statement in the language)* (not called by the shipped scripts) | done | `SetAlignment`: `ecs::effects::alignment::AddClamped`: the value is added to the player's alignment, as the original does despite the name; out of -1 to 1 nothing happens. Our wiki differs: the original adds the value to the alignment rather than setting it ([magic](../../bw1-notes/magic.md#player-alignment-galignment-gplayer-0x60-srcecseffectsalignment-componentsalignment)) |
| Gives a player's town nearest a position within a radius: *get nearest town at ‹value› for player ‹value› radius ‹value›* (not called by the shipped scripts) | done | `GetNearestTownOfPlayer`: `ecs::map_cells::FindPlayerTownAtPos`, only that player's towns within the radius, handed to the script as a found thing |
| Holds back one of a rival god's behaviours: *set computer player ‹value› suppression ‹value› ‹value›* (not called by the shipped scripts) | todo | `SetComputerPlayerSuppression` logs "not implemented" |
| Finds a building of a kind in a town, built at least so far: *get building ‹value› in ‹value› min built ‹value› [excluding scripted]* (not called by the shipped scripts) | todo | `CallBuildingInTown` logs "not implemented" and pushes 0 |
| Gives a rival god's attitude to a player: *get computer player ‹player1› attitude to player ‹player2›* (not called by the shipped scripts) | todo | `GetComputerPlayerAttitude` logs "not implemented" and pushes nought |
| Loads a rival god's personality from a file: *load computer player ‹value› personality ‹value›* (not called by the shipped scripts) | todo | `LoadComputerPlayerPersonality` logs "not implemented" |
| Saves a rival god's personality to a file: *save computer player ‹value› personality ‹value›* (not called by the shipped scripts) | todo | `SaveComputerPlayerPersonality` logs "not implemented" |
| Gives how many a building can hold: *capacity of ‹container›* (not called by the shipped scripts) | todo | `ObjectCapacity` logs "not implemented" and pushes nought |
| Gives how allied two players are: *get player ‹player1› ally percentage with player ‹player2›* (not called by the shipped scripts) | todo | `GetPlayerAlly` logs "not implemented" and pushes nought |
| Sets a player's land balance: *set ‹value› land balance ‹value›* (not called by the shipped scripts) | todo | `SetLandBalance` logs "not implemented" |
| Sets how much an object's belief counts: *set ‹value› belief scale ‹value›* (not called by the shipped scripts) | todo | `SetObjectBeliefScale` logs "not implemented" |
| Adds food or wood to a building site (unconfirmed): *add for building ‹value› to ‹value›* (not called by the shipped scripts) | todo | `GameAddForBuilding` logs "not implemented" |
