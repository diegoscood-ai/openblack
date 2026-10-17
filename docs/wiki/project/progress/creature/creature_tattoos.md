# Creature tattoos

The player can tattoo their creature with up to eight symbols, one on each of up to eight places on its body, each in a
colour of their choosing. Tattoos are put on and taken off in a tattoo editor, reached by clicking the creature in the
temple's Creature Cave (or from the player's profile in the main menu), and are painted into the creature's skin, where
they stay as it grows, changes shape and turns evil or good.

**Progress: 32/56 done, 10 partial — 66%**

How the original does it, in our wiki: [The creature: groundwork, random streams and what is unknown](../../../creature/overview.md).

## Opening the editor

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| In the Creature Cave, clicking the creature zooms the room's camera onto it and opens the tattoo editor over it | partial | a click on the creature target zooms onto it and opens the editor as the zoom starts, the camera following the editor's view (`TempleCameraModel::OpenTattooEditor`, `UpdateCaveTargets`); the game finds the creature by picking the mesh it draws, and the temple's creature room draws no creature yet, so only its point of the room's mesh, with no reach, finds it. The cave screen's "Tattoo editor" button (`src/Gui/CreatureCaveScreen.cpp`) and the debug menu's Debug > Tattoo Editor open it too |
| The hand's tooltip over the creature in the cave says it can be tattooed | done | `src/3D/TempleToolTips.cpp` (the tattoo tooltip for the creature target) |
| Zooming back out of the creature, or leaving the room, closes the editor | done | closing the editor zooms back out, and the movement keys, another room or leaving the temple close it as it is (`TempleCameraModel::UpdateCaveTargets`, `CloseTattooEditor`) |
| The main menu's player page has an "Edit Tattoo" button that opens the same editor on a stand-in creature, before any game is started | todo | the button is drawn (`src/Gui/GameMenu.cpp`, `Action::EditTattoo`) but nothing handles its action |
| The editor is always about the local player's own creature, whatever creature the camera follows | done | `CreatureCaveSystem::GetCreature` (the player's creature through Creature Mode and the leash service) |
| With no creature yet there is nothing to tattoo | done | the cave screen says the player has no creature (test `CreatureCaveSystemTest.WithoutACreatureThereIsNothingToTell`); the menu's stand-in creature is todo (row above) |
| In a network game the editor is not offered from the cave while the game says the creature is locked (unconfirmed exactly when) | todo | no network play; see [../multiplayer](../multiplayer/) |

## The editor screen

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A line of help text at the top tells the player to drag the symbols onto or off the creature | done | `CreatureCaveScreen.cpp` uses the game's text for it |
| The sixteen symbols are shown as pictures in two columns of eight, one either side of the creature | done | `src/Gui/TattooEditorDialog.cpp` (`DraggedSymbol` in `src/Gui/Controls.cpp`) |
| The creature is shown large in the middle, in 3D, as it really looks, with its current tattoos | todo | no close-up view of the creature; the temple's creature room draws no creature yet |
| Dragging on the creature turns it round and tilts the view up and down; let go, it keeps turning and slows to a stop | done | the round button below the creature steers the view, held between an eighth of and as high above the creature as it is far from it (`creature_tattoo_editor::Steer`, `Turn`; tests `TattooEditor.SteeringSpeedsTheViewUpByHowFarThePointerIs`, `TheViewTurnsRoundItsFocusAndSlowsDown`, `TheViewStaysBetweenAnEighthAndAsHighAsItIsFar`), and in the cave the camera takes the view |
| Left alone, the shown creature plays one of three idle actions every 20 to 40 seconds, up to ten times; a click stops it and starts the wait again | todo | nothing in our tree |
| Two colour pickers: a palette of 32 by 128 colours, and a brightness bar | done | `ColourPicker` (`src/Gui/Controls.cpp`) in `TattooEditorDialog`, the palette's cell from `creature_tattoo_editor::PaletteCell` (test `TattooEditor.ThePaletteCellSpansTheWholeControl`) |
| Brightness above the middle draws the palette colour towards white, below it towards black | done | `creature_tattoo::Brightened`, used by the dialog and by `PaletteColour`; test `CreatureTattoo.PaletteColoursBrightenAndDarken` |
| The chosen colour tints all sixteen symbol pictures so the player sees it before placing | done | `TattooEditorDialog::ApplyTint` |
| A symbol dragged over the creature highlights the nearest place under the cursor: only places facing the camera count, and only within about 64 pixels of the cursor | partial | the rule is `creature_tattoo_editor::SiteUnderPointer` (test `TattooEditor.ThePlaceUnderThePointerIsTheNearestFacingOne`), fed through `TattooEditorSystem::Hover`; nothing feeds it the places' screen points yet, which the game takes from the creature's drawn mesh in the cave |
| Each place has a name: back, head, chest, bottom, left arm, right arm, left hand, right hand | done | `creature_cave::k_SiteNames`, shown from the game's texts |
| Enter accepts and Escape cancels, as the dialog's two buttons do | done | `TattooEditorDialog::KeyDown`, OK and Cancel |
| Edits show on the creature at once; Cancel puts back the tattoos it had when the editor opened | done | `TattooEditorSystem` paints the skin as each tattoo goes on or off and `Cancel` writes back those it opened with; tests `TattooEditorSystem.ASymbolDroppedOverAPlaceGoesIntoTheSkin`, `CancelPutsBackTheTattoosItOpenedWith` |
| In a network game, OK after changing the tattoos warns with a message box instead of closing | todo | the rule is `creature_tattoo_editor::Ok` (test `TattooEditor.OkWarnsOnlyInANetworkGameAfterAChange`); no network play |
| A sound plays as a tattoo is put on, and another as one is lifted off | todo | nothing in our tree |
| A force-feedback mouse gives a pulse as a tattoo is put on or lifted off | todo | openblack has no force feedback |

## Putting tattoos on and taking them off

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Dropping a symbol on a highlighted place tattoos it there in the chosen colour | partial | `creature_tattoo_editor::Drop` and `TattooEditorSystem::Drop` (tests `TattooEditor.ASymbolDroppedOnAPlaceGoesInTheFirstEmptySlot`, `TattooEditorSystem.ASymbolDroppedOverAPlaceGoesIntoTheSkin`); no place is under the pointer in the game until the places' screen points are fed (row above) |
| A creature wears at most one tattoo on each place and at most eight in all | done | `creature_tattoo::Slots` |
| Dropping the same symbol on a place that already has it only changes its colour | done | `creature_tattoo::SlotFor`; test `CreatureTattoo.DesignsGoInTheSlotAsTheEditorPutsThem` |
| Otherwise the symbol takes the first empty slot, or failing that replaces whatever is on that place | done | `creature_tattoo::SlotFor`; same test |
| Clicking a tattoo on the body lifts it off, emptying its place, so it can be dragged to another place or off the creature | partial | `creature_tattoo_editor::Lift`, `TattooEditorDialog::Update` (tests `TattooEditor.LiftingTakesTheLastTattooOnThePlaceKeepingItsSymbol`, `TattooEditorSystem.LiftingTakesTheTattooOffTheSkin`); waits for the places' screen points like the rows above |
| Lifting a tattoo off sets the colour pickers to its colour | partial | the symbols take its colour (`TattooEditorDialog::Update`); the pickers' arrows stay where they were |
| Dropping a lifted tattoo away from the creature leaves it off | done | `creature_tattoo_editor::Drop` with no place (test `TattooEditor.DroppingAwayFromThePlacesChangesNothing`) |
| Only places the species has can be tattooed | partial | the species' places are loaded (`Loaders.cpp`) and painting skips disabled ones (`CreatureSkin.cpp`), but the cave page still offers all eight |

## Where the designs come from

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| There are sixteen designs: the game's player symbols, laid out four by four in 64-texel cells of the symbol sheet | done | `creature_tattoo::DesignFromAtlas`; test `CreatureTattoo.DesignsComeFromTheBlueOfTheirCells` |
| A design is a mask of sixteen levels from fully skin to fully tattoo | done | `creature_tattoo::Mask`, top four bits of the sheet |
| Each design is kept at five sizes, 64 down to 4 texels across, each averaging squares of four of the one before | done | `DesignFromAtlas`; test `CreatureTattoo.SmallerMasksAverageSquaresOfFourRoundingDown` |
| The player's chosen symbol (picked on the player page) is written into that player's cell of the player symbol sheet when tattoos are rebuilt | partial | nothing rebuilds the sheet; `CreatureSkinArtLoader` reads `Data/Textures/PlayersSymbols.raw` as the game last wrote it and fills blank cells from `OriginalChooseSymbol.raw` (`Game.cpp`, `Loaders.cpp`). Whether the game's tattoos are cut from the original sheet or the rebuilt one is unconfirmed |
| A player can use an image of their own as their symbol, kept in their profile | todo | nothing in our tree (unconfirmed whether it also becomes a tattoo design) |
| On low detail settings the designs are kept at half size, 32 texels across | todo | openblack always uses 64 |

## How tattoos are drawn

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Tattoos are painted into the creature's skin textures, not drawn on top of the model | done | `creature_skin::Compose` in `CreatureSkinSystem` |
| Each place is a point on one of the species' base skins, with a size, a number of quarter turns and a flip | done | `creature_tattoo::Site`, loaded per species (`Loaders.cpp`, `CreatureRig::tattooSites`); test `MorphCreatureBlock.Version21HasActionPointsEyesTattooSitesAndBank` |
| The mask used depends on the tattoo's size: the largest for a quarter of the skin's width or more, smaller ones for smaller tattoos | done | `creature_tattoo::MaskLevel`; test `CreatureTattoo.MaskSizeFollowsTheTattoosSize` |
| The mask is turned and flipped as the place says | done | `creature_tattoo::Oriented`; test `CreatureTattoo.MasksTurnAndFlip` |
| The tattoo spans its size of the largest mask and is centred on the place | done | `creature_tattoo::Paint`; test `CreatureTattoo.TattoosSpanTheirSizeOfTheLargestMask` |
| Each skin texel moves towards the tattoo's colour by the mask's level, a 4-bit channel at a time | done | `creature_tattoo::PaintTexel`; test `CreatureTattoo.TexelsMoveTowardsTheColourIn15ths` |
| A tattoo that would run over the edge of the skin is not painted | done | test `CreatureTattoo.TattoosOverAnEdgeArentPainted` |
| Tattoos are painted after the skin is blended towards evil or good, and wounds and blood are painted over them | done | `creature_skin::Compose`; test `CreatureMarks.SkinsArePaintedTattoosThenWoundsThenBlood`; see [marks.md](marks.md) and [appearance.md](appearance.md) |
| Being in the skin, tattoos stay put as the creature's body morphs fatter, thinner, evil or good, and grow with it | done | the morphs move the mesh, not the texture |
| The skin is repainted only when the tattoos, the alignment or the marks change | done | the revision counters in `CreatureSkinSystem` |
| The local player's creature always shows the local player's current tattoo set, including the editor's unsaved preview | partial | openblack paints each creature from its own slots, which is the same in a single-player game |
| Other players' creatures show their owners' tattoos, sent over the network in a compact form | todo | no network play; see [../multiplayer](../multiplayer/) |

## Keeping tattoos

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Tattoos stay on the creature through the game and from land to land | todo | no carrying of the creature between lands; `LOAD_MY_CREATURE` (`ECS/PlayerCreature.cpp`) takes the file's species, size, alignment and strength but not its tattoos; see [saves_and_files.md](saves_and_files.md) |
| Tattoos are saved and loaded with a saved game | todo | no saved games yet; see [../engine](../engine/) |
| A saved creature file keeps the tattoos as eight words: design, place and colour each | done | `creature_tattoo::FromWord` and `ToWord`, `CreatureMindFileBody.cpp`; test `CreatureMindFileBody.TattooWordsRoundTrip` |
| A saved creature file can also carry a block of tattoo image data | partial | kept and written back unchanged (`CreatureMindModel.cpp`), never drawn |
| Older creature files without tattoos give an untattooed creature | done | `CreatureMindFileBody.cpp` leaves them out; test `CreatureMindFileBody.OlderFilesLeaveTheRestToTheSpecies` |
| A creature made from a saved creature file wears its tattoos | partial | only the debug spawner's saved-creature list puts them on (`CreatureSpawnerMindFiles.cpp`, `CreatureSpawner.cpp`); `LOAD_MY_CREATURE` does not |
| Swapping to another creature from a silver scroll keeps or drops the tattoos (unconfirmed which) | todo | no creature swapping yet; see [species_choice.md](species_choice.md) |
| Uploading a creature to the online creature database sends its tattoos with it | todo | no online service; see [saves_and_files.md](saves_and_files.md) |

## Scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| No challenge script of the five lands reads or changes tattoos | n/a | nothing to do |
| Creature Isle's tattoo types (the Brotherhood, the super gods, fighters) for its scripts | n/a | Creature Isle; the values are listed in `src/ScriptHeaders/ScriptEnums.h` only |
