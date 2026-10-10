/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureDesires.h"

#include <cmath>

#include <algorithm>
#include <limits>

#include "Common/GUtilsDistance.h"
#include "Creature/CreatureLayers.h"

using namespace openblack;
using namespace openblack::creature_desires;
namespace animations = openblack::creature_layers::animations;

namespace
{
constexpr std::array<std::string_view, k_DesireCount> k_Names {
    "Impress",
    "Compassion",
    "Anger",
    "Play",
    "Hunger",
    "Fear",
    "Curiosity",
    "Poo",
    "Tiredness",
    "Idle with player",
    "Wanderlust",
    "Puke",
    "Build home",
    "Bring home",
    "Water",
    "Restore health",
    "Be friends",
    "Attract attention",
    "Manifest state",
    "Get warmer",
    "Get colder",
    "Scratch",
    "Run from player",
    "Rest",
    "Obey player",
    "Illness",
    "Obey creature",
    "Sadness",
    "Stay near home",
    "Tell player",
    "Play with player",
    "Tell creature",
    "Educate friend",
    "Follow player",
    "Get high",
    "Hang around home",
    "Mental illness",
    "Miss friend",
    "Look around",
    "Steal",
};

/// The action that shows each desire the creature can show
struct Emote
{
	Desire desire;
	size_t animation;
};
constexpr std::array<Emote, 20> k_Emotes {{
    {Desire::Impress, animations::k_Summon},
    {Desire::Compassion, animations::k_FeelingNice},
    {Desire::Anger, animations::k_Taunt},
    {Desire::Play, animations::k_FeelPlayful},
    {Desire::Hunger, animations::k_Hungry},
    {Desire::Fear, animations::k_Frightened},
    {Desire::Curiosity, animations::k_Confused},
    {Desire::Poo, animations::k_NeedAPoo},
    {Desire::Tiredness, animations::k_Tired},
    {Desire::IdleWithPlayer, animations::k_FeelPlayful},
    {Desire::Water, animations::k_Hot},
    {Desire::BeFriends, animations::k_Happy},
    {Desire::AttractAttention, animations::k_FriendlyWave},
    {Desire::GetWarmer, animations::k_Cold},
    {Desire::GetColder, animations::k_Hot},
    {Desire::Rest, animations::k_Tired},
    {Desire::Sadness, animations::k_Sad},
    {Desire::PlayWithPlayer, animations::k_FeelPlayful},
    {Desire::MissFriend, animations::k_Sad},
    {Desire::Steal, animations::k_FeelPlayful},
}};
} // namespace

std::string_view creature_desires::Name(Desire desire)
{
	const auto index = static_cast<size_t>(desire);
	return index < k_Names.size() ? k_Names.at(index) : "Unknown";
}

float creature_desires::Sigmoid(float threshold, float value)
{
	return gutils::CreatureSigmoidThreshold(threshold, value);
}

float creature_desires::SigmoidStep(float threshold, float value)
{
	return gutils::SigmoidThreshold(threshold, value);
}

Desires creature_desires::Create(const std::array<DesireSetup, k_DesireCount>& setup,
                                 const std::function<float(float, float)>& uniform)
{
	Desires desires;
	for (size_t i = 0; i < k_DesireCount; ++i)
	{
		const auto& from = setup.at(i);
		auto& desire = desires.desires.at(i);
		desire.max = from.max;
		desire.decay = uniform(from.decayMin, from.decayMax);
		desire.increaseSeconds = from.increaseSeconds;
		desire.weight = from.weight;
		for (const auto& source : from.sources)
		{
			if (source.type < k_NoSource && desire.sources.size() < k_MaxSources)
			{
				desire.sources.push_back(source);
			}
		}
	}
	return desires;
}

void creature_desires::ActivateForPhase(Desires& desires, std::span<const PhaseDesires> phases, size_t phase)
{
	for (auto& desire : desires.desires)
	{
		desire.suppressedTurns = 0;
		desire.activated = false;
	}
	for (size_t i = 0; i <= phase && i < phases.size(); ++i)
	{
		for (const auto desire : phases[i].add)
		{
			desires[desire].activated = true;
		}
		for (const auto desire : phases[i].remove)
		{
			desires[desire].activated = false;
		}
	}
}

void creature_desires::UpdateSources(Desires& desires, const SourceReader& read)
{
	for (auto& desire : desires.desires)
	{
		for (auto& source : desire.sources)
		{
			if (const auto value = read ? read(source.type, desires) : std::nullopt)
			{
				source.value = *value;
			}
			source.value *= source.multiplier;
		}
	}
}

void creature_desires::UpdateDesires(Desires& desires, float turnsPerSecond)
{
	desires.sum = 0.0f;
	for (auto& desire : desires.desires)
	{
		if (!desire.activated)
		{
			continue;
		}
		float drive = 0.0f;
		for (auto& source : desire.sources)
		{
			const auto sourceDrive = Sigmoid(source.threshold, source.value);
			source.drive += sourceDrive;
			drive += sourceDrive;
		}
		const auto increase =
		    desire.increaseSeconds > 0.0f && turnsPerSecond > 0.0f ? drive / (turnsPerSecond * desire.increaseSeconds) : 0.0f;
		if (desire.suppressedTurns > 0)
		{
			--desire.suppressedTurns;
		}
		desire.value = increase > 0.0f && desire.suppressedTurns == 0 ? desire.value + increase : desire.value * desire.decay;
		desire.value = std::clamp(desire.value, 0.0f, std::max(desire.max, 0.0f));
		desires.sum += desire.value;
	}
}

void creature_desires::Suppress(Desires& desires, Desire desire, float seconds, float turnsPerSecond)
{
	auto& state = desires[desire];
	const auto turns = static_cast<uint32_t>(std::max(seconds * turnsPerSecond, 0.0f));
	state.suppressedTurns = std::max(state.suppressedTurns, turns);
}

void creature_desires::Unsuppress(Desires& desires, Desire desire)
{
	desires[desire].suppressedTurns = 0;
}

void creature_desires::UnsuppressAll(Desires& desires)
{
	for (auto& state : desires.desires)
	{
		state.suppressedTurns = 0;
	}
}

void creature_desires::MakeFullyDominantWithFullSources(Desires& desires, Desire desire, float floor)
{
	auto& chosen = desires[desire];
	if (chosen.activated)
	{
		for (size_t i = 0; i < k_DesireCount; ++i)
		{
			auto& state = desires.desires.at(i);
			state.value = i == static_cast<size_t>(desire) ? state.max : floor;
		}
	}
	for (auto& source : chosen.sources)
	{
		if (source.type != sources::k_AngerFromDamage && source.type != sources::k_RestoreHealthFromLife)
		{
			source.value = 1.0f;
		}
	}
	chosen.activated = true;
}

bool creature_desires::detail::HeldDownByDominant(Desire other, bool needsToo)
{
	switch (other)
	{
	case Desire::Hunger:
	case Desire::Poo:
	case Desire::Tiredness:
	case Desire::Water:
		return needsToo;
	case Desire::IdleWithPlayer:
	case Desire::RestoreHealth:
	case Desire::BeFriends:
	case Desire::ManifestState:
	case Desire::Rest:
	case Desire::PlayWithPlayer:
	case Desire::HangAroundAtHome:
	case Desire::LookAround:
		return false;
	default:
		return true;
	}
}

namespace
{
/// The seconds truncated to a whole number, as the game keeps them. Beyond the integers' range the game's own result
/// depends on the processor; this gives the one of a processor with SSE2
uint32_t WholeSeconds(float seconds)
{
	constexpr float k_IntegerLimit = 2147483648.0f;
	if (!(seconds > -k_IntegerLimit && seconds < k_IntegerLimit))
	{
		return 0x80000000u;
	}
	return static_cast<uint32_t>(static_cast<int32_t>(seconds));
}
} // namespace

void creature_desires::detail::SetDominant(DominantDesire& dominant, Desires& desires, Desire desire, float seconds,
                                           bool needsToo, uint32_t turnsPerSecond, float floor)
{
	dominant = {.desire = desire, .turns = 0, .seconds = WholeSeconds(seconds), .needsToo = needsToo};
	desires[desire].activated = true;
	const auto perSecond = static_cast<float>(turnsPerSecond);
	for (size_t i = 0; i < k_DesireCount; ++i)
	{
		const auto other = static_cast<Desire>(i);
		if (other != desire && HeldDownByDominant(other, needsToo))
		{
			Suppress(desires, other, seconds, perSecond);
		}
	}
	// Compassion makes it want to make friends too; its value then goes back to the floor as compassion takes over,
	// but it is free, active and its sources full
	if (desire == Desire::Compassion)
	{
		Unsuppress(desires, Desire::BeFriends);
		MakeFullyDominantWithFullSources(desires, Desire::BeFriends, floor);
	}
	Unsuppress(desires, desire);
	MakeFullyDominantWithFullSources(desires, desire, floor);
}

void creature_desires::detail::StepDominant(DominantDesire& dominant, Desires& desires, uint32_t turnsPerSecond)
{
	if (!dominant.desire.has_value())
	{
		return;
	}
	Unsuppress(desires, *dominant.desire);
	++dominant.turns;
	// The game counts whole seconds by its whole turns a second. With none (turns over a second long) its division
	// would fail; here the desire then never runs out
	if (turnsPerSecond > 0 && dominant.turns / turnsPerSecond > dominant.seconds)
	{
		UnsuppressAll(desires);
		dominant.desire.reset();
	}
}

void creature_desires::detail::ClearDominant(DominantDesire& dominant, Desires& desires)
{
	if (dominant.desire.has_value())
	{
		UnsuppressAll(desires);
	}
	dominant.desire.reset();
}

uint32_t creature_desires::detail::DominantTurnsLeft(const DominantDesire& dominant, uint32_t turnsPerSecond)
{
	if (!dominant.desire.has_value() || turnsPerSecond == 0)
	{
		return 0;
	}
	// It runs out on the turn the count reaches one second past its seconds
	const auto end = (static_cast<uint64_t>(dominant.seconds) + 1) * turnsPerSecond;
	const auto left = end > dominant.turns ? end - dominant.turns : 0;
	return static_cast<uint32_t>(std::min<uint64_t>(left, std::numeric_limits<uint32_t>::max()));
}

void creature_desires::ChangeSource(Desires& desires, uint32_t type, float amount)
{
	for (auto& desire : desires.desires)
	{
		for (auto& source : desire.sources)
		{
			if (source.type == type)
			{
				source.value = std::clamp(source.value + amount, 0.0f, 1.0f);
			}
		}
	}
}

std::optional<float> creature_desires::SourceValue(const DesireState& desire, uint32_t type)
{
	const auto found = std::ranges::find(desire.sources, type, &Source::type);
	if (found == desire.sources.end())
	{
		return std::nullopt;
	}
	return found->value;
}

std::optional<size_t> creature_desires::EmoteFor(Desire desire)
{
	const auto found = std::ranges::find(k_Emotes, desire, &Emote::desire);
	if (found == k_Emotes.end())
	{
		return std::nullopt;
	}
	return found->animation;
}

std::optional<Desire> creature_desires::StrongestShowable(const Desires& desires, float minimum)
{
	std::optional<Desire> strongest;
	float strongestValue = minimum;
	for (const auto& emote : k_Emotes)
	{
		const auto& state = desires[emote.desire];
		if (state.activated && state.value > strongestValue)
		{
			strongest = emote.desire;
			strongestValue = state.value;
		}
	}
	return strongest;
}

Desire creature_desires::FindDominant(const Desires& desires, std::optional<Desire> except)
{
	auto dominant = Desire::Impress;
	float strongest = 0.0f;
	for (size_t i = 0; i < k_DesireCount; ++i)
	{
		const auto desire = static_cast<Desire>(i);
		const auto& state = desires.desires.at(i);
		if (state.activated && desire != except && state.value > strongest)
		{
			dominant = desire;
			strongest = state.value;
		}
	}
	return dominant;
}
