/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "InfluenceSystem.h"

#include <utility>

#include <spdlog/spdlog.h>

#include "Audio/Audio.h"
#include "ECS/Components/VirtualInfluence.h"
#include "ECS/PlayerColours.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Particles/ManaPathMaths.h"
#include "Worship/Citadel.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{
/// The player whose interface openblack has: the local one
PlayerNames InterfacePlayer()
{
	return Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
}

/// The local player's: the first time it is asked, their entity gets one as the game starts it (full strength, switched
/// on). None without a local player's entity
virtual_influence::State* LocalVirtualInfluence()
{
	auto& registry = Locator::entitiesRegistry::value();
	if (const auto carrier = std::as_const(registry).Front<ecs::components::VirtualInfluence>(); carrier != entt::null)
	{
		return &registry.Get<ecs::components::VirtualInfluence>(carrier).state;
	}
	const auto player = magic::players::EntityOf(InterfacePlayer());
	return player == entt::null ? nullptr : &registry.AssignState<ecs::components::VirtualInfluence>(player).state;
}

/// A map position as the influence code takes it, in metres on the ground
glm::vec3 PointOf(const map_coords::MapCoords& place)
{
	const auto metres = map_coords::ToMetres(place);
	return {metres.x, 0.0f, metres.y};
}

/// The hum of the hand past the border: InGame's sample 129, heard alike from both sides
constexpr int k_HumSample = 129;
/// Played in mode 2, not tracked, the .sad's own settings left as they are
constexpr int k_HumMode = 2;

/// The hum is the player's own
audio::Owner HumOwner(PlayerNames player)
{
	return audio::Owner::Key(static_cast<uint32_t>(player));
}

void StartHum(PlayerNames player, uint32_t pitch)
{
	audio::PlayOptions options;
	options.sample = {audio::Bank(audio::SfxBank::InGame), k_HumSample};
	options.owner = HumOwner(player);
	options.is3D = false;
	options.track = false;
	options.mode = k_HumMode;
	options.pitch = static_cast<int>(pitch);
	audio::PlaySoundEffect(options);
}
} // namespace

void InfluenceSystem::ProcessTurn([[maybe_unused]] uint32_t turn)
{
	influence::ProcessTurn();
}

void InfluenceSystem::UpdateBorders()
{
	influence::Update3DInfluence();
}

void InfluenceSystem::Update(std::chrono::duration<float, std::milli> gameTime)
{
	// The hand's point is tested against the circles only while the game runs, once the hand has a point
	if (!game_clock::IsPaused())
	{
		if (const auto& hands = Locator::handSystem::value().GetPlayerHandPositions(); hands[0].has_value())
		{
			influence::ProcessHandCrossing(*hands[0]);
		}
	}
	// Every ripple ages by the frame's game time, which is none while paused. The frame's whole milliseconds come back
	// exactly from the float
	influence::UpdateRipples(static_cast<uint32_t>(gameTime.count()));
}

float InfluenceSystem::PlayerInfluence(PlayerNames player, const glm::vec3& position) const
{
	return influence::CalculatePlayerInfluence(player, position);
}

float InfluenceSystem::PlayerInfluence(PlayerNames player, const glm::vec3& position, influence::CalcType type,
                                       bool includeAllies) const
{
	return influence::CalculatePlayerInfluence(player, position, type, includeAllies);
}

bool InfluenceSystem::IsInAntiInfluence(PlayerNames player, const glm::vec3& position) const
{
	return influence::IsInAntiInfluence(player, position);
}

std::span<const influence::Circle> InfluenceSystem::GetCircles() const
{
	return influence::Circles();
}

bool InfluenceSystem::IsBorderShown(PlayerNames player) const
{
	return influence::BoundaryShown(player);
}

std::span<const influence::Ripple> InfluenceSystem::GetRipples() const
{
	return influence::Ripples();
}

void InfluenceSystem::SetVirtualInfluence(PlayerNames player, bool enabled)
{
	// Every interface of the player is switched: a player without one has nothing to switch
	if (player != InterfacePlayer())
	{
		return;
	}
	if (auto* state = LocalVirtualInfluence(); state != nullptr)
	{
		virtual_influence::SetEnabled(*state, enabled);
	}
}

void InfluenceSystem::ProcessVirtualInfluence(const map_coords::MapCoords& handPlace)
{
	auto* state = LocalVirtualInfluence();
	if (state == nullptr)
	{
		return;
	}
	const auto player = InterfacePlayer();
	const auto hand = PointOf(handPlace);
	const auto& citadel = Locator::infoConstants::value().citadel;
	// Asked in the game's order: an anti-influence ring first, which leaves the player's own influence unasked
	const bool shielded = influence::IsInAntiInfluence(player, hand);
	const bool inside = !shielded && influence::IsInPlayerRawInfluence(player, hand);
	const auto tenthBefore = static_cast<int>(state->fraction * 10.0f);
	// Every waning turn draws on the worship sites' spare chants, even with no strength left to keep
	const float chants =
	    virtual_influence::Wanes(*state, shielded, inside) ? worship::citadel::UseChantsForVirtualInfluence(player) : 0.0f;
	virtual_influence::ProcessTurn(*state,
	                               {
	                                   .hand = handPlace,
	                                   .turn = game_clock::Turn(),
	                                   .handShielded = shielded,
	                                   .handInInfluence = inside,
	                                   .influencePower = influence::InfluencePower(player),
	                                   .chants = chants,
	                               },
	                               {
	                                   .maxDistance = citadel.virtualInfluenceMaxDistance,
	                                   .maxTurns = citadel.virtualInfluenceMaxGameTicks,
	                                   .chantsToDouble = citadel.virtualInfluenceChantsToDouble,
	                               });
	// The log follows the strength a tenth at a time, with what the worship sites spared that turn
	if (static_cast<int>(state->fraction * 10.0f) != tenthBefore)
	{
		SPDLOG_LOGGER_DEBUG(
		    spdlog::get("game"),
		    "The hand {} its player's influence at ({:.1f}, {:.1f}): strength {:.2f}, worship sites' chants {:.2f}",
		    inside ? "is in" : (shielded ? "is shielded from" : "is out of"), hand.x, hand.z, state->fraction, chants);
	}
}

void InfluenceSystem::ProcessVirtualInfluenceAtClearMap(const map_coords::MapCoords& handPlace, bool newGame)
{
	// The player service holds it from the clear's start until the next land's local player takes it up
	if (!Locator::playerSystem::has_value())
	{
		return;
	}
	auto* kept = Locator::playerSystem::value().VirtualInfluenceAtClearMap(newGame);
	if (kept == nullptr)
	{
		return;
	}
	const auto player = InterfacePlayer();
	const auto& citadel = Locator::infoConstants::value().citadel;
	// The land's citadels, towns and rings are deleted already: the hand is asked about none of them
	virtual_influence::ProcessTurn(kept->state,
	                               virtual_influence::AtClearMap(handPlace, game_clock::Turn(),
	                                                             influence::IsInfluenceEverywhere(),
	                                                             influence::InfluencePower(player)),
	                               {
	                                   .maxDistance = citadel.virtualInfluenceMaxDistance,
	                                   .maxTurns = citadel.virtualInfluenceMaxGameTicks,
	                                   .chantsToDouble = citadel.virtualInfluenceChantsToDouble,
	                               });
}

void InfluenceSystem::DrawVirtualInfluence(const glm::vec3& handPosition, uint32_t frameGameMs, bool handEffectsDrawn)
{
	// The trace's line, once a second of real time, while the hand is out of its player's influence
	constexpr uint32_t k_TraceEveryMs = 1000;
	_traceMs += game_clock::FrameRealMs();
	const bool traceDue = _trace && _traceMs >= k_TraceEveryMs;
	// Only once the game has made it: a draw before that has nothing to show
	auto& registry = Locator::entitiesRegistry::value();
	const auto carrier = std::as_const(registry).Front<ecs::components::VirtualInfluence>();
	if (carrier == entt::null)
	{
		if (traceDue)
		{
			_traceMs = 0;
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Virtual influence: no state yet");
		}
		return;
	}
	auto& state = registry.Get<ecs::components::VirtualInfluence>(carrier).state;
	const auto player = InterfacePlayer();
	// The drawn hand's place on the ground, at no altitude
	const auto hand = map_coords::FromMetres({handPosition.x, handPosition.z});
	const auto handPoint = PointOf(hand);
	const auto anchorPoint = PointOf(state.anchor);
	const bool handShielded = influence::IsInAntiInfluence(player, handPoint);
	const bool anchorShielded = influence::IsInAntiInfluence(player, anchorPoint);
	const virtual_influence::DrawInputs inputs {
	    .hand = hand,
	    .handShielded = handShielded,
	    .handInInfluence = !handShielded && influence::IsInPlayerRawInfluence(player, handPoint),
	    .anchorShielded = anchorShielded,
	    .anchorInInfluence = !anchorShielded && influence::IsInPlayerRawInfluence(player, anchorPoint),
	};
	const auto frame = handEffectsDrawn ? virtual_influence::DrawFrame(state, inputs) : virtual_influence::FrameDraw {};
	if (traceDue && !inputs.handInInfluence)
	{
		_traceMs = 0;
		const auto anchorMetres = map_coords::ToMetres(state.anchor);
		SPDLOG_LOGGER_INFO(
		    spdlog::get("game"),
		    "Virtual influence: hand out of influence at ({:.1f}, {:.1f}), shielded {}; {}; strength {:.2f}, off {}; was "
		    "inside {} (last at ({:.1f}, {:.1f}), turn {}), last place in still inside {} (shielded {}); shown {}, hum "
		    "started {}",
		    handPoint.x, handPoint.z, inputs.handShielded,
		    Locator::handSystem::has_value() ? Locator::handSystem::value().DescribeHandEffectsDraw() : "no hand",
		    state.fraction, state.disabled, state.anchor.x != 0, anchorMetres.x, anchorMetres.y, state.lastInsideTurn,
		    inputs.anchorInInfluence, inputs.anchorShielded, frame.shown.has_value(), state.soundStarted);
	}
	if (!handEffectsDrawn)
	{
		return;
	}
	if (frame.stopHum)
	{
		audio::StopSoundEffect(k_HumSample, HumOwner(player), audio::SfxBank::InGame);
	}
	if (!frame.shown.has_value())
	{
		return;
	}
	const auto& shown = *frame.shown;
	if (shown.startPitch.has_value())
	{
		StartHum(player, *shown.startPitch);
	}
	// A spark of the mana path, from the hand on the ground back to the path's start, in the player's colour dimmed by
	// the strength
	if (const auto scale = virtual_influence::EmitManaPath(_manaPathClock, shown.pathStrength, static_cast<float>(frameGameMs));
	    scale.has_value() && Locator::particleSystem::has_value())
	{
		const uint32_t colour = ecs::player_colours::Colour(player, ecs::player_colours::LandNumber()) | 0xFF000000u;
		Locator::particleSystem::value().AddHandManaPathSpark({
		    .from = map_coords::ToWorld(hand),
		    .to = map_coords::ToWorld(shown.pathStart),
		    .rgb = virtual_influence::ScaleColour(colour, *scale) & 0xFFFFFFu,
		});
	}
	// Started again when it has played out, and held to the strength left
	if (!audio::IsPlaying(HumOwner(player), k_HumSample, audio::SfxBank::InGame))
	{
		StartHum(player, shown.humPitch);
	}
	audio::SetPitch(audio::Bank(audio::SfxBank::InGame), HumOwner(player), k_HumSample, static_cast<int>(shown.humPitch));
}

const virtual_influence::State* InfluenceSystem::GetVirtualInfluence(PlayerNames player) const
{
	if (player != InterfacePlayer() || !Locator::entitiesRegistry::has_value())
	{
		return nullptr;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto carrier = registry.Front<ecs::components::VirtualInfluence>();
	return carrier == entt::null ? nullptr : &registry.Get<const ecs::components::VirtualInfluence>(carrier).state;
}
