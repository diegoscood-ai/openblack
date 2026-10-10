# Villager tools and carried items

What a villager holds in its hand: the tool of the job it is doing (axe, fishing rod, crook, spade, hammer, saw or
mallet), the wood or food it is carrying, and the football. Tools are only drawn props; the only thing a villager ever
lets fall as a real object is a log of the wood it carries. The load itself (how much food and wood, the storage pit
trips, the slowdown) is in [jobs.md](jobs.md); a thrown villager's flight is in
[../physics/thrown_living.md](../physics/thrown_living.md).

openblack: villagers carry their loads and draw their tools and loads in their hands (`CarriedObjectFor` in
`src/ECS/Villager/VillagerResources.cpp`, `src/ECS/CarriedProps.cpp`), and a villager thrown, launched or killed lets
its wood fall as a log (`villager::CreateDroppedResource`). A script cannot yet set what a villager holds.

**Progress: 56/66 done, 2 partial — 86%**

How the original does it, in our wiki: [Villagers: data, state machine and speed](../../bw1-notes/villagers.md).

## The objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A villager can hold one of fourteen objects: axe, fishing rod, crook, saw, bag, ball, hammer, heavy mallet, scythe, spade, a plain piece of wood, and a log of evergreen, fruit tree or hardwood; or nothing | done | the carried object ids (`SkeletalAnimation::carriedObject`, `src/ECS/VillagerAnimations.cpp`, `src/ECS/Villager/VillagerResources.cpp`) |
| Each object is one mesh from the mesh pack, the same for every tribe: the "O_" objects of `Data/AllMeshes.h` (axe 342, bag 343, ball 344, evergreen branch 347, fruit branch 348, hardwood branch 349, crook 354, fishing rod 355, hammer 367, heavy mallet 378, saw 383, scythe 384, spade 390, wood in hand 406) | done | one prop mesh per object, the same for every tribe (`k_PropMeshes`, `CarriedObjectMesh`, `src/ECS/CarriedProps.cpp`) |
| The pack's second set of tools (the "U_" axe, bag, ball, crook, fishing rod, hammer, mallet, saw, scythe, spade) is never held: info.dat uses them as furniture scenery | done | only the "O_" meshes are props (`src/ECS/CarriedProps.cpp`); the furniture's placing is undetermined |
| The logs, wheat and dead fish "in hand" meshes of the pack (372, 405, 370) are not villager objects either: a villager carrying food shows a bag, never fish or wheat | done | food shows as the bag (`CarriedObjectFor`, `src/ECS/Villager/VillagerResources.cpp`) |

## Which tool each job uses

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Foresters hold an axe from the moment they head for the forest, while they look for a tree and while they chop, and when they go to a big forest; finishing work puts it away | done | the state rows' carried object (`state_info::CarriedObject`) through `SetStateCarriedObject` (`src/ECS/Villager/VillagerResources.cpp`), in the forester's ported states |
| Fishermen hold a fishing rod on the way to their fishing spot and while they fish | done | the fishing states' row (`SetStateCarriedObject`) |
| Shepherds hold a crook through all their work: looking for a flock, taking control of it, leading it to water, food and home, waiting for it and fetching strays; the slaughter itself is not drawn | todo | the shepherd's states are TODO rows |
| Farmers hold a spade on the way to the field and while they dig up the crop, but sow empty-handed | done | the farmer's states' rows (`src/ECS/Villager/VillagerFarmer.cpp`) |
| Builders, at each stroke of building, pick one of three clips at random and hold its tool: hammering with a hammer, sawing with a saw, or the sledgehammer swing with a heavy mallet; an into-clip already started keeps its own tool | done | one of the three clips at random sets the hammer, the saw or the heavy mallet, unless the carried object is locked (`src/ECS/VillagerAnimations.cpp`) |
| Housewives carry a bag from the storage pit to their home (picked up with the pot-on-the-head clip, walked home with the carrying-pot clip) | n/a | the housewife's states are TODO rows. Our wiki differs: the housewife's day, her trips to the storage pit included, is dead code in this version of the original ([villagers](../../bw1-notes/villagers.md#pregnancy-and-births)) |
| Villagers fighting a fire with water hold a bag while they throw the water; fetching the water and beating the flames are done empty-handed | done | the water-fighting rows give the bag (`SetStateCarriedObject`); in this version a villager fighting with water gives up at once (`src/ECS/Systems/Implementations/VillagerFire.cpp`) |
| A footballer picking up the ball for a restart holds a ball and runs with it until it puts it down; footballers hold nothing while playing, celebrating, watching or doing the wave | todo | no football; see play_and_gossip.md; [../town/football.md](../town/football.md#the-player-and-the-creature) |
| Traders, missionaries, worship suppliers, workshop suppliers and builders on their way to the site have no tool of their own: they show their load (a bag of food or a piece of wood) | partial | builders on their way show their load (`CarriedObjectFor`); the traders', missionaries' and suppliers' states are TODO rows |
| Breeder disciples and children hold nothing, and leaders have no tool | done | children and leaders hold nothing (no state row gives them a tool); there is no craftsman job |
| Disciples doing a job go through that job's states, so they hold its tool (a forester disciple has the axe, and so on) | partial | a disciple goes through its job's states and so would hold its tool, but no disciple is made yet; See disciples.md |
| The scythe is never held: no state and no code gives it | done | no state row gives the scythe |
| Villagers taking wood from a rock, a tree or a pot play the chopping clip with nothing in their hands | done | `TAKE_WOOD_FROM_TREE` and `TAKE_WOOD_FROM_POT` with the chopping clip and nothing held; the rock's rows are TODO |

## When the tool or load shows

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| What a villager holds is worked out again each time it picks a new animation: its load first, then the state it is heading for, then the state it is in; a state that names an object (or "nothing") overrides the one before | done | `CarriedObjectFor`: the load, then the final state's row, then the top state's (`src/ECS/Villager/VillagerResources.cpp`), at each clip change (`src/ECS/VillagerAnimations.cpp`) |
| The load shows when the villager holds more than 50 wood (a piece of wood or a log) or else more than 100 food (a bag); smaller loads are carried but not seen | done | `minWoodToShowGraphic` and `minFoodToShowGraphic` (`CarriedObjectFor`) |
| A villager holding both shows the wood if it has more than 50, even when it has more food | done | the wood first (`CarriedObjectFor`) |
| A builder whose next state is building never shows a bag of food | done | the building final state shows no bag (`CarriedInput::finalIsBuilding`) |
| A badly wounded villager (life at or below the crawling level) shows no load, but the tool of its job state still shows | done | at or below `lifeWhenCrawlsWounded` no load (`CarriedObjectFor`) |
| The tool and the load disappear while the villager decides what to do next, has nothing to do, gossips, mourns, inspects or points at something, is scared stiff, confused, poisoned, on fire, fainting or weak on the ground, flees an object or a predator, flies, lands, drowns, dies or is being eaten | done | the state rows' "nothing" (`SetStateCarriedObject`) |
| At worship the villager holds nothing: walking to the site, dancing, praying, resting at the altar and eating there; the same for dancing outside worship and the artifact dance | done | the worship and dance rows give nothing |
| At home the villager is not drawn at all (sleeping, eating, cooking, housework), so neither is what it holds; sleeping on the floor, eating dinner, giving birth and making love all hold nothing | done | a villager at home is hidden, so its prop is not drawn (`src/ECS/CarriedProps.cpp`) |
| Walking home, going for a drop-off, fleeing a creature or panicking, the villager keeps showing its load | done | the rows that keep the load (`CarriedObjectFor`) |
| A villager under a script keeps the object it had when the script took it over, as long as the script holds it or plays an animation on it | done | the script states keep the current object (`src/ECS/VillagerAnimations.cpp`) |

## Drawing and animation

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The object is drawn fixed to the grip bone at the end of the villager's arm (bone 15 of its skeleton), turning with it through every frame of the animation | done | linked to bone 15 of the pose, the grip at the end of its -X arm (`src/ECS/CarriedProps.cpp`) |
| It is drawn only with the villager: not while the player's hand holds the villager, and not in a state whose animation is hidden (inside homes, hiding at the worship site) | done | not while hidden or in the hand (`src/ECS/CarriedProps.cpp`) |
| A villager walking with any object except a saw, hammer or ball uses the carrying walk clip; running or sprinting with one uses the carrying run clip; men and women share both | done | the carrying walk and run clips for any object but the saw, the hammer and the ball (`src/ECS/VillagerAnimations.cpp`) |
| A wounded villager crawls or limps instead, whatever it carries | done | the wounded clips come first (`src/ECS/VillagerAnimations.cpp`) |
| The landing clip has a variant for landing on the feet while carrying, but landing itself always holds nothing, so the plain one is always played (outside scripts) | done | `VillagerLandedClip` with nothing carried (`src/ECS/VillagerAnimations.cpp`) |
| Picking up and putting down loads have their own clips: picking up sticks at the storage pit, putting them down at a building site, picking up and putting down a bag; a drop-off at the pit always uses the bag clip, even for wood | done | the states' own clips from info.dat and the animation table (`src/ECS/VillagerAnimationTable.h`) |

## Sounds of tool use

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Chopping a tree sounds two axe blows and two axe releases per clip (blows at about 1.5 s and 3.6 s) | done | the clips' sound events (`src/Audio/Services/AnimationSounds.cpp`); see ../audio/sound_effects.md |
| Sawing alternates the forward and backward saw sounds, each stroke stopping the other's | done | the saw's forward and back sounds, each stopping the other (`src/Audio/Services/AnimationSounds.cpp`) |
| Both the hammering and the sledgehammer clips play the sledgehammer sound once per clip; the separate hammer sound is never used by a villager | done | the clips' sound events (`src/Audio/Services/AnimationSounds.cpp`) |
| Fishing plays one cast sound per clip | done | the clips' sound events (`src/Audio/Services/AnimationSounds.cpp`) |
| Sowing and digging up crops are silent: the digging sounds belong to two farmer clips the farming states never play | done | the farming states play the silent clips |
| Kicks in football each play the kick sound | todo | no football; See play_and_gossip.md |

## Picked up, thrown, hurt or killed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Picking a villager up with the hand takes nothing from it: it keeps its wood and food while held, and a gentle put-down leaves them with it | done | the pick-up takes nothing (`ecs::living::InterfaceSetInMagicHand`, `src/ECS/LivingPhysics.cpp`); a gentle put-down lands it with its load |
| A villager the hand lets go of so that it flies, rather than being set down, lets its wood fall as a log that flies on beside it with the villager's speed | done | `villager::CreateDroppedResource` from `src/ECS/Physics/FromHand.cpp`; see ../physics/thrown_living.md |
| Anything else that launches a villager does the same first: being knocked by a flying object or an explosion, thrown or spat out by a creature, or taken by a tornado or vortex | done | the same from the knock and the other launches (`src/ECS/LivingPhysics.cpp`) |
| A log falls only when the villager shows a load or a tool and holds more than 50 wood; a villager holding nothing visible keeps its wood through the throw | done | `DroppedLogFor`: a shown object and more than 50 wood (`src/ECS/Villager/VillagerResources.cpp`) |
| Food is never dropped by a throw: the villager keeps it and still has it when it lands | done | only the wood falls (`CreateDroppedResource`) |
| Tools are never dropped, lost or picked up: they are only drawn, and come back as soon as the villager returns to its work | done | tools are only the drawn prop (`src/ECS/CarriedProps.cpp`) |
| Every death lets the log fall where the villager is (if it shows something and holds more than 50 wood): starving, old age, drowning, chanting, a miracle or spell, a fall, being sacrificed, being eaten by an animal, and a script letting go of a villager whose life has run out | done | `VillagerDead` calls `CreateDroppedResource` (`src/ECS/Villager/VillagerDeath.cpp`); See death.md |
| A villager eaten by a creature drops nothing | done | a villager the creature eats is deleted without a death drop (`ecs::life::Kill`, `src/ECS/Life.cpp`) |
| On death everything else the villager carried, food and the rest of the wood, is lost; it never becomes a pile | done | `DropWood` and `DropFood` after the log (`src/ECS/Villager/VillagerDeath.cpp`) |
| A hungry villager eats from the food it is carrying (eating out, eating at home, clearing away dinner, eating at the worship site) | done | `EatFoodHeld` (`src/ECS/Villager/VillagerFood.cpp`) |

## The dropped log

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The log is a fallen pine trunk holding exactly the wood the villager carried, drawn with the mesh of what the villager was holding (a log of the carried tree type, plain wood, or even the tool it was showing) | done | the log holds the villager's wood and is drawn with its carried object's mesh (`CreateDroppedResource`, `src/ECS/Villager/VillagerResources.cpp`) |
| It is placed on the ground and raised clear of anything it overlaps before it flies | done | put on the ground and raised until it intersects nothing (`CreateDroppedResource`) |
| It flies without hitting other objects, so it hurts nothing and knocks nothing over on its way | done | `PhysicsObjects::AddDroppedObject` hits no other object; see ../physics/collisions.md |
| Once down it is an ordinary fallen tree: the hand can pick it up and throw it, it can burn, and villagers wanting wood come to collect it | done | the log is a fallen tree (`components::DeadTree`): the hand, the fire and the wood reaction treat it as one; See ../nature/ |
| Picked up again, a log of evergreen, fruit or hardwood gives back that tree type; a log drawn with any other mesh counts as evergreen | done | the carried tree type of a fallen tree (`CarriedTreeTypeOf`, `src/ECS/Systems/Implementations/VillagerResourceReactions.cpp`) |

## Logs by tree type

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Wood taken from a tree, a felled tree or a fallen trunk is carried as a log of that tree's type; wood from a storage pit, a pot, a rock or a big forest is carried as its own kind | done | the tree type kept with the wood (`PickupWood`, `src/ECS/Villager/VillagerResources.cpp`) |
| Conifers, pines, cypresses and copses give evergreen logs; beech, birch, cedar, oak, olive, palms, bushes, hedges and burnt trees give hardwood; big forests give hardwood; storage pits, pots and rocks give plain wood | done | from the tree records of info.dat |
| No tree gives fruit-tree logs, so the fruit branch is never seen in a villager's hand | done | no tree record gives it |
| The tree type stays with the villager after it drops its wood, until its next wood pick-up replaces it | done | the tree type stays in the villager's flags until the next pick-up (`PickupResource`) |

## Scripts and story

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A script can set the object a villager holds (any of the list, or nothing); the next animation change overwrites it unless the villager is under the script | todo | SET_OBJECT_CARRYING only logs "not implemented" (`src/CHLApi.cpp`) |
| Land 2's introduction of the disciple gives its villager a heavy mallet | todo | SET_OBJECT_CARRYING is a stub |
| The worship tutorial gives the villager building the worship site a heavy mallet | todo | SET_OBJECT_CARRYING is a stub |
| Land 5's waving villagers are given a fishing rod and an axe | todo | SET_OBJECT_CARRYING is a stub |

## Things the game does not do

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers never carry scaffolds: the code for builders bringing one from the workshop is left empty, so only the hand and the creature move them | done | no villager state carries a scaffold; only the hand's and the creature's code move them (`src/ECS/Scaffolds.cpp`) |
| Mothers never carry babies; children walk after their mothers | done | children walk after their mothers (`CHILD_FOLLOWS_MOTHER`); nobody carries a baby |
| There are no tribe-specific tools: every tribe uses the same meshes | done | one prop table for every tribe (`src/ECS/CarriedProps.cpp`) |
| A forester does not carry the tree it fells: the felled trunk lies where it fell, and villagers who want wood come to it and carry the wood away as a log | done | the felled tree falls and lies; villagers take its wood through the wood reaction (`src/ECS/Villager/VillagerForester.cpp`, `src/ECS/Systems/Implementations/VillagerResourceReactions.cpp`); See jobs.md |
| What a villager in the creature's hand shows, and whether it keeps its load there | todo | Undetermined: the creature's holding of villagers was not traced |
