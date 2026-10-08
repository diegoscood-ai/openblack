# Desires

A creature is driven by about forty desires (hunger, anger, compassion, the wish to play, to sleep, to be friends …).
Each desire grows from its sources (low energy, darkness, the player's strokes, its own innate character …) once they
pass their thresholds, fades otherwise, and the strongest desires decide what the creature does and what it shows the
player.

**Progress: 22/71 done, 29 partial — 51%**

How the original does it, in our wiki: [The creature: groundwork, random streams and what is unknown](../../bw1-notes/creature.md).

## How desires work

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every desire has its own value, maximum, decay and weight, set per species from the game's creature tables | done | `creature_desires::Create` (`src/Creature/CreatureDesires.h`), `SetupFor` in `CreatureMindSystem.cpp`; test `CreatureDesires.CreatedFromTheSetup` |
| A desire grows from up to eight sources, each with a value and a threshold | done | `creature_desires::Source`, `k_MaxSources` (`src/Creature/CreatureDesires.h`) |
| How hard a source drives its desire follows the game's sigmoid of how far its value is past its threshold (a table of 41 steps) | done | `creature_desires::Sigmoid` (the game's 41 floats, `gutils::SigmoidThreshold`); test `CreatureDesires.TheSigmoidSteps` |
| A desire grows by its sources' drive over its species' growing time, never past its maximum | done | `creature_desires::UpdateDesires` in `CreatureMindSystem::ProcessTurn`; test `CreatureDesires.ADesireGrowsByItsDriveOverItsIncreaseTime` |
| A desire nothing drives fades by its decay each turn | done | `creature_desires::UpdateDesires`; test `CreatureDesires.ADesireWithoutDriveFades` |
| Each creature's decay is picked at random within its species' range when it is made | done | `creature_desires::Create` with a synchronised `GameFloatRange` draw per desire in desire order; test `CreatureDesires.EachDesireDrawsItsDecayOnceInDesireOrder`. Our wiki differs: the range is the desire's own, the same for every species, and whether the drawn number is a decay or a starting value is still open ([creature.md](../../bw1-notes/creature.md#the-creature-tables-in-infodat)) |
| Sources that follow the creature's state are read again every turn; the rest fade by their own multiplier | done | `creature_desires::UpdateSources` with `ReadSource` in `CreatureMindSystem::ProcessTurn`; test `CreatureDesires.SourcesReadTheirStateThenFade` |
| Events push sources up or down (a stroke, a slap, a nasty miracle, a fizzled miracle) | partial | `creature_desires::ChangeSource` from `CreatureMindSystem::LearnFromFeedback`: only the stroke, slap and copying pushes exist; nasty miracles, fizzles, the player's deeds, villagers and deserving objects push nothing; test `CreatureDesires.FeedbackPushesEverySourceOfAType` |
| A desire can be held back for a while, during which it only fades | done | `creature_desires::Suppress`; test `CreatureDesires.ASuppressedDesireOnlyFades` |
| Having drunk it doesn't want water for a while; having had a poo, it doesn't want another for longer | done | `TakeEffect` in `CreatureMindSystem.cpp` (`k_WaterSuppressSeconds` 20, `k_PooSuppressSeconds` 60; unconfirmed against the game's times) |
| Deciding on a desire holds back the desires opposed to it | done | `creature_learning::SuppressOpposed` in `CreatureMindSystem::Adopt`; test `CreatureLearning.DecidingSuppressesOpposedDesires` |
| Doing an action multiplies the desire it serves down by the action table's factor | done | `Satisfied` in `CreatureMindSystem.cpp`, from the game's action table |
| A desire's weight (how much it matters) and its sources' thresholds change with the player's lessons | done | `creature_learning::LearnDesireLesson` in `CreatureMindSystem::LearnFromFeedback`; see [learning](learning_from_feedback.md) |
| Lessons about one desire spread to the desires that depend on it | done | `creature_learning::Spread`; test `CreatureLearning.SpreadsLessonsByDependency` |
| The sum of all active desires is kept and feeds the wish to show how it feels | done | `Desires::sum`, `ReadSource` (`k_ManifestState`) in `CreatureMindSystem.cpp` |
| Only desires the creature's stage of growing up has brought are active | done | `creature_desires::ActivateForPhase` in `CreatureMindSystem::ProcessTurn`; test `CreatureDesires.GrowingUpBringsAndTakesDesires`; see [development phases](development_phases.md) |
| The dominant desire is the strongest active one | done | `creature_desires::StrongestShowable`, the dominant desire attribute in `mind_detail::BeliefOf`; test `CreatureDesires.TheStrongestShowableDesire` |
| The game finds the dominant desire that can be helped, and the one that can be shown, separately | partial | only the strongest showable one exists, above a stand-in minimum of 0.2 (`creature_mind::k_MinDesireShown`) |
| The strongest physical desire (hunger, thirst, tiredness, poo) is found for emergencies | partial | the idle mind sees to the strongest need above a stand-in threshold of 0.3 (`creature_mind::k_ActOnNeed`) rather than the game's rule; test `CreatureNeedsMind.TheStrongestNeedWithTheMeansAtHandIsSeenTo` |
| A desire can be made fully dominant, its source changed, as the mood spells and lessons do | done | `creature_learning::MakeFullyDominant` (lessons, copying); the mood spells set their desire to its maximum (`creature_spells::Apply`); tests `CreatureLearning.DominanceAndActions`, `CreatureSpells.TheMoodSpellsMakeTheirDesireDominant` |
| A desire can be made least dominant (below the weakest of the others), as a slap for it does | done | `creature_learning::MakeLeastDominant` in `CreatureMindSystem::LearnFromFeedback` |
| The mood and need spells make one desire dominant and hold the others down while they last | partial | `creature_spells::Apply` (through `spell_creature::ProcessTurn`) turns the desire on at its maximum, and at the end makes it the least; the other desires are not held down; test `CreatureSpells.TheMoodSpellsMakeTheirDesireDominant`; see [creature spells](../miracles/) |
| Leashes force a desire (anger on the aggression leash, compassion on the compassion leash, obedience when led) | partial | `mind.leash.forcedDesire` set by `LeashSystem`, read in `CreatureMindSystem::PlanCreature`; see [leash](leash.md) |
| Each source has the species' starting value and threshold: innate niceness, aggression, lethargy, friendliness and communicativeness are a species' character | done | `SetupFor` reads the per-species source tables |
| A creature's desires and sources are saved in and restored from its mind file | partial | read back from the file (`creature_mind_model::FromFile` in `CreatureMindSystem::TakeUpFile`); `SaveMind` writes them but nothing in the game saves the mind; see [saves and files](saves_and_files.md) |

## The desires and what drives them

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| To impress: grows from watching the player impress and from seeing things that deserve it; shown by impressive poses, throws and miracles | partial | the desire and its actions (`ShowImpressiveAnimation`, `ThrowToImpress` in `CreaturePlanActions.cpp`) exist; neither source is fed |
| Compassion: from watching the player be kind, from seeing those who deserve it, from being content, and from innate niceness | partial | a stroke before it can learn pushes the watching source, innate niceness is set; seeing the deserving and contentment are not fed; acted on by stroking (`Stroke`); creatures cast no heals |
| Anger: from watching the player be cruel, from those who deserve it, from being dissatisfied, from being hurt, from sadness, and innate aggression | partial | slaps push "from being hurt", sadness is read, innate aggression set; real damage, dissatisfaction and watching don't push it; acted on by hurling, stomping and kicking; no lightning |
| To play: from watching the player play and from watching villagers play | partial | strokes push the first; villagers playing feed nothing; acted on by throwing things about, kicking balls and silly faces |
| Hunger: from low energy, from watching villagers eat, and from sadness | partial | low energy is read each turn (`creature_physiology::SourceValue`); sadness and watching villagers eat are not fed |
| Fear: from darkness, from being hurt, and from seeing frightening miracles | partial | darkness reads night as fully on or off (`IsNight`), slaps push it; real damage and miracles don't; acted on by running away and being frightened on the spot |
| Curiosity | partial | the desire and its actions (examining by picking up, looking and following) exist; nothing feeds its source beyond its starting value |
| To poo, from the poo built up by eating | done | `creature_physiology::SourceValue`; test `CreatureNeedsMind.APooTakesFourSecondsAndDropsAsItEnds` |
| Tiredness: from exhaustion, laziness, night time, sadness, and innate lethargy | partial | exhaustion, night and sadness read; laziness not fed |
| To idle about with the player | partial | the desire exists; its source isn't fed and its actions (following the hand, going to the middle of the screen) are missing |
| Wanderlust | todo | no exploring actions (coast, towns, hills); see [idle behaviour](idle_behaviour.md) |
| To be sick | partial | the action exists (`Puke`); nothing makes it want to be sick |
| To build its home | todo | see [home and pen](home_and_pen.md) |
| To bring things home | todo | see [home and pen](home_and_pen.md) |
| Thirst, from dehydration | done | `creature_physiology::SourceValue`; test `CreatureNeedsMind.ItDrinksAtTheWatersEdge` |
| To restore its health, from lost life | partial | the source reads life; resting to get better and healing itself aren't actions (sleep heals) |
| To be friends, and innate friendliness | partial | the sources exist; only smiling and waving at a friend are acted on (`SmileAtFriend`, `WaveAtFriend`); see [friends](friends_and_other_creatures.md) |
| To get the player's attention, from loneliness and from lack of interaction | partial | read from seconds alone, fully after one and two minutes (`ReadSource`), measures not confirmed; acted on by being pathetic, howling and pointing at the camera |
| To show how it feels, and innate communicativeness | partial | read from the sum of desires, fully at 3 (unconfirmed); the communicate-state action plays |
| To get warmer, and to get colder | partial | read from warmth (`creature_physiology::SourceValue`); shown by shivering and showing it's hot; starting fires and warming or cooling spells are missing |
| To scratch, from itchiness | done | read from itchiness (`creature_physiology::SourceValue`); the `Scratch` action plays |
| To run away from the player | partial | read from a poor attitude to the player (unconfirmed); the run-away action works; forgiven when stroked for it (`CreatureMindSystem::LearnFromFeedback`) |
| To rest | partial | the desire exists; resting on the spot and going home to recover are missing |
| To obey the player | partial | forced by the leash; see [leash](leash.md) |
| Illness | partial | the ill spell turns it on (`creature_spells::EffectOf`); sneezing plays; curing illness is missing |
| To obey another creature | todo | |
| Sadness | partial | strokes hold it back (`Suppress` in `LearnFromFeedback`); the `BeSad` action plays; nothing in the world pushes it (fizzles, lost fights, lost friends) |
| To go home | todo | see [home and pen](home_and_pen.md) |
| To tell the player what it thinks of him | todo | showing how nice it thinks the player is, being cross with the player |
| To play with the player | todo | the throwing game with the player and being silly with the player |
| To tell another creature what it thinks of him | todo | see [friends](friends_and_other_creatures.md) |
| To teach a friend | todo | see [friends](friends_and_other_creatures.md) |
| To follow what the player seems to want | partial | a stroke while copying pushes it (`k_FollowPlayerSource`); see [learning by watching](learning_by_observation.md) |
| To get high | todo | |
| To hang around at home | partial | hanging around (`HangAroundAtHome`) walks somewhere nearby rather than near its home |
| Mental illness | todo | |
| To miss a friend | todo | |
| To look around | partial | looking about plays, and looking at the mountains, the sea, the sun and the moon (`CreaturePlanActions.cpp`); see [idle behaviour](idle_behaviour.md) |
| To steal | todo | |

## Scripts and desires

The script language itself is the story domain's: see [../story/](../story/).

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A script can set a desire's value | todo | CREATURE_SET_DESIRE_VALUE is a `NotImplemented` stub in `src/CHLApi.cpp` |
| A script can turn a desire on or off | todo | both CREATURE_SET_DESIRE_ACTIVATED natives are `NotImplemented` stubs in `src/CHLApi.cpp` |
| A script can set a desire's maximum | todo | CREATURE_SET_DESIRE_MAXIMUM is a `NotImplemented` stub in `src/CHLApi.cpp` |
| A script can turn off every desire | todo | CREATURE_TURN_OFF_ALL_DESIRES is a `NotImplemented` stub in `src/CHLApi.cpp` |
| A script can make one desire the creature's only one, and turn that off again | todo | SET_CREATURE_ONLY_DESIRE and SET_CREATURE_ONLY_DESIRE_OFF are `NotImplemented` stubs in `src/CHLApi.cpp` |
| A script can ask whether a creature's desire is a given one | todo | CREATURE_DESIRE_IS is a `NotImplemented` stub in `src/CHLApi.cpp` |
| A script can turn a desire on and make it as strong as it gets | todo | |
