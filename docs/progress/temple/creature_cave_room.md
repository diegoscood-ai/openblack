# Creature cave room

The temple's room for the creature. Everything about the cave itself (the creature, its scrolls, belts and medals,
tattoos) is in ../creature/creature_cave.md; this file only lists the room's part of the temple.

**Progress: 3/6 done, 1 partial — 58%**

## The room in the temple

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A door of the main room leads to the cave | done | `TempleInterior::EnterRoom`, and the room keys in `Game::ProcessTempleRoomKeys` (`src/Game.cpp`) |
| The waterfall slides and sounds, the fire crackles where the creature stands | done | `TempleInterior` plays `temple_sounds::CreatureCaveFire` each frame the room is drawn and stops the fire and water on leaving (`audio::LeaveCitadel`); test `CreatureCaveEffects.TheRoomsSoundsAtTheFireAndTheWaterfall` |
| Flames, smoke and the waterfall's spray and mist | done | `src/3D/CreatureCaveEffects.cpp`, started by `TempleInterior`; test `test_creature_cave_effects` |
| The cave's contents | partial | `src/Gui/CreatureCaveScreen.cpp`, `CreatureCaveSystem`, `src/3D/CreatureCaveTrophies.cpp`; tests `test_creature_cave*`, `CreatureCaveSystemTest.*`; see ../creature/creature_cave.md |
| Clicking the creature opens the tattoo editor | todo | the creature is found as a target (`CreatureCaveTargets::Target::Creature`) but the click does nothing: TODO in `src/Camera/TempleCameraModel.cpp`; see ../creature/creature_tattoos.md |
| The dummies start the creature's fight practice when no temple script runs | todo | the belts and medals only zoom the camera: TODO in `TempleCameraModel::ZoomToCaveTarget` |
