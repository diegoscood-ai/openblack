# Testbed scenarios

The ready-made scenarios of the testbed (about 200 of them on raffclar's branch, none yet in our tree), grouped by what
they show, and the parts of the game that have none yet. A row is done when there are scenarios that show that part
working; the framework is in [testbed.md](testbed.md).

**Progress: 0/34 done, 0 partial — 0%**

How the original does it, in our wiki: [openblack internals](../../bw1-notes/openblack-internals.md).

## The creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Idle: fidgeting, hanging around, wandering | todo | no testbed in our tree |
| Faces, emotions, gestures and the emotes of its desires; eyes and blinking | todo | no testbed in our tree |
| What it looks at: watching a walker | todo | no testbed in our tree |
| Needs: thirst, hunger, sleep by night and day, poo, puking, fainting, cold, exhaustion | todo | no testbed in our tree |
| Growing up: a time-lapse, the stages unlocking desires, the morph lineup | todo | no testbed in our tree |
| Looks: skins and hair by alignment, tattoos and wounds | todo | no testbed in our tree |
| Footprints per species, in snow, grass and shallows, the first of April smileys | todo | no testbed in our tree |
| Moving: walking, running and turning, routes round obstacles, round the lake and through small trees | todo | no testbed in our tree |
| Objects: picking up, looking over, putting down, throwing, eating, knocking down trees, pointing | todo | no testbed in our tree |
| The hand on it: stroking each part, slapping, the status panel, the hand's look by alignment | todo | no testbed in our tree |
| The leashes: leading, tying, keeping home, the leash keys and picker, other players' creatures | todo | no testbed in our tree |
| Fights: by themselves, charged and quick blows, blocks, knock-outs, slapping other gods' creatures | todo | no testbed in our tree |
| Learning: rewards and punishments, by watching, from mind files, copying the player | todo | no testbed in our tree |
| Creature Mode, following it with C, and the Creature Cave with its tattoos | todo | no testbed in our tree |
| Its voices by species and size, and the sounds of the land | todo | no testbed in our tree |
| Its shadow, its reflection, light on the land through the day | todo | no testbed in our tree; the light on the land through the day can be set from the debug bar's World menu |

## Miracles and effects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every miracle cast, held, powered up and its effect, each in its own scenarios | todo | no testbed in our tree; the debug Magic window casts any miracle (`src/Debug/Magic.cpp`, `MiraclesCaster.cpp`) |
| Dispensers, globes and seeds from worship | todo | no testbed in our tree |
| The creature casting its spells | todo | no testbed in our tree |
| Fire spreading, blasts, water putting fire out | todo | no testbed in our tree |
| Particles and sprites: sparkles, smoke, steam, mist, sorted sprites | todo | no testbed in our tree |
| Gestures drawn through the recogniser | todo | no testbed in our tree; the debug Gestures window draws and sends gestures (`src/Debug/Gestures.cpp`) |
| Hand navigation: dragging, edge turning, tilting, both buttons, middle button | todo | no testbed in our tree |

## Other uses

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Set ups for trying the editor on | todo | no testbed in our tree |
| Crowd benchmarks of creatures and villagers | todo | no testbed in our tree |

## Parts of the game without scenarios

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers' lives and jobs | todo | no testbed in our tree |
| Towns growing, their desires and building | todo | no testbed in our tree |
| Buildings: construction, the workshop's scaffolds, damage and repair | todo | no testbed in our tree |
| Wild animals and livestock on their own | todo | no testbed in our tree |
| Worship and prayer power from worshippers | todo | no testbed in our tree |
| Weather on its own: climates, rain and snow, wind | todo | no testbed in our tree; the debug Weather window forces climates, rain and storms (`src/Debug/Weather.cpp`) |
| The temple and its rooms | todo | no testbed in our tree; the debug Temple window goes to each room (`src/Debug/Temple.cpp`) |
| Land scripts and challenges | todo | no testbed in our tree |
| Physics of thrown objects other than the creature's and the miracles' | todo | no testbed in our tree |
