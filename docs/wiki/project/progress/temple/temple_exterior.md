# Temple exterior and entrance

The player's temple on the island: built at the start of the story, it changes its look with the player's alignment and
influence, and its entrance is the way inside.

**Progress: 12/16 done, 3 partial — 84%**

## Its look

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The temple's outside is one of three sizes by the player's share of influence, in five stages from evil to good | done | `worship::citadel::Process` (`src/Worship/Citadel.cpp`) gives `TempleExteriorSystem::Step` the alignment target and twice the player's share of the influence (`influence::InfluencePower`, every slot); `src/3D/TempleExteriorMorph.cpp`; tests `TempleExteriorMorph.SizeTargetIsTwiceTheShareOfInfluence`, `TempleExteriorMorph.AlignmentTargetIsThePlayersFromZeroToJustShortOfOne` |
| Its mesh is blended from the four nearest of those meshes | done | `TempleExteriorSystem::Blend`, `TempleExteriorMorph`; test `TempleExteriorMorph.BetweenStagesAndSizesItBlendsTheFour` |
| Its textures blend from evil to neutral to good | done | `TempleExteriorSystem::BlendTexture`; test `TempleExteriorMorph.TexturesGoEvilToNeutralToGood` |
| The look moves a step a turn towards the player's alignment, the rest of the way when near | done | `TempleExteriorMorph::Turn` from `worship::citadel::Process` each turn; tests `TempleExteriorMorph.ATurnStepsTowardTheTargetsAndBlendsPastThreeHundredths`, `TempleAlignmentMenu.theTurnsToGoAreTheGamesStepsOf0016` |
| Each player's temple follows its own player | done | one `components::TempleExterior` per heart, following its owner's alignment and influence (`worship::citadel::Process`); how a temple is damaged, destroyed and lost: [../story/losing_and_game_over.md](../story/losing_and_game_over.md) |
| The first, unfinished temple of the opening, built by the villagers | partial | the plan (`CitadelArchetype::CreatePlanned`), its six-pile building site (`src/ECS/Town/BuildingSites.cpp`) and the heart drawn by its percent built (`worship::citadel::Process`) exist; whether Land 1's opening builds it as the original is not checked; see ../story/land_1.md and ../building/construction.md; a cut early draft of the opening at the temple: [see_the_citadel.md](../story/silver_scrolls/see_the_citadel.md) |
| The temple has worship sites around it | done | six slots round the citadel in `src/Worship/Citadel.cpp` (never on Land 1, as the original); see ../worship/worship_sites.md |

## Going in and out

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The entrance is its own object at the temple's door | done | `CitadelArchetype::CreateEntrance` (`components::CitadelEntrance`, `src/ECS/Archetypes/CitadelArchetype.cpp`), collided by the hand's pick through `worship::citadel::EntranceCollides` |
| The Action button on the entrance takes the player inside | done | the hand's tap on the entrance (`hand_tap::Register<CitadelEntrance>` in `CitadelArchetype.cpp`) calls `worship::citadel::EntranceTap`, which activates the temple interior; its tooltip is `worship::citadel::EntranceToolTipFor` |
| The camera flies to the entrance and in | todo | `TempleInterior::Activate` cuts straight to the room's path in; see ../camera/temple_camera.md |
| Scripts take the player in and out of the temple | done | `ENTER_EXIT_CITADEL` (`EnterExitCitadel` in `src/CHLApi.cpp`) |
| Scripts ask whether the player is inside the temple and where its entrance is | partial | `INSIDE_TEMPLE` works (`game_clock::IsInsideCitadel`); `GET_TEMPLE_ENTRANCE_POSITION` is a stub in `src/CHLApi.cpp` |
| Leaving fades out to white and back on the island | done | `TempleInterior::Deactivate` fades from white over a second (`FadeFrom`) |
| Inside, the world is paused, its ambience fades out and the temple's music plays | done | `TempleInterior::Activate` pauses the world (`game_clock::Pause`) and gives the pause back on leaving; the ambience is turned off and the citadel's music plays by alignment (`GameMusic::ProcessCitadelMusic`, `audio::ProcessCitadelTurn`); tests `GameMusicTest.CitadelMusicInsideTheCitadel`, `PausedTurnTimer.*` |
| Help is held back inside the temple | partial | inside, only the temple's help scripts run (`Game::ProcessTempleTurn`), and the help system runs on the temple's turns as our wiki says the original does; whether the world's help waits for the player to come out is not checked; see ../interface/help_system.md |
| A key takes the player straight into a room (the creature's cave) | done | the room keys (`Game::ProcessTempleRoomKeys`: main room, creature room, challenge, save game, options, library) go in at that room; F5 alone shows the cave without a temple (`CreatureCaveSystem`); test `CreatureCaveSystemTest.WithoutATempleF5ShowsTheCaveOnItsOwn` |
