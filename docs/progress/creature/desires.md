# Desires

A creature is driven by about forty desires (hunger, anger, compassion, the wish to play, to sleep, to be friends …).
Each desire grows from its sources (low energy, darkness, the player's strokes, its own innate character …) once they
pass their thresholds, fades otherwise, and the strongest desires decide what the creature does and what it shows the
player.

**Progress: 25/71 done, 0 chosen, 26 partial, 11 inferred — 51%**

How the original does it, in our wiki: [The creature: groundwork, random streams and what is unknown](../../../creature/mimicry.md).

## How desires work

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every desire has its own value, maximum, decay and weight, set per species from the game's creature tables | done | `creature_desires::Create` (`src/Creature/CreatureDesires.h`), `SetupFor` in `CreatureMindSystem.cpp`; test `CreatureDesires.CreatedFromTheSetup` |
| A desire grows from up to eight sources, each with a value and a threshold | done | `creature_desires::Source`, `k_MaxSources` (`src/Creature/CreatureDesires.h`) |
| How hard a source drives its desire follows the game's sigmoid of how far its value is past its threshold (a table of 41 steps) | done | `creature_desires::Sigmoid` (the game's 41 floats, `gutils::SigmoidThreshold`); test `CreatureDesires.TheSigmoidSteps` |
| A desire grows by its sources' drive over its species' growing time, never past its maximum | done | `creature_desires::UpdateDesires` in `CreatureMindSystem::ProcessTurn`; test `CreatureDesires.ADesireGrowsByItsDriveOverItsIncreaseTime` |
| A desire nothing drives fades by its decay each turn | done | `creature_desires::UpdateDesires`; test `CreatureDesires.ADesireWithoutDriveFades` |
| Each creature's per-turn decay of each desire is drawn at random within the desire's own range (the same for every species) when it is made | done | `creature_desires::Create` with a synchronised `GameFloatRange` draw per desire in desire order, from the desire row's range, as the original's 0x4DC100 ([info-dat.md](../../../formats/info-dat.md#the-creature-tables-in-infodat)); test `CreatureDesires.EachDesireDrawsItsDecayOnceInDesireOrder` |
| Sources that follow the creature's state are read again every turn; the rest fade by their own multiplier | done | `creature_desires::UpdateSources` with `ReadSource` in `CreatureMindSystem::ProcessTurn`; test `CreatureDesires.SourcesReadTheirStateThenFade` |
| Events push sources up or down (a stroke, a slap, a blow, a nasty miracle, a fizzled miracle) | partial 40% | `creature_desires::ChangeSource`: the stroke, the slap and copying (`CreatureMindSystem::LearnFromFeedback`), the strike's fear and anger from damage (`src/ECS/CreatureStruck.cpp`) and a nasty miracle's fright (test `CreatureMindSystemTest.ANastyMiracleFrightensItAndItRunsUnlessOnTheRopeOrUnafraid`); fizzles, the player's deeds, villagers and deserving objects push nothing; test `CreatureDesires.FeedbackPushesEverySourceOfAType` |
| A desire can be held back for a while, during which it only fades | done | `creature_desires::Suppress`; test `CreatureDesires.ASuppressedDesireOnlyFades` |
| Having drunk it doesn't want water for a while; having had a poo, it doesn't want another for longer | inferred | `TakeEffect` in `CreatureMindSystem.cpp` (`k_WaterSuppressSeconds` 20, `k_PooSuppressSeconds` 60): openblack's times; the original's hold on these desires after drinking or a poo is not read |
| Deciding on a desire holds back the desires opposed to it | done | `creature_learning::SuppressOpposed` in `CreatureMindSystem::Adopt`; test `CreatureLearning.DecidingSuppressesOpposedDesires` |
| Doing an action multiplies the desire it serves down by the action table's factor | done | `Satisfied` in `CreatureMindSystem.cpp`, from the game's action table (the action's desire and its multiplier, as the original's 0x4DC521, [info-dat.md](../../../formats/info-dat.md)); test `CreatureLearning.AfterAnActionItsDesireIsMultipliedToTheFloorFirstThenItsMaximum` |
| A desire's weight (how much it matters) and its sources' thresholds change with the player's lessons | done | `creature_learning::LearnDesireLesson` in `CreatureMindSystem::LearnFromFeedback`; see [learning](learning_from_feedback.md) |
| Lessons about one desire spread to the desires that depend on it | done | `creature_learning::Spread`; test `CreatureLearning.SpreadsLessonsByDependency` |
| The sum of all active desires is kept and feeds the wish to show how it feels | partial 70% | `Desires::sum`, `ReadSource` (`k_ManifestState`) in `CreatureMindSystem.cpp`; the source's reading (full at 3) is not read in the exe |
| Only desires the creature's stage of growing up has brought are active | done | `creature_desires::ActivateForPhase` in `CreatureMindSystem::ProcessTurn`; test `CreatureDesires.GrowingUpBringsAndTakesDesires`; see [development phases](development_phases.md) |
| The dominant desire is the strongest active one | done | `creature_desires::StrongestShowable`, the dominant desire attribute in `mind_detail::BeliefOf`; test `CreatureDesires.TheStrongestShowableDesire` |
| The game finds the dominant desire that can be helped, and the one that can be shown, separately | partial 30% | only the strongest showable one exists, above a stand-in minimum of 0.2 (`creature_mind::k_MinDesireShown`) |
| The strongest physical desire (hunger, thirst, tiredness, poo) is found for emergencies | partial 30% | the idle mind sees to the strongest need above a stand-in threshold of 0.3 (`creature_mind::k_ActOnNeed`) rather than the game's rule; test `CreatureNeedsMind.TheStrongestNeedWithTheMeansAtHandIsSeenTo` |
| A desire can be made fully dominant, its source changed, as the mood spells and lessons do | done | `creature_learning::MakeFullyDominant` (lessons, copying); the mood spells set their desire to its maximum (`creature_spells::Apply`); tests `CreatureLearning.DominanceAndActions`, `CreatureSpells.TheMoodSpellsMakeTheirDesireDominant` |
| A desire can be made least dominant (below the weakest of the others), as a slap for it does | done | `creature_learning::MakeLeastDominant` in `CreatureMindSystem::LearnFromFeedback` |
| The mood and need spells make one desire dominant and hold the others down while they last | done | the spells make their desire fully dominant through the shared slot and hold the others down for its seconds (`SpellCreature`, `creature_spell_mind::SetCheatDominant`), as the original's 0x4DCE20 ([leash.md](../../../creature/leash.md)); at the end it is made the least; test `CreatureSpells.TheMoodSpellsMakeTheirDesireDominant`; see [creature spells](../miracles/) |
| Leashes force a desire (anger on the aggression leash, compassion on the compassion leash, obedience when led) | partial 80% | the leash makes its desire dominant for 36000 s as it is put on and every turn (`creature_spell_mind::SetCheatDominant` from `LeashSystem`), the mind counts it down each turn (`StepCheat` in `CreatureMindSystem::ProcessTurn`), as the original's cheat-dominant desire ([leash.md](../../../creature/leash.md)); obedience when led is the leash's own; see [leash](leash.md) |
| Each source has the species' starting value and threshold: innate niceness, aggression, lethargy, friendliness and communicativeness are a species' character | done | `SetupFor` reads the per-species source tables; test `CreatureMindSystemTest.TheDesiresStartFromTheSpeciesTables` |
| A creature's desires and sources are saved in and restored from its mind file | partial 50% | read back from the file (`creature_mind_model::FromFile` in `CreatureMindSystem::TakeUpFile`), the decay and weight left as drawn, as the original's reader ([info-dat.md](../../../formats/info-dat.md#the-creature-tables-in-infodat)); `SaveMind` writes them but the game's saving of the mind is not wired (issue #187); see [saves and files](saves_and_files.md) |

## The desires and what drives them

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| To impress: grows from watching the player impress and from seeing things that deserve it; shown by impressive poses, throws and miracles | partial 40% | the desire and its actions (`ShowImpressiveAnimation`, `ThrowToImpress` in `CreaturePlanActions.cpp`) exist; seeing again a whole thing more than a tenth useful to it stirs "from seeing things" by a half, at most once in 200 turns a thing (`StirSourcesOnSighting`, as the original's 0x4D7E80, [mind.md](../../../creature/mind.md)); watching the player is not fed |
| Compassion: from watching the player be kind, from seeing those who deserve it, from being content, and from innate niceness | partial 50% | a stroke before it can learn pushes the watching source, innate niceness is set; seeing again a whole thing useful to it stirs "from seeing those who deserve it" (`StirSourcesOnSighting`); contentment is not fed; acted on by stroking (`Stroke`) |
| Anger: from watching the player be cruel, from those who deserve it, from being dissatisfied, from being hurt, from sadness, and innate aggression | partial 60% | slaps and a thrown thing's blow push "from being hurt" (`creature_struck`), sadness is read, innate aggression set, seeing again a whole thing useful to it stirs "from those who deserve it" (`StirSourcesOnSighting`); dissatisfaction and watching don't push it; acted on by hurling, stomping and kicking |
| To play: from watching the player play and from watching villagers play | partial 30% | strokes push the first; villagers playing feed nothing; acted on by throwing things about, kicking balls and silly faces |
| Hunger: from low energy, from watching villagers eat, and from sadness | partial 40% | low energy is read each turn (`creature_physiology::SourceValue`); sadness and watching villagers eat are not fed |
| Fear: from darkness, from being hurt, and from seeing frightening miracles | partial 60% | darkness reads night as fully on or off (`IsNight`, not checked against the exe), slaps and a thrown thing's blow push it (`creature_struck`), a nasty miracle frightens it (test `CreatureMindSystemTest.ANastyMiracleFrightensItAndItRunsUnlessOnTheRopeOrUnafraid`); acted on by running away and being frightened on the spot |
| Curiosity | partial 30% | the desire and its actions (examining by picking up, looking and following) exist; nothing feeds its source beyond its starting value |
| To poo, from the poo built up by eating | done | `creature_physiology::SourceValue`; test `CreatureNeedsMind.APooTakesFourSecondsAndDropsAsItEnds` |
| Tiredness: from exhaustion, laziness, night time, sadness, and innate lethargy | partial 70% | exhaustion, night and sadness read; laziness not fed |
| To idle about with the player | partial 30% | the desire exists and its actions (going to the hand, going to the middle of the screen, following the player) are plan actions (raffclar PR #513, not read in the exe); its source isn't fed |
| Wanderlust | done | its exploring actions (hills, ridge, coast, towns, beach) are carried out; the planner's gate (`creature_planner::MayPlan`) refuses it past the fifth stage of growing up with exhaustion at 0.8 or more, as the original's 0x4DF090 ([plans-and-needs.md](../../../creature/plans-and-needs.md)) |
| To be sick | partial 20% | the action exists (`Puke`); nothing makes it want to be sick |
| To build its home | pending | see [home and pen](home_and_pen.md) |
| To bring things home | inferred | plan actions `BringHome`, `BringFoodHome`, `TakeFoodFromFieldToHome` (`src/Creature/CreatureBringHome.cpp`); not read against the exe; see [home and pen](home_and_pen.md) |
| Thirst, from dehydration | done | `creature_physiology::SourceValue`; test `CreaturePhysiology.TheBodyDrivesItsDesireSources` |
| To restore its health, from lost life | inferred | the source reads life; the plan actions rest to get better, go home to recover, heal itself and rest on the spot serve it (raffclar's rest and home lane, unverified against v1.42, [plans-and-needs.md](../../../creature/plans-and-needs.md)) |
| To be friends, and innate friendliness | partial 20% | the sources exist; only smiling and waving at a friend are acted on (`SmileAtFriend`, `WaveAtFriend`); see [friends](friends_and_other_creatures.md) |
| To get the player's attention, from loneliness and from lack of interaction | done | loneliness from the turns in a row the player's camera has not looked at it, lack of interaction from the turns since the leash last sent it to act, each in whole seconds, full after one and two minutes (`creature_attention::Loneliness`, `LackOfInteraction`); planned only once ignored, more than two minutes not looked at (`IgnoredByPlayer`, the planner's gate), as the original's sources 36, 37 and gate 0x4DF0C0 ([mind.md](../../../creature/mind.md#the-attention-its-god-pays-it), [plans-and-needs.md](../../../creature/plans-and-needs.md)); acted on by being pathetic, howling and pointing at the camera |
| To show how it feels, and innate communicativeness | partial 50% | read from the sum of desires, fully at 3 (not read in the exe); the communicate-state action plays |
| To get warmer, and to get colder | partial 50% | read from warmth (`creature_physiology::SourceValue`); planned only at a warmth of -0.3 or less, or 0.3 or more, as the original's gates ([plans-and-needs.md](../../../creature/plans-and-needs.md)); shown by shivering and showing it's hot; starting fires and warming or cooling spells are missing |
| To scratch, from itchiness | done | read from itchiness (`creature_physiology::SourceValue`); the `Scratch` action plays |
| To run away from the player | partial 60% | planned only below an attitude of 0 to the player, as the original's gate 0x4DF150 ([plans-and-needs.md](../../../creature/plans-and-needs.md)); what its source reads is not checked; the run-away action works; forgiven when stroked for it (`CreatureMindSystem::LearnFromFeedback`) |
| To rest | inferred | the desire exists; resting on the spot and going home to recover are plan actions (raffclar's rest and home lane, unverified against v1.42, [plans-and-needs.md](../../../creature/plans-and-needs.md)) |
| To obey the player | partial 50% | forced by the leash; see [leash](leash.md) |
| Illness | partial 30% | the ill spell turns it on (`creature_spells::EffectOf`); sneezing plays; curing illness is missing |
| To obey another creature | pending | |
| Sadness | partial 30% | strokes hold it back (`Suppress` in `LearnFromFeedback`); the `BeSad` action plays; nothing in the world pushes it (fizzles, lost fights, lost friends) |
| To go home | inferred | its sources follow the distance from home (`creature_desires::HomeSource`; raffclar's rest and home lane, unverified against v1.42, [plans-and-needs.md](../../../creature/plans-and-needs.md)); see [home and pen](home_and_pen.md) |
| To tell the player what it thinks of him | inferred | its actions, showing how nice it thinks the player is and being cross with the player, are plan actions (raffclar PR #513, not read in the exe) |
| To play with the player | inferred | being silly with the player (raffclar PR #513) and the throwing game with the player, a toy thrown at the nearest of the local player's hands (`PlayThrowingGameWithPlayer`, raffclar PR #420), are plan actions (`CreaturePlanActions.cpp`), not read in the exe |
| To tell another creature what it thinks of him | pending | see [friends](friends_and_other_creatures.md) |
| To teach a friend | pending | see [friends](friends_and_other_creatures.md) |
| To follow what the player seems to want | partial 30% | a stroke while copying pushes it (`k_FollowPlayerSource`); the original plans it only once a deed of the player's was noticed (gate 0x4DF190, [plans-and-needs.md](../../../creature/plans-and-needs.md)); see [learning by watching](learning_by_observation.md) |
| To get high | pending | |
| To hang around at home | partial 40% | its source falls from 1 at home to 0 a hundred units away (`creature_desires::HomeSource`); the original plans it only within 60 m of home (gate 0x4DF1C0, [plans-and-needs.md](../../../creature/plans-and-needs.md)); hanging around (`HangAroundAtHome`) walks somewhere nearby rather than near its home |
| Mental illness | pending | |
| To miss a friend | pending | |
| To look around | partial 50% | looking about plays, and looking at the mountains, the sea, the sun and the moon (`CreaturePlanActions.cpp`); see [idle behaviour](idle_behaviour.md) |
| To steal | pending | |

## Scripts and desires

The script language itself is the story domain's: see [../story/](../story/).

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A script can set a desire's value | inferred | CREATURE_SET_DESIRE_VALUE (`CreatureSetDesireValue` in `src/CHLApi.cpp`, `creature_desires::SetValueFromScript`: kept from 0 to 1, all of it filling its sources); tests `ScriptGuideOgreNatives.TheOgresDesires`, `CreatureDesiresFromScript.AllOfADesireFillsItsOwnSources`; the original's native is not read |
| A script can turn a desire on or off | inferred | both CREATURE_SET_DESIRE_ACTIVATED natives (`CreatureSetDesireActivated78`, `79` in `src/CHLApi.cpp`) set the desire's or every desire's activation; the original's natives are not read |
| A script can set a desire's maximum | inferred | CREATURE_SET_DESIRE_MAXIMUM (`CreatureSetDesireMaximum` in `src/CHLApi.cpp`) sets it as given; the original's native is not read |
| A script can make one desire the creature's only one, and turn that off again | done | `CreatureMindSystem::SetOnlyDesire` / `ClearOnlyDesire` behind SET_CREATURE_ONLY_DESIRE and its OFF, through the cheat-dominant desire with the needs held down too, as the original's 0x6F476D ([leash.md](../../../creature/leash.md)) |
| A script can turn off every desire | done | CREATURE_TURN_OFF_ALL_DESIRES (`CreatureTurnOffAllDesires` in `src/CHLApi.cpp`, through `CreatureMindSystem::ChangeDesires`): every desire is no longer active. A mind not yet set up is set up first, with its stage's desires active, so that the change stands. On a thing that is not a creature it logs "No script for creature" and changes nothing (the original would crash: [vanilla bugs](../vanilla_bugs/creature.md#harmless)). raffclar PR #524; unverified against v1.42 |
| A script can ask whether a creature's desire is a given one | pending | CREATURE_DESIRE_IS is a `NotImplemented` stub in `src/CHLApi.cpp` |
| A script can turn a desire on and make it as strong as it gets | inferred | which native does this is not identified |
