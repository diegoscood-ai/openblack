# Temple hand and controls

How the player acts inside the temple: the hand follows the cursor over the rooms' walls and floors, presses the rooms'
controls and scrolls, and the keys move between rooms.

**Progress: 9/11 done, 1 partial — 86%**

## The hand

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand is put where the cursor meets the room, turned to the surface | done | `TempleInterior::GetCursorHit`, from the room camera's pick (`TempleCameraModel`) |
| The hand is drawn in the temple's light | done | `RenderingSystemTemple` draws the player's hand in the temple pass |
| The hand holds a spell's seed or the creature's leash collar in the temple | todo | (unconfirmed what the hand can hold inside) |
| The tooltip is drawn by the hand | done | `TempleInterior::UpdateToolTips` puts the tooltip by the hand (`GameInterface::SetHandOnScreen`) |
| The hand's pose for pointing and pressing in the temple | partial | inside, the hand takes its citadel state and stays drawn (`HandSystem::RequiredHandState`); the pose that state gives is not checked; see ../hand/poses_and_states.md |

## Controls

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A press on a scroll or button takes the mouse until let go | done | `TempleInterior::HoldControl` |
| Escape goes back to the main room, and out from there | done | `TempleInterior::Escape` |
| Keys go straight to a room | done | `Game::ProcessTempleRoomKeys`; see ../camera/temple_camera.md |
| The temple has its own game turn every 100 milliseconds | done | while paused inside, `game_clock::PausedTurnTimer` gives a turn every 100 ms and `Game::ProcessTempleTurn` runs it |
| Temple scripts run while the player is inside (temple help and specials) | done | `Game::ProcessTempleTurn` looks in the temple help and temple special scripts (`LookIn(TempleHelp or TempleSpecial)`) each temple turn; see ../engine/script_vm.md |
| The world room tab of the options, inside the temple | done | `gui::GameMenu::SetInsideTemple` shows the World Room tab; see ../interface/main_menu.md |
