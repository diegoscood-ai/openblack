# Friends and other creatures

When creatures meet they size each other up. Each keeps a feeling for every other creature it knows, and from that
they make friends, play together, teach each other, sulk or quarrel, and go looking for a friend they miss. Fights
between creatures are in [fighting.md](fighting.md).

**Progress: 13/56 done, 0 chosen, 13 partial, 2 inferred — 37%**

How the original does it, in our wiki: [The creature: groundwork, random streams and what is unknown](../../../creature/overview.md).

## Noticing and judging other creatures

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Another creature is among the most interesting things a creature looks at | done | `src/Creature/CreatureLook` (`Interest::Creature`, third after doves and the citadel), fed by `CreatureMindSystem`; test `CreatureLook.DovesAndTheCitadelCatchTheEyeMostAndFixedThingsLeast` |
| A creature keeps a feeling, from nasty to nice, for each other creature it knows | done | `creature_learning::AttitudeTo` in the mind (`CreatureMindLearning.cpp`); test `CreatureLearning.AttitudesToCreatures` |
| It judges another creature by whose it is, its species and whether it is stronger | done | the decision trees test the other creature's belief ([beliefs_and_opinions.md](beliefs_and_opinions.md)): whose, the species, "stronger" (drawn taller than the believer's player's creature, `AttributeIsHarderThanMe` 0x4D40E0) and the height band of its drawn size (`AttributeCreatureHeight` 0x4D3C70) as the exe ([mind.md](../../../creature/mind.md#the-attributes-a-belief-reads)); tests `CreatureMindSystemTest.AnotherCreatureIsHarderWhenDrawnTallerThanTheBelieversPlayersCreature`, `CreatureDecisionTree.AnotherCreaturesHeightIsInFourBandsOfTheSizeItIsDrawnAt` |
| How much more life and strength an opponent has, and how many dangerous miracles it knows, weigh on whether to take it on | inferred | the exe has `HowMuchStrongerIsOpponentThanMe` 0x4C3BF0, `HowMuchMoreLifeHasOpponentThanMe` 0x4C3C70 and `HowManyDangerousSpellsOpponentKnows` 0x4C3D00, but xref finds no reference to them, so whether the game weighs them is unverified |
| Whether another creature seems friendly | partial 50% | the rule as the exe's object filter (`IsCreatureWhoSeemsFriendly`, the Creature override 0x4E45E0): neither wants to be angry with or afraid of the other, by the desire each most wants about the other; the creature's own fear does not stop it, as the original (`creature_mind::SeemsFriendly`, `CreatureMindSystem::SeemsFriendly`; test `CreatureCastAgenda.AnotherCreatureSeemsFriendlyUnlessItIsAngryOrAfraidOrTheCreatureIsAngry`); that desire is still the planner's own weighing |
| Another creature can be frightening, and it runs from it | done | plan action `RunAwayFromObject` takes any living thing, other creatures included (`CreaturePlanActions.cpp`), through `CreatureLocomotionSystem::FleeFrom`; tests `CreatureFrightening.OnlyFearListsRunningAwayFromAnObject`, `CreatureLocomotionSystemTest` (`FleeFrom`) |
| Inspect another creature: walks up and looks it over | done | `creature_mind::InspectCreature`: near it at four of its heights, facing it 0.1 s, then the watch, curious, as the exe's `ConstructSubActionsForInspectCreature` 0x4A2A40 (GoNear at `GetHeight` × 4, TurnToFace 0.1, the main watch 0x35); taken up from another creature's reaction, which every creature makes as it is made (2026-10-20, [mind.md](../../../creature/mind.md)); tests `CreatureActionAgendas.LookingACreatureOverGoesNearItFacesItAndWatchesIt`, `CreatureReactionRules.WhatItDoesOnTakingUpACreatureAFlyingThingOrTheHand` |
| Look at another creature with a friend | pending | the exe's action `ExamineOtherCreatureWithFriend` (builder 0x4BEAA0) |
| A creature only reacts to another again after a while | partial 60% | another creature is urgent again only once due a look-over: not for the info's inspect-creature seconds (`GCreatureInfo` +0x2CC; tests `CreatureReactionRules.AnotherCreatureIsUrgentOnceDueALookOver`, `ACreatureIsDueALookOverOnlyAfterTheWait`); whether how it feels about the other also weighs is not read; see [reactions.md](reactions.md) |
| Being led together on a leash makes two creatures warmer (compassion leash) or cooler (aggression leash) to each other | done | `LeashSystem` with `creature_leash::AttitudeStepDue` and `AttitudeStep`: at once, then after more than 600 turns, the turn kept on the other creature (`CreatureLeashAttitude`); tests `LeashRules.LeashedCreaturesWarmOrCoolToEachOther`, `TheFirstAttitudeStepIsAtOnceThenAfterMoreThan600Turns`, `LeashSystemTest.LeashedToAnotherCreatureTheyWarmAtOnceThenByTheOthersLastStep` |
| Scripts can force two creatures to be friends | partial 50% | `CREATURE_FORCE_FRIENDS` (`player_creature::ForceFriends`, as the exe's `GScript::CreatureForceFriends` 0x6F5770: each in the other's list once, newest first); tests `PlayerCreatureNativesTest.ForceFriends*`. Nothing reads the list yet: the original's readers (a friend cannot be attacked, `CanBeAttackedByCreature` 0x4E4280; no usefulness for a friend, `GetActivityObjectUsefulness`; a deleted friend dropped) are not ported ([players-creature.md](../../../creature/players-creature.md)) |

## Asking and answering

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature asks another to be friends; the other decides whether to accept | partial 75% | `creature_friendship::AnswerOf` asked by a `RequestPartner` step, answered in `CreatureMindSystem::RequestPartner` (`CreatureMindFriends.cpp`): the rule as the exe's 0x4F64D0 (free, then befriending wanted above fear and anger, or a script's always-agree [0xC64148]); test `CreatureFriendship.FriendshipAndBeingShownHaveTheirOwnDesire`. The worth of each desire about the asker is the planner's own weighing (no believed place), not the exe's `CalculateDesirabilityOfProposedActivity` 0x4C45C0 |
| A creature asks another to play; the other may refuse | partial 75% | `creature_friendship::AnswerOf`, the first step of `WaveAtFriend`: the rule as the exe's 0x4F6340 (play or befriending wanted most of play, befriending, fear and anger; none above 0 refuses); test `CreatureFriendship.PlayIsAgreedToWhenPlayOrFriendshipIsWantedMost`; each desire's worth as the row above |
| A creature asks another to fight; the other may refuse | done | a fight a script forces asks first (`creature_mind::FightCreature`: `RequestPartner` with the fight request and an angry face), and every fight start asks (`CreatureFightSystem::StartFight`): from half to twice the asker's height and the other free for it, as the exe's 0x4F62D0 ([fights.md](../../../creature/fights.md)); tests `CreatureFriendship.AFightIsAgreedToOnlyNearInHeight`, `CreatureFriendship.AFightIsAskedForThenFought` |
| A creature asks a friend to teach it; the friend may oblige | pending |  |
| A creature busy elsewhere can't join in, and the asker waits for it to be free | partial 50% | a dead creature, one whose script control differs from the asker's, or one with another partner refuses (`creature_friendship::AvailabilityOf`, as the exe's 0x4F6270; test `CreatureFriendship.AvailabilityOfTheAskedCreature`); the asker gives up; whether the original's request step (0x503A50) waits instead is not read |
| The player is told when the other creature refused to fight, play or be friends | pending | the refusal's explanation is chosen (`Answer::explanation`, as the exe's help 0x28 with 5 or 6) but not raised: it waits for the creature's help messages; see [lessons_and_help.md](lessons_and_help.md) |
| A friend doing something worth following draws the creature to join in | inferred | no such rule found in the exe; its `FollowFriendAround` action (builder 0x4A2330, the row below) is not ported |

## Friendly actions

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Smile at a friend | done | plan action `SmileAtFriend`, as the original's: it goes near the friend to within three times its height, faces it for 4 s and a draw of up to 4 more, smiling with fluttering eyelids, then half the time shows it is happy (`creature_mind::SmileAtFriend`, `CreatureFriendAgendas.cpp`; the exe's builder 0x4A2190); test `CreatureFriendship.SmilingAtAFriendGoesNearFacesItAndHalfTheTimeShowsItIsHappy` |
| Wave at a friend | done | plan action `WaveAtFriend`, a joint action as the exe's `ConstructSubActionsForWaveAtFriend` 0x4BA7B0: the play request, the friend stood still, told to face the waver 0.1 s, the waver faces it 1.4 s, both wave (72), the friend let go (`creature_mind::WaveAtFriend`, `CreatureFriendAgendas.cpp`); test `CreatureFriendship.WavingAtAFriendIsAJointAction` |
| Kiss a friend | pending | the exe's builder 0x4A8B50 |
| Follow a friend around | pending | the exe's builder 0x4A2330 |
| Dance with a friend | pending | the exe's builder 0x4A2560 |
| Run a race with a friend | pending | the exe's builder 0x4A92E0 |
| Throw stones into the sea with a friend; play throwing games; throw stones at a can | pending | the exe's `ThrowStonesInTheSeaWithFriend` 0x4A0540 and `PlayThrowingGameWithFriend` 0x4B7030; the can is not checked |
| Sit on top of a hill with a friend | pending | the exe's builder 0x4AAB90 |
| Drink, poo, sit and sleep with a friend | done | plan actions `DrinkWithFriend`, `PooWithFriend`, `SitWithFriend`, `SleepWithFriend`, joint actions side by side (`creature_mind::DrinkWithFriend` and the others, `CreatureFriendAgendas.cpp`): the friend agrees to be friends, both go to their places, the asker waits for the friend to stand still, and both do it; the steps, clips and times as the exe's builders 0x4B19A0, 0x4B1FD0, 0x4B2540, 0x4B3030 (the places at a height either way, the sit radius 1.7 heights, the sit 10 s and a draw, the sleep 17 s, the poo 4 s, the face after it 4 s and a draw of 4); tests `CreatureJointActions.*` |
| Eat from a field or eat fish with a friend, or get the friend to fetch it | pending | the exe's `EatFromFieldWithFriend` 0x4BB6F0, `EatFishWithFriend` 0x4BC8C0, `GetFriendToGiveMeFoodFromField` 0x4BBD80, `GetFriendToGiveMeFish` 0x4BCCA0 |
| Be happy with a friend | done | plan action `BeHappyWithFriend`, a joint action side by side (`creature_mind::BeHappyWithFriend`), only when it is not sad: the befriend request, the two places a height either way of the middle, the wait, facing 0.1 s with fluttering eyelids, both play the happy clip (55), as the exe's builder 0x4B2B90; test `CreatureJointActions.BeingHappyWithAFriend` |
| Go to the beach with a friend | pending | the exe's builder 0x4B35E0 |
| Show a friend a miracle: a destructive one (fireball, lightning) or a creating one (food, wood) | pending | the exe's `ShowFriendDestructiveSpell` 0x4A6F70 and `ShowFriendCreationSpell` 0x4A75D0 (with `HowMuchFriendNeedsToKnowFireball` / `Lightning` / `MagicFood` / `MagicWood`) |
| Show a friend its home or its citadel | pending | the exe's `ShowFriendMyHome` 0x4A7D40 and `ShowFriendMyCitadel` 0x4A8410 |
| Give a friend food or a toy | pending | the exe's `GiveToCreature` 0x4A37B0 (`GiveFriendToy` calls it) |
| Teach a friend: the friend learns the actions and miracles it is shown | pending | see [learning_by_observation.md](learning_by_observation.md) |
| Talk to a friend, howl at it | partial 50% | howling: plan action `HowlAtFriend` faces the friend for 2 s, looking at it, then howls (217), as the exe's builder 0x4C1ED0 (`creature_mind::HowlAtFriend`); talking (`TalkToFriend` 0x4C25D0) pending; no test of the howl |
| Get a friend's attention | pending | the exe's builder 0x4BE7A0 |
| Help a town with a friend | partial 80% | plan action `HelpTownWithFriend` (`creature_mind::TownWithFriend`): both go near the town's storage pit; the steps follow the exe's builder 0x4BDEC0 (the befriend request, facing 0.1 s with fluttering eyelids, the happy clip, going near, the forced desire for both); the end (the first to take up compassion for the town stops the other, kept as a vanilla bug) is not read in the exe; test `CreatureJointActions.HelpingOrAttackingATownWithAFriend` |
| Cast an amusing miracle on another creature: freeze, small, big, weak, strong, fat, thin, invisible, nice, angry, hungry, frightened, tired, ill, thirsty, itchy | partial 70% | the row `CastAmusingSpellOnCreature` at a creature that seems friendly, a known and payable spell drawn, then the 1-in-5 emote, near at four heights, back two (`creature_mind::CastAmusingAt`), as the exe's builder 0x4C0640 ([casting.md](../../../creature/casting.md#the-spell-to-amuse-another-creature)); whether it seems friendly goes by the original's rule, but the desire about the other creature is the planner's own weighing; see [creature_casting.md](creature_casting.md) |
| Cast warming or cooling on another creature, cure its illness, give it food | pending | warming and cooling are the exe's action rows 136 and 137 ([casting.md](../../../creature/casting.md)) |

## Unfriendly actions

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Argue with a friend | pending | the exe's builder 0x4B4430 |
| Mope about with a friend | pending | the exe's builder 0x4B4C20 |
| Confuse a friend | pending | the exe's builder 0x4B50D0 |
| Show off to a friend | pending | the exe's builder 0x4B55D0 |
| Kiss a friend's backside | pending | the exe's builder 0x4B4280 |
| Order a friend around, and the friend obeys | partial 30% | within a joint action the asker's orders are obeyed at once (`CreatureMindSystem::TakeOrder`), as the exe's `AddOrder` steps in the builders above; ordering a friend around as an action of its own (`OrderFriendAround` 0x4B9AD0) is pending |
| Tell another creature to go away | pending | the exe's `TellCreatureToSodOff` 0x4B9450 |
| Show another creature it hates it | pending | the exe's `ShowCreatureYouHateHim` 0x4AC780 |
| Show another creature how nice it reckons it is | pending | the exe's `ShowCreatureHowNiceYouReckon` 0x4AFFC0 |
| Attack a town with a friend | partial 80% | plan action `AttackWithFriend` (`creature_mind::TownWithFriend` with anger), only when the nearest town it knows is more useful to its anger than to its compassion; the steps follow the exe's builder 0x4BD920 as for helping; the weighing is the planner's own; test `CreatureJointActions.WhatLetsEachJointActionStart` |
| Pick a fight with a creature it is angry with | done | `CreatureFightSystem::ProcessTurn` (`fight::WantsToFight`); see [fighting.md](fighting.md) |

## Missing a friend

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Pine for a friend that is gone | partial 50% | plan action `PineForFriend` (`creature_mind::PineForFriend`): faces the friend 2 s, points at it 2 s, the sad clip (56) as the main step, as the exe's builder 0x4ABCF0; the missed-friends source 58 is a pure rule (`creature_friendship::MissedFriends`; test `CreatureFriendship.FeelingsFadeAndFriendsAreMissed`) not yet read by the desires, waiting for the attention a sighting gives ([desires.md](desires.md)) |
| Look for a missing friend | partial 50% | plan action `LookForFriend`: points at it 2 s, the sad clip (56), then goes near it to three times its own height, as the exe's builder 0x4ABE60 (`creature_mind::LookForFriend`); test `CreatureFriendship.MissingAFriend`; driven only by the missed-friends desire's other sources until source 58 is read |

## Minds and control

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Obey another creature: wants to do what a creature it looks up to tells it | pending | the desire exists, nothing feeds it ([desires.md](desires.md)) |
| Swap minds with another creature: they act on each other's learning | done | `SWAP_CREATURE`, `CreatureMindSystem::SwapMinds` (`CreatureMindSwap.cpp`, the pure `creature_swap`), as the exe's `Creature::SwapMinds` 0x47BEE0: the leashes off, the minds crossed, each body keeping its name, size and alignment and easing into the other's, the forced plan, the sightings rescaled ([players-creature.md](../../../creature/players-creature.md#swapping-minds)); test `CreatureMindSystemTest.SwappedMindsCrossOverWhileEachBodyKeepsItsNameSizeAndAlignment` |
| A computer player's creature takes on another god's creature, by the computer's choice | pending | see [../multiplayer](../multiplayer/) |
| The dominant creature of a pair leads, the other follows | pending |  |
