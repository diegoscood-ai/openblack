/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownBelief.h"

#include <cmath>

#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

#include "Audio/Services/Guidance.h"
#include "ECS/Components/Town.h"
#include "ECS/Influence/Influence.h"
#include "ECS/MapCells.h"
#include "ECS/MapCoords.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Town/TownDesire.h"
#include "ECS/Town/TownQueries.h"
#include "GameClock.h"
#include "Help/ToolTips.h"
#include "InfoConstants.h"
#include "LandBalance.h"
#include "Locator.h"
#include "PSys/Rules/SurfRevol.h"

// Belief.cpp of runblack.exe W120 (TownBelief.h). Every float rule is x87 on 32-bit stored values: the port keeps the
// original's order in float and uses double where the original keeps an x87 intermediate that is not stored
// (approximate: double is not the 80-bit register, the products of two floats are exact in both)

namespace openblack::ecs::town_belief
{
using components::Town;
using components::TownBelief;

namespace
{
/// g_game +0x205A5B, GPlayer::IsNeutral 0x64AC08: slot 7 (GGame::SetupPlayers 0x550458 and GGame::Load 0x554988
/// mov byte [+0x205A5B], 7; no other writer)
constexpr PlayerNames k_Neutral = PlayerNames::NEUTRAL;
/// g_game +0x205A59 and GPlayer::IsMemberOfThisPlayer(MyInterfaceStatus) 0x64D750. (inferred) PLAYER_ONE, as
/// TownDesire's isLocalPlayer
constexpr PlayerNames k_LocalPlayer = PlayerNames::PLAYER_ONE;

/// fn_0069B550's array 0xD4ED00 (data +0, capacity +4, count +8; grown by 10, fn_0069B6B0)
std::vector<BeliefSprite> g_BeliefSprites;
detail::ToolTipSink g_ToolTipSink;
detail::HelpSink g_HelpSink;

constexpr size_t Slot(PlayerNames player)
{
	return static_cast<size_t>(player);
}

constexpr bool InRange(PlayerNames player)
{
	return static_cast<size_t>(player) < k_Players;
}

Town* TownOf(entt::entity town)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return nullptr;
	}
	auto& registry = Locator::entitiesRegistry::value();
	return town != entt::null && registry.Valid(town) ? registry.TryGet<Town>(town) : nullptr;
}

bool Valid(entt::entity object)
{
	return object != entt::null && Locator::entitiesRegistry::has_value() &&
	       Locator::entitiesRegistry::value().Valid(object);
}

/// ftol(f x 10000) [0x8C58A4]: truncated toward 0 (the product of a float and 10000 is exact in a double)
int32_t Amount(float f)
{
	return static_cast<int32_t>(static_cast<double>(f) * k_AmountScale);
}

/// GPlayer::GetPlayerColour 0x64D800: 0xBFF0B8[GetRemapedPlayer] (psys::surf_revol's table; the land remap is not
/// applied there either)
uint32_t PlayerColour(PlayerNames player)
{
	return psys::surf_revol::PlayerColour(static_cast<int>(player));
}

/// &town +0x14 / an object's GetLHPoint 0x605C40 (map_coords::ToWorld of its MapCoords)
glm::vec3 GroundPoint(entt::entity object)
{
	return map_coords::ToWorld(object::MapCoordsOf(object));
}

void Help(detail::HelpSprite kind, entt::entity town)
{
	if (g_HelpSink)
	{
		g_HelpSink(kind, town);
		return;
	}
	switch (kind)
	{
	case detail::HelpSprite::LosingBelief:
		// fn_0071CCC0 (town): HelpSpritesPlayNow(19 LosingBelief) && adults + children (+0x618 + +0x61C) -> list 10
		if (const auto* t = TownOf(town); t != nullptr)
		{
			audio::guidance::HelpSpritesLosingBelief(t->stats.adults + t->stats.children);
		}
		break;
	case detail::HelpSprite::GeneralBad:
		audio::guidance::HelpSpritesGeneralBad(GroundPoint(town)); // fn_0071CD70(&town.pos) 0x43872B
		break;
	case detail::HelpSprite::GeneralGood:
		audio::guidance::HelpSpritesGeneralGood(GroundPoint(town)); // fn_0071CDF0(&town.pos) 0x438755
		break;
	}
}

/// ReduceBelief 0x437FD0 on a town component already found
void Reduce(Town& t, PlayerNames player, float r)
{
	if (!InRange(player))
	{
		return;
	}
	auto& b = t.belief;
	const size_t n = Slot(player);
	b.belief.at(n) -= r; // 0x437FE3..0x437FEB, not clamped: the belief can go below 0
	if (Valid(t.centre)) // 0x437FEF: +0x9A4
	{
		b.reduceAccumulator.at(n) += r; // 0x437FF9..0x438004
		if (static_cast<double>(b.reduceAccumulator.at(n)) > k_ReduceDrawThreshold) // 0x43800B..0x438016 (jne skips <=)
		{
			// 0x43803C: DrawBelief(-acc, town centre, GetPlayer(n)); a negative amount draws nothing (0x438817 jle)
			DrawBelief(-b.reduceAccumulator.at(n), t.centre, player);
			b.reduceAccumulator.at(n) = 0.0f; // 0x438041
		}
	}
}

} // namespace

// ---- Small methods --------------------------------------------------------------------------------------------------

void ResetDesireThresholds(TownBelief& belief)
{
	// fn_00437E50: GTownDesireInfo 0xDA2930 + d x 0x90, +0x3C desireAffectsBeliefAfter (0xDA296C .. 0xDA32FC)
	if (!Locator::infoConstants::has_value())
	{
		belief.desireThreshold.fill(0.0f); // (openblack) no info: the zeroed allocation
		return;
	}
	const auto& info = Locator::infoConstants::value().townDesire;
	for (size_t d = 0; d < k_Desires; ++d)
	{
		belief.desireThreshold.at(d) = info.at(d).desireAffectsBeliefAfter;
	}
}

void Init(Town& town)
{
	auto& b = town.belief;
	// 0x437DDE..0x437DF4: the 8 slots
	b.belief.fill(0.0f);
	b.cap.fill(k_InitialCap);
	b.pending.fill(0.0f);
	b.reduceAccumulator.fill(0.0f);
	b.lastAddedTurn.fill(0);
	// 0x437DF6..0x437E2C: belief[neutral] = fn_0073E4B0(town) = Town +0x5D8 (written raw, no SetBelief)
	b.belief.at(Slot(k_Neutral)) = b.beliefInNeutralPlayer;
	b.boredom.fill(k_InitialBoredom); // 0x437E30..0x437E40 rep stosd 0x29
	ResetDesireThresholds(b);         // 0x437E44
}

void SetBelief(TownBelief& belief, PlayerNames player, float value)
{
	if (!InRange(player)) // (openblack) a guard of the array
	{
		return;
	}
	const size_t n = Slot(player);
	// 0x4387D4: fld cap; fcomp v; test ah, 1 -> C0 (cap < v) -> cap
	belief.belief.at(n) = belief.cap.at(n) < value ? belief.cap.at(n) : value;
}

float GetBeliefInPlayer(const TownBelief& belief, PlayerNames player)
{
	return InRange(player) ? belief.belief.at(Slot(player)) : 0.0f;
}

float GetBeliefInPlayer(entt::entity town, PlayerNames player)
{
	const auto* t = TownOf(town);
	return t != nullptr ? GetBeliefInPlayer(t->belief, player) : 0.0f;
}

void SetCap(TownBelief& belief, PlayerNames player, float cap)
{
	if (InRange(player))
	{
		belief.cap.at(Slot(player)) = cap; // 0x438A00
	}
}

float GetCap(const TownBelief& belief, PlayerNames player)
{
	return InRange(player) ? belief.cap.at(Slot(player)) : 0.0f; // 0x438A20
}

float GetAddedThisPeriod(const TownBelief& belief, PlayerNames player)
{
	return InRange(player) ? belief.addedThisPeriod.at(Slot(player)) : 0.0f;
}

float GetAddedThisPeriodRatio(const TownBelief& belief, PlayerNames player)
{
	if (!InRange(player))
	{
		return 0.0f;
	}
	const float added = belief.addedThisPeriod.at(Slot(player));
	if (added == 0.0f)
	{
		return 0.0f;
	}
	const auto mine = static_cast<double>(belief.belief.at(Slot(player)));
	return static_cast<float>(static_cast<double>(added) / (mine * 10.0));
}

void SetBeliefInPlayer(Town& town, PlayerNames player, float value)
{
	if (player == k_Neutral)
	{
		town.belief.beliefInNeutralPlayer = value; // 0x73BA87: +0x5D8
	}
	SetBelief(town.belief, player, value);
}

void ReduceBelief(entt::entity town, PlayerNames player, float r)
{
	if (auto* t = TownOf(town); t != nullptr)
	{
		Reduce(*t, player, r);
	}
}

void AddToBoredomMultiplier(TownBelief& belief, size_t reaction, float f)
{
	if (reaction >= k_Reactions)
	{
		return;
	}
	auto& value = belief.boredom.at(reaction);
	const float b = value + f;
	value = b < 0.0f ? 0.0f : b; // 0x4387A2 fcom 0; C0 -> 0
}

float GetBoredomMultiplier(entt::entity town, size_t reaction)
{
	const auto* t = TownOf(town);
	if (t == nullptr || reaction >= k_Reactions)
	{
		return 1.0f; // 0x56FE70: no town -> 1.0
	}
	return t->belief.boredom.at(reaction);
}

float GetMaxBeliefMeNotIncluded(const TownBelief& belief, PlayerNames player)
{
	float best = 0.0f;
	for (size_t i = 0; i < k_Players; ++i)
	{
		if (i != Slot(player) && belief.belief.at(i) > best)
		{
			best = belief.belief.at(i);
		}
	}
	return best;
}

float RelativeBelief(const TownBelief& belief, PlayerNames player, PlayerNames thingPlayer)
{
	if (thingPlayer != player)
	{
		return GetBeliefInPlayer(belief, thingPlayer);
	}
	const float mine = GetBeliefInPlayer(belief, player);
	float bestDifference = k_RelativeStart;
	float result = 0.0f;
	for (size_t i = 0; i < k_Players; ++i)
	{
		if (i == Slot(player))
		{
			continue;
		}
		const float difference = belief.belief.at(i) - mine;
		if (difference > bestDifference)
		{
			bestDifference = difference;
			result = belief.belief.at(i);
		}
	}
	return result;
}

float RivalRecentRatio(const TownBelief& belief, PlayerNames player, const std::function<bool(PlayerNames)>& allied)
{
	// 0x438A40: the slots 0..5
	constexpr size_t k_RivalSlots = 6;
	float best = k_RivalRecentStart;
	std::optional<size_t> rival;
	for (size_t i = 0; i < k_RivalSlots; ++i)
	{
		if (i == Slot(player) || (allied && allied(static_cast<PlayerNames>(i))))
		{
			continue;
		}
		if (belief.recent.at(i) > best)
		{
			best = belief.recent.at(i);
			rival = i;
		}
	}
	const float mine = GetBeliefInPlayer(belief, player);
	if (!rival.has_value() || mine == 0.0f)
	{
		return 0.0f;
	}
	// 0x438AC9..0x438AFD: x = 2 belief[i] / belief[P]; fld st0; fld st0; fmulp = x^2; the min with 1
	const double x = 2.0 * static_cast<double>(belief.belief.at(*rival)) / static_cast<double>(mine);
	const double squared = x * x;
	return squared < 1.0 ? static_cast<float>(squared) : 1.0f;
}

float BeliefRatioVsRivals(const TownBelief& belief, PlayerNames player, const std::function<bool(PlayerNames)>& allied)
{
	float best = 0.0f;
	for (size_t i = 0; i < k_Players; ++i)
	{
		const auto other = static_cast<PlayerNames>(i);
		if (other == player || other == k_Neutral || (allied && allied(other)))
		{
			continue;
		}
		if (belief.belief.at(i) > best)
		{
			best = belief.belief.at(i);
		}
	}
	const float mine = GetBeliefInPlayer(belief, player);
	return mine == 0.0f ? 0.0f : static_cast<float>(static_cast<double>(best) / static_cast<double>(mine));
}

float LostTownScale()
{
	return land_balance::LostTownScale();
}

// ---- The fold fn_004383D0 -------------------------------------------------------------------------------------------

void Fold(entt::entity town)
{
	auto* t = TownOf(town);
	if (t == nullptr || !Locator::infoConstants::has_value())
	{
		return;
	}
	const auto& info = Locator::infoConstants::value();
	auto& b = t->belief;
	// 0x4383E1..0x4383F1: P = town.GetPlayer() (vt +0x1C) -> [esp+0x18], its number -> [esp+0x14]
	const PlayerNames owner = t->owner;
	const float lost = LostTownScale(); // [0xBF33F0]

	// 1. pending -> belief, every player (0x4383FA..0x4384B4)
	for (size_t i = 0; i < k_Players; ++i)
	{
		const float d = b.beliefScale * b.pending.at(i); // 0x438402..0x43840B: Town +0x5DC x pending[i]
		if (d == 0.0f || std::isnan(d)) // 0x43840F..0x43841A: C3 -> skip, and pending[i] is NOT cleared
		{
			continue;
		}
		b.addedThisPeriod.at(i) += d; // 0x438420..0x438426
		if (Valid(t->centre))         // 0x438428..0x438434: +0x9A4
		{
			// 0x438436..0x438471: fn_0069B550(tc, tc.pos.GetLHPoint(), ftol(d x 10000), GetPlayer(i).GetPlayerColour())
			QueueBeliefSprite(GroundPoint(t->centre), Amount(d), PlayerColour(static_cast<PlayerNames>(i)));
		}
		ResetDesireThresholds(b);                                     // fn_00437E50 0x43847B, whoever the player
		SetBelief(b, static_cast<PlayerNames>(i), b.belief.at(i) + d); // 0x438480..0x4384A1, capped
		b.pending.at(i) = 0.0f;                                       // 0x4384A6
	}

	// 2. the boredom of every reaction (0x4384BA..0x4384EE): x = [0xBF33F0] x ReactionInfo[k] +0x4C + boredom[k]
	for (size_t k = 0; k < k_Reactions; ++k)
	{
		const double x = static_cast<double>(lost) *
		                     static_cast<double>(info.reaction.at(k).additionToTownBoredomMultipliers) +
		                 static_cast<double>(b.boredom.at(k));
		if (x < static_cast<double>(k_BoredomCeiling)) // 0x4384CF fcom 1; C0 -> store; otherwise unchanged
		{
			b.boredom.at(k) = static_cast<float>(x);
		}
	}

	// 3. the desires that cost the owner belief (0x4384F0..0x4385BE), not for the neutral player (0x43850E)
	if (owner != k_Neutral)
	{
		for (size_t j = 0; j < k_Desires; ++j)
		{
			const auto& desireInfo = info.townDesire.at(j);
			auto& threshold = b.desireThreshold.at(j);
			// 0x438552..0x43855E: +0x90 + +0xD4 + +0x118 = Town::GetDesire 0x73E400. (approximate) GetDesire is rounded
			// to float, the original compares the x87 sum
			const float s = town_desire::GetDesire(t->desire, j);
			if (s > threshold) // 0x438565..0x43856C: C0 | C3 -> skip
			{
				// 0x438571..0x438587: (s - threshold) x GTownDesireInfo[j] +0x38 desireToBeliefScale
				const auto r = static_cast<float>((static_cast<double>(s) - static_cast<double>(threshold)) *
				                                  static_cast<double>(desireInfo.desireToBeliefScale));
				Reduce(*t, owner, r); // ReduceBelief 0x438594
			}
			// 0x438599..0x4385B5: above GBeliefInfo +0x1C minimumThreshold (0.25) -> -= +0x48 (it can step below once)
			if (threshold > info.belief.minimumThreshold)
			{
				threshold -= desireInfo.desireToBeliefThresholdDecay;
			}
		}
	}

	// 4. the neutral belief pinned to Town +0x5D8 (0x4385CA..0x4385F7), through SetBelief (capped)
	SetBelief(b, k_Neutral, b.beliefInNeutralPlayer);

	// 5. the strongest player and the decay of recent (0x4385FC..0x43865F). GetNextPlayerAndNeutral 0x550980: the slots
	//    0..7 in order; `test ah, 1; jne`: only < skips, so a tie goes to the higher slot (the owner included)
	size_t best = Slot(owner);
	float bestValue = b.belief.at(best); // kept in the argument slot [esp+0x38]
	const float decay = info.player.computerPlayerBeliefChangeDecay; // GPlayer +0x64 -> GPlayerInfo +0x48 (0.997)
	for (size_t n = 0; n < k_Players; ++n)
	{
		if (b.belief.at(n) >= bestValue) // 0x438624..0x43863F
		{
			bestValue = b.belief.at(n);
			best = n;
		}
		b.recent.at(n) *= decay; // 0x438641..0x43864C
	}

	// 6. the conversion (0x438661..0x438769) when the strongest is not the owner (town.GetPlayer().number, 0x43866F)
	if (best == Slot(t->owner))
	{
		return;
	}
	const auto newOwner = static_cast<PlayerNames>(best);
	// 0x438677..0x438688: SetBelief(best, GBeliefInfo +0x14 claimedTownBeliefMultiplier (1.5) x belief[best])
	SetBelief(b, newOwner,
	          static_cast<float>(static_cast<double>(info.belief.claimedTownBeliefMultiplier) *
	                             static_cast<double>(b.belief.at(best))));
	if (owner != k_Neutral) // 0x4386AF
	{
		// 0x4386BD..0x4386EE: every town of P (+0xA50, next +0x75C; this one is still in the list):
		// SetBelief(P, GBeliefInfo +0x18 lostATownBeliefInPlayerMultiplier (0.9) x belief[P] x [0xBF33F0])
		for (const auto other : map_cells::TownsOf(owner))
		{
			if (auto* o = TownOf(other); o != nullptr)
			{
				const double value = static_cast<double>(info.belief.lostATownBeliefInPlayerMultiplier) *
				                     static_cast<double>(o->belief.belief.at(Slot(owner))) * static_cast<double>(lost);
				SetBelief(o->belief, owner, static_cast<float>(value));
			}
		}
	}
	// 0x438704..0x43875D: the old owner local -> HelpSpritesGeneralBad, else the new one local -> GeneralGood; then
	// GPlayer::TakeOverTown fn_00649810(newP, town) (0x438733 / 0x43875D)
	if (owner == k_LocalPlayer)
	{
		Help(detail::HelpSprite::GeneralBad, town);
	}
	else if (newOwner == k_LocalPlayer)
	{
		Help(detail::HelpSprite::GeneralGood, town);
	}
	TakeOverTown(newOwner, town);
}

void TakeOverTown(PlayerNames player, entt::entity town)
{
	auto* t = TownOf(town);
	if (t == nullptr)
	{
		return;
	}
	// fn_00649810 -> fn_0073A7D0 (0x6498EA): +0xF20 = 0 (0x73A7F7), then SetPlayer fn_0073A8F0 (0x73A904): +0x2C.
	// (pending) the rest: see the header
	const auto previous = t->owner;
	t->emptyCountdown = 0;
	t->owner = player;
	if (auto logger = spdlog::get("game"); logger != nullptr)
	{
		SPDLOG_LOGGER_DEBUG(logger, "town_belief: town {} taken over by player {} (was {})", t->id,
		                    static_cast<int>(player), static_cast<int>(previous));
	}
}

void SetTownEmpty(entt::entity town)
{
	auto* t = TownOf(town);
	if (t == nullptr)
	{
		return;
	}
	Init(*t); // 0x74108E
	for (size_t i = 0; i < k_Players; ++i)
	{
		SetBelief(t->belief, static_cast<PlayerNames>(i), t->belief.beliefInNeutralPlayer);
	}
	if (t->owner != k_Neutral)
	{
		TakeOverTown(k_Neutral, town);
	}
}

// ---- GBelief::ProcessOncePerTurn 0x4380B0 ---------------------------------------------------------------------------

void ProcessOncePerTurn()
{
	// 0x4380B6..0x4380CB: g_game +0x205A40 % 10 (unsigned div)
	if (game_clock::Turn() % k_ProcessEvery != 0 || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const size_t me = Slot(k_LocalPlayer); // 0x4380D1..0x438100: localPlayer.number (+0xB5)
	float myAdded = 0.0f;
	// g_game +0x205C84, the newest town first (the Town ctor's head insertion 0x73964D..0x739656)
	const auto towns = town_queries::TownsNewestFirst();
	for (size_t p = 0; p < k_Players; ++p) // ebx 0x7A0..0x7C0
	{
		for (const auto town : towns) // 0x43810C..0x43816E
		{
			auto& b = registry.Get<Town>(town).belief;
			const float a = b.addedThisPeriod.at(p); // 0x438128 (0x438060)
			if (p == me && a > 0.0f)                  // 0x438131..0x438150
			{
				myAdded += a;
			}
			b.addedThisPeriod.at(p) = 0.0f; // 0x438158
			// 0x438163: sum += belief[p], then GetPlayer(p).game_stats (+0xA44) fn_0056A3B0(sum) 0x438177..0x438187:
			// the player's world belief max (+0x1070) / min (+0x1074). (not ported) statistics, no reader in openblack
		}
	}
	if (myAdded > k_ToolTipMinimum) // 0x43819C..0x4381AB
	{
		// 0x4381B1: v = myAdded x 1000; ToolTips::ForceToolTips(0xEE1, v) 0x4381C9 (with a leftover ecx: ToolTips does
		// not use `this`, 0x5C9C60)
		const float v = myAdded * k_ToolTipScale;
		if (g_ToolTipSink)
		{
			g_ToolTipSink(k_ToolTipBelieversGained, v);
		}
		else
		{
			help::tooltips::Force(k_ToolTipBelieversGained, v);
		}
		// 0x4381D0..0x438281: new ValueSpinner (0xA0, Belief.cpp line 0xAD) "%4.0f" of v at MyInterface +0x3B8 (the
		// point under the hand: x / 6553.6, GetAltitude + +8, z / 6553.6), colour bytes 00 FF FF FF, ValueSpinner::Init
		// 0x833A00 (+0x18 = 5.0, +0x14 = [0xECCD08]). (pending) openblack has no ValueSpinner
	}
	CheckLosingBelief(k_LocalPlayer); // fn_00438340(localPlayer) 0x4382B9, inside the % 10 block
}

void CheckLosingBelief(PlayerNames player)
{
	// 0x438345: nothing on land 1 (g_game +0x205A08)
	if (influence::LandNumber() == 1 || !InRange(player) || !Locator::infoConstants::has_value())
	{
		return;
	}
	const auto warn = static_cast<double>(Locator::infoConstants::value().belief.beliefLeftWhenHelpSpritesWarn);
	const size_t n = Slot(player);
	for (const auto town : map_cells::TownsOf(player)) // P +0xA50, next +0x75C
	{
		const auto* t = TownOf(town);
		if (t == nullptr)
		{
			continue;
		}
		const double mine = static_cast<double>(t->belief.belief.at(n)) - warn;
		for (size_t i = 0; i < k_Players; ++i) // the neutral slot included
		{
			if (i == n)
			{
				continue;
			}
			if (mine < static_cast<double>(t->belief.belief.at(i))) // 0x43837B..0x43838A
			{
				if (player == k_LocalPlayer) // P.IsMemberOfThisPlayer(MyInterfaceStatus())
				{
					Help(detail::HelpSprite::LosingBelief, town); // fn_0071CCC0(t) 0x4383C4
				}
				return; // the first town found only
			}
		}
	}
}

// ---- DrawBelief and the belief sprites ------------------------------------------------------------------------------

void DrawBelief(float f, entt::entity thing, std::optional<PlayerNames> player)
{
	const int32_t v = Amount(f); // 0x438800..0x43880E
	if (v <= 0)                  // 0x438815 jle
	{
		return;
	}
	glm::vec3 position {0.0f}; // (openblack) 0: the original leaves it uninitialised without a thing
	if (Valid(thing))
	{
		// 0x438827..0x438864: {x / 6553.6, GetAltitude(pos) + pos +8 + GetHeight (vt +0x42C), z / 6553.6}
		position = GroundPoint(thing);
		position.y += object::GetHeight(thing);
	}
	// 0x43886C..0x43887B: g_game +0x14 & 0x4000 -> the "B n" ValueSpinner (Belief.cpp line 0x15B, fn_00833A50). (not
	// ported, §8 Q3) a debug flag no one sets
	const uint32_t colour = player.has_value() ? PlayerColour(*player) : k_NoPlayerColour; // 0x4388DD..0x4388EA
	QueueBeliefSprite(position, v, colour);                                                  // 0x4388F5
}

void QueueBeliefSprite(const glm::vec3& position, int32_t amount, uint32_t colour)
{
	// 0x69B550..0x69B56B: [0xC02A08] != 1 (a constant 1, §8 Q4) || [0xD4ED30] >= 400 -> nothing
	if (g_BeliefSprites.size() >= k_MaxBeliefSprites)
	{
		return;
	}
	g_BeliefSprites.push_back({position, amount, colour});
}

std::optional<BeliefSprite> PopBeliefSprite()
{
	if (g_BeliefSprites.empty())
	{
		return std::nullopt;
	}
	const auto last = g_BeliefSprites.back(); // 0x69B781: the last entry
	g_BeliefSprites.pop_back();
	return last;
}

size_t BeliefSpriteCount()
{
	return g_BeliefSprites.size();
}

void detail::SetHelpSinkForTests(HelpSink sink)
{
	g_HelpSink = std::move(sink);
}

void detail::SetToolTipSinkForTests(ToolTipSink sink)
{
	g_ToolTipSink = std::move(sink);
}

void detail::ClearForTests()
{
	g_BeliefSprites.clear();
}
} // namespace openblack::ecs::town_belief
