/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LoadPlayerInterface.h"

#include <cstdint>

#include <bit>
#include <variant>

#include <WorldReader.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "ECS/Registry.h"
#include "Enums.h"
#include "Locator.h"
#include "Magic/Gestures/PowerUpSystem.h"
#include "Worship/PlayerSpellIcons.h"

using namespace openblack;
using namespace openblack::ecs;
using gamesave::InterfaceStatusRecord;
using systems::SavedDrawnHand;
using systems::SavedHandStatus;

namespace
{
/// The player whose interface the hand is
constexpr auto k_HandPlayer = PlayerNames::PLAYER_ONE;

/// The drawn hand's state while it holds an object, and where its saved words keep the hold type, whether the object
/// was drawn sorted before the hand took it and how far it hangs below the hand
constexpr uint32_t k_DrawnHandHolding = 4;
constexpr size_t k_DrawnHoldType = 0;
constexpr size_t k_DrawnSortingKept = 1;
constexpr size_t k_DrawnLowering = 3;

glm::vec3 VectorOf(const gamesave::Point& point)
{
	return {point[0], point[1], point[2]};
}

gamesave::Point PointOf(const glm::vec3& vector)
{
	return {vector.x, vector.y, vector.z};
}

/// The three values of the throw's data from the first given
glm::vec3 ThrowPart(const InterfaceStatusRecord& record, size_t first)
{
	return {record.throwAndHand.at(first), record.throwAndHand.at(first + 1), record.throwAndHand.at(first + 2)};
}

void SetThrowPart(InterfaceStatusRecord& record, size_t first, const glm::vec3& vector)
{
	record.throwAndHand.at(first) = vector.x;
	record.throwAndHand.at(first + 1) = vector.y;
	record.throwAndHand.at(first + 2) = vector.z;
}

/// Where each part of the throw's data starts: the velocity, the angular velocity, the hand's position, its angles
constexpr size_t k_ThrowVelocity = 0;
constexpr size_t k_ThrowAngular = 3;
constexpr size_t k_ThrowHandPosition = 6;
constexpr size_t k_ThrowAngles = 9;

/// The record of the number in the loaded file
const gamesave::SavedRecord* RecordAt(const gamesave::WorldRead& world, gamesave::ObjectNumber number)
{
	if (number == 0 || number > world.objects.size() || !world.objects[number - 1].has_value())
	{
		return nullptr;
	}
	return &*world.objects[number - 1];
}
} // namespace

SavedHandStatus saved_hand::StatusOf(const InterfaceStatusRecord& record, std::optional<entt::entity> held)
{
	const auto& place = record.position.position;
	return SavedHandStatus {
	    .syncedPlace = {.x = place.x, .z = place.z, .altitude = place.altitude},
	    .throwVelocity = ThrowPart(record, k_ThrowVelocity),
	    .throwAngular = ThrowPart(record, k_ThrowAngular),
	    .throwHandPosition = ThrowPart(record, k_ThrowHandPosition),
	    .throwAngles = ThrowPart(record, k_ThrowAngles),
	    .unlockTurnsHeld = static_cast<int32_t>(record.secondWord),
	    .stillTurns = record.thirdWord,
	    .syncedHand = VectorOf(record.point),
	    .cameraPosition = VectorOf(record.cameraPosition),
	    .cameraFocus = VectorOf(record.cameraFocus),
	    .turnHand = VectorOf(record.reportedHandPosition),
	    .turnDelta = VectorOf(record.secondPoint),
	    .turnSpeed = record.handSpeed,
	    .turnHeading = record.firstFloat,
	    .turnRate = record.secondFloat,
	    .turnVelocity = VectorOf(record.handVelocity),
	    .turnSideAcceleration = record.thirdFloat,
	    .held = held,
	};
}

void saved_hand::WriteLive(InterfaceStatusRecord& record, const SavedHandStatus& status, gamesave::ObjectNumber held,
                           std::optional<int32_t> lastSeedType)
{
	const auto& place = status.syncedPlace;
	record.position.position = {.x = place.x, .z = place.z, .altitude = place.altitude};
	SetThrowPart(record, k_ThrowVelocity, status.throwVelocity);
	SetThrowPart(record, k_ThrowAngular, status.throwAngular);
	SetThrowPart(record, k_ThrowHandPosition, status.throwHandPosition);
	SetThrowPart(record, k_ThrowAngles, status.throwAngles);
	// the one hand: whether it holds something and what, its one place 0 when empty; the count of hands holding
	const bool holding = held != 0;
	record.hand.byte = holding ? 1 : 0;
	record.hand.held.numbers = {held};
	record.handHoldingSomething = holding ? 1 : 0;
	record.secondWord = static_cast<uint32_t>(status.unlockTurnsHeld);
	record.thirdWord = status.stillTurns;
	record.point = PointOf(status.syncedHand);
	record.cameraPosition = PointOf(status.cameraPosition);
	record.cameraFocus = PointOf(status.cameraFocus);
	record.reportedHandPosition = PointOf(status.turnHand);
	record.secondPoint = PointOf(status.turnDelta);
	record.handSpeed = status.turnSpeed;
	record.firstFloat = status.turnHeading;
	record.secondFloat = status.turnRate;
	record.handVelocity = PointOf(status.turnVelocity);
	record.thirdFloat = status.turnSideAcceleration;
	if (lastSeedType.has_value())
	{
		record.fifthWord = static_cast<uint32_t>(*lastSeedType);
	}
}

std::optional<SavedDrawnHand> saved_hand::DrawnHandOf(const gamesave::GameTail& tail, gamesave::ObjectNumber heldNumber,
                                                      std::optional<entt::entity> held)
{
	if (tail.handWord != k_DrawnHandHolding || !held.has_value() || heldNumber == 0 || tail.handObject.number != heldNumber)
	{
		return std::nullopt;
	}
	return SavedDrawnHand {.held = held,
	                       .holdType = static_cast<HoldType>(tail.handWords.at(k_DrawnHoldType)),
	                       .lowering = std::bit_cast<float>(tail.handWords.at(k_DrawnLowering)),
	                       .hadNeedsSorting = tail.handWords.at(k_DrawnSortingKept) != 0};
}

void saved_hand::WriteDrawnHand(gamesave::GameTail& tail, const systems::DrawnHandNow& drawn, gamesave::ObjectNumber heldNumber,
                                gamesave::ObjectNumber spellNumber)
{
	tail.handWord = static_cast<uint32_t>(drawn.state);
	tail.handObject = {heldNumber};
	if (heldNumber != 0)
	{
		tail.handWords.at(k_DrawnHoldType) = static_cast<uint32_t>(drawn.holdType);
		tail.handWords.at(k_DrawnSortingKept) = drawn.hadNeedsSorting ? 1U : 0U;
		tail.handWords.at(k_DrawnLowering) = std::bit_cast<uint32_t>(drawn.lowering);
	}
	if (drawn.place.has_value())
	{
		tail.handPoint = PointOf(*drawn.place);
	}
	tail.handObjects.at(1) = {spellNumber};
	if (drawn.effectAtBone.has_value())
	{
		tail.handByte = *drawn.effectAtBone ? 1 : 0;
	}
}

saved_hand::Restored saved_hand::Load(const Registry& registry, systems::HandSystemInterface& hand,
                                      const gamesave::WorldRead& world, const saved_world_write::LoadedObjects& loaded)
{
	Restored restored;
	for (const auto& [number, object] : loaded)
	{
		if (object.type != InterfaceStatusRecord::k_Type || object.id != static_cast<uint64_t>(k_HandPlayer))
		{
			continue;
		}
		const auto* record = std::get_if<InterfaceStatusRecord>(RecordAt(world, number));
		if (record == nullptr)
		{
			continue;
		}
		restored.found = true;
		// the object in the hand, when the status's hand holds one and the load made it
		std::optional<entt::entity> held;
		if (record->hand.byte != 0 && !record->hand.held.numbers.empty() && record->hand.held.numbers.front() != 0)
		{
			restored.heldNumber = record->hand.held.numbers.front();
			if (const auto found = loaded.find(restored.heldNumber);
			    found != loaded.end() && found->second.entity != entt::null && registry.Valid(found->second.entity))
			{
				held = found->second.entity;
				restored.heldMade = true;
			}
			else if (const auto log = spdlog::get("game"); log != nullptr)
			{
				SPDLOG_LOGGER_INFO(log, "The hand held object {}, which the load did not make: the hand is put back empty",
				                   restored.heldNumber);
			}
		}
		hand.RestoreSavedStatus(StatusOf(*record, held));
		// then the drawn hand, as the game's record keeps it
		const auto* game = world.objects.empty() || !world.objects.front().has_value()
		                       ? nullptr
		                       : std::get_if<gamesave::GameHead>(&*world.objects.front());
		if (const auto drawn = game != nullptr ? DrawnHandOf(game->tail, restored.heldNumber, held) : std::nullopt;
		    drawn.has_value())
		{
			hand.RestoreSavedDrawnHand(*drawn);
			restored.drawnHolding = true;
		}
		// the seed type the repeat gesture asks for again, the interface's and the player's spell icons' copy
		restored.lastSeedType = static_cast<int32_t>(record->fifthWord);
		if (Locator::magicSystem::has_value())
		{
			magic::gestures::State().lastSeedType = restored.lastSeedType;
		}
		if (Locator::worshipState::has_value())
		{
			worship::player::SetLastSeedType(k_HandPlayer, static_cast<SpellSeedType>(restored.lastSeedType));
		}
		break;
	}
	return restored;
}

std::string saved_hand::ReadoutOf(const Restored& restored)
{
	if (!restored.found)
	{
		return "no status for the hand's player";
	}
	const auto held = restored.heldNumber == 0 ? std::string("nothing")
	                                           : fmt::format("object {} ({})", restored.heldNumber,
	                                                         restored.heldMade ? "in the hand" : "not made, left out");
	return fmt::format("held {}, last seed type {}", held, restored.lastSeedType);
}
