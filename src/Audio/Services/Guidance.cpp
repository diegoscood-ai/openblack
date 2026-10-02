/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Guidance.h"

#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <deque>
#include <string>
#include <string_view>
#include <utility>

#include <spdlog/spdlog.h>

#include "Audio/GAudio/AudioSystem.h"
#include "Audio/GameQueries.h"
#include "Audio/Services/SpookyVoices.h"
#include "Audio/Services/Voices.h"
#include "Common/RandomNumberManager.h"
#include "Locator.h"

// Every function cites its original in Guidance.h; the comments here give the instructions the order comes from.
// Disassembly: dev\tmp_dis\audio\voices_guidance_71ab10.txt.

using namespace openblack;
using namespace openblack::audio;
using namespace openblack::audio::guidance;

namespace
{
constexpr float k_DesireThreshold = 0.3f;           // 0x98012C
constexpr float k_DesireDistance = 200.0f;          // 0x980130: the camera range and the max distance factor
constexpr float k_DesireTurns = 50.0f;              // 0x980128
constexpr float k_ThingTurns = 300.0f;              // 0x980134
constexpr uint32_t k_ThingForgetTurns = 600;        // 0x980138 (0x258, fn_0071AE10 0x71AE65)
constexpr float k_ResourceTownDistance = 100.0f;    // 0x98013C
constexpr float k_ResourcePleased = 0.5f;           // 0x980140
constexpr float k_ResourceDispleased = 0.25f;       // 0x980144
constexpr float k_ResourceMaxDistance = 200.0f;     // 0x980148
constexpr float k_AttackMaxDistance = 200.0f;       // 0x980158
constexpr float k_BeliefMaxDistance = 200.0f;       // 0x980164
constexpr float k_BeliefVisibility = 0.3f;          // 0x980168
constexpr float k_HelpSpritesRange = 300.0f;        // 0x980174
constexpr float k_HeartBeatMaxDistance = 500.0f;    // 0x71C5D7: push 0x43FA0000
constexpr int k_HeartBeatSample = 45;               // 0x71C600 / 0x71C63B: G_HeartBeat (InGame)
constexpr float k_MsPerTurn = 100.0f;               // [0xD01A38] (inferred: 100 ms a turn, SoundTags.cpp)
constexpr uint32_t k_DeathInVillageText = 0x1658;   // 0x71C846: HELP_TEXT_DEATH_IN_VILLAGE_06
constexpr std::string_view k_ScriptWithText = "MultiHelpJustTalkWithText";     // 0xBF1988
constexpr std::string_view k_ScriptWithNoText = "MultiHelpJustTalkWithNoText"; // 0xC22254

struct GuidanceState: State
{
	/// +0x90 / +0x94: LastThings {thing, turn}, newest first (0x71AE99..0x71AEAA)
	std::deque<std::pair<uint32_t, uint32_t>> things;
	std::array<SpriteList, k_SpriteLists> lists {};
	RandomFn random;
};
GuidanceState g_Guidance;

bool Trace()
{
	static const bool k_Trace = std::getenv("OPENBLACK_GUIDANCE_TRACE") != nullptr;
	return k_Trace;
}

uint32_t Turn()
{
	const auto& queries = Queries();
	return queries.turn ? queries.turn() : 0;
}

uint32_t LocalPlayer()
{
	const auto& queries = Queries();
	return queries.localPlayerNumber ? queries.localPlayerNumber() : 0;
}

/// The owner +0x20 the original gives its samples: the player's number as a pointer, 0 = none
Owner PlayerOwner(uint32_t number)
{
	return number == 0 ? Owner::None() : Owner::Key(number);
}

/// GUtils::GetDistanceInMetres 0x74CD70 / fn_0074CCE0 + 0x74DCC0: hypotenuse(dx, dz) of the MapCoords x 10 / 65536.
/// (approximated) the exact length of the world points; the original's hypotenuse 0x74F680 uses the table 1 / sqrt of
/// _FUN_0074f620 (about 0.1 %) on the 16.16 MapCoords
float Distance(glm::vec3 a, glm::vec3 b)
{
	return std::hypot(b.x - a.x, b.z - a.z);
}

std::optional<glm::vec3> CameraPosition()
{
	const auto& queries = Queries();
	if (!queries.camera)
	{
		return std::nullopt;
	}
	const auto camera = queries.camera();
	return camera ? std::optional<glm::vec3>(camera->position) : std::nullopt;
}

/// The hand's distance test of the HelpSprites remarks (GInterface+0x3B8, fn_00605CD0, < 300)
bool NearHand(glm::vec3 point)
{
	const auto& queries = Queries();
	const auto hand = queries.handPosition ? queries.handPosition() : std::nullopt;
	return hand && Distance(point, *hand) < k_HelpSpritesRange;
}

/// HelpSpritesPlayNow(type) && the list's random text -> HelpSpiritSay (the shape of 0x71C960..0x71D070)
void SayFromList(Type type, size_t list)
{
	HelpSpiritSay(GetRandomSample(list), type);
}

/// fn_0071D390: the entries before the first 0; all 34 set count 33 (0x71D3A3)
uint32_t ListCount(const SpriteList& list)
{
	uint32_t n = 0;
	while (n < list.size())
	{
		if (list.at(n) == 0)
		{
			return n;
		}
		++n;
	}
	return n - 1;
}

/// The cube of the x87 loops (mov eax, 2; dec; fmul; jne): x * x * x
double Cube(double x)
{
	return x * x * x;
}

/// CheckTownDesiresSFX 0x71B130(sample*, value*, thing*)
void CheckTownDesires(uint32_t& sample, float& value, std::optional<glm::vec3>& thing, glm::vec3 camera)
{
	const auto& queries = Queries();
	if (!queries.desireTowns)
	{
		return;
	}
	const auto towns = queries.desireTowns();
	// 0x71B14F: the best distance starts at 200 (0x980130); 0x71B17B..0x71B1C2: a town with a storage pit and people
	float best = k_DesireDistance;
	const DesireTown* chosen = nullptr;
	for (const auto& town : towns)
	{
		if (!town.storagePit || town.population == 0)
		{
			continue;
		}
		const float d = Distance(*town.storagePit, camera); // GetInfo 0x74CD50 (pit +0x14, the camera's MapCoords)
		if (d < best)
		{
			best = d;
			chosen = &town;
		}
	}
	if (chosen == nullptr)
	{
		return;
	}
	thing = chosen->position; // 0x71B1DE: *thing = the town
	// 0x71B1FA..0x71B256: the 17 desires {+0x37C value, +0x380 type}
	for (const auto& desire : chosen->desires)
	{
		const float raw = desire.raw; // GetRawDesire(type) 0x71B1FF
		const uint32_t text = DesireSample(desire.type, desire.value);
		if (text == 0)
		{
			continue;
		}
		const float score = DesireScore(chosen->id, best, raw, text);
		if (score > value) // fcom [ebp]; test ah, 0x41; jne
		{
			value = score;
			sample = text;
		}
	}
}

/// CheckWorshipSiteDesiresSFX 0x71B270(sample*, value*, thing*)
void CheckWorshipSiteDesires(uint32_t& sample, float& value, std::optional<glm::vec3>& thing, glm::vec3 camera)
{
	const auto& queries = Queries();
	if (!queries.worshipSites)
	{
		return;
	}
	const auto citadel = queries.worshipSites();
	if (!citadel)
	{
		return; // 0x71B2AB: no citadel (GPlayer+0xA48)
	}
	float best = k_DesireDistance;
	const WorshipDesire::Site* site = nullptr;
	for (const auto& candidate : citadel->sites)
	{
		if (!candidate || !candidate->worshippers)
		{
			continue;
		}
		const float d = Distance(candidate->position, camera);
		if (d < best)
		{
			best = d;
			site = &*candidate;
		}
	}
	if (site == nullptr)
	{
		return;
	}
	const float food = site->foodDesire;
	const float need = citadel->need < 1.0f ? citadel->need : 1.0f; // 0x71B319..0x71B332
	// 0x71B33E..0x71B34F: both texts are drawn with the food value (push ebp twice)
	const uint32_t foodText = DesireSample(17, food);
	const uint32_t needText = DesireSample(18, food);
	const float foodScore = foodText != 0 ? DesireScore(site->id, best, food, foodText) : 0.0f;
	const float needScore = needText != 0 ? DesireScore(site->id, best, need, needText) : 0.0f;
	if (foodScore > needScore) // 0x71B3A2..0x71B3B7
	{
		sample = foodText;
		value = foodScore;
		thing = citadel->citadelPosition;
	}
	if (needScore != 0.0f) // 0x71B3D2..0x71B3E1: the need wins whenever it scored, even below the food's
	{
		sample = needText;
		value = needScore;
		thing = citadel->citadelPosition;
	}
}
} // namespace

// ---- random numbers -----------------------------------------------------------------------------------------------

void guidance::SetRandom(RandomFn random)
{
	g_Guidance.random = std::move(random);
}

uint32_t guidance::LHRand(uint32_t n, uint32_t& seed)
{
	// 0x7DB600: lea 9a, 73a, 293a, shl 5, + a + 0x24DF: seed * 9377 + 9439, then ror 13 and the remainder
	seed = seed * 9377u + 0x24DFu;
	seed = (seed >> 13) | (seed << 19);
	return seed % n;
}

uint32_t guidance::LocalRand(uint32_t n)
{
	if (n == 0)
	{
		return 0; // 0x6DE574
	}
	if (g_Guidance.random)
	{
		return g_Guidance.random(n);
	}
	// (approximated) openblack's generator stands for LHRand on g_game+0x205A3C
	if (!Locator::rng::has_value())
	{
		return 0;
	}
	return Locator::rng::value().NextValue<uint32_t>(0, n - 1);
}

float guidance::LocalFloatRand(float x)
{
	if (x == 0.0f)
	{
		return 0.0f; // 0x6DE597..0x6DE5AD
	}
	// 0x6DE5B9..0x6DE5DA: LHRand(0xFFFF) as a 64-bit integer, x it, x 0x37800080
	constexpr float k_Scale = 1.5259022e-5f;
	return static_cast<float>(static_cast<double>(LocalRand(0xFFFF)) * x * k_Scale);
}

void guidance::SetSpriteLists(const std::array<SpriteList, k_SpriteLists>& lists)
{
	g_Guidance.lists = lists;
}

// ---- the core -----------------------------------------------------------------------------------------------------

void guidance::Init()
{
	// 0x71AC74..0x71ACC0: a new LH_SamplePlayOptions (ctor 0x10010E90), bank GAudio+0x3D0 (Guidance), +0x08 = 0, +0x0C = 0,
	// +0x50 = 2
	g_Guidance.options = sample_play::Options {};
	g_Guidance.options.is3D = false;
	g_Guidance.options.track = false;
	g_Guidance.options.mode = 2;
	g_Guidance.sample = {Bank(SfxBank::Guidance), 0};
	// 0x71ACCF..0x71AD17
	const uint32_t turn = Turn();
	for (size_t t = 0; t < k_TypeCount; ++t)
	{
		const auto first = static_cast<int32_t>(turn - LocalRand(k_Types.at(t).base));
		g_Guidance.lastPlayed.at(t) = first > 0 ? turn - LocalRand(k_Types.at(t).base) : 0;
	}
	// 0x71AD19..0x71AD69: the LastThings list emptied
	g_Guidance.things.clear();
	// 0x71AD6F..0x71ADD5
	g_Guidance.lastDesireSample = 0;      // +0x98
	g_Guidance.lastBeliefSample = 0;      // +0x9C
	g_Guidance.heartBeatPlaying = false;  // +0xA0
	g_Guidance.heartBeatOverride = 0.0f;  // +0xB8
	g_Guidance.heartBeatPhase = 0.0f;     // +0xB4
	g_Guidance.heartBeatPulse = 0.0f;     // +0xAC
	g_Guidance.heartBeatPulsePrevious = 0.0f; // +0xB0
	g_Guidance.believers = 0.0f;          // +0xC4
	g_Guidance.beliefShare = 0.0f;        // +0xC8
	g_Guidance.totemHeight = 0.0f;        // +0xBC
	g_Guidance.alignmentChange = 0.0f;    // +0xC0
	g_Guidance.heartBeatPitch = 30.0f;    // +0xA8 = 0x41F00000
	g_Guidance.oneOffs.fill(false);       // +0xD0..+0xEC (rep stosd, 7)
	g_Guidance.enabled = true;            // [0xC221CC] = 1
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Guidance: Init at turn {}", turn);
	}
}

void guidance::Close()
{
	g_Guidance.things.clear();
}

void guidance::ResetForTests()
{
	auto random = std::move(g_Guidance.random);
	auto lists = g_Guidance.lists;
	g_Guidance = GuidanceState {};
	g_Guidance.random = std::move(random);
	g_Guidance.lists = lists;
	g_Guidance.enabled = true;
	g_Guidance.options.is3D = false;
	g_Guidance.options.track = false;
	g_Guidance.options.mode = 2;
	g_Guidance.sample = {Bank(SfxBank::Guidance), 0};
}

const State& guidance::GetState()
{
	return g_Guidance;
}

uint32_t guidance::TimeSinceLastPlayed(Type type)
{
	return Turn() - g_Guidance.lastPlayed.at(static_cast<size_t>(type));
}

uint32_t guidance::Interval(Type type)
{
	// 0x71AEE3..0x71AF49: r = LocalFloatRand(1.0) kept as a float; r^3 in the x87; 5 base as a 64-bit integer x (1 - r^3)
	const float r = LocalFloatRand(1.0f);
	const uint32_t base = k_Types.at(static_cast<size_t>(type)).base;
	const auto five = static_cast<double>(static_cast<int32_t>(base * 5u));
	const auto spread = static_cast<int32_t>(five * (1.0 - Cube(r))); // __ftol: truncation
	return LocalRand(static_cast<uint32_t>(spread)) + base;
}

bool guidance::PlayNow(Type type)
{
	const auto& info = k_Types.at(static_cast<size_t>(type));
	const auto& queries = Queries();
	if (!info.always)
	{
		const int land = queries.landNumber ? queries.landNumber() : 0;
		const bool multiplayer = queries.multiplayerGame && queries.multiplayerGame();
		const bool playground = queries.playgroundGame && queries.playgroundGame();
		if (land == 1 && !multiplayer && !playground)
		{
			return false; // 0x71AF6F..0x71AF8F
		}
	}
	const int level = queries.helpLevel ? queries.helpLevel() : 3; // 0x71AF99..0x71AFB1
	if (level < info.helpLevel)
	{
		return false;
	}
	const uint32_t interval = Interval(type); // 0x71AFBE, then GetTimeSinceLastPlayed 0x71AFC8
	return TimeSinceLastPlayed(type) > interval;
}

bool guidance::HelpSpritesPlayNow(Type type)
{
	return PlayNow(Type::HelpSprites) && PlayNow(type);
}

Channel guidance::PlaySample(bool is3D, uint32_t textOrSample, uint32_t owner, int type, int volume, int pitch,
                             int field2C, std::optional<glm::vec3> point, float maxDistance, bool isText)
{
	auto& options = g_Guidance.options;
	options.volume = volume;   // +0x28 (0x71C6FD)
	options.pitch = pitch;     // +0x48
	// +0x2C (the ctor's 90): kept, not modelled (sample_play::Options has no +0x2C; its use in LHSamplePlay is not read)
	g_Guidance.field2C = field2C;
	// 0x71C717..0x71C72E: the voice table 0x96BA38 (+8 of 0x96BA30) for a text
	g_Guidance.sample.number = static_cast<int>(isText ? voices::Table().Get(textOrSample).sample : textOrSample);
	options.owner = PlayerOwner(owner); // +0x20
	if (is3D)
	{
		options.is3D = true; // +0x08
		if (!point)
		{
			return k_NoChannel; // 0x71C757: no MapCoords, nothing (and lastPlayed stays)
		}
		// 0x71C75D..0x71C7C1: (x / 6553.6, GetAltitude + y, z / 6553.6), max = maxDistance, min = maxDistance x 0.333
		options.position = *point;
		options.maxDistance = maxDistance;
		options.minDistance = maxDistance * 0.333f;
		options.callerMask = 0x180;
		options.track = false;
	}
	else
	{
		options.is3D = false; // 0x71C7CA
	}
	PlayOptions play;
	static_cast<sample_play::Options&>(play) = options;
	play.sound = 0;
	play.sample = g_Guidance.sample;
	const auto channel = PlaySoundEffect(play); // 0x71C7DE
	if (type < static_cast<int>(k_TypeCount))
	{
		g_Guidance.lastPlayed.at(static_cast<size_t>(type)) = Turn(); // 0x71C7E7..0x71C7F8
	}
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Guidance: type {} {} {} {} -> sample {} {} (channel {})", type,
		                   isText ? "text" : "sample", textOrSample, is3D ? "3D" : "2D", BankGroup(play.sample.bank),
		                   play.sample.number, channel);
	}
	return channel;
}

void guidance::HelpSpiritSay(uint32_t text, Type type)
{
	const auto& queries = Queries();
	const auto script = type == Type::JustTalkNoText ? k_ScriptWithNoText : k_ScriptWithText; // 0x71D276
	const bool started = queries.helpRunMessage && queries.helpRunMessage(text, text, script);
	if (queries.helpTriggerCategory)
	{
		queries.helpTriggerCategory(8); // 0x71D2BD
	}
	const uint32_t turn = Turn();
	if (static_cast<size_t>(type) < k_TypeCount)
	{
		g_Guidance.lastPlayed.at(static_cast<size_t>(type)) = turn; // 0x71D2D4
	}
	g_Guidance.lastSpiritSay = turn; // +0x2C
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Guidance: HelpSpiritSay({}, type {}) {} {}", text, static_cast<int>(type),
		                   script, started ? "started" : "not started");
	}
}

void guidance::OneOff(int index)
{
	if (index < 0 || index >= static_cast<int>(k_OneOffs.size()))
	{
		return; // (openblack) the original indexes its 7 flags unchecked
	}
	auto& done = g_Guidance.oneOffs.at(static_cast<size_t>(index));
	if (done)
	{
		return; // 0x71D0BF
	}
	const auto& oneOff = k_OneOffs.at(static_cast<size_t>(index));
	if (LocalFloatRand(1.0f) > oneOff.probability) // fcomp; test ah, 0x41; je: skipped when r > p
	{
		return;
	}
	HelpSpiritSay(oneOff.text, Type::OneOff); // 0x71D0E5: push 0x20
	done = true;
}

uint32_t guidance::GetRandomSample(size_t list)
{
	if (list >= k_SpriteLists)
	{
		return 0;
	}
	const auto& entries = g_Guidance.lists.at(list);
	return entries.at(LocalRand(ListCount(entries)));
}

uint32_t guidance::GetRandomSampleBasedOnValue(size_t list, float value)
{
	if (list >= k_SpriteLists)
	{
		return 0;
	}
	// 0x71D320..0x71D358: the value clamped to 0..1
	if (value < 0.0f)
	{
		value = 0.0f;
	}
	else if (value > 1.0f)
	{
		value = 1.0f;
	}
	const auto& entries = g_Guidance.lists.at(list);
	const auto n = static_cast<int32_t>(static_cast<double>(ListCount(entries)) * value); // fild qword, fmul, __ftol
	return entries.at(LocalRand(static_cast<uint32_t>(n))); // r < n <= ListCount <= 33
}

uint32_t guidance::TimeSinceThingSeen(uint32_t thing)
{
	const uint32_t turn = Turn();
	for (auto& [id, seen] : g_Guidance.things)
	{
		if (id == thing)
		{
			if (turn - seen > k_ThingForgetTurns) // 0x71AE65: jbe
			{
				seen = turn;
			}
			return turn - seen;
		}
	}
	g_Guidance.things.emplace_front(thing, turn); // 0x71AE99..0x71AEAA: the new node is the head
	return 0;
}

uint32_t guidance::DesireSample(uint32_t desire, float value)
{
	// 0x71AA90..0x71AAA7: x = value - LocalFloatRand(value x 0x3EAAAAAB) in the x87
	const double x = static_cast<double>(value) - static_cast<double>(LocalFloatRand(value * 0.33333334f));
	if (desire >= k_DesireTexts.size())
	{
		return 0; // (openblack) the original reads past the table
	}
	const auto& texts = k_DesireTexts.at(desire);
	if (x >= 0.85f) // 0x980324
	{
		return texts[0];
	}
	if (x >= 0.65f) // 0x980320
	{
		return texts[1];
	}
	if (x >= 0.45f) // 0x98031C
	{
		return texts[2];
	}
	return 0;
}

float guidance::DesireScore(uint32_t thing, float distance, float value, uint32_t sample)
{
	// 0x71B41A..0x71B465: t0 = min(TimeSince(0) / 50, 1), kept as a float
	float t0 = static_cast<float>(TimeSinceLastPlayed(Type::TownDesire)) / k_DesireTurns;
	if (!(t0 < 1.0f))
	{
		t0 = 1.0f;
	}
	// 0x71B46B..0x71B4B4: t1 = min(fn_0071AE10(thing) / 300, 1); the function is called again for the value kept
	float t1 = static_cast<float>(TimeSinceThingSeen(thing)) / k_ThingTurns;
	if (t1 < 1.0f)
	{
		t1 = static_cast<float>(TimeSinceThingSeen(thing)) / k_ThingTurns;
	}
	else
	{
		t1 = 1.0f;
	}
	const double seen = 1.0 - Cube(t1); // 0x71B4B4..0x71B4C4
	// 0x71B4CA..0x71B4ED: d = min(distance / 200, 1), 1 - d^2
	double d = static_cast<double>(distance / k_DesireDistance);
	if (!(d < 1.0))
	{
		d = 1.0;
	}
	const double near = 1.0 - d * d;
	// 0x71B4F3..0x71B51E: v = min(value, 1) (stored as a float), v^3
	const float v = value < 1.0f ? value : 1.0f;
	const double v3 = Cube(v);
	// 0x71B520..0x71B549: the sample said last time (+0x98) -> t0^3
	const double s = sample == g_Guidance.lastDesireSample ? Cube(t0) : 1.0;
	// 0x71B549..0x71B55D: 2 t0 (s v^3 near seen)
	const double r = seen * (v3 * s * near);
	return static_cast<float>(2.0 * (r * static_cast<double>(t0)));
}

// ---- the turn ------------------------------------------------------------------------------------------------------

void guidance::ProcessTownDesireSFX()
{
	if (Turn() % 10 != 0) // 0x71B02D..0x71B03A
	{
		return;
	}
	if (!PlayNow(Type::TownDesire))
	{
		return;
	}
	const auto camera = CameraPosition(); // GInterfaceStatus+0xB0
	if (!camera)
	{
		return; // (openblack) the original always has the interface's camera
	}
	uint32_t sample = 0;
	float value = 0.0f;
	std::optional<glm::vec3> thing;
	CheckTownDesires(sample, value, thing, *camera);
	CheckWorshipSiteDesires(sample, value, thing, *camera);
	if (!(value > k_DesireThreshold)) // 0x71B09F: test ah, 0x41; jne
	{
		return;
	}
	// 0x71B0AC..0x71B0C8: x = value - LocalFloatRand(value x 0.5)
	const double x = static_cast<double>(value) - static_cast<double>(LocalFloatRand(value * 0.5f));
	if (!thing)
	{
		return; // 0x71B0CC
	}
	PlaySample(true, sample, LocalPlayer(), static_cast<int>(Type::TownDesire), 127, 100, 90, thing,
	           static_cast<float>(k_DesireDistance * x), true);
	g_Guidance.lastDesireSample = sample; // 0x71B114: +0x98
}

void guidance::ProcessHeartBeatSFX()
{
	auto& g = g_Guidance;
	// 0x71C1A2..0x71C1AD: off the tenth turns the value is kept and the beat still runs (jne 0x71C3B8: fn_0071C460(+0xA4))
	if (Turn() % 10 != 0)
	{
		HeartBeat(g.heartBeatValue);
		return;
	}
	const auto& queries = Queries();
	const auto input = queries.heartBeat ? queries.heartBeat() : HeartBeatInput {};
	// 0x71C1B6..0x71C1FF: +0xA4 = the sum of the towns' GetRawDesire(3)
	g.heartBeatValue = 0.0f + input.protectionDesire;
	const float p = input.believers;   // 0x71C20E
	const float q = input.beliefShare; // fn_0064B700 0x71C224
	// 0x71C229..0x71C26C: ((+0xC8 + 0.001) / (q + 0.001) - 1) + ((+0xC4 + 0.001) / (p + 0.001) - 1) + +0xA4
	constexpr double k_Small = 0.0010000000474974513; // 0x8AA3B0
	const double shareTerm = (static_cast<double>(g.beliefShare) + k_Small) / (static_cast<double>(q) + k_Small) - 1.0;
	const double believersTerm =
	    (static_cast<double>(g.believers) + k_Small) / (static_cast<double>(p) + k_Small) - 1.0;
	g.heartBeatValue = static_cast<float>(shareTerm + believersTerm + static_cast<double>(g.heartBeatValue));
	// 0x71C272..0x71C2A0: both smoothed by 0.1 (0x8AB22C)
	g.believers = static_cast<float>((static_cast<double>(p) - g.believers) * 0.1f + g.believers);
	g.beliefShare = static_cast<float>((static_cast<double>(q) - g.beliefShare) * 0.1f + g.beliefShare);
	// 0x71C2A6..0x71C379: another player's creature near the local player's town: 1 - max(d - 100, 0) / 400 when d < 400
	for (const uint32_t distance : input.enemyCreatureDistances)
	{
		const auto d = static_cast<double>(distance);
		if (!(d < 400.0)) // 0x980170
		{
			continue;
		}
		double over = d - 100.0; // 0x98016C
		if (!(over > 0.0))
		{
			over = 0.0;
		}
		g.heartBeatValue = static_cast<float>(1.0 - over / 400.0 + g.heartBeatValue);
	}
	// 0x71C37F..0x71C3AE
	if (g.heartBeatValue < 0.0f)
	{
		g.heartBeatValue = 0.0f;
	}
	else if (g.heartBeatValue > 1.0f)
	{
		g.heartBeatValue = 1.0f;
	}
	HeartBeat(g.heartBeatValue); // 0x71C3C1 (then the debug line "HeartBeatvalue: %.2f")
}

float guidance::MoonPhase(int64_t unixTime)
{
	// 0x86A845..0x86A888: the whole days (the magic division by 86400, truncated) - 0x2AD2, x the double 0x9A3BE8; the
	// fraction by __ftol (truncation) and (1 - it) x the double 0x8D45D8
	const auto days = static_cast<int32_t>(unixTime / 86400) - 0x2AD2;
	const double cycles = static_cast<double>(days) * 0.03386318012808897;
	const double fraction = cycles - static_cast<double>(static_cast<int64_t>(cycles));
	return static_cast<float>((1.0 - fraction) * 6.2831854820251465);
}

void guidance::HelpSpritesCheckMoonPhase()
{
	auto& countdown = g_Guidance.moonCountdown;
	countdown = static_cast<uint32_t>(static_cast<int32_t>(countdown) - 1); // 0x71D1C8..0x71D1D1
	if (static_cast<int32_t>(countdown) >= 1)
	{
		return;
	}
	const auto& queries = Queries();
	if (!(queries.visualNight && queries.visualNight()))
	{
		return; // 0x71D1DC: IsVisualNight 0x5575E0
	}
	// 0x71D1E5..0x71D1FD: fn_0086A7F0 - pi (0x8C36A0), kept as a float
	const float x = MoonPhase(spooky::UnixTime()) - 3.14159274f;
	if (spooky::NightNow(spooky::LocalTime()) && std::abs(static_cast<double>(x)) < 0.15000000596046448) // 0x9804D0
	{
		OneOff(5);              // 0x71D22D
		countdown = 0x927C0;    // 600000
		return;
	}
	countdown = static_cast<uint32_t>(static_cast<int32_t>(static_cast<double>(x) * x * 12000.0f)); // 0x9804C8
}

void guidance::ProcessGameTurn()
{
	// OPENBLACK_TEST_GUIDANCE_SAY=<turn>:<HELP_TEXT> (openblack test hook): HelpSpiritSay(text, OneOff) once at that turn
	if (static const char* k_Say = std::getenv("OPENBLACK_TEST_GUIDANCE_SAY"); k_Say != nullptr)
	{
		const std::string_view hook(k_Say);
		if (const auto colon = hook.find(':'); colon != std::string_view::npos)
		{
			const auto turn = static_cast<uint32_t>(std::strtoul(std::string(hook.substr(0, colon)).c_str(), nullptr, 10));
			const auto text = static_cast<uint32_t>(std::strtoul(std::string(hook.substr(colon + 1)).c_str(), nullptr, 10));
			if (Turn() == turn)
			{
				SPDLOG_LOGGER_INFO(spdlog::get("audio"), "OPENBLACK_TEST_GUIDANCE_SAY: HelpSpiritSay({}) at turn {}", text, turn);
				HelpSpiritSay(text, Type::OneOff);
			}
		}
	}
	spooky::Process();            // 0x54E711
	HelpSpritesCheckMoonPhase();  // 0x54E716
	ProcessTownDesireSFX();       // 0x54E729
	// GConfirmation::Process 0x54E731: milestone C7
	// GInterfaceStatus::Process 0x5DC50D for MyInterface()+0x39C: (approximated) its place in the turn
	ProcessHeartBeatSFX();
}

// ---- the events ----------------------------------------------------------------------------------------------------

uint32_t guidance::ResourceDropSample(float need, RainType type)
{
	// 0x71B5F0..0x71B7B7: three texts from LocalRand(3) (0, 1, 2)
	const auto pick = [](uint32_t first) { return first + LocalRand(3); };
	switch (type)
	{
	case RainType::Food:
		if (!(need < k_ResourcePleased))
		{
			return pick(0x1352); // PLEASED_FOOD_01..03
		}
		return need < k_ResourceDispleased ? pick(0x135B) : 0; // DISPLEASED_FOOD_01..03
	case RainType::Wood:
		if (!(need < k_ResourcePleased))
		{
			return pick(0x1355); // PLEASED_WOOD_01..03
		}
		return need < k_ResourceDispleased ? pick(0x1355) : 0; // the same (0x71B6E8..0x71B711)
	case RainType::Rain:
		if (!(need < k_ResourcePleased))
		{
			return pick(0x1358); // PLEASED_RAIN_01..03
		}
		return need < k_ResourceDispleased ? pick(0x1358) : 0; // the same (0x71B660..0x71B689)
	default:
		return 0;
	}
}

void guidance::ResourceDropSFX(glm::vec3 point, RainType type)
{
	if (!PlayNow(Type::ResourceDrop))
	{
		return;
	}
	const auto& queries = Queries();
	const auto needs = queries.townResourceNeeds ? queries.townResourceNeeds(point, k_ResourceTownDistance) : std::nullopt;
	if (!needs)
	{
		return; // 0x71B598: no town
	}
	if (type == RainType::None || type > RainType::Rain)
	{
		return; // GetResourceDropSample's default: 0
	}
	const uint32_t text = ResourceDropSample(needs->at(static_cast<size_t>(type) - 1), type);
	if (text == 0)
	{
		return;
	}
	PlaySample(true, text, LocalPlayer(), static_cast<int>(Type::ResourceDrop), 127, 100, 90, point, k_ResourceMaxDistance,
	           true);
}

int guidance::StrongestEffect(const std::array<float, 7>& effects)
{
	// fn_0071BE40: the best starts at 0 and 7; 3, 5 and 6 are skipped
	int best = 7;
	float strongest = 0.0f;
	for (int i = 0; i < 7; ++i)
	{
		if (i == 3 || i == 5 || i == 6)
		{
			continue;
		}
		float v = effects.at(static_cast<size_t>(i));
		if (i == 0)
		{
			v = v * 0.001f; // 0x8AC418
			if (!(v < 1.0f))
			{
				v = 1.0f;
			}
		}
		if (v > strongest) // fcom; test ah, 0x41; jne
		{
			strongest = v;
			best = i;
		}
	}
	return best;
}

uint32_t guidance::AttackerSample(Attacker attacker)
{
	switch (attacker)
	{
	case Attacker::LightningSpell:
		return 0x132E + LocalRand(10); // 0x71BC47..0x71BC98: LIGHTNING_01..10
	case Attacker::Rock:
		return 0x1342 + LocalRand(10); // 0x71BCD0: ROCKS_01..10
	case Attacker::Creature:
		return 0x1338 + LocalRand(10); // 0x71BD50: MONSTER_01..10
	case Attacker::OtherSpell:
	case Attacker::Thing:
	default:
		return 0; // 0x71BC20 / 0x71BC99
	}
}

void guidance::TownAttackSFX(const TownAttack& attack)
{
	if (PlayNow(Type::TownAttack) && attack.population != 0)
	{
		const int effect = StrongestEffect(attack.effects);
		if (effect != 7)
		{
			// 0x71B80D..0x71BB43: a list whose new nodes go first (fn_0071D470 / fn_0071D3B0)
			std::deque<uint32_t> list;
			for (uint32_t text = 0x131A; text <= 0x1323; ++text) // ATTACK_01..10
			{
				list.push_front(text);
			}
			if (effect == 0)
			{
				for (uint32_t text = 0x1324; text <= 0x132D; ++text) // FIRE_01..10
				{
					list.push_front(text);
				}
			}
			if (attack.attacker)
			{
				if (const uint32_t sample = AttackerSample(*attack.attacker); sample != 0)
				{
					for (int i = 0; i < 10; ++i)
					{
						list.push_front(sample);
					}
				}
			}
			// 0x71BB48..0x71BB74: maxDistance factor min(+0xEC0 x 0.2, 1) + 1
			float factor = attack.severity * 0.2f;
			if (!(factor < 1.0f))
			{
				factor = 1.0f;
			}
			factor += 1.0f;
			// 0x71BB78..0x71BB8E: fn_0071D440(LocalRand(count))
			const uint32_t index = LocalRand(static_cast<uint32_t>(list.size()));
			const uint32_t text = index < list.size() ? list.at(index) : 0;
			PlaySample(true, text, LocalPlayer(), static_cast<int>(Type::TownAttack), 127, 100, 90, attack.position,
			           k_AttackMaxDistance * factor, true);
		}
	}
	// 0x71BBD4..0x71BC0A
	if (attack.townIsMine)
	{
		HelpSpritesTownBeingAttacked(attack);
	}
}

void guidance::HelpSpritesTownBeingAttacked(const TownAttack& attack)
{
	if (!attack.townOfLocalPlayer || !attack.causedPlayer || attack.causedIsLocalPlayer || attack.population == 0)
	{
		return; // 0x71C8AA..0x71C8E0
	}
	if (!PlayNow(Type::TownBeingAttacked))
	{
		return;
	}
	const auto n = static_cast<size_t>(*attack.causedPlayer);
	if (n >= attack.aggression.size() || !(attack.aggression.at(n) > 1.0f)) // 0x98015C
	{
		return;
	}
	SayFromList(Type::TownBeingAttacked, 0);
}

void guidance::StartRaiseTotemSFX(float height)
{
	g_Guidance.totemHeight = height;
}

void guidance::EndRaiseTotemSFX(float height)
{
	if (height > g_Guidance.totemHeight && PlayNow(Type::RaiseTotem))
	{
		// 0x71BEF7: fn_0071C690, whose result picks nothing (dec eax; je; dec eax; ret)
	}
}

int guidance::AlignmentClass(float alignment)
{
	if (alignment > 0.55f) // 0x98014C
	{
		return 1;
	}
	if (alignment < -0.55f) // 0x980150
	{
		return 2;
	}
	return 0;
}

uint32_t guidance::DiscipleText(uint32_t disciple, int alignmentClass)
{
	// fn_0071AB70
	if (disciple == 10)
	{
		if (alignmentClass < 0)
		{
			return 0;
		}
		if (alignmentClass <= 1)
		{
			return LocalFloatRand(1.0f) > 0.5f ? 0x12FB : 0x12FC; // GOOD_LIVE_HERE_01 / _02
		}
		if (alignmentClass == 2)
		{
			return LocalFloatRand(1.0f) > 0.5f ? 0x12FD : 0x12FE; // EVIL_LIVE_HERE_01 / _02
		}
		return 0;
	}
	// 0x98040C: the texts of VILLAGER_DISCIPLE 0..9
	constexpr std::array<uint32_t, 10> k_Texts {0, 4863, 4865, 4868, 4867, 4869, 0, 4870, 4864, 4866};
	return disciple < k_Texts.size() ? k_Texts.at(disciple) : 0;
}

void guidance::MakeDiscipleSFX(uint32_t disciple, float localAlignment)
{
	if (!PlayNow(Type::Disciple))
	{
		return;
	}
	const uint32_t text = DiscipleText(disciple, AlignmentClass(localAlignment));
	PlaySample(false, text, LocalPlayer(), static_cast<int>(Type::Disciple), 85, 100, 90, std::nullopt, 0.0f, true);
}

void guidance::BeliefSFX(const std::array<float, 8>& beliefs, uint32_t player, glm::vec3 point, float distanceToCamera,
                         int alignment)
{
	// 0x437F41..0x437F67: the strongest belief, from 0
	float strongest = 0.0f;
	for (const float belief : beliefs)
	{
		if (strongest < belief)
		{
			strongest = belief;
		}
	}
	if (player >= beliefs.size())
	{
		return;
	}
	const float mine = beliefs.at(player);
	if (!(mine < strongest)) // 0x437F84: only a player below the strongest
	{
		return;
	}
	const auto value = static_cast<float>((static_cast<double>(mine) + 0.0001f) / (static_cast<double>(strongest) + 0.0001f));
	BeliefSample(point, distanceToCamera, value, alignment);
}

void guidance::BeliefSample(glm::vec3 point, float distance, float value, int alignment)
{
	if (!PlayNow(Type::Belief))
	{
		return;
	}
	// 0x71BF89..0x71BFAA: x = value - LocalFloatRand(value x 1/3) (0x8AB26C), kept as a float
	const auto x = static_cast<float>(static_cast<double>(value) - LocalFloatRand(value * 0.33333334f));
	if (alignment != 1 && alignment != 2)
	{
		alignment = LocalRand(2) != 0 ? 2 : 1; // 0x71BFB5..0x71BFC7
	}
	const uint32_t first = alignment == 1 ? 0x134C : 0x134F; // GOOD_AWE / EVIL_AWE
	uint32_t text = 0;
	if (!(x < 0.7f))
	{
		text = first;
	}
	else if (!(x < 0.4f))
	{
		text = first + 1;
	}
	else if (x > 0.05f)
	{
		text = first + 2;
	}
	else
	{
		return;
	}
	if (!(BeliefVisibility(distance, text) > k_BeliefVisibility))
	{
		return;
	}
	PlaySample(true, text, LocalPlayer(), static_cast<int>(Type::Belief), 127, 100, 90, point, k_BeliefMaxDistance, true);
}

float guidance::BeliefVisibility(float distance, uint32_t text)
{
	float t0 = static_cast<float>(TimeSinceLastPlayed(Type::TownDesire)) / k_DesireTurns;
	if (!(t0 < 1.0f))
	{
		t0 = 1.0f;
	}
	double d = static_cast<double>(distance / k_BeliefMaxDistance);
	if (!(d < 1.0))
	{
		d = 1.0;
	}
	const double near = 1.0 - d * d;
	if (text == g_Guidance.lastBeliefSample) // 0x71C154: +0x9C
	{
		return static_cast<float>(Cube(t0) * near * t0);
	}
	return static_cast<float>(near * t0);
}

void guidance::DeathInVillageSFX()
{
	if (!PlayNow(Type::DeathInVillage))
	{
		return;
	}
	const uint32_t text = k_DeathInVillageText + LocalRand(5); // 0x71C825
	PlaySample(false, text, LocalPlayer(), static_cast<int>(Type::DeathInVillage), 127, 100, 90, std::nullopt, 0.0f,
	           true);
}

void guidance::SetHeartBeatOverride(float value)
{
	g_Guidance.heartBeatOverride = value;
}

void guidance::HeartBeat(float value)
{
	auto& g = g_Guidance;
	if (g.heartBeatOverride != 0.0f) // 0x71C476..0x71C48A: fn_0071C400 (x 100 (0x8AB41C) x the double 0.4 0x8CF2B8)
	{
		g.heartBeatPitch = static_cast<float>(static_cast<double>(g.heartBeatOverride * 100.0f) * 0.4);
	}
	else
	{
		g.heartBeatPitch = static_cast<float>((static_cast<double>(value) * 70.0f + 30.0f - g.heartBeatPitch) * 0.1f +
		                                      g.heartBeatPitch);
	}
	// 0x71C4BF..0x71C518: fn_0071C420 (x 0.025, 0x8D150C) x [0xD01A38] x 0.001, brought to <= 1
	const double rate = static_cast<double>(g.heartBeatPitch) * 0.025f;
	g.heartBeatPhase = static_cast<float>(rate * k_MsPerTurn * 0.001f + g.heartBeatPhase);
	if (g.heartBeatPhase > 1.0f)
	{
		do
		{
			g.heartBeatPhase -= 1.0f;
		} while (g.heartBeatPhase > 1.0f);
	}
	// 0x71C51A..0x71C546
	g.heartBeatPulsePrevious = g.heartBeatPulse;
	g.heartBeatPulse = static_cast<float>((1.0 - std::cos(static_cast<double>(g.heartBeatPhase) * 6.2831854820251465)) * 0.5);
	// 0x71C54C..0x71C645: the local interface (openblack's only one) with a living citadel heart
	const auto& queries = Queries();
	const auto input = queries.heartBeat ? queries.heartBeat() : HeartBeatInput {};
	if (!input.citadelHeart)
	{
		return;
	}
	const uint32_t owner = LocalPlayer();
	const auto pitch = static_cast<int>(g.heartBeatPitch); // __ftol
	const auto guidanceBank = g.sample.bank;
	g.sample.bank = Bank(SfxBank::InGame); // GAudio+0x3AC
	g.options.loops = -1;
	g.options.mode = 2;
	PlaySample(true, k_HeartBeatSample, owner, static_cast<int>(Type::HeartBeat), 127, pitch, 90, input.citadelHeart,
	           k_HeartBeatMaxDistance, false);
	g.sample.bank = guidanceBank; // 0x71C618: GAudio+0x3D0 again
	g.options.loops = 0;
	SetPitch(Bank(SfxBank::InGame), PlayerOwner(owner), k_HeartBeatSample, pitch); // fn_00428740 0x71C63F
}

void guidance::StopHeartBeat()
{
	StopSoundEffect(k_HeartBeatSample, PlayerOwner(LocalPlayer()), SfxBank::InGame);
	g_Guidance.heartBeatPlaying = false;
}

float guidance::HeartBeatPulse(float turnFraction)
{
	return (g_Guidance.heartBeatPulse - g_Guidance.heartBeatPulsePrevious) * turnFraction + g_Guidance.heartBeatPulsePrevious;
}

void guidance::HelpSpritesCreatureBeingAttacked()
{
	if (g_Guidance.enabled && HelpSpritesPlayNow(Type::CreatureBeingAttacked))
	{
		SayFromList(Type::CreatureBeingAttacked, 1);
	}
}

void guidance::HelpSpritesCreatureAttackingThem()
{
	if (g_Guidance.enabled && HelpSpritesPlayNow(Type::CreatureAttackingThem))
	{
		SayFromList(Type::CreatureAttackingThem, 2);
	}
}

void guidance::HelpSpritesLosingVillagers(glm::vec3 villager)
{
	if (HelpSpritesPlayNow(Type::LosingVillagers) && NearHand(villager))
	{
		SayFromList(Type::LosingVillagers, 3);
	}
}

void guidance::HelpSpritesAttackingTown(bool townOfLocalPlayer, uint32_t population)
{
	if (townOfLocalPlayer)
	{
		return; // 0x71CA20
	}
	if (HelpSpritesPlayNow(Type::AttackingTown) && population != 0)
	{
		SayFromList(Type::AttackingTown, 4);
	}
}

void guidance::HelpSpritesLowOnFood(const HelpTown& town, float value)
{
	if (HelpSpritesPlayNow(Type::LowOnFood) && town.population != 0 && town.storagePitFunctional && NearHand(town.position))
	{
		HelpSpiritSay(GetRandomSampleBasedOnValue(5, value), Type::LowOnFood);
	}
}

void guidance::HelpSpritesLowOnWood(const HelpTown& town, float value)
{
	if (HelpSpritesPlayNow(Type::LowOnWood) && town.population != 0 && town.storagePitFunctional && NearHand(town.position))
	{
		HelpSpiritSay(GetRandomSampleBasedOnValue(6, value), Type::LowOnWood);
	}
}

void guidance::HelpSpritesInjuredPeople(const HelpTown& town)
{
	if (HelpSpritesPlayNow(Type::InjuredPeople) && town.population != 0 && NearHand(town.position))
	{
		SayFromList(Type::InjuredPeople, 7);
	}
}

void guidance::HelpSpritesLowOnPeople(const HelpTown& town)
{
	if (HelpSpritesPlayNow(Type::LowOnPeople) && town.population != 0 && NearHand(town.position))
	{
		SayFromList(Type::LowOnPeople, 8);
	}
}

void guidance::HelpSpritesVillagerUnhappy(const HelpTown& town)
{
	if (HelpSpritesPlayNow(Type::VillagersUnhappy) && town.population != 0 && town.storagePitFunctional &&
	    NearHand(town.position))
	{
		SayFromList(Type::VillagersUnhappy, 9);
	}
}

void guidance::HelpSpritesLosingBelief(uint32_t population)
{
	if (HelpSpritesPlayNow(Type::LosingBelief) && population != 0)
	{
		SayFromList(Type::LosingBelief, 10);
	}
}

void guidance::HelpSpritesOtherVillages(uint32_t population)
{
	if (HelpSpritesPlayNow(Type::OtherVillages) && population != 0)
	{
		SayFromList(Type::OtherVillages, 11);
	}
}

void guidance::HelpSpritesCreatureFight()
{
	if (HelpSpritesPlayNow(Type::CreatureFight))
	{
		SayFromList(Type::CreatureFight, 12);
	}
}

void guidance::HelpSpritesGeneralBad(glm::vec3 point)
{
	const auto& queries = Queries();
	if (HelpSpritesPlayNow(Type::GeneralBad) && queries.pointOnScreen && queries.pointOnScreen(point))
	{
		SayFromList(Type::GeneralBad, 13);
	}
}

void guidance::HelpSpritesGeneralGood(glm::vec3 point)
{
	const auto& queries = Queries();
	if (HelpSpritesPlayNow(Type::GeneralGood) && queries.pointOnScreen && queries.pointOnScreen(point))
	{
		SayFromList(Type::GeneralGood, 14);
	}
}

void guidance::HelpSpritesKillingPeople(bool onScreen)
{
	if (HelpSpritesPlayNow(Type::KillingPeople) && onScreen)
	{
		SayFromList(Type::KillingPeople, 15);
	}
}

void guidance::HelpSpritesAlignmentProcess(float change, float alignment, float maxChangePerTurn)
{
	auto& g = g_Guidance;
	// 0x71CEB0..0x71CEC9: +0xC0 = 0.95 (0x980178) x +0xC0 + change
	g.alignmentChange = static_cast<float>(static_cast<double>(0.95f) * g.alignmentChange + change);
	// 0x71CED4..0x71CEEF: only past 2 (0x98017C) x the player's maximum change a turn
	if (!(2.0f * maxChangePerTurn < std::abs(g.alignmentChange)))
	{
		return;
	}
	// 0x71CF05..0x71CF32
	const bool same = static_cast<double>(alignment) * g.alignmentChange > 0.0;
	const float threshold = same ? 0.75f : 0.4f; // 0x980180 / 0x980184
	if (!(threshold < std::abs(alignment)))
	{
		return;
	}
	const bool good = alignment > 0.0f;
	if (same)
	{
		// 0x71CF62..0x71CF71: fn_0071D040 (type 29, list 20) when good, fn_0071D010 (type 28, list 19) when not
		const auto type = good ? Type::VeryEvil : Type::VeryGood;
		if (HelpSpritesPlayNow(type))
		{
			SayFromList(type, good ? 20 : 19);
		}
		return;
	}
	// fn_0071CF90(good): type 0x1A - good (25 / 26), list 0x11 - good (16 / 17)
	const auto type = good ? Type::GoodBeingEvil : Type::EvilBeingGood;
	if (HelpSpritesPlayNow(type))
	{
		SayFromList(type, good ? 16 : 17);
	}
}

void guidance::HelpSpritesWorshippersDying()
{
	if (HelpSpritesPlayNow(Type::WorshippersDying))
	{
		SayFromList(Type::WorshippersDying, 18);
	}
}

void guidance::HelpSpritesDestroyBuilding(bool onScreen)
{
	if (HelpSpritesPlayNow(Type::DestroyBuilding) && onScreen)
	{
		SayFromList(Type::DestroyBuilding, 21);
	}
}
