/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "HandSystem.h"
#include "HandSystemDetail.h"

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <tuple>

#include <fmt/format.h>
#include <glm/gtc/type_ptr.hpp>

#include <spdlog/spdlog.h>

#include <L3DFile.h>
#include <LNDFile.h>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include "3D/AllMeshes.h"
#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandIslandInterface.h"
#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/Archetypes/HandArchetype.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "Audio/AudioManagerInterface.h"
#include "Camera/Camera.h"
#include "Windowing/WindowingInterface.h"
#include "Camera/CameraModel.h"
#include "Common/RandomNumberManager.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "Magic/Objects/MagicTree.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Influence/Influence.h"
#include "ECS/PotResource.h"
#include "ECS/Components/Sprite.h"
#include "Graphics/Texture2D.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/StoragePitStore.h"
#include "FileSystem/FileSystemInterface.h"
#include "InfoConstants.h"
#include "LandBalance.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::systems::hand_detail;

float HandSystem::HeldFill() const noexcept
{
	if (!_held)
	{
		return 0.0f;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	if (const auto* pot = registry.TryGet<const Pot>(*_held); pot != nullptr && pot->maxAmount > 0)
	{
		return std::clamp(static_cast<float>(pot->amount) / static_cast<float>(pot->maxAmount), 0.0f, 1.0f);
	}
	return 0.0f;
}

PotInfo HandSystem::PotInfoOf(entt::entity entity) noexcept
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity))
	{
		return PotInfo::_COUNT;
	}
	const auto* pot = registry.TryGet<const Pot>(entity);
	if (pot == nullptr)
	{
		return PotInfo::_COUNT;
	}
	if (pot->type != PotInfo::_COUNT)
	{
		return pot->type;
	}
	const auto& pots = Locator::infoConstants::value().pot;
	const auto meshId = registry.Get<const Mesh>(entity).id;
	for (size_t i = 0; i < pots.size(); ++i)
	{
		if (resources::HashIdentifier(pots[i].meshId) == meshId)
		{
			return static_cast<PotInfo>(i);
		}
	}
	return PotInfo::_COUNT;
}

void HandSystem::UpdateMultiPickUp(float seconds, bool actionHeld) noexcept
{
	if (!_held || !_pickSource || !actionHeld)
	{
		_pickSource.reset();
		_pickFish = false;
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(*_pickSource) || !registry.Valid(*_held))
	{
		_pickSource.reset();
		_pickFish = false;
		return;
	}
	if (_pickFish)
	{
		if (!UpdateFishPickUp(seconds))
		{
			_pickSource.reset();
			_pickFish = false;
		}
		return;
	}
	if (_pickField)
	{
		if (!UpdateFieldPickUp(seconds))
		{
			_pickSource.reset();
			_pickField = false;
		}
		return;
	}
	auto* source = registry.TryGet<Pot>(*_pickSource);
	auto* pile = registry.TryGet<Pot>(*_held);
	if (source == nullptr || pile == nullptr)
	{
		_pickSource.reset();
		return;
	}
	// PileResource::ProcessInInteract (0x66E520), once per game turn while the locked select lasts (no distance
	// check). The values come from the hand pot's info:
	//   ticks = (1000 / msPerTurn) * multiPickUpRampTime, t = clamp(n / ticks, 0, 1)
	//   amount = (int)(perTurn + (perTurnEnd - perTurn) * t^2), limited by the source and maxAmountCanBePickedUp.
	const auto handType = PotInfoOf(*_held);
	const bool wood = handType == PotInfo::HandWood;
	const auto& info = Locator::infoConstants::value().pot[static_cast<size_t>(wood ? PotInfo::HandWood : PotInfo::HandFood)];
	constexpr float k_TurnSeconds = 0.1f;
	const auto store = StoragePitStore::OwnerOf(*_pickSource);
	const auto resource = wood ? ResourceType::Wood : ResourceType::Food;
	_pickTime += seconds;
	_pickTurnAccumulator += seconds;
	bool changed = false;
	while (_pickTurnAccumulator >= k_TurnSeconds)
	{
		_pickTurnAccumulator -= k_TurnSeconds;
		// GInterfaceStatus::Process 0x5DC558: the locked select ends where the hand (status+0xC8; here the x,z it is
		// frozen at) is out of the player's influence (CalculatePlayerInfluence(.., 0, 0, allies) <= 0)
		if (influence::CalculatePlayerInfluence(PlayerNames::PLAYER_ONE, _pickLock) <= 0.0f)
		{
			_pickSource.reset();
			break;
		}
		++_pickTurns;
		const float ticks = std::max(1.0f, info.multiPickUpRampTime / k_TurnSeconds);
		const float t = std::clamp(static_cast<float>(_pickTurns) / ticks, 0.0f, 1.0f);
		auto take = static_cast<uint32_t>(static_cast<float>(info.amountPickedUpPerTurn) +
		                                  static_cast<float>(info.amountPickedUpPerTurnEnd - info.amountPickedUpPerTurn) * t * t);
		const uint32_t room = info.maxAmountCanBePickedUp > pile->amount ? info.maxAmountCanBePickedUp - pile->amount : 0u;
		const uint32_t available = store != entt::null ? StoragePitStore::GetResource(store, resource) : source->amount;
		take = std::min({take, available, room, 65535u - pile->amount});
		if (take == 0)
		{
			_pickSource.reset();
			break;
		}
		if (store != entt::null)
		{
			StoragePitStore::RemoveResource(store, resource, take);
		}
		else
		{
			source->amount = static_cast<uint16_t>(source->amount - take);
		}
		pile->amount = static_cast<uint16_t>(pile->amount + take);
		changed = true;
		// UpdateMultiPickup(type, t^2): the looping pick-up sound's pitch, 60 + 180 t^2 percent (UpdatePickupSound)
		_pickupSoundFraction = t * t;
	}
	if (changed && _pickSource)
	{
		// An emptied loose pile is deleted; the piles of a store stay (buried when empty), and the store already
		// resized the piles it took from.
		if (store == entt::null && source->amount == 0)
		{
			registry.Destroy(*_pickSource);
			_pickSource.reset();
		}
		else if (store == entt::null)
		{
			SinkPile(*_pickSource);
		}
		registry.SetDirty();
	}
	if (std::getenv("OPENBLACK_HAND_TRACE") != nullptr)
	{
		static float traceTime = 0.0f;
		traceTime += seconds;
		if (traceTime > 0.5f)
		{
			traceTime = 0.0f;
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Pick trace: turn {} hand pile {}, source left {}", _pickTurns, pile->amount,
			                   _pickSource ? static_cast<int>(source->amount) : -1);
		}
	}
}

void HandSystem::SinkPile(entt::entity pile) noexcept
{
	// Every Just{Add,Remove}Resource calls SetSize: piles ease to their new sink offset, plain pots rescale.
	archetypes::PotArchetype::SetSize(pile, true);
}

void HandSystem::PutDownHandPot(entt::entity pot) noexcept
{
	// Pot::ApplyThisToObject 0x66DDD0 on the land: Pot::AddResourceToPos 0x66F270 (ECS/PotResource) at the pot's position
	// with its amount, IsPoisoned and no speed-up, from the local player's interface. It merges into the stores and
	// same-resource pots of the 3x3 cells around (each within 2 x its 2D radius, 1.2 for a store), else it makes a
	// MagicWood / MagicFood pile.
	auto& registry = Locator::entitiesRegistry::value();
	const auto type = PotInfoOf(pot);
	const bool wood = type == PotInfo::HandWood;
	const auto& held = registry.Get<Pot>(pot);
	const auto amount = held.amount;
	const bool poisoned = held.poisoned;
	auto position = registry.Get<Transform>(pot).position;
	position.y = Locator::terrainSystem::value().GetHeightAt(glm::vec2(position.x, position.z));
	registry.Destroy(pot);
	registry.SetDirty();
	if (amount == 0)
	{
		return;
	}
	const auto resource = wood ? ResourceType::Wood : ResourceType::Food;
	const pot_resource::Dropper dropper {true, PlayerNames::PLAYER_ONE, true};
	const auto put = pot_resource::AddResourceToPos(position, dropper, resource, amount, poisoned, false);
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand: put down {} {} ({} into stores or pots there)", amount, wood ? "wood" : "food", put);
}

std::optional<entt::entity> HandSystem::FindWoodStore(glm::vec3 point) const noexcept
{
	const auto& registry = Locator::entitiesRegistry::value();
	std::optional<entt::entity> store;
	float best = std::numeric_limits<float>::max();
	registry.Each<const StoragePit, const Transform>([&](entt::entity entity, const StoragePit&, const Transform& transform) {
		float radius = 8.0f;
		if (const auto* fixed = registry.TryGet<const Fixed>(entity); fixed != nullptr)
		{
			radius = std::max(radius, fixed->boundingRadius);
		}
		const float distance = glm::distance(glm::vec2(point.x, point.z), glm::vec2(transform.position.x, transform.position.z));
		if (distance <= radius && distance < best)
		{
			best = distance;
			store = entity;
		}
	});
	return store;
}

void HandSystem::DepositInStore(entt::entity object, entt::entity store) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	// Object::DoDeleteObjectAndTakeResource: AddResource(WOOD, GetDefaultResource()), with
	// Tree::GetWoodValue = life (1 for a fresh tree) * woodValue * scale * GLandBalance::Values[5] (2 in Land2).
	const auto type = registry.AllOf<Tree>(object) ? registry.Get<Tree>(object).type : registry.Get<DeadTree>(object).type;
	const auto& info = Locator::infoConstants::value().tree.at(static_cast<size_t>(type));
	// x GetWoodValueMultiplier (vt 0x868): a MagicTree's +0x70 (woodValueMultiplier x tribal power), else 1
	auto wood = static_cast<uint32_t>(static_cast<float>(info.woodValue) * registry.Get<Transform>(object).scale.x *
	                                  land_balance::Get(5) * magic::magic_tree::WoodValueMultiplier(object));
	const uint32_t total = wood;
	StoragePitStore::AddResource(store, ResourceType::Wood, wood);
	static constexpr auto k_TreeMulch = std::array<audio::SoundId, 4> {
	    audio::SoundId::G_TreeMulch_01, audio::SoundId::G_TreeMulch_02, audio::SoundId::G_TreeMulch_03,
	    audio::SoundId::G_TreeMulch_04};
	PlaySample(Locator::rng::value().Choose(k_TreeMulch));
	DropRoots(object, false);
	registry.Destroy(object);
	registry.SetDirty();
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand: {} wood added to the village store", total);
}
