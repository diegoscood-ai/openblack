# Online services and PC extras

The game's links to the internet and to the player's PC outside the game world itself: the online account, the creature's
web page and the creature upload, maps fetched from the internet, the GameSpy lobby, and the force-feedback mouse. Lionhead's
servers are long gone, so the online parts are **n/a** in openblack, but are described here as the game did them. Network
play itself is owned by [../multiplayer/network_play.md](../multiplayer/network_play.md); the short summary of these extras
is [../multiplayer/online_services.md](../multiplayer/online_services.md). The real weather and the contact names have their
own files: [real_weather.md](real_weather.md) and [villager_names_from_contacts.md](villager_names_from_contacts.md).

Rows reachable only with a force-feedback device have the status `device`, counted apart and in no score (user, 2026-10-27; [progress/README.md](../README.md#status)).

**Progress: 5/19 done, 0 chosen, 3 partial, 0 inferred, 7 device-only (not scored) — 37%**

## Connecting

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| At start-up the game checks whether the computer is online, trying newer Internet Explorer's network check, then Internet Explorer 4's connected state, then the dial-up connections list | pending | our tree makes no such check (no online code) |

## The creature's web page

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each time the creature's mind and body are saved, the game also writes a web page about the creature | partial 75% | written every 600 turns the local player's creature lives (`ecs::creature_web_page_writer::ProcessTurn`, tests `CreatureWebPage.*`); the mind and body saves of those turns are not ported (owned by ../creature/saves_and_files.md), and no game run has yet shown the page written at the 600th turn |
| The page goes in the player's profile folder, in an `html` folder made if missing, named after the creature: `<creature name>.html` | done | `creature_web_page::PagePath` and `ecs::creature_web_page_writer::Write` (the player's folder, its html folder made first; nothing without a player's folder); tests `CreatureWebPage.AnAsciiNameStaysAsItIs`, `CreatureWebPage.TheNameIsConvertedAsTheGameConvertsIt`; the name is converted as the original converts it, through the machine's code page with Windows' best fit (off Windows, the Western code page with "?" for the rest) ([the player's creature](../../../creature/players-creature.md#what-openblack-does-with-the-players-creature)) |
| The page is made from the template `Scripts/creature_html.tmpl`, line by line, each `[$key$]` replaced by the creature's value for that key | done | `creature_web_page::Fill`, the template read through the byte cache; tests `CreatureWebPage.ThePageIsTheTemplateWithItsKeysFilledIn`, `CreatureWebPage.AKeyWithNoValueEndsItsLineThere`, `CreatureWebPage.TheClosingCharactersArePassedOverAsTheGameDoes`, `CreatureWebPage.TheTemplateIsReadAndWrittenAsATextFile`, `CreatureWebPage.LongLinesAreReadInPiecesOf2047Characters` |
| The page is titled "Creature Cave" and shows the creature's name, age, alignment, strength, health, energy, villagers killed and battles won | done | the shipped template filled in (`creature_web_page::FieldsOf`); test `CreatureWebPage.TheShippedTemplateGivesTheOriginalsPage` (the original's sisi.html byte for byte, with the game's data), `CreatureWebPage.FractionsHaveTwoDigitsRoundedAsTheGamesLibraryRounds` |
| It shows up to five pictures of the creature, `creatureshot_0.jpg` to `creatureshot_4.jpg` from the profile folder, faded to 40% until the mouse is over one, each opening larger in a pop-up | pending | the page is written with the template's own code for them, byte for byte (`CreatureWebPage.TheShippedTemplateGivesTheOriginalsPage`), but nothing takes the pictures: the creature shots are not ported (see the last row) |
| Its frames, scrolls, banner, logo and good and evil pictures come from `Scripts/html` | done | read 2026-11-07: the template names them from `../../../scripts/html/` (its `ImageRoot`), so from the page in `Profiles/<player>/html/` they are the working folder's `scripts/html`, where the game keeps them (`banner.gif`, `bg2.gif`, `framel.gif` and the rest); openblack writes the page byte for byte from the template (`CreatureWebPage.TheShippedTemplateGivesTheOriginalsPage`) into `Profiles/<player>/html/` of the working folder, as the original (`creature_web_page::PagePath`, `ecs::creature_web_page_writer::Write`) |
| The game also offers keys the shipped template does not use: fatness, exhaustion, dehydration, itchiness, amount of poo, animals killed, creatures killed, battles fought, poos, mushrooms eaten, how nice the player has been and how much attention the player has given | done | `creature_web_page::FieldsOf`, in the original's order; tests `CreatureWebPage.TheValuesAreTheGamesKeysInItsOrder`, `CreatureWebPage.NiceAndAttentionAreTheNumbersOfTheGamesTexts` (the two last are the numbers of the game's texts, not the texts) |
| It offers each of the creature's 40 desires, a caption for each of the five pictures, 24 further values and one more chosen from 16 (unconfirmed which) | partial 80% | the 40 desires' levels by their names in info.dat, the 24 counts of beliefs of each type and the first miracle on the creature of the 16 (`creature_web_page::FieldsOf`, `ecs::creature_web_page_writer::FactsOf`; test `CreatureWebPage.TheDesiresAreNamedByTheirRowsInTheTables`); missing: the five captions, which the creature shots set ([the player's creature](../../../creature/players-creature.md#the-creatures-files)) |
| The pictures are taken by scripts that snapshot the creature | pending | `UPDATE_SNAPSHOT_PICTURE` is real in `src/CHLApi.cpp`; read 2026-11-07: the web page's pictures are not a script's: in the same 600-turn save, right after the page, the game itself may take one (fn_0047BA70, from `Creature::ProcessState`: with the player looking at the creature and at least 10 seconds since the last, then fn_0047BBE0 and `CreatureRoom::TakeScreenShot` 0x789A70, [the player's creature](../../../creature/players-creature.md#the-creatures-files)); not ported |

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
| The game supports Immersion's force-feedback ("TouchSense") mice through Immersion's library, `IFC22.dll` | device | our tree has no force feedback; a gamepad rumble or haptics could stand in; only with a force-feedback device: the force-feedback mouse and its `.ifr` effects are set up only when `CImmMouse::Initialize` succeeds (0x5D86A0, the check at 0x5D870B), which it never does without one |
| The effects are loaded from `Data/Immersion/FullFeedback.ifr`, or `Data/Immersion/Tactile.ifr` for plain vibrating mice (unconfirmed which mouse uses which) | device | our tree does not read the `.ifr` files (unconfirmed which mouse uses which); only with a force-feedback device: the force-feedback mouse and its `.ifr` effects are set up only when `CImmMouse::Initialize` succeeds (0x5D86A0, the check at 0x5D870B), which it never does without one |
| A dialog sets the strength: Minimum, Normal or Maximum | device | no such dialog in our tree; only with a force-feedback device: the options' sixth tab, the force-feedback page, is added only when a force-feedback mouse was set up (`AddOptionsTabs` 0x53F540, the check at 0x53F559) |
| There are 49 effects: fur, slap, hit, gesture success, grip, uproot, the hand hitting the influence edge, leash pull, gesture trail, spell crackle, heartbeat, sprinkling food, wood and water, burning, a miracle's pop, heavy, the trademark, the mushroom challenge, three fireball strengths, lightning bolt, something moving in the hand, casting, shield, flocks flying and on the ground, creature spells, a hit that is not allowed, a moving fish, a command obeyed, the Hanoi puzzle, planting, and the hand passing over a miracle, the land, straw, smooth things, wood, canvas, a challenge scroll, a story scroll, foliage, the creed, the singing stones, a vortex, the temple, a teleport and special crops | device | our tree lists them (`ImmersionEffectType`, `src/Enums.h`) and objects' info carries their in-hand effect (`src/InfoConstants.h`), but nothing plays them; only with a force-feedback device: the force-feedback mouse and its `.ifr` effects are set up only when `CImmMouse::Initialize` succeeds (0x5D86A0, the check at 0x5D870B), which it never does without one |
| Each turn the hand picks one effect: what it holds if anything, else what it is over; when that changes, the old effect stops and the new one starts | device | nothing picks an effect in our tree; only with a force-feedback device: the hand's feel of what it holds or is over is picked only by `GInterface::UpdateCollideImmersion` (0x5D8320), which returns at once while no force-feedback mouse was set up |
| Clicking the game's menus gives a click effect | device | no force feedback in our tree; only with a force-feedback device: the exe starts every force-feedback effect through `GInterface::StartImmersion` (0x5D8A10), which returns at once while no force-feedback mouse was set up |
| Putting a tattoo on the creature or taking it off gives a pulse | device | no force feedback in our tree; see ../creature/creature_tattoos.md; only with a force-feedback device: the exe starts every force-feedback effect through `GInterface::StartImmersion` (0x5D8A10), which returns at once while no force-feedback mouse was set up |
| Scripts can start and stop effects, stop them all, and ask whether a force-feedback mouse is present | partial 50% | `START_IMMERSION`, `STOP_IMMERSION` and `STOP_ALL_IMMERSION` are stubs in `src/CHLApi.cpp`; `IMMERSION_EXISTS` always answers no, which is the game's answer without such a mouse; the force-feedback part (starting and stopping effects, and the answer yes with a device) is not ported (user, 2026-10-27, device): `START_IMMERSION`, `STOP_IMMERSION` and `STOP_ALL_IMMERSION` only call `GInterface::StartImmersion`, `StopImmersion` and `StopAllImmersion` (`GScript` 0x70BA20, 0x70BA50, 0x70BA80), each of which returns at once while no force-feedback mouse was set up |
