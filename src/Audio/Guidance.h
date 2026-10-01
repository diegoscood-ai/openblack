/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <functional>
#include <optional>
#include <vector>

#include <glm/vec3.hpp>

#include "Audio.h"

// GGuidance (SoundGuidance.cpp, 0x71AB10..0x71D490, milestone B9 of dev\tmp_dis\audio\PLAN.md): the villagers'
// reactions on Guidance.sad (their desires, a resource dropped on their town, an attack, a disciple, belief, the deaths)
// and the advisors' remarks (types 9..32 through the help script "MultiHelpJustTalkWithText"), each gated by the type's
// interval and the help level. Disassembly: dev\tmp_dis\audio\voices_guidance_71ab10.txt; notes: voices.md §2.7 and
// docs/bw1-notes/audio.md (B9).
//
// The original has one GGuidance per GInterfaceStatus (+0x30); every caller uses the local player's
// (GGame::MyInterfaceStatus), so openblack keeps one. What it reads from the game comes from GameQueries (the guidance
// section): towns, worship sites, the citadel heart... Unset, they are the neutral values of a game without those
// systems, and nothing plays. The callers that openblack does not have yet (the town's aggressor, the totems, the
// creature, belief, disciples, the villagers' death) find the functions here with the original's arguments.
//
// Random numbers: GRand::LocalRand 0x6DE570 (0 for 0, else LHRand(n) 0x7DB600 on g_game+0x205A3C, in [0, n)) and
// LocalFloatRand 0x6DE590 (0 for 0, else x * LHRand(0xFFFF) * (1 / 65535.f)). By default on openblack's generator
// (approximated: not LHRand's sequence); SetRandom gives the tests LHRand with a seed.

namespace openblack::audio::guidance
{

/// GGuidance::GUIDANCE_SFX_TYPE, the 33 rows of 0x980190
enum class Type : uint8_t
{
	TownDesire = 0,        ///< ProcessTownDesireSFX 0x71B020
	ResourceDrop = 1,      ///< ResourceDropSFX 0x71B570
	TownAttack = 2,        ///< TownAttackSFX 0x71B7C0
	RaiseTotem = 3,        ///< EndRaiseTotemSFX 0x71BED0
	Disciple = 4,          ///< MakeDiscipleSFX 0x71BF10
	Belief = 5,            ///< fn_0071BF70 (GGuidance::BeliefSFX 0x437F40)
	HeartBeat = 6,         ///< fn_0071C460
	DeathInVillage = 7,    ///< fn_0071C810 (Villager::VillagerDead 0x7507B8)
	HelpSprites = 8,       ///< the gate of HelpSpritesPlayNow 0x71AFF0
	TownBeingAttacked = 9, ///< 9..30: HELP_SPRITES_GUIDANCE (info.dat, 22 lists) + 9
	CreatureBeingAttacked = 10,
	CreatureAttackingThem = 11,
	LosingVillagers = 12,
	AttackingTown = 13,
	LowOnFood = 14,
	LowOnWood = 15,
	InjuredPeople = 16,
	LowOnPeople = 17,
	VillagersUnhappy = 18,
	LosingBelief = 19,
	OtherVillages = 20,
	CreatureFight = 21,
	GeneralBad = 22,
	GeneralGood = 23,
	KillingPeople = 24,
	GoodBeingEvil = 25,
	EvilBeingGood = 26,
	WorshippersDying = 27,
	VeryGood = 28,
	VeryEvil = 29,
	DestroyBuilding = 30,
	JustTalkNoText = 31, ///< HelpSpiritSay's "MultiHelpJustTalkWithNoText" (0x71D276; no caller found)
	OneOff = 32,         ///< fn_0071D0B0's remarks (0x71D0E5)

	_Count
};
inline constexpr size_t k_TypeCount = static_cast<size_t>(Type::_Count);

/// A row of 0x980190 (12 bytes)
struct TypeInfo
{
	uint32_t base;     ///< +0: the interval's base in turns (Interval 0x71AEE0) and Init's spread (0x71ACDD)
	int32_t helpLevel; ///< +4: the help level it needs (PlayNow 0x71AFB3)
	bool always;       ///< +8: not muted on land 1 of a single-player campaign (PlayNow 0x71AF5D)
};
/// 0x980190..0x98031C
inline constexpr std::array<TypeInfo, k_TypeCount> k_Types {{
    {50, 1, false},    {25, 1, false},    {40, 1, true},     {50, 1, false},   {0, 0, true},      {30, 1, true},
    {0, 1, true},      {0, 1, true},      {100, 1, true},    {1000, 2, false}, {1500, 2, false},  {2000, 3, false},
    {1000, 2, false},  {1000, 3, true},   {2500, 3, false},  {2500, 3, false}, {1000, 4, false},  {2500, 2, false},
    {3000, 3, false},  {1000, 2, false},  {1000, 2, false},  {200, 1, true},   {1000, 2, false},  {1000, 2, false},
    {500, 2, false},   {5000, 2, true},   {5000, 2, true},   {600, 1, false},  {10000, 2, false}, {10000, 2, false},
    {1000, 3, true},   {0, 0, true},      {0, 0, true},
}};

/// The 22 lists of HELP_SPRITES_GUIDANCE (info.dat GHelpSpritesGuidance, the objects at 0xD99BD8 + 0x98 k): HELP_TEXT
/// ids, the list ends at the first 0 (fn_0071D390, at most 0x22 = 34)
using SpriteList = std::array<uint32_t, 34>;
inline constexpr size_t k_SpriteLists = 22;

/// fn_0071D0B0's table 0x980440: {HELP_TEXT, probability} by its argument
struct OneOffInfo
{
	uint32_t text;
	float probability;
};
inline constexpr std::array<OneOffInfo, 7> k_OneOffs {{
    {3308, 0.02f},
    {3317, 0.0002f},
    {3318, 0.01f},
    {3321, 0.01f},
    {3325, 0.05f},
    {3326, 0.1f},
    {3328, 0.025f},
}};

/// fn_0071AA90's table 0x980328 by TOWN_DESIRE_INFO (the samples for x >= 0.85, >= 0.65 and >= 0.45)
inline constexpr std::array<std::array<uint32_t, 3>, 19> k_DesireTexts {{
    {4967, 4968, 4969}, // 0 VILLAGER_VOICE_DESIRE_FOOD_01..03
    {4964, 4965, 4966}, // 1 DESIRE_WOOD
    {0, 0, 0},
    {4983, 4984, 4985}, // 3 DESIRE_PROTECTION
    {4977, 4978, 4979}, // 4 DESIRE_MERCY
    {4973, 4973, 4973}, // 5 DESIRE_BUILD_03 (three times)
    {4980, 4981, 4982}, // 6 DESIRE_EXPAND
    {0, 0, 0},
    {4974, 4975, 4976}, // 8 DESIRE_OFFSPRING
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {4992, 4993, 4994}, // 17 GUIDANCE_SAMPLE_NEEDWORSHIPPERS_FOOD_01..03 (the worship site's food)
    {4996, 4997, 4998}, // 18 GUIDANCE_SAMPLE_NEEDWORSHIPPERS_01..03 (the citadel's +0x70)
}};

/// GRand::LocalRand: a number in [0, n), 0 for n == 0
using RandomFn = std::function<uint32_t(uint32_t n)>;
/// The generator of LocalRand / LocalFloatRand (tests); an empty one is openblack's (approximated)
void SetRandom(RandomFn random);
/// _LHRand 0x7DB600: seed = ror13(seed * 9377 + 9439); seed % n
[[nodiscard]] uint32_t LHRand(uint32_t n, uint32_t& seed);
/// GRand::LocalRand 0x6DE570
[[nodiscard]] uint32_t LocalRand(uint32_t n);
/// GRand::LocalFloatRand 0x6DE590: x * LocalRand(0xFFFF) * 1.5259022e-5f (0x8D6050 = 1 / 65535), 0 for x == 0
[[nodiscard]] float LocalFloatRand(float x);

/// The HELP_SPRITES_GUIDANCE lists of info.dat (Game, after loading it; the tests)
void SetSpriteLists(const std::array<SpriteList, k_SpriteLists>& lists);

// ---- the core (0x71AC70..0x71C800, 0x71D270..0x71D480) -------------------------------------------------------------

/// GGuidance::Init 0x71AC70 (GInterfaceStatus::Init 0x5DD1CB / ResolveLoad 0x5DD194): the options +0x8 (bank Guidance
/// GAudio+0x3D0, 2D, track 0, mode 2, the ctor's defaults otherwise); for each type lastPlayed (+0xC + 4 t) = turn -
/// LocalRand(base) if that is > 0, else 0 (0x71ACCF..0x71AD17: a positive first draw is drawn again for the value kept);
/// the LastThings list +0x90 emptied; +0x98..+0xC8 = 0, +0xA8 = 30.0, the one-off flags +0xD0..+0xEC = 0; and
/// [0xC221CC] = 1. openblack calls it when a land starts (its turn counter back to 0).
void Init();
/// GGuidance::Close 0x71ABF0 (GInterfaceStatus::UnInit 0x5DD226): the options freed, the LastThings list emptied
void Close();

/// GetTimeSinceLastPlayed 0x71ADF0: turn - lastPlayed[t] (unsigned)
[[nodiscard]] uint32_t TimeSinceLastPlayed(Type type);
/// fn_0071AEE0: base + LocalRand(ftol(5 base (1 - r^3))), r = LocalFloatRand(1) (turns; new numbers each call)
[[nodiscard]] uint32_t Interval(Type type);
/// GGuidance::PlayNow 0x71AF50: a type not `always` is muted on land 1 (g_game+0x205A08) of a single-player game that is
/// not the playground (g_game+0x205A0C); then the help level (HelpSystem+0x45F8 ? +0x45F4 : 0) must reach the type's,
/// and TimeSinceLastPlayed > Interval (unsigned)
[[nodiscard]] bool PlayNow(Type type);
/// HelpSpritesPlayNow 0x71AFF0: PlayNow(HelpSprites) && PlayNow(type)
[[nodiscard]] bool HelpSpritesPlayNow(Type type);

/// GGuidance::PlaySample 0x71C6F0(is3D, textOrSample, owner, type, volume, pitch, +0x2C, MapCoords*, maxDistance,
/// isText): the persistent options get +0x28 volume, +0x48 pitch, +0x2C, the sample +0x24 (the voice table 0x96BA38
/// when isText) and the owner +0x20 (the player's number: GetPlayerNumber 0x64A790); 3D: +0x08 = 1 and, only with a
/// point (none: nothing at all, 0x71C757), +0x30 = the point, +0x58 = maxDistance, +0x54 = maxDistance * 0.333
/// (0x8D8734), the mask +0x1C = 0x180 (the .sad keeps neither) and +0x0C = 0; 2D: +0x08 = 0 (the rest stays as the
/// last 3D one left it). Then GAudio::PlaySoundEffect 0x429E30 and, for a type < 33, lastPlayed = turn.
/// `point` is the MapCoords as a world point (x, GetAltitude + its height, z).
Channel PlaySample(bool is3D, uint32_t textOrSample, uint32_t owner, int type, int volume, int pitch, int field2C,
                   std::optional<glm::vec3> point, float maxDistance, bool isText);

/// GGuidance::HelpSpiritSay 0x71D270(text, type): HelpSystem::RunMessage(text, text, type == 31 ?
/// "MultiHelpJustTalkWithNoText" : "MultiHelpJustTalkWithText") (GameQueries::helpRunMessage), TriggerCategory(8)
/// (GameQueries::helpTriggerCategory), then lastPlayed[type] = turn for a type < 33 and +0x2C = turn (whether the
/// script started or not)
void HelpSpiritSay(uint32_t text, Type type);
/// fn_0071D0B0(k): once per land (+0xD0 + 4 k): if LocalFloatRand(1) <= k_OneOffs[k].probability, HelpSpiritSay(text,
/// OneOff) and the flag set (GSpookyVoices::Process 1, fn_0071D100 4, HelpSpritesCheckMoonPhase 5, GatheringBox 3)
void OneOff(int index);

/// GHelpSpritesGuidance::GetRandomSample 0x71D300: list[LocalRand(count)] (fn_0071D390: the entries before the first 0;
/// all 34 set counts 33)
[[nodiscard]] uint32_t GetRandomSample(size_t list);
/// GetRandomSampleBasedOnValue 0x71D320(value): value clamped to 0..1, list[LocalRand(ftol(count * value))]
[[nodiscard]] uint32_t GetRandomSampleBasedOnValue(size_t list, float value);

/// The things GGuidance remembers (+0x90, LastThings 0x71AEC0 {thing, turn}): fn_0071AE10(thing) gives the turns since
/// the thing was first seen, starting again past 600 (0x258); a new thing is added with the turn and gives 0
[[nodiscard]] uint32_t TimeSinceThingSeen(uint32_t thing);
/// fn_0071AA90(desire, value): x = value - LocalFloatRand(value / 3); x >= 0.85 / 0.65 / 0.45 (0x980324 / 0x980320 /
/// 0x98031C) -> k_DesireTexts[desire][0 / 1 / 2], else 0
[[nodiscard]] uint32_t DesireSample(uint32_t desire, float value);
/// fn_0071B410(thing, distance, value, sample): 2 t0 s v^3 (1 - d^2) (1 - t1^3) with t0 = min(TimeSince(TownDesire)
/// / 50, 1), t1 = min(TimeSinceThingSeen(thing) / 300, 1), d = min(distance / 200, 1), v = min(value, 1), s = t0^3 when
/// the sample is the last desire's (+0x98), else 1 (0x980128, 0x980134, 0x980130)
[[nodiscard]] float DesireScore(uint32_t thing, float distance, float value, uint32_t sample);

// ---- the turn (GGame::ProcessTurn 0x54E711..0x54E729, GInterfaceStatus::Process 0x5DC50D) --------------------------

/// GGuidance::ProcessTownDesireSFX 0x71B020 (every 10 turns, PlayNow(TownDesire)): CheckTownDesiresSFX 0x71B130 and
/// CheckWorshipSiteDesiresSFX 0x71B270 (GameQueries::desireTowns / worshipSites) choose a sample and its value; above 0.3
/// (0x98012C), x = value - LocalFloatRand(value / 2) and the sample plays 3D at the thing with maxDistance 200 x
/// (0x980130), volume 127, pitch 100, +0x2C 90; +0x98 = the sample
void ProcessTownDesireSFX();
/// GGuidance::ProcessHeartBeatSFX 0x71C190 (every 10 turns, for the local interface): the heart beat's value +0xA4 from
/// GameQueries::heartBeat, clamped to 0..1, then fn_0071C460
void ProcessHeartBeatSFX();
/// GGuidance::HelpSpritesCheckMoonPhase 0x71D1C0 (static, every turn): a countdown [0xC221D0]; at its end, at visual
/// night (IsVisualNight 0x5575E0), the moon's phase (fn_0086A7F0) - pi: real night (fn_0072E3B0) and |phase - pi| <
/// 0.15 (double 0x9804D0) -> OneOff(5) and 600000 turns (0x927C0); else ftol((phase - pi)^2 * 12000) (0x9804C8)
void HelpSpritesCheckMoonPhase();
/// The audio part of GGame::ProcessTurn, in its order: GSpookyVoices::Process 0x54E711, HelpSpritesCheckMoonPhase
/// 0x54E716, ProcessTownDesireSFX(MyInterfaceStatus) 0x54E729 (GConfirmation::Process 0x54E731 is milestone C7), and
/// the heart beat of GInterfaceStatus::Process 0x5DC50D
void ProcessGameTurn();

/// fn_0086A7F0: the moon's phase from the real clock, 2 pi (1 - frac(days * 0.03386318)) (the double 0x9A3BE8 = 1 /
/// 29.5306; days = time() / 86400 - 10962 as an int, 0x86A845..0x86A85C; 2 pi = the double 0x8D45D8)
[[nodiscard]] float MoonPhase(int64_t unixTime);

// ---- the events (callers in the game) ------------------------------------------------------------------------------

/// The value of GetGuidanceResourceType (RESOURCE_RAIN_TYPE) and of Pot::AddResourceToPos's type (0x66F4EB..0x66F502)
enum class RainType : uint8_t
{
	None = 0,
	Food = 1, ///< Pot food (0x71BDEF), Animal 0x71BE10; Pot::AddResourceToPos RESOURCE_TYPE 0
	Wood = 2, ///< Pot wood, Tree 0x71BE20, DeadTree 0x71BE30; RESOURCE_TYPE 1
	Rain = 3,
};
/// GGuidance::ResourceDropSFX 0x71B570(IS, MapCoords, type) (Pot::AddResourceToPos 0x66F509, a new pile from the local
/// interface; Object::DoDeleteObjectAndTakeResource 0x63A9E6, a resource given to a store by the local interface):
/// PlayNow(ResourceDrop), the nearest town within 100 (0x98013C, MapCoords::GetNearestTown 0x6020E0: GameQueries::
/// townResourceNeeds), GetResourceDropSample 0x71B5F0, then 3D at the point, maxDistance 200 (0x980148)
void ResourceDropSFX(glm::vec3 point, RainType type);
/// GetResourceDropSample 0x71B5F0(town, type): the sum of the town's three values for the type >= 0.5 (0x980140) ->
/// PLEASED_<type>_01 + LocalRand(3); < 0.25 (0x980144) -> DISPLEASED_FOOD for food, PLEASED_<type> for wood and rain
/// (the original's: their DISPLEASED texts are never used); else 0
[[nodiscard]] uint32_t ResourceDropSample(float need, RainType type);

/// The GetSampleForAttack of each kind (0x71BC20..0x71BD50): a thing 0; a spell of type 6 (fn_0072B200) a
/// LIGHTNING text, other spells 0; a rock ROCKS; a creature MONSTER (each 10 + LocalRand(10))
enum class Attacker : uint8_t
{
	Thing,
	LightningSpell,
	OtherSpell,
	Rock,
	Creature,
};
[[nodiscard]] uint32_t AttackerSample(Attacker attacker);
/// What TownAttackSFX 0x71B7C0 and HelpSpritesTownBeingAttacked 0x71C870 read of a town and its EffectValues
struct TownAttack
{
	uint32_t population {0};               ///< Town +0x618 + +0x61C
	glm::vec3 position {0.0f};             ///< Town +0x14
	float severity {0.0f};                 ///< Town +0xEC0 (maxDistance x min(0.2 v, 1) + 1)
	std::array<float, 7> effects {};       ///< EffectValues +0x8..+0x20 (fn_0071BE40: 0 x 0.001 clamped, 1, 2, 4)
	/// EffectValues +0x28, the thing that caused it (nullopt: none): its GetSampleForAttack (vt +0xDC) is asked inside
	/// TownAttackSFX, after the lists are built (0x71BAD8), as the original draws its LocalRand(10) there
	std::optional<Attacker> attacker;
	bool townIsMine {false};               ///< the town's player IsMemberOfThisPlayer(MyInterfaceStatus)
	bool townOfLocalPlayer {false};        ///< the town's player is g_game's local one (+0x205A59)
	std::optional<uint32_t> causedPlayer;  ///< EffectValues::GetCausedPlayer 0x525910 (nullopt: none), its number
	bool causedIsLocalPlayer {false};      ///< that player is the local one
	std::array<float, 8> aggression {};    ///< Town +0x9F8 + 0x80 n (n = the caused player's number)
};
/// fn_0071BE40: the strongest of the effects 0, 1, 2 and 4 (0 scaled by 0.001 (0x8AC418) and capped at 1) above 0;
/// 7 when none
[[nodiscard]] int StrongestEffect(const std::array<float, 7>& effects);
/// TownAttackSFX 0x71B7C0 (Town::UpdateAggressor 0x73CA76; not in openblack yet): PlayNow(TownAttack), a town with
/// people and an effect (StrongestEffect != 7): a list of the ten ATTACK texts, + the ten FIRE ones for effect 0, + the
/// attacker's sample ten times, one of them at random (fn_0071D440(LocalRand(n))) 3D at the town, maxDistance 200
/// (0x980158) x (min(severity x 0.2, 1) + 1); then, always, HelpSpritesTownBeingAttacked when the town is the local
/// interface's
void TownAttackSFX(const TownAttack& attack);
/// HelpSpritesTownBeingAttacked 0x71C870: the local player's town, caused by another player, with people, PlayNow
/// (TownBeingAttacked) (not HelpSpritesPlayNow) and that player's aggression (+0x9F8 + 0x80 n) > 1 (0x98015C) ->
/// HelpSpiritSay(list 0)
void HelpSpritesTownBeingAttacked(const TownAttack& attack);

/// GGuidance::StartRaiseTotemSFX 0x71BEB0 (TotemStatue::NetworkUnfriendlyStartLockedSelect 0x738620): +0xBC = height
void StartRaiseTotemSFX(float height);
/// GGuidance::EndRaiseTotemSFX 0x71BED0 (0x738666): height > +0xBC and PlayNow(RaiseTotem) -> fn_0071C690 (the alignment
/// class), and nothing else (the code tests it and returns: no sample plays in W120)
void EndRaiseTotemSFX(float height);
/// fn_0071C690: the local player's alignment (GetAlignmentValue 0x64D6A0) > 0.55 (0x98014C) -> 1, < -0.55 (0x980150)
/// -> 2, else 0
[[nodiscard]] int AlignmentClass(float alignment);
/// fn_0071AB70(disciple, class): VILLAGER_DISCIPLE 10 -> class 0 / 1: LocalFloatRand(1) > 0.5 ? GOOD_LIVE_HERE_01 (4859)
/// : _02, class 2: EVIL_LIVE_HERE_01 / _02 (4861 / 4862), other 0; disciple 0..9 -> the table 0x98040C
[[nodiscard]] uint32_t DiscipleText(uint32_t disciple, int alignmentClass);
/// GGuidance::MakeDiscipleSFX 0x71BF10(IS, disciple) (Object::InitialisePhysicsFromHand 0x6372EA): PlayNow(Disciple),
/// then DiscipleText(disciple, AlignmentClass) 2D, volume 85, pitch 100, +0x2C 90
void MakeDiscipleSFX(uint32_t disciple, float localAlignment);

/// GUIDANCE_ALIGNMENT of BeliefSFX: 1 good, 2 evil, any other a coin (LocalRand(2) ? 2 : 1, 0x71BFB5)
/// GGuidance::BeliefSFX 0x437F40 (GBelief::AddToBelief 0x437F2A; not in openblack yet): the believed-in player's
/// belief below the strongest of the 8 (GBelief +0x8), fn_0071BF70(point, (b + 0.0001) / (max + 0.0001), alignment)
void BeliefSFX(const std::array<float, 8>& beliefs, uint32_t player, glm::vec3 point, float distanceToCamera,
               int alignment);
/// fn_0071BF70(IS, MapCoords, x, alignment): PlayNow(Belief); x - LocalFloatRand(x / 3); good: >= 0.7 GOOD_AWE_01,
/// >= 0.4 _02, > 0.05 _03; evil EVIL_AWE (0x8AB238, 0x8C7A44, 0x8AC3F4); fn_0071C0D0(the distance from the interface
/// (IS+0x14) to the point, alignment, text) > 0.3 (0x980168) -> 3D at the point, maxDistance 200 (0x980164)
void BeliefSample(glm::vec3 point, float distance, float value, int alignment);
/// fn_0071C0D0(distance, alignment, text): t0 (1 - d^2) with t0 = min(TimeSince(TownDesire) / 50, 1), d = min(distance /
/// 200, 1); t0^4 (1 - d^2) when text is +0x9C (which nothing writes: always the first)
[[nodiscard]] float BeliefVisibility(float distance, uint32_t text);

/// fn_0071C810(IS) (Villager::VillagerDead 0x7507B8: a villager of the local player killed by another with the death
/// table's +0xC, 0x99A370): PlayNow(DeathInVillage) -> DEATH_IN_VILLAGE_06 + LocalRand(5) (5720) 2D, volume 127
void DeathInVillageSFX();

/// The heart beat's input (fn_0071C460 is fed by ProcessHeartBeatSFX; fn_0071C3F0 sets +0xB8)
/// fn_0071C3F0(v) (CitadelHeart fn_00465C70 0x465DA3 / 0x466489): +0xB8 = v (0: the beat follows +0xA4)
void SetHeartBeatOverride(float value);
/// fn_0071C460(v): +0xA8 = +0xB8 ? +0xB8 x 100 x 0.4 (fn_0071C400) : (30 + 70 v - +0xA8) x 0.1 + +0xA8 (0x8BF51C,
/// 0x92B2C8, 0x8AB22C); the phase +0xB4 += +0xA8 x 0.025 x [0xD01A38] x 0.001 brought to <= 1; +0xB0 = +0xAC, +0xAC =
/// (1 - cos(2 pi +0xB4)) / 2; for the local interface with a living citadel heart: InGame 45 (bank +0x3AC) looping (-1),
/// mode 2, 3D at the citadel, maxDistance 500, pitch ftol(+0xA8), then the options back (Guidance, loops 0) and
/// GAudio fn_00428740(InGame, owner, 45, pitch)
void HeartBeat(float value);
/// fn_0071C650 (no caller found): StopPlayingSoundEffect(45, owner, InGame) and +0xA0 = 0
void StopHeartBeat();
/// fn_0071C450: +0xAC; fn_0071C430: +0xB0 + (+0xAC - +0xB0) x g_game+0x205D64 (the turn's fraction)
[[nodiscard]] float HeartBeatPulse(float turnFraction);

// The advisors' remarks: each needs HelpSpritesPlayNow(type) (TownBeingAttacked: PlayNow) and then says one text of
// its list (type - 9) through HelpSpiritSay.

/// fn_0071C930 (no caller found): [0xC221CC] && HelpSpritesPlayNow(CreatureBeingAttacked) -> list 1
void HelpSpritesCreatureBeingAttacked();
/// fn_0071C960 (Town::UpdateAggressor 0x73CAFA): [0xC221CC] && HelpSpritesPlayNow(CreatureAttackingThem) -> list 2
void HelpSpritesCreatureAttackingThem();
/// HelpSpritesLosingVillagers 0x71C990(villager) (Villager::VillagerDead 0x7508C5): the villager within 300 (0x980174)
/// of the hand (GInterface+0x3B8, fn_00605CD0) -> list 3
void HelpSpritesLosingVillagers(glm::vec3 villager);
/// fn_0071C9F0(town) (Town::UpdateAggressor 0x73CB24): a town not of the local player (g_game+0x205A59), with people ->
/// list 4
void HelpSpritesAttackingTown(bool townOfLocalPlayer, uint32_t population);
/// What the HelpSprites remarks about a town read of it
struct HelpTown
{
	uint32_t population {0};          ///< +0x618 + +0x61C
	bool storagePitFunctional {false}; ///< GetStoragePit 0x73B5B0 and its IsFunctional (vt +0xD4)
	glm::vec3 position {0.0f};         ///< +0x14
};
/// HelpSpritesLowOnFood 0x71CA60(town, value) (Town::CalculateDesireForFood 0x747FA0): people, a working storage pit,
/// within 300 of the hand -> list 5 by value (GetRandomSampleBasedOnValue)
void HelpSpritesLowOnFood(const HelpTown& town, float value);
/// fn_0071CAF0(town, value) (0x7481BC): the same with list 6 (LowOnWood)
void HelpSpritesLowOnWood(const HelpTown& town, float value);
/// fn_0071CB80(town) (no caller found): people, within 300 of the hand -> list 7
void HelpSpritesInjuredPeople(const HelpTown& town);
/// HelpSpritesLowOnPeople 0x71CBE0(town) (Villager::VillagerDead 0x7508EF): people, within 300 -> list 8
void HelpSpritesLowOnPeople(const HelpTown& town);
/// HelpSpritesVillagerUnhappy 0x71CC40(town, value) (TownDesire::Process 0x745C8A): people, a working storage pit,
/// within 300 -> list 9 (the value is not used)
void HelpSpritesVillagerUnhappy(const HelpTown& town);
/// fn_0071CCC0(town) (fn_00438340 0x4383C4): people -> list 10
void HelpSpritesLosingBelief(uint32_t population);
/// fn_0071CD00(town) (no caller found): people -> list 11
void HelpSpritesOtherVillages(uint32_t population);
/// fn_0071CD40(arena) (CameraModeNew3::StartFight 0x45A772 / 0x45A7B2) -> list 12
void HelpSpritesCreatureFight();
/// fn_0071CD70(MapCoords) (fn_004383D0 0x43872B, Creature dance 0x5039E7) / fn_0071CDF0 (0x438755, 0x50394F): the point
/// on screen (fn_0081F1D0: GameQueries::pointOnScreen) -> list 13 / 14
void HelpSpritesGeneralBad(glm::vec3 point);
void HelpSpritesGeneralGood(glm::vec3 point);
/// fn_0071CE70(villager) (Villager::VillagerDead 0x75078B): the villager's object on screen (fn_0081F1A0: the
/// bounding box, GameQueries::thingOnScreen) -> list 15
void HelpSpritesKillingPeople(bool onScreen);
/// HelpSpritesAlignmentProcess 0x71CEB0(change) (GAlignment::ProcessForPlayer 0x4141D9 for the local player, every
/// turn: GetMaxAlignmentChangePerGameTurn x the pending change +0xC): +0xC0 = 0.95 +0xC0 + change (0x980178); past
/// 2 (0x98017C) x maxChange (GPlayer+0x64 +0x10), with the alignment a: same sign as +0xC0 and |a| > 0.75 (0x980180) ->
/// a > 0 ? VeryEvil (fn_0071D040, list 20) : VeryGood (fn_0071D010, list 19) (sic); opposite and |a| > 0.4 (0x980184)
/// -> a > 0 ? GoodBeingEvil (25, list 16) : EvilBeingGood (26, list 17) (fn_0071CF90)
void HelpSpritesAlignmentProcess(float change, float alignment, float maxChangePerTurn);
/// fn_0071CFE0 (Villager::VillagerDead 0x75087C, death reason 4 of a villager of the local player) -> list 18
void HelpSpritesWorshippersDying();
/// HelpSpritesDestroyBuilding 0x71D070(abode) (Abode::ApplyEffectsDueToPhysicalDestruction 0x406781: the abode's +0x90
/// +0x18 < 0.4 (0x980188) destroyed by the local player): the abode on screen -> list 21
void HelpSpritesDestroyBuilding(bool onScreen);

/// GGuidance state for the tests and the debug view
struct State
{
	std::array<uint32_t, k_TypeCount> lastPlayed {}; ///< +0xC
	uint32_t lastSpiritSay {0};                      ///< +0x2C
	uint32_t lastDesireSample {0};                   ///< +0x98
	uint32_t lastBeliefSample {0};                   ///< +0x9C
	bool heartBeatPlaying {false};                   ///< +0xA0 (only fn_0071C650 writes it)
	float heartBeatValue {0.0f};                     ///< +0xA4
	float heartBeatPitch {30.0f};                    ///< +0xA8 (Init 0x71ADB1: 30.0)
	float heartBeatPulse {0.0f};                     ///< +0xAC
	float heartBeatPulsePrevious {0.0f};             ///< +0xB0
	float heartBeatPhase {0.0f};                     ///< +0xB4
	float heartBeatOverride {0.0f};                  ///< +0xB8
	float totemHeight {0.0f};                        ///< +0xBC
	float alignmentChange {0.0f};                    ///< +0xC0
	float believers {0.0f};                          ///< +0xC4 (smoothed GetProportionOfWorldPopulationWhoBelieveInMe)
	float beliefShare {0.0f};                        ///< +0xC8 (smoothed fn_0064B700)
	std::array<bool, 7> oneOffs {};                  ///< +0xD0
	bool enabled {false};                            ///< [0xC221CC] (Init sets 1; fn_0071C930 / fn_0071C960 read it)
	uint32_t moonCountdown {1};                      ///< [0xC221D0] (static, 1 at start)
	sample_play::Options options;                    ///< +0x8, the persistent LH_SamplePlayOptions
	Sample sample;                                   ///< its bank +0x04 and sample +0x24
	/// its +0x2C (ctor 90): recorded only (sample_play::Options does not model it; not read in LHSamplePlay)
	int field2C {90};
};
[[nodiscard]] const State& GetState();
/// For the tests: the state as Init leaves it (Init itself needs the turn query)
void ResetForTests();

} // namespace openblack::audio::guidance
