# Online services and PC extras

The game's links to the internet and to the player's PC outside the game world itself: the online account, the creature's
web page and the creature upload, maps fetched from the internet, the GameSpy lobby, and the force-feedback mouse. Lionhead's
servers are long gone, so the online parts are **n/a** in openblack, but are described here as the game did them. Network
play itself is owned by [../multiplayer/network_play.md](../multiplayer/network_play.md); the short summary of these extras
is [../multiplayer/online_services.md](../multiplayer/online_services.md). The real weather and the contact names have their
own files: [real_weather.md](real_weather.md) and [villager_names_from_contacts.md](villager_names_from_contacts.md).

**Progress: 0/26 done, 2 partial — 4%**

## Connecting

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| At start-up the game checks whether the computer is online, trying newer Internet Explorer's network check, then Internet Explorer 4's connected state, then the dial-up connections list | pending | our tree makes no such check (no online code) |

## The creature's web page

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each time the creature's mind and body are saved, the game also writes a web page about the creature | pending | our tree writes no creature web page; when the creature's mind is saved is owned by ../creature/saves_and_files.md |
| The page goes in the player's profile folder, in an `html` folder made if missing, named after the creature: `<creature name>.html` | pending | no creature web page in our tree; e.g. `Profiles/A/<profile>/html/aaa.html` in the game data |
| The page is made from the template `Scripts/creature_html.tmpl`, line by line, each `[$key$]` replaced by the creature's value for that key | pending | the template `Scripts/creature_html.tmpl` is not read by our tree |
| The page is titled "Creature Cave" and shows the creature's name, age, alignment, strength, health, energy, villagers killed and battles won | pending | no creature web page in our tree |
| It shows up to five pictures of the creature, `creatureshot_0.jpg` to `creatureshot_4.jpg` from the profile folder, faded to 40% until the mouse is over one, each opening larger in a pop-up | pending | no creature web page in our tree |
| Its frames, scrolls, banner, logo and good and evil pictures come from `Scripts/html` | pending | no creature web page in our tree |
| The game also offers keys the shipped template does not use: fatness, exhaustion, dehydration, itchiness, amount of poo, animals killed, creatures killed, battles fought, poos, mushrooms eaten, how nice the player has been and how much attention the player has given | pending | no creature web page in our tree; a modder's template could show them |
| It offers each of the creature's 40 desires, a caption for each of the five pictures, 24 further values and one more chosen from 16 (unconfirmed which) | pending | no creature web page in our tree (unconfirmed what the 24 and the 16 are) |
| The pictures are taken by scripts that snapshot the creature | pending | `UPDATE_SNAPSHOT_PICTURE` is real in `src/CHLApi.cpp`; which scripts take the pictures is not confirmed (unconfirmed) |

## Uploading the creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| At the same time the creature is packed into one file, `creature.lhp`, in the profile folder | pending | our tree reads creature mind files (`components/creaturemind`, `resources::CreatureMindLoader`) but does not write `creature.lhp` |
| The file holds a creature block with every value the web page offers, and a pictures block with the JPEG pictures | pending | no `creature.lhp` in our tree |
| Its header carries a random number from a generator seeded with the clock | pending | no `creature.lhp` in our tree |
| The account's login name is written into it with every character other than letters, digits and "-" changed to "_" | pending | no `creature.lhp` and no online account in our tree |

## Creatures in online games

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Creatures keep what they learn online | pending | no online games in our tree; see ../multiplayer/multiplayer_rules.md |

## Maps from the internet

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Multiplayer maps live in the `Online Maps` folder, each a `.map` file with a `.thm` thumbnail | pending | our tree does not read the `Online Maps` folder; see ../terrain/land_loading.md and ../multiplayer/skirmish.md |
| The local list, `localmaps.txt`, gives each map's title, file and number of players: "Bombardment - 2 players", "King of the hill - 3 players" and "The four corners of Eden - 4 players" | pending | our tree does not read `localmaps.txt` |
| Each map's winning conditions, with default, least and most values and the steps for the left and right mouse buttons, come from `ConditionTemplate.txt` | pending | our tree does not read `ConditionTemplate.txt`; see ../multiplayer/multiplayer_rules.md |

## Force-feedback mouse

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game supports Immersion's force-feedback ("TouchSense") mice through Immersion's library, `IFC22.dll` | pending | our tree has no force feedback; a gamepad rumble or haptics could stand in |
| The effects are loaded from `Data/Immersion/FullFeedback.ifr`, or `Data/Immersion/Tactile.ifr` for plain vibrating mice (unconfirmed which mouse uses which) | pending | our tree does not read the `.ifr` files (unconfirmed which mouse uses which) |
| A dialog sets the strength: Minimum, Normal or Maximum | pending | no such dialog in our tree |
| There are 49 effects: fur, slap, hit, gesture success, grip, uproot, the hand hitting the influence edge, leash pull, gesture trail, spell crackle, heartbeat, sprinkling food, wood and water, burning, a miracle's pop, heavy, the trademark, the mushroom challenge, three fireball strengths, lightning bolt, something moving in the hand, casting, shield, flocks flying and on the ground, creature spells, a hit that is not allowed, a moving fish, a command obeyed, the Hanoi puzzle, planting, and the hand passing over a miracle, the land, straw, smooth things, wood, canvas, a challenge scroll, a story scroll, foliage, the creed, the singing stones, a vortex, the temple, a teleport and special crops | partial 20% | our tree lists them (`ImmersionEffectType`, `src/Enums.h`) and objects' info carries their in-hand effect (`src/InfoConstants.h`), but nothing plays them; re-measured 2026-10-21: the 49 effects are listed and each object's in-hand effect is read; none is played (the playing is the missing 80%) |
| Each turn the hand picks one effect: what it holds if anything, else what it is over; when that changes, the old effect stops and the new one starts | pending | nothing picks an effect in our tree |
| Clicking the game's menus gives a click effect | pending | no force feedback in our tree |
| Putting a tattoo on the creature or taking it off gives a pulse | pending | no force feedback in our tree; see ../creature/creature_tattoos.md |
| Scripts can start and stop effects, stop them all, and ask whether a force-feedback mouse is present | partial 50% | `START_IMMERSION`, `STOP_IMMERSION` and `STOP_ALL_IMMERSION` are stubs in `src/CHLApi.cpp`; `IMMERSION_EXISTS` always answers no, which is the game's answer without such a mouse; re-measured 2026-10-21: the presence query answers as the original without such a mouse; starting and stopping effects are stubs |
