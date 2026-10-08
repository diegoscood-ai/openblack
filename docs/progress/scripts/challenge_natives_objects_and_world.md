# Challenge natives: objects, villagers and the world

The challenge scripts' functions for objects in the world: finding, creating, moving and deleting them, their properties and script states, flocks, containers, fire, clicks and hits, special effects, mist, games and walking paths. The game has 464 of these functions in all; the language statement each comes from is shown in italics, and "called" counts are calls in the shipped `challenge.chl`. How the virtual machine runs them is in [../engine/script_vm.md](../engine/script_vm.md); what each challenge is about is in [../story/](../story/).

**Progress: 31/122 done, 18 partial — 33%**

## Used by the shipped scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Whether the player has clicked an object: *‹object› clicked* (called 84 times in 37 scripts) | done | `GameThingClicked` in `src/CHLApi.cpp`: whether the hand's last tapped object is the thing (`HandSystem::GetClickedObject`); a challenge scroll that saves is tapped again, but there are no save rooms |
| Puts a villager, animal or other living thing into one of its script states (walking to a place, dancing, sitting and so on): *state ‹object› ‹state›* (called 893 times in 150 scripts) | partial | `SetScriptState`: villagers and animals (and each member of a script container) through `ecs::script_held::SetLivingScriptState`; a creature logs "not implemented" |
| Puts a living thing into a script state that takes a position: *state [state] [position] (a state statement that takes a position)* (called 20 times in 16 scripts) | todo | `SetScriptStatePos` logs "not implemented" |
| Puts a living thing into a script state that takes a number with a fraction: *state [state] [number] (a state statement that takes a fraction)* (called 20 times in 16 scripts) | todo | `SetScriptFloat` logs "not implemented" |
| Puts a living thing into a script state that takes a whole number: *state [state] [number] (a state statement that takes a whole number)* (called 862 times in 140 scripts) | partial | `SetScriptUlong`: a villager's script clip and times (`ecs::villager::SetScriptAnimation`), also for a container's villagers; a creature logs "not implemented" |
| Reads one of an object's properties (health, age, food, wood, altitude, belief, scale, speed and dozens more): *‹prop› of ‹object›* (called 1421 times in 257 scripts) | partial | `GetProperty`: scale and age of villagers and animals, speed, flying, drowning, the built percentage, a highlight's height; the other properties, other things' scale and height and the creature's age log "not implemented"; `test/test_script_living_props.cpp` |
| Changes one of an object's properties: *‹prop› of ‹object› = ‹val›* (called 636 times in 175 scripts) | partial | `SetProperty`: scale, speed and age of villagers and animals, a highlight's height and the built percentage; the other properties log "not implemented" |
| Gives an object's position: *[ ‹obj id› ]* (called 4112 times in 351 scripts) | partial | `GetPosition`: a thing's position, a flock's first member, a villager already at its destination gives the destination; the animals' destination case is not ported |
| Moves an object to a position at once: *set ‹obj id› position to ‹position›* (called 127 times in 54 scripts) | done | `SetPosition`: the thing put on the ground at once with no slide (`ecs::NotifyTeleported`); a living thing the script controls goes into its script state |
| Finds an object of a kind at a position: *get ‹type› [‹subtype›] at ‹position› [excluding scripted]* (called 82 times in 43 scripts) | done | `Call`: `FindForScript` in `src/CHLApi.cpp`, the nearest thing of the type and sub-type in the map cells (`ecs::map_cells::FindNearForScript`), a town by `GetNearestTown`; `test/test_town_get_town.cpp` |
| Creates an object of a kind (villager, animal, rock, building, toy, marker and so on) at a position: *marker ‹type› ‹subtype› at ‹position›* (called 1818 times in 264 scripts) | partial | `Create`: `CreateScriptObject` makes markers, features, villagers, animals, rocks, mobile objects, trees, animated statics, the shark, puzzle games, weather things, one-shot spells, dispensers, scaffolds, timers and vortices; rewards, creatures, dead trees, stores, balls, totems and highlights are not made ([map-loading](../../bw1-notes/map-loading.md#pending)) |
| Makes a living thing walk to a position, ending within a radius of it: *move ‹object› position to ‹position› [radius ‹radius›]* (called 528 times in 156 scripts) | partial | `MoveGameThing`: villagers (`ecs::villager::SetupMoveToPos`), animals (`ecs::animal_ai::ScriptMoveTo`), flocks and other objects; a creature logs "not implemented" |
| Turns an object to face a position: *set ‹object› focus to ‹position›* (called 601 times in 138 scripts) | partial | `SetFocus`: villagers and animals snap their yaw (`ecs::living::SetFocus`), also a container's members; creatures and other objects log "not implemented" |
| Creates an empty flock at a position: *flock at ‹position›* (called 29 times in 20 scripts) | done | `FlockCreate`: `ecs::script_containers::CreateFlock` (`src/ECS/ScriptContainers.cpp`); `test/test_script_flocks.cpp` |
| Adds an animal or villager to a flock, optionally as its leader: *attach ‹obj› to ‹flock› [as leader]* (called 102 times in 43 scripts) | done | `FlockAttach`: `ecs::script_containers::Attach`, as leader or not |
| Takes a member out of a flock: *detach [‹obj›] from ‹flock›* (called 18 times in 10 scripts) | done | `FlockDetach`: `ecs::script_containers::Detach` |
| Breaks a flock up: *disband ‹flock›* (called 9 times in 7 scripts) | done | `FlockDisband`: `ecs::script_containers::Disband` |
| Gives how many things a flock, town or container holds: *size of ‹container›* (called 56 times in 19 scripts) | done | `IdSize`: `ecs::script_containers::Size` |
| Whether an object is in a flock: *‹obj› in ‹flock›* (called 3 times in 2 scripts) | todo | `FlockMember` logs "not implemented" and pushes false |
| Gives the position of the player's hand in the world: *hand position* (called 11 times in 5 scripts) | done | `GetHandPosition`: the player's hand's position |
| Deletes an object, plainly, fading it away, exploding it or with the temple's explosion: *delete ‹obj› with fade/with explode/with temple explode* (called 376 times in 121 scripts) | partial | `ObjectDelete`: modes 0 to 2 delete the thing and a container is disbanded; the fade ghost and the break-up are not drawn, and the creature's fizz, the temple heart and puzzle games log "not implemented" |
| Finds an object of a kind within a radius of a position: *get ‹type› [‹subtype›] at ‹position› radius ‹radius› [excluding scripted]* (called 159 times in 70 scripts) | done | `CallNear`: `FindForScript` with the radius and the distance check |
| Starts a special effect (sparkles, smoke, explosions and so on) at a position for a time: *create special effect ‹effect› at ‹position› [time ‹duration›]* (called 132 times in 49 scripts) | done | `SpecialEffectPosition`: `psys::manager::CreateSpotVisual` (`src/Particles/PSysManager.cpp`) for the time |
| Starts a special effect on an object for a time: *create special effect ‹effect› on ‹target› [time ‹duration›]* (called 62 times in 17 scripts) | done | `SpecialEffectObject`: a spot visual at the object's position, closed when the object goes |
| Makes villagers perform a dance around a place for a time: *make ‹obj› dance ‹type› around ‹position› [time ‹duration›]* (called 2 times in 2 scripts) | todo | `DanceCreate` logs "not implemented" and pushes 0 |
| Finds an object of a kind inside a town, flock or container: *get ‹type› [‹subtype›] in ‹container› [excluding scripted]* (called 24 times in 13 scripts) | done | `CallIn`: `ecs::script_containers::Find` in a town, flock or container, handed to the script as a found thing |
| Whether a living thing has finished the animation or state the script gave it: *‹obj› played* (called 319 times in 97 scripts) | partial | `Played`: villagers (`ecs::villager::IsScriptAnimationComplete`) and puzzle games; animals and creatures log "not implemented" and give false |
| Finds an object of a kind inside a town or container and within a radius of a position: *get ‹type› [‹subtype›] in ‹container› at ‹pos› radius ‹radius› [excluding scripted]* (called 3 times in 3 scripts) | todo | `CallInNear` logs "not implemented" and pushes 0 |
| Makes a living thing play an animation instead of its state's own: *set ‹obj› anim ‹anim type›* (called 56 times in 21 scripts) | partial | `OverrideStateAnimation`: villagers (`ecs::VillagerSetClip`) and animals (`ecs::SetAnimalAnim`); a creature logs "not implemented"; the remind record for the script is not ported |
| Makes villagers around an object react to it (run away, gather round and so on) (unconfirmed): *attach reaction ‹object› ‹reaction›* (called once in 1 script) | todo | `CreateReaction` logs "not implemented" |
| Gives the text of what the hand would do with an object: *get action text for ‹obj›* (called 2 times in 2 scripts) | done | `GetActionTextForObject` pops nothing and always pushes help text 828, as the original |
| Fills a town, flock or container with things of a kind: *populate ‹obj› with ‹quantity› ‹type› [‹subtype›]* (called 9 times in 8 scripts) | todo | `PopulateContainer` logs "not implemented" |
| Throws an object off along a heading at a speed: *set ‹value› velocity heading ‹value› speed ‹value›* (called 3 times in 3 scripts) | todo | `SetHeadingAndSpeed` logs "not implemented" |
| Whether an object is blown about by the wind: *enable/disable ‹object› affected by wind* (called 17 times in 10 scripts) | todo | `SetAffectedByWind` logs "not implemented" |
| Finds an object of a kind inside a town or container but not near a position: *get ‹type› [‹subtype›] in ‹container› not near ‹pos› radius ‹radius› [excluding scripted]* (called once in 1 script) | todo | `CallInNotNear` logs "not implemented" and pushes 0 |
| Gives the state a living thing is in: *state of ‹obj›* (called 2 times in 2 scripts) | todo | `GetObjectState` logs "not implemented" |
| Gives the land's height at a position: *land height at ‹position›* (called 9 times in 4 scripts) | done | `GetLandHeight`: `ecs::sea_cells::ScriptLandHeight`, -10 over the sea or off the map; `test/test_sea_cells.cpp` |
| Forgets the object the player last clicked: *clear clicked object* (called 43 times in 16 scripts) | done | `ClearClickedObject`: `HandSystem::ClearClicked` |
| Hands an object back to the game after a script has controlled it (it goes back to its own life): *release ‹obj›* (called 216 times in 96 scripts) | partial | `ReleaseFromScript`: `ecs::script_held::ReleaseFromScript` (`src/ECS/ScriptHeld.cpp`): villagers decide what to do (in the physics once out), animals wander, a flock's controlled members are released; a creature's and a ball's release and the music are not ported; `test/test_release_from_script.cpp` |
| Gives how many poisoned things a container holds: *poisoned size of ‹container›* (called once in 1 script) | todo | `IdPoisonedSize` logs "not implemented" and pushes nought |
| Whether an object is poisoned: *‹obj› poisoned* (called 2 times in 1 script) | todo | `IsPoisoned` logs "not implemented" and pushes false |
| Finds an object of a kind in a container that isn't poisoned: *get not poisoned ‹type› [‹subtype›] in ‹container› [excluding scripted]* (called once in 1 script) | todo | `CallNotPoisonedIn` logs "not implemented" and pushes 0 |
| Whether the player can move an object: *enable/disable ‹obj› moveable* (called 97 times in 37 scripts) | done | `SetIdMoveable`: `ecs::object_flags::SetMoveable` |
| Whether the player can pick an object up: *enable/disable ‹obj› pickup* (called 145 times in 55 scripts) | done | `SetIdPickupable`: `ecs::object_flags::SetPickupable`, read by the hand |
| Whether an object is on fire: *‹obj› on fire* (called 3 times in 2 scripts) | done | `IsOnFire`: `magic::script::IsOnFire` (`src/Magic/Script/CHLFire.cpp`); `test/test_fire.cpp` |
| Whether there is fire within a radius of a position: *fire near ‹position› radius ‹radius›* (called 3 times in 3 scripts) | done | `IsFireNear`: `magic::script::IsFireNear` |
| Poisons an object (food) or clears the poison: *enable/disable ‹obj› poisoned* (called 5 times in 1 script) | todo | `SetPoisoned` logs "not implemented" |
| Sets an object's temperature (fire spreads by temperature): *set ‹obj› temperature ‹temperature›* (called 5 times in 3 scripts) | done | `SetTemperature`: `magic::script::SetTemperature` |
| Sets an object on fire at a burning speed, or puts it out: *enable/disable ‹object› on fire ‹burn speed›* (called 36 times in 12 scripts) | done | `SetOnFire`: `magic::script::SetOnFire` |
| Sends an object towards a target over a time: *set ‹obj› target ‹position› time ‹time›* (called 8 times in 8 scripts) | todo | `SetTarget` logs "not implemented" |
| Makes a villager walk one of the paths laid in the land, forwards or backwards between two points: *set ‹object› forward/reverse walk path ‹camera enum› from ‹val from› to ‹val to›* (called 4 times in 2 scripts) | partial | `WalkPath`: villagers walk a `camera.edt` track in their script state (`ecs::living::StartWalkPath`), mobile objects and the shark too (`ecs::StartMobileWalkPath`); animals and creatures log "not implemented" |
| Whether an object is of a kind: *‹object› type ‹type› [‹subtype›]* (called 10 times in 9 scripts) | todo | `IsOfType` logs "not implemented" and pushes false |
| Forgets the object last hit: *clear hit object* (called 5 times in 3 scripts) | todo | `ClearHitObject` logs "not implemented" |
| Whether an object has been hit: *‹object› hit* (called 5 times in 4 scripts) | todo | `GameThingHit` logs "not implemented" and pushes false |
| Gives the object a player's hand or a creature is holding: *get held by ‹value›* (called 24 times in 16 scripts) | todo | `GetObjectHeld199` logs "not implemented" and pushes 0 |
| Lets a living thing's animation be sped up or slowed down: *enable/disable ‹creature› anim time modify* (called 2 times in 1 script) | todo | `SetAnimationModify` logs "not implemented" |
| Gives the kind of an object: *get ‹object› type* (called once in 1 script) | todo | `GameType` logs "not implemented" |
| Gives the sub-kind of an object (which animal, which building and so on): *get ‹object› sub type* (called 10 times in 9 scripts) | todo | `GameSubType` logs "not implemented" |
| Creates an object of a kind at a position with an angle and a scale: *create with angle ‹angle› and scale ‹scale› ‹type› [‹subtype›] at ‹position›* (called 93 times in 29 scripts) | partial | `CreateWithAngleAndScale`: `CreateScriptObject` with the angle in degrees and the scale; the same kinds as create are missing |
| Turns an object (a dispenser, a gate and so on) on or off: *enable/disable ‹object› active* (called 26 times in 22 scripts) | partial | `SetActive`: highlights, spell dispensers and scaffolds (an active one forces its building); other things log "not implemented" |
| Whether an object still exists: *‹obj id› exists* (called 388 times in 145 scripts) | partial | `ThingValid`: whether the entity still exists; the original asks its scripts' table, and our entity ids can be reused by a later thing |
| Stops villagers reacting to an object in one way (unconfirmed): *detach reaction ‹object› ‹reaction›* (called once in 1 script) | todo | `RemoveReactionOfType` logs "not implemented" |
| Gives how much of its animation a living thing has played: *get ‹object› played percentage* (called 2 times in 1 script) | todo | `PlayedPercentage` logs "not implemented" |
| Creates a patch of mist of a colour, size and transparency: *create mist at ‹pos› scale ‹scale› red ‹r› green ‹g› blue ‹b› transparency ‹transparency› height ratio ‹height ratio›* (called once in 1 script) | todo | `CreateMist` logs "not implemented" (the land script's mists are made, see [map-loading](../../bw1-notes/map-loading.md#map-mist-create_mist)) |
| Fades mist from one size and transparency to another over a time: *set ‹mist› fade start scale ‹start scale› end scale ‹end scale› start transparency ‹start transparency› end transparency ‹end transparency› time ‹duration›* (called once in 1 script) | todo | `SetMistFade` logs "not implemented" |
| Gives the object a player's hand or a creature is holding (second form): *get held by ‹creature›* (called 4 times in 2 scripts) | todo | `GetObjectHeld273` logs "not implemented" and pushes 0 |
| Draws an object in full detail however far away it is: *enable/disable ‹object› high graphics detail* (called 333 times in 66 scripts) | done | `SetHighGraphicsDetail`: `ecs::super_villager::SetHighGraphicsDetail` swaps in the high-detail family mesh with its eyes, drawn in game; `test/test_super_villager_eyes.cpp` |
| Turns a villager into a skeleton or back: *enable/disable ‹object› skeleton* (called 9 times in 6 scripts) | todo | `SetSkeleton` logs "not implemented" |
| Gives a spot visual (a glow or mark) a target position: *add ‹object› target at ‹position›* (called 6 times in 3 scripts) | todo | `AddSpotVisualTargetPos` logs "not implemented" |
| Gives a spot visual a target object: *add ‹object› target on ‹target›* (called 7 times in 6 scripts) | todo | `AddSpotVisualTargetObject` logs "not implemented" |
| Makes an object impossible to destroy, or not: *enable/disable ‹object› indestructible* (called 98 times in 42 scripts) | partial | `SetIndestructable`: sets or clears the `Indestructible` component; a script container's own handling is not ported |
| Turns a living thing to keep looking at an object: *set ‹object› focus on ‹target›* (called 2 times in 1 script) | todo | `SetFocusOnObject` logs "not implemented" |
| Lets a living thing stop looking at an object: *release ‹creature› focus* (called 4 times in 3 scripts) | todo | `ReleaseObjectFocus` logs "not implemented" |
| Whether an immersion (force feedback) effect exists: *immersion exists* (called 2 times in 2 scripts) | todo | `ImmersionExists` logs "not implemented" and pushes false |
| Opens or closes an object (a chest, a gate, a phone box): *open/close ‹object›* (called 12 times in 6 scripts) | todo | `SetOpenClose` logs "not implemented" |
| Finds a living thing of a kind in a state within a radius: *get ‹type› [‹subtype›] in state ‹state› at ‹position› radius ‹radius› [excluding scripted]* (called once in 1 script) | todo | `CallNearInState` logs "not implemented" and pushes 0 |
| Gives an object's information bits (unconfirmed): *get ‹object› info bits* (called once in 1 script) | todo | `ObjectInfoBits` logs "not implemented" |
| Whether fire hurts an object: *enable/disable ‹object› hurt by fire* (called 44 times in 15 scripts) | done | `SetHurtByFire`: `magic::script::SetHurtByFire` |
| Starts a special effect made for the game's cut scenes: *start jc special ‹feature›* (called 10 times in 2 scripts) | partial | `PlayJcSpecial`: the intro specials 0, 1, 2, 4 and 5 (`ecs::intro_special::Play`), the missionaries' boat (6) and the camera bookmarks on and off (14, 15); the script graphics object (3) logs "not implemented"; `test/test_intro_special.cpp` |
| Whether an object is locked in an interaction: *‹object› locked interaction* (called 5 times in 4 scripts) | todo | `IsLockedInteraction` logs "not implemented" and pushes false |
| Turns a cut-scene special effect on an object on or off: *enable/disable jc special ‹feature› on ‹target›* (called 35 times in 8 scripts) | done | `ThingJcSpecial`: `ecs::super_villager::ThingJcSpecial`, the family's cinematic orders |
| Whether a villager is male: *‹object› is male* (called once in 1 script) | todo | `SexIsMale` logs "not implemented" and pushes false |
| Whether an object is active: *‹object› active* (called once in 1 script) | todo | `IsActive` logs "not implemented" and pushes false |
| Finds a flying thing of a kind within a radius: *get ‹type› [‹subtype›] flying at ‹position› radius ‹radius› [excluding scripted]* (called 2 times in 1 script) | todo | `CallFlying` logs "not implemented" and pushes 0 |
| Fades an object in over a time: *set ‹object› fade in [time ‹time›]* (called once in 1 script) | todo | `SetObjectFadeIn` logs "not implemented" |
| Gives the hand's state (empty, holding, casting and so on): *get hand state* (called 4 times in 4 scripts) | done | `GetHandState`: the hand's interface state of the last turn (`HandSystem::GetInterfaceHandState`) |
| Gives the object the player last clicked: *get object clicked* (called once in 1 script) | todo | `GetObjectClicked` logs "not implemented" and pushes 0 |
| Whether an object can be set on fire: *enable/disable ‹object› set on fire* (called 17 times in 7 scripts) | done | `SetSetOnFire`: `magic::script::SetSetOnFire` |
| Makes a villager carry an object: *set ‹object› carrying ‹carried obj›* (called 5 times in 3 scripts) | todo | `SetObjectCarrying` logs "not implemented" |
| Finds a dead thing within a radius of a position: *get dead at ‹position› radius ‹radius›* (called once in 1 script) | todo | `GetDeadLiving` logs "not implemented" and pushes 0 |
| Gives the first thing in a container: *get first in ‹container›* (called 2 times in 2 scripts) | todo | `GetFirstInContainer` logs "not implemented" and pushes 0 |
| Gives the next thing in a container: *get next in ‹container› after ‹after›* (called 2 times in 2 scripts) | todo | `GetNextInContainer` logs "not implemented" and pushes 0 |

## Not used by the shipped scripts

The game has these but no shipped script calls them; mods and fan-made challenges can.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Gives the object a player or creature last dropped: *get dropped by ‹creature›* (not called by the shipped scripts) | todo | `GetObjectDropped` logs "not implemented" and pushes 0 |
| Forgets the object a player or creature last dropped: *clear dropped by ‹creature›* (not called by the shipped scripts) | todo | `ClearDroppedByObject` logs "not implemented" |
| Stops villagers reacting to an object (unconfirmed): *detach reaction ‹object›* (not called by the shipped scripts) | todo | `RemoveReaction` logs "not implemented" |
| Gives where an object is heading: *destination of ‹obj›* (not called by the shipped scripts) | todo | `GetObjectDestination` logs "not implemented" and pushes a zero position |
| Forgets the position the player last clicked: *clear clicked position* (not called by the shipped scripts) | done | `ClearClickedPosition`: `HandSystem::ClearClickedPosition` |
| Whether the player has clicked near a position: *‹value› clicked radius ‹value›* (not called by the shipped scripts) | done | `PositionClicked`: `HandSystem::PositionClicked`, the last clicked land point within the radius |
| Gives the object under the hand: *get object hand is over* (not called by the shipped scripts) | todo | `GetObjectHandIsOver` logs "not implemented" and pushes 0 |
| Finds a poisoned object of a kind in a container: *get poisoned ‹type› [‹subtype›] in ‹container› [excluding scripted]* (not called by the shipped scripts) | todo | `CallPoisonedIn` logs "not implemented" and pushes 0 |
| Gives how far along its path a walker is: *get ‹object› walk path percentage* (not called by the shipped scripts) | done | `GetWalkPathPercentage`: `ecs::living::GetWalkPathPercentage` for a living thing, 1 for anything else |
| Gives the slowest speed in a flock: *get slowest speed in ‹flock›* (not called by the shipped scripts) | todo | `GetSlowestSpeed` logs "not implemented" and pushes nought |
| Gives the arena (unused in the shipped scripts) (unconfirmed): *get the arena (no statement in the language)* (not called by the shipped scripts) | todo | `GetArena` logs "not implemented" and pushes 0 |
| Gives the football pitch in a town: *get football pitch in ‹town›* (not called by the shipped scripts) | todo | `GetFootballPitch` logs "not implemented" and pushes 0 |
| Stops all the games in a town: *stop all games for ‹value›* (not called by the shipped scripts) | todo | `StopAllGames` logs "not implemented" |
| Puts a villager into a game (football) for the home or away side: *attach ‹value› to game ‹value› for home/away team* (not called by the shipped scripts) | todo | `AttachToGame` logs "not implemented" |
| Takes a villager out of a game's side: *detach ‹value› in game ‹value› from home/away team* (not called by the shipped scripts) | todo | `DetachFromGame` logs "not implemented" |
| Takes the player out of a game's side: *detach player from game ‹value› from home/away team* (not called by the shipped scripts) | todo | `DetachUndefinedFromGame` logs "not implemented" |
| Makes an object answer only to scripts: *enable/disable ‹value› only for scripts* (not called by the shipped scripts) | todo | `SetOnlyForScripts` logs "not implemented" |
| Starts a match with a referee: *start ‹value› with ‹value› as referee* (not called by the shipped scripts) | todo | `StartMatchWithReferee` logs "not implemented" |
| Gives the size of a side in a game: *get size of ‹value› home/away team* (not called by the shipped scripts) | todo | `GameTeamSize` logs "not implemented" |
| Gives the object last hit: *get hit object* (not called by the shipped scripts) | todo | `GetHitObject` logs "not implemented" and pushes 0 |
| Gives the object that did the hitting: *get object which hit* (not called by the shipped scripts) | todo | `GetObjectWhichHit` logs "not implemented" and pushes 0 |
| Gives how faded an object is: *get ‹object› fade* (not called by the shipped scripts) | todo | `GetObjectFade` logs "not implemented" and pushes nought |
| Whether a villager is a skeleton: *‹object› skeleton* (not called by the shipped scripts) | todo | `IsSkeleton` logs "not implemented" and pushes false |
| Keeps an object inside an area (unconfirmed): *confine an object (no statement in the language)* (not called by the shipped scripts) | todo | `ConfinedObject` logs "not implemented" |
| Lets an object out of its area (unconfirmed): *release a confined object (no statement in the language)* (not called by the shipped scripts) | todo | `ClearConfinedObject` logs "not implemented" |
| Gives the flock an object belongs to: *get ‹member› flock* (not called by the shipped scripts) | todo | `GetObjectFlock` logs "not implemented" and pushes 0 |
| Whether a cut-scene special effect has finished: *jc special ‹feature› played* (not called by the shipped scripts) | done | `IsPlayingJcSpecial`: true, except special 13 (the pick-up clip, which never wraps) |
| Whether a flock is within its limits: *‹object› within flock limits* (not called by the shipped scripts) | todo | `FlockWithinLimits` logs "not implemented" and pushes false |
| Clears an actor's mind (unconfirmed): *clear an actor's mind (no statement in the language)* (not called by the shipped scripts) | todo | `ClearActorMind` logs "not implemented" |
| Starts an object over: *restart ‹object›* (not called by the shipped scripts) | todo | `RestartObject` logs "not implemented" |
