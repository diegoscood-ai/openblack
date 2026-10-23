# Miracle gestures

Gestures are how the player works with miracles: a spiral calls up the miracle selection and the miracle's own gesture
brings it into the hand, a circle sizes a storm or a shield, and each miracle's power-up gestures make it stronger. The
miracles themselves, and what they do once cast, are in [../miracles/](../miracles/).

**Progress: 20/24 done, 0 chosen, 2 partial, 0 inferred — 91%**

How the original does it, in our wiki: [Magic: the core of the miracles](../../../magic/gestures.md).

## Sizing with a circle

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A circle is waited for while the hand holds a miracle sized by a circle (the storms, the tornado, the two shields) and the Action button is held | done | `ProcessPowerUpSystem` step (a) in `src/Magic/Gestures/PowerUpSystem.cpp` (a held seed with a `sizingGesture`, the action press latched) |
| The circle is waited for whether or not the miracle in the hand is ready to cast yet | done | Step (a) does not test whether the seed is ready |
| The circle's middle on the land and its size across the land are handed to the miracle, which is cast there at that size | done | The circle's point and size go into the cast (`circlePending` in the hand's apply, `src/ECS/Systems/Implementations/HandSpellSeed.cpp`) |
| Once drawn, the circle is remembered for five seconds of game time and no new circle is waited for meanwhile | done | `k_CircleLife` 5 s and `circlePending` in `PowerUpSystem.cpp` (game time, not while paused) |
| Releasing a circle-sized miracle with no circle remembered fails, with the failure sound | done | `HandSpellSeed.cpp`: the cast's packet carries the circle's gesture only while a circle is remembered, and a sized seed whose packet lacks it goes to `FailApply` (`G_SpellCastFailure` and the failed-apply effect), as the original; the rule is `ecs::hand_seed::SizingGestureAllowsCast` (`src/ECS/HandSeedStep.h`), test `HandSeedStep.ASizedSeedIsCastOnlyWithItsCircle` |
| Starting the power-up system forgets a remembered circle | done | `TrySetupPowerUpGestures` clears the pending circle and its timer first, with or without a seed in the hand (`PowerUpSystem.cpp`), as the original; test `GestureSystem.settingThePowerUpsUpForgetsTheCircle` |

## Powering up the miracle in the hand

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each miracle has up to three power-up gestures (spiral, inverse spiral and others), read from the game's tables | done | `GSpellSeedInfo::powerUpGestures` (`src/InfoConstants.h`), read by `TrySetupPowerUpGestures` |
| Power-up gestures are only waited for while the hand holds a ready, uncast miracle made at one of the player's worship icons | done | `HoldingChargingSeed` (ready, not cast, from an icon) in `PowerUpSystem.cpp`; the worship icons are in `src/Worship`, so this works on lands with worship sites (none on Land 1, as in the original) |
| Every power-up level's gesture is waited for except the level already asked for | done | `PowerUpGestures` skips `currentPowerUpGesture`; a level needs the player to have its magic and the icon to offer it (`PowerUpAvailable`) |
| Drawing a power-up gesture asks the worship icon for the extra power; the level only changes once the icon has charged it, and any surplus goes back to the site | done | `SetPowerUpCharge` makes the icon charge that level (`src/Worship/GestureIconProvider.cpp`, `src/Worship/WorshipSpellIcon.cpp`); the seed's `SetPowerUp` sends a surplus back to the site (`src/Magic/Core/SpellSeed.cpp`) |
| A help message is given when a power-up is asked for | done | Help event 18 is counted in the help profile (`src/Help/HelpProfile.cpp`) |

## Calling up a miracle by gesture

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| With nothing in the hand, a spiral opens the miracle selection for the player's own miracles | done | `ProcessPowerUpSystem` step (c) and `Selection::Open`, with the worship icons as the `IconProvider` (`src/Worship/GestureIconProvider.cpp`); test `Gestures.selectionWithFakeIcon`. Only where worship sites exist |
| An inverse spiral opens the selection for miracles meant for the creature | done | Step (c), category `selectionSystemGestureCreature` |
| With the selection open, drawing a miracle's own gesture brings that miracle from its worship site into the hand | done | `Selection::Stage` then `IconProvider::RequestSpell`, which charges the best icon (`src/Worship/PlayerSpellIcons.cpp`); test `Gestures.selectionWithFakeIcon` (FORK_RIGHT requests seed 4) |
| The selection only offers miracles the player has and whose worship site can supply them | done | `ForEachRequestableIcon` and `IconValidForRequest` (`icon::ValidForRequestSpell` in `src/Worship/WorshipSpellIcon.cpp`) |
| The selection closes by itself after 30 seconds | done | `Selection::Stage` with `selectionSystemTimeOut` from the tables: strictly past it the selection closes, and that stage still takes a miracle's gesture, as the original; paused frames add nothing. Test `Selection.ThirtySecondsCloseItThoughThatStageStillLooks` |
| A scribble closes the selection | done | `Selection::Stage` takes the scribble before any miracle's gesture and closes (help event 21) in `PowerUpSystem.cpp`, as the original; test `Selection.AScribbleClosesIt` |
| Drawing an R repeats the last miracle called up | done | Step (d): `RepeatLookedFor` (no fight, an R in the tables, the selection not open with the hand ready, the hand ready, `CanRepeat`) and `RepeatStage` (the R's icon with the last miracle), then `RepeatLast` (`RepeatLastSpell`, `src/Worship/PlayerSpellIcons.cpp`). The tables' 40 s repeat time-out has no reader in the frozen exe (only the tables' generic access), so no time limit applies, as in ours. Tests `RepeatStage.RIsLookedForOnlyWithTheHandReadyAndNothingElseOpen`, `RepeatStage.TheRShowsTheLastMiracleAndRepeatsItWhenDrawn` |
| A miracle called up only becomes active in the hand after a short delay (1.5 seconds) | done | `delayBeforeSeedActive` in `seed::ProcessInHand` (`src/Magic/Core/SpellSeed.cpp`) |
| While the selection is open the gestures that can be drawn are shown as icons on the screen | partial 90% | The gesture icons ([gestures](../../../magic/gestures.md#the-gesture-icons)): the pass's table makes the list at the pass's end, made again only when the table changes (`GestureIconList`, `src/Magic/Gestures/GestureIcons.h`), each icon's symbol in the player's colour at its slot at the bottom right, its sparkles along the shape and the fade while a help text is on the screen, drawn by `Renderer::DrawGestureIcon` (`src/Graphics/RendererGestureIcons.cpp`); before each symbol the depth of the box above it is reset and its model drawn there with a camera of its own (`src/Magic/Gestures/GestureIconModels.h`): the miracle bubble for the spiral, the leash's mesh for the leashes, and for a seed's icon (the inverse spiral's too, seed 22) the seed's graphic of its magic: its mesh turning (a phial with its frames and pulse, the fat and thin squashes, the big and small second draws, the invisible one in the land's light), the sprites of its holder effect stepped by the icon's draws on the local stream and drawn with that camera, and its power-up bands (`src/Magic/Gestures/GestureIconSeeds.h`); tests `GestureIcons.*`, `GestureIconModels.*` and `GestureIconSeeds.*` with the original's numbers; the gesture window lists the icons, their models and the seeds' graphics. Not drawn there yet: the freeze phial's frozen look (openblack has no frozen draw of a model), the holder effects' mists and meshes (the storm's cloud, the water's mist, the shield's sphere), the glints on a phial, and the box's depth reset, placed behind the eye by `screen_point::WorldAtDepth` ([gestures](../../../magic/gestures.md#pending)) |
| The tutorial counts when the selection is opened and when a miracle is called up by gesture, to know the player has learnt it | done | Help events 15, 16 and 17 are counted in the help profile (`src/Help/HelpProfile.cpp`), which the scripts read through the help event functions in `src/CHLApi.cpp` |

## Other rules

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every recognised gesture starts a force-feedback effect on supporting mice | pending | To port as the original's (user, 2026-10-19): `Success` starts immersion effect 3 on a recognition, and the pass's other effects are 8, 9 and 10 ([gestures](../../../magic/gestures.md#seeds-the-help-and-the-addresses)). Waits for the force feedback, which openblack does not have yet |
| The game counts the gestures drawn, for the help and the statistics | partial 90% | Every gesture help event (14 to 23) also counts in the help record's gesture total (`src/Help/HelpProfile.cpp`, test `HelpProfile.EventsAlsoCountAsTheirGroups`), shown as "Gestures Done" in the statistics box ([../interface/statistics_counted.md](../interface/statistics_counted.md)); as in the original, only this computer's interface raises them. Help event 19, the creature's special move drawn in a fight, waits for the creature's fight controls (the fight's gestures send packets 0x22 and 0x5A, [gestures](../../../magic/gestures.md#what-is-looked-for-and-when)) |
| Rival gods controlled by the computer call up miracles by gesture or from their icons too | pending | Waits for the gods' casting actions: there are no computer gods' hands, and their minds make a miracle's gesture far from its icon ([computer hands](../../../hand/computer-hands.md)). See [../hand/other_players_hands.md](../hand/other_players_hands.md) |
