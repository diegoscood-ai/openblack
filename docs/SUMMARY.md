# openblack wiki

How the original Black & White (the v1.42 `runblack.exe`) behaves, with addresses and formulas, and what openblack does. Every page has the same four sections: Summary, The original, openblack, Pending.

## 🛠️ Project

- [Progress dashboard](wiki/project/dashboard.md)
- [Milestones](wiki/project/milestones.md)
- [raffclar's work in our tree](wiki/project/raffclar-status.md)
- [Reading this wiki](wiki/project/reading-the-wiki.md)
- [Port audit: raffclar's target build (history)](wiki/project/port-audit.md)
- [A reconstruction, not a decompilation](wiki/project/reconstruction.md)
- [Not in the original](wiki/project/not-in-the-original.md)
- [Deliberate deviations taken from raffclar's tree](wiki/project/deviations.md)
- [Credits and sources](wiki/project/credits-and-sources.md)
- [Refactor conventions](wiki/project/refactor-conventions.md)
- [Port decisions: engine and world](wiki/project/port-decisions-engine-world.md)
- [Port decisions: villagers, towns and animals](wiki/project/port-decisions-beings.md)
- [Port decisions: the creature](wiki/project/port-decisions-creature.md)
- [Port decisions: magic](wiki/project/port-decisions-magic.md)
- [Port decisions: hand, camera and interface](wiki/project/port-decisions-hand-camera-interface.md)
- [Port decisions: audio, video and scripts](wiki/project/port-decisions-audio-video-scripts.md)
- [Testing and verifying a change](wiki/project/testing.md)
- [Testbed scenarios](wiki/project/scenarios.md)
- [The minimal mod loader](wiki/project/mod-loader.md)
- [Progress: what openblack does, row by row](wiki/project/progress/README.md)
  - one folder per domain under wiki/project/progress/ (31 domains, 359 files; the viewer lists them from INDEX)
  - [Progress index](wiki/project/progress/INDEX.md)
- [Research tools](wiki/project/research-tools.md)
- [Debugging openblack](wiki/project/debugging.md)
- [openblack internals](wiki/project/internals.md)

## ⚙️ Engine

- [The engine loop: turns and frames](wiki/engine/loop.md)
- [The game clock](wiki/engine/game-clock.md)
- [Recording and replaying the input](wiki/engine/input-recording.md)
- [Random numbers and streams](wiki/engine/random.md)
- [Floating point](wiki/engine/floating-point.md)
- [Coordinates, distances and angles](wiki/engine/coordinates-and-angles.md)
- [Matrices and the Zoomer](wiki/engine/matrices-and-zoomer.md)
- [Object size and map cells](wiki/engine/object-size-and-map-cells.md)
- [Saved games](wiki/engine/saved-games.md)

## 🎨 Rendering

- [The frame and its draw stages](wiki/rendering/frame.md)
- [The object draw list](wiki/rendering/object-draw-list.md)
- [Visible blocks and level of detail](wiki/rendering/visible-blocks-and-lod.md)
- [Textures](wiki/rendering/textures.md)
- [Model lighting and colour](wiki/rendering/model-lighting.md)
- [Transparency and render modes](wiki/rendering/transparency-and-render-modes.md)
- [Meshes and level of detail](wiki/rendering/meshes-and-lod.md)
- [Shadows and reflections](wiki/rendering/shadows-and-reflections.md)

## 💥 Physics and particles

- [Physics](wiki/physics/physics.md)
- [Physics impacts](wiki/physics/physics-impacts.md)
- [Particles: the PSys core](wiki/physics/particles.md)
- [Particle creators](wiki/physics/particle-creators.md)
- [Particle rules](wiki/physics/particle-rules.md)

## 🌍 World

- [Terrain](wiki/world/terrain.md)
- [Haze and mist](wiki/world/haze-and-mist.md)
- [Sea, coast and rivers](wiki/world/sea-coast-and-rivers.md)
- [Water](wiki/world/water.md)
- [Sea life and fixed scenery](wiki/world/sea-life-and-scenery.md)
- [Sky](wiki/world/sky.md)
- [Day and night](wiki/world/day-and-night.md)
- [Weather and climate](wiki/world/weather.md)
- [Trees and forests](wiki/world/trees.md)
- [Objects](wiki/world/objects.md)
- [Buildings](wiki/world/buildings.md)
- [Plans and building sites](wiki/world/building-sites.md)
- [Vortexes](wiki/world/vortex.md)
- [Fire](wiki/world/fire.md)

## 🧑‍🌾 Beings

- [Villagers: data and states](wiki/beings/villager-states.md)
- [Villagers: speed and scripts](wiki/beings/villager-speed-and-scripts.md)
- [Villagers: deciding what to do](wiki/beings/villager-decisions.md)
- [Towns](wiki/beings/towns.md)
- [Villagers: home, food, sleep, age and births](wiki/beings/villager-life.md)
- [Villagers: jobs](wiki/beings/villager-jobs.md)
- [Villagers: reactions](wiki/beings/villager-reactions.md)
- [Villagers: picked up, dropped, hurt and dead](wiki/beings/villager-hand-and-death.md)
- [Walking round obstacles](wiki/beings/wall-hugging.md)
- [Routes and footpaths](wiki/beings/routes.md)
- [Animals: species and the turn](wiki/beings/animals.md)
- [Animals: reactions, flocks and bodies](wiki/beings/animal-life.md)
- [Skeletal animation](wiki/beings/animation.md)
- [Dances](wiki/beings/dances.md)

## 🐄 Creature

- [The creature](wiki/creature/overview.md)
- [The creature's body](wiki/creature/body.md)
- [Drawing the creature](wiki/creature/drawing.md)
- [The creature's pose and animation](wiki/creature/pose-and-animation.md)
- [The creature's mind](wiki/creature/mind.md)
- [Plans and needs](wiki/creature/plans-and-needs.md)
- [The creature casting miracles](wiki/creature/casting.md)
- [The creature under a script](wiki/creature/script-control.md)
- [Hits and catches](wiki/creature/hits-and-catch.md)
- [Fights](wiki/creature/fights.md)
- [The leash](wiki/creature/leash.md)
- [The leash's rope](wiki/creature/leash-rope.md)
- [The temple's leash posts](wiki/creature/temple-leash-posts.md)
- [Mimicry, towns and miracle reactions](wiki/creature/mimicry.md)
- [The player's creature](wiki/creature/players-creature.md)
- [The tattoo editor](wiki/creature/tattoos.md)

## ✨ Magic

- [The spell core](wiki/magic/spell-core.md)
- [Gestures](wiki/magic/gestures.md)
- [Casting from the hand](wiki/magic/hand-casting.md)
- [Worship](wiki/magic/worship.md)
- [The temple and the citadel](wiki/magic/temple-and-citadel.md)
- [The village totem](wiki/magic/village-totem.md)
- [The puzzle totem](wiki/magic/puzzle-totem.md)
- [Influence and alignment](wiki/magic/influence-and-alignment.md)
- [Food, wood, water and heal](wiki/magic/food-wood-water-heal.md)
- [Forest](wiki/magic/forest.md)
- [Flocks](wiki/magic/flocks.md)
- [Fireball and lightning](wiki/magic/fireball-and-lightning.md)
- [Lightning explosion and exploding objects](wiki/magic/lightning-explosion.md)
- [Shields](wiki/magic/shields.md)
- [Teleport](wiki/magic/teleport.md)
- [Storm and tornado](wiki/magic/storm-and-tornado.md)
- [The falling spell](wiki/magic/falling-spell.md)

## ✋ Hand

- [The hand: animation and placement](wiki/hand/animation-and-placement.md)
- [Picking: the object under the cursor and the near object](wiki/hand/picking.md)
- [Hand and interface states](wiki/hand/states.md)
- [Grabbing, holding and throwing](wiki/hand/grab-hold-and-throw.md)
- [The computer players' hands](wiki/hand/computer-hands.md)
- [The computer players' minds](wiki/hand/computer-minds.md)

## 🎥 Camera

- [The game camera](wiki/camera/game-camera.md)
- [The player camera](wiki/camera/player-camera.md)
- [The script camera and camera tracks](wiki/camera/script-camera.md)

## 🖥️ Interface

- [Tooltips](wiki/interface/tooltips.md)
- [Screen, text and read-outs](wiki/interface/screen-and-readouts.md)
- [Script highlights](wiki/interface/script-highlights.md)
- [Advisors and help](wiki/interface/advisors-and-help.md)
- [The temple interior](wiki/interface/temple-interior.md)

## 🔊 Audio

- [The audio engine](wiki/audio/engine.md)
- [Banks and channels](wiki/audio/banks-and-channels.md)
- [Music](wiki/audio/music.md)
- [Voices and texts](wiki/audio/voices.md)
- [Script audio](wiki/audio/script-audio.md)
- [World sounds](wiki/audio/world-sounds.md)
- [Magic and weather sounds](wiki/audio/magic-and-weather-sounds.md)
- [Creature and interface sounds](wiki/audio/creature-and-interface-sounds.md)

## 🎬 Video

- [Bink video](wiki/video/bink.md)
- [Film playback](wiki/video/playback.md)

## 📜 Scripts

- [The land script reader](wiki/scripts/map-script-reader.md)
- [Land script objects](wiki/scripts/map-script-objects.md)
- [CHL commands that move things](wiki/scripts/script-commands.md)
- [Land changes, skipping the tutorial and saving](wiki/scripts/land-change-and-saving.md)
- [The Land 1 intro](wiki/scripts/land1-intro.md)

## 🗂️ File formats

- [info.dat](wiki/formats/info-dat.md)
- [Land and script files](wiki/formats/land-and-script-files.md)
- [Media files and data packs](wiki/formats/media-files.md)
