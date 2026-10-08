# Scrolls and signs

The scrolls that float over the people and places of a land to mark a challenge or a tip, and the signposts the player
reads. Which challenges each land has, and what its gold and silver scrolls lead to, are in ../story/; the scrolls
inside the temple are in ../temple/.

**Progress: 9/18 done, 4 partial — 61%**

How the original does it, in our wiki: [The hand and the interface](../../bw1-notes/hand-and-interface.md).

## Challenge scrolls

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A land script puts a scroll up at a place or over a character, tied to a challenge | done | `CREATE_HIGHLIGHT` in `src/CHLApi.cpp` makes a highlight at the point with its challenge id (`ecs::script_highlight::Create`); `HIGHLIGHT_PROPERTIES` names it |
| Gold scrolls mark the story's challenges and silver ones the side challenges | done | the info rows Gold and Silver (`script_highlight::Info` in `src/ECS/ScriptHighlight.h`); which challenge each marks is the land script's |
| A bronze scroll kind | done | row 0 `Info::Bronze` (the plain mini scroll, no particles); (unconfirmed where the game uses it) |
| Scrolls glint with sparkles in their colour | done | the silver and gold rows start their glints at creation, with the highlight as their target (`script_highlight::Create`); the bronze row has none |
| All scrolls pulse together, smoothly, once every turn of a shared clock | partial | the shared pulse is stepped every turn (`StepPulse`, test `ScriptHighlight.PulseOfProcessHighlights`), but the active glow it drives is not drawn (`ExtrasOf` has no caller) |
| Tapping a scroll starts its challenge, shown by its active sparkles | partial | a tap lights the scroll and starts its active effect (`InterfaceTap`, `SetActivated`); starting the challenge through the temple's challenge room is pending |
| A started scroll stays as a reminder: tapping it again replays the challenge's last message | partial | a tap on a scroll with a challenge counts the help record's Reminder event (`InterfaceTap`); replaying the challenge's message is pending with the challenge room |
| The hand can only tap a scroll inside the player's influence (unless the scroll says otherwise) | done | a highlight is tapped wherever the hand is (`hand_tap::SendsTap`; test `ScriptHighlight.TappedOutsideTheInfluence`). Our wiki differs: a highlight's influence test is always off, so the original taps it outside the influence too ([hand and interface](../../bw1-notes/hand-and-interface.md#tapping-objects)) |
| Scrolls can't be picked up, burnt, crushed or knocked over, and the creature leaves them alone | partial | never placed in the hand (`HandSystem` keeps a highlight out of the pick-up); burning, crushing, knocking over and the creature not checked |
| A scroll can be set to draw at a height and turned on or off by scripts | done | `SET_PROPERTY` YPOS (`script_highlight::SetYPos`), `SET_ACTIVE`, and `SET_DRAW_HIGHLIGHT` hiding the scrolls (`help::script_control`) |
| The advisors point the scroll out when it comes up, or at random later | todo | only the first did-you-know tap runs "FirstDYKExplained"; see [help_system.md](help_system.md) |
| Scrolls are kept in saved games | todo | see ../engine/ |
| The temple's world room can hide or show every challenge's scroll | todo | the main room's challenges button only changes its map (`src/3D/TempleToggles.cpp`); see ../temple/ |

## Tips and signs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| "Did you know" scrolls in the world give a tip when tapped | done | `script_highlight::InterfaceTap` opens the help bubble with the sign's text and marks it read (`src/Help/Bubble.cpp`); tests `HelpSystem.BubbleOpensWithEachTappedSignsTextAndTheSameSignClosesIt`, `DidYouKnowBubble.*` |
| A building whose info names a did-you-know gets the sign by its door | done | (added) `abodes::CreateAbodeSurroundingObjects` in `src/ECS/Abodes.cpp` makes the did-you-know highlight at the entrance point ([intro](../../bw1-notes/intro.md#script-highlights)) |
| A scroll can be the way into the land's vortex to the next land | todo | see ../story/ |
| Signposts that show their words when the hand is over them | todo | (unconfirmed which lands have them and what they say); the did-you-know signs are read by tapping, above |
| The temple's room signs, lit for the door under the hand | done | `src/3D/TempleSigns.cpp`; see ../temple/ |
