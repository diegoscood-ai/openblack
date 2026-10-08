# Disciples

A villager the player picks up and puts down on something becomes a disciple of that work and keeps at it for good:
on a field a farmer, in a forest a forester, at the sea a fisherman, at a building site a builder, by another villager a
breeder, in another town a missionary, at the workshop a craftsman, at the storage pit a trader, at the worship site a
worshipper.

**Progress: 1/21 done, 7 partial — 21%**

How the original does it, in our wiki: [Villagers: data, state machine and speed](../../bw1-notes/villagers.md).

## Making disciples

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Dropping a villager from the hand onto a place makes it a disciple of the work there | todo | the hand picks up and drops villagers, but the landing makes no disciple (TODO in `src/ECS/LivingPhysics.cpp`) |
| The disciple's work is chosen from what the drop point is (field, forest, sea, building site, workshop, storage pit, worship site, villager, other town) | todo | the disciple table is read (`src/ECS/Villager/VillagerDisciple.cpp`) but no drop point chooses a type |
| Only an adult in its own town (or a missionary in another) can be made a disciple | todo |  |
| A disciple shows its icon above its head when the hand is near | todo |  |
| The town counts its disciples of each kind | done | the town stats count the disciples by type (`town_stats::Compute`, `src/ECS/Town/TownStats.cpp`); no villager is made a disciple yet |
| Making a disciple moves the player's alignment as the town tables say | todo | See ../worship/ |
| Disciples interacted with by the hand ask what state they would take up | todo |  |
| Disciples with nothing to do look for their work, then wait | partial | `DISCIPLE_NOTHING_TO_DO` and `DiscipleDecideWhatToDo` are ported (`src/ECS/Villager/VillagerDisciple.cpp`), but dormant: nothing makes a disciple |
| Picking a disciple up and dropping it back in town frees it | todo | the hand's reset of a disciple is a TODO (`src/ECS/LivingPhysics.cpp`) |
| The town takes back its disciples when it changes owner | todo |  |

## Each discipline

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Farmer disciple: farms fields endlessly | partial | the farmer's job check is ported (`DiscipleDecideWhatToDo`), dormant |
| Forester disciple: fells trees for the store endlessly | partial | the forester's job check is ported (`DiscipleDecideWhatToDo`), dormant |
| Fisherman disciple: fishes endlessly | partial | the fisherman's job check is ported (`DiscipleDecideWhatToDo`), dormant |
| Builder disciple: builds and repairs sites | partial | the builder's job check and its full-site rule are ported (`DiscipleDecideWhatToDo`, `src/ECS/Villager/VillagerBuild.cpp`), dormant |
| Breeder disciple: wanders the town making babies with willing partners, the villagers reacting to him | todo | the breeder's check returns 0 and `BREEDER_DISCIPLE` is a TODO row |
| Missionary disciple: goes to another town and preaches, winning belief for the player | todo | `MISSIONARY_DISCIPLE` is a TODO row; the town turn's missionary step is a TODO |
| Craftsman disciple: keeps the workshop supplied with wood | todo | the craftsman's check calls the workshop desire, which is a TODO; see [../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md) |
| Trader disciple: carries goods between towns | todo | the trader's check returns 0; the trader's states are TODO rows |
| Worship disciple: worships at the site endlessly | partial | the worship disciple goes to worship (`DiscipleDecideWhatToDo`), dormant |
| Protection disciple | n/a | in the table but never made in the game |
| Change-house disciple: moves to the abode it is dropped by | partial | the change-house case and state 234 are ported (`DiscipleDecideWhatToDo`, `src/ECS/Villager/VillagerHome.cpp`), dormant |
| Villagers that come out of an arrival vortex given a flock become disciples from the vortex, standing in a crowd, until the script disbands the flock | todo | the vortex disciple's case is ported (`SCRIPT_IN_CROWD`, a TODO row), and nothing makes one; only Land 2's arrival gives one: [../story/portals_per_land.md](../story/portals_per_land.md#land-2-arrival); how a vortex throws villagers out: [../story/portals.md](../story/portals.md#arriving) |
