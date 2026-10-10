# Reactions

A creature reacts to what happens around it: miracles cast near it, fires, deaths, fights, things flying past, food,
balls, teleport stones, the player's hand at work, things crushed and things that hit it, and other creatures. Each kind
of reaction has a priority that decides whether it drops what it is doing, and a time before it reacts to the same
kind again.

**Progress: 1/42 done, 1 partial — 4%**

How the original does it, in our wiki: [The creature: groundwork, random streams and what is unknown](../../bw1-notes/creature.md).

## How reactions are taken up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Reactions reach every creature within their range, as they do villagers | todo | the shared reactions (`src/ECS/Effects/Reactions.cpp`, `ReactionsSystem`) reach villagers and animals only: a creature is no living class there yet |
| A creature takes a reaction up only after its species' reaction time from the game's tables | todo | the table's creature reaction times are read (`InfoConstants.h`) but nothing uses them |
| After reacting to a kind of thing, it won't react to that kind again for a time from the game's tables, longer or shorter than villagers' | todo | the table's value is read (`InfoConstants.h`) but nothing uses it |
| A more urgent reaction, nearer, takes over from what it reacts to, but only after a while | todo | no creature reactions |
| A creature fighting or knocked out takes no reactions up | todo | no creature reactions |
| While copying the player it takes up only the most urgent reactions | todo | no creature reactions |
| A script can turn reactions off for a creature | todo |  |
| No reactions during the dance editor, while forced to faint or carried by a teleport | todo |  |
| A shield it is under keeps reactions from outside off it | todo | no creature reactions; a shield only shapes its routes (`magic::map_shield::CreatureMustAvoid` in `src/ECS/RoutePlanWorld.cpp`) |
| Reacting, it stops what it was doing and picks up again afterwards | todo | no creature reactions |
| Its priority for each kind of reaction depends on the creature (how frightened or curious it is) | todo |  |

## Miracles

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A nasty miracle near it frightens it: its fear rises | todo | no creature reactions to miracles |
| Frightened by a nasty miracle, it may start back in fright, then runs away from where it struck | todo |  |
| Not afraid, or on the learning leash, it goes to look at a nasty miracle instead | todo |  |
| A nice miracle of its own player's draws it to look: it may turn and point, goes up to it, points, or waits there puzzled | todo |  |
| It takes no notice of another player's nice miracle or shield | todo |  |
| A shield of its own player's draws it to look, as a nice miracle does | todo |  |
| A shield struck or destroyed means nothing to it | todo |  |
| A magic tree is taken up but does nothing | todo |  |
| It is impressed by its own player's miracles and those of its player's other creature, not by its own | todo |  |
| It learns a miracle it reacts to, if cast by a creature or a human player, and a seed's miracle teaches only the first to learn it | partial | `CreatureMindSystem::SeeMiracle` and the watching rules (`src/Creature/CreatureWatching`) exist, but only the debug spawner calls them; see [learning by watching](learning_by_observation.md) |
| Frozen, its mind learns nothing | todo | no freeze check in the watching code |
| A teleport stone used near it draws it aside into the stone | todo | the teleport miracle moves no creature; see [../miracles/teleport.md](../miracles/teleport.md) |
| Caught by a tornado it faints | todo | the tornado takes no creature; see [../miracles/tornado.md](../miracles/tornado.md) |
| A miracle hitting it during a fight makes it reel | todo | the fight's recoils (`src/Creature/CreatureFight`) are for blows only; no miracle reaches a fighting creature; see [fighting](fighting.md) |
| Fleeing a miracle, it weighs how urgent fleeing is against what it does | todo |  |

## Other things that happen near it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A fire near it: it goes to look, flees, or puts it out | todo |  |
| A death near it | todo |  |
| A fight between others near it: it goes to watch | todo |  |
| Something flying through the air: it watches it, and may try to catch it | todo | no catching: the physics' catch check leaves the creature's part pending (`src/ECS/Physics/FromHand.cpp`, `PhysicsObjects.cpp`); see [../physics/throwing_and_landing.md](../physics/throwing_and_landing.md) |
| Food dropped or thrown near it | todo | see [feeding and thrown things](feeding_and_thrown_things.md) |
| A ball near it: it goes to play | todo |  |
| Another creature near it: it goes to greet, inspect or confront it | todo | see [friends](friends_and_other_creatures.md) |
| Another creature fainting near it | todo |  |
| Something crushed near it | todo |  |
| Something it can't make sense of: it looks confused | todo |  |
| Hit by something thrown, it reacts to the blow | todo | the creature has a physics body that thrown things hit (`src/ECS/CreaturePhysics.cpp`), but it is not hurt, angered, frightened or swayed; see [feeding and thrown things](feeding_and_thrown_things.md) |

## The player's hand

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand picks something up near it: it watches, and may come to see | todo | the reaction exists for villagers only |
| The hand puts things in a store: it watches, and may copy it | todo |  |
| The hand uses a totem: it watches | todo |  |
| The hand comes near it: it looks at the hand, or runs from it if it fears the player | todo |  |
| Stroked or slapped, it reacts at once, by the part of the body touched | done | `CreatureHandSystem` with `src/Creature/CreatureFeedback.h` (the part of the body by the nearest bone); tests `CreatureHandSystemTest.Resting_TheHandStrokes_SweptFastItSlaps`; see [learning from feedback](learning_from_feedback.md) |
