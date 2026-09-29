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
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/Sprite.h"
#include "Graphics/Texture2D.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/StoragePitStore.h"
#include "FileSystem/FileSystemInterface.h"
#include "InfoConstants.h"
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
	// check; TODO: stop outside the player's influence). The values come from the hand pot's info:
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
	// Pot::AddResourceToPos: merge into a same-resource pile or store within a 9-cell spiral (taken as 15 m), else a
	// new MagicWood / MagicFood pile; PILE*SMALL sounds below 200, PILE* otherwise.
	auto& registry = Locator::entitiesRegistry::value();
	const auto type = PotInfoOf(pot);
	const bool wood = type == PotInfo::HandWood;
	const auto amount = registry.Get<Pot>(pot).amount;
	auto position = registry.Get<Transform>(pot).position;
	position.y = Locator::terrainSystem::value().GetHeightAt(glm::vec2(position.x, position.z));
	registry.Destroy(pot);
	registry.SetDirty();
	if (amount == 0)
	{
		return;
	}
	const auto& pots = Locator::infoConstants::value().pot;
	const auto resource = wood ? ResourceType::Wood : ResourceType::Food;
	constexpr float k_MergeRadius = 15.0f;
	std::optional<entt::entity> target;
	float best = k_MergeRadius;
	registry.Each<const Pot, const Transform>([&](entt::entity entity, const Pot&, const Transform& transform) {
		const auto other = PotInfoOf(entity);
		if (other == PotInfo::_COUNT || other == PotInfo::HandWood || other == PotInfo::HandFood ||
		    pots[static_cast<size_t>(other)].resourceType != resource)
		{
			return;
		}
		const float distance = glm::distance(glm::vec2(position.x, position.z), glm::vec2(transform.position.x, transform.position.z));
		if (distance < best)
		{
			best = distance;
			target = entity;
		}
	});
	// A store pile, or a store with no pile in range: the store takes it (StoragePit::AddResource).
	auto intoStore = target ? StoragePitStore::OwnerOf(*target) : entt::null;
	if (const auto store = wood ? FindWoodStore(position) : std::nullopt; store && !target)
	{
		intoStore = *store;
	}
	if (intoStore != entt::null)
	{
		StoragePitStore::AddResource(intoStore, resource, amount);
	}
	else if (target && registry.Valid(*target))
	{
		auto& into = registry.Get<Pot>(*target);
		into.amount = static_cast<uint16_t>(std::min<uint32_t>(65535u, into.amount + amount));
		SinkPile(*target);
	}
	else
	{
		const auto pile = archetypes::PotArchetype::Create(position, 0.0f, wood ? PotInfo::MagicWood : PotInfo::MagicFood, amount);
		if (pile != entt::null)
		{
			SinkPile(pile);
		}
	}
	using audio::SoundId;
	static constexpr auto k_FoodSmall = std::array<SoundId, 6> {SoundId::G_PileFoodSmall_01, SoundId::G_PileFoodSmall_02,
	                                                            SoundId::G_PileFoodSmall_03, SoundId::G_PileFoodSmall_04,
	                                                            SoundId::G_PileFoodSmall_05, SoundId::G_PileFoodSmall_06};
	static constexpr auto k_Food = std::array<SoundId, 2> {SoundId::G_PileFood_01, SoundId::G_PileFood_02};
	static constexpr auto k_WoodSmall = std::array<SoundId, 6> {SoundId::G_PileWoodSmall_01, SoundId::G_PileWoodSmall_02,
	                                                            SoundId::G_PileWoodSmall_03, SoundId::G_PileWoodSmall_04,
	                                                            SoundId::G_PileWoodSmall_05, SoundId::G_PileWoodSmall_06};
	static constexpr auto k_Wood = std::array<SoundId, 6> {SoundId::G_PileWood_01, SoundId::G_PileWood_02, SoundId::G_PileWood_03,
	                                                       SoundId::G_PileWood_04, SoundId::G_PileWood_05, SoundId::G_PileWood_06};
	if (wood)
	{
		PlaySample(amount < 200 ? Locator::rng::value().Choose(k_WoodSmall) : Locator::rng::value().Choose(k_Wood));
	}
	else
	{
		PlaySample(amount < 200 ? Locator::rng::value().Choose(k_FoodSmall) : Locator::rng::value().Choose(k_Food));
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand: put down {} {} ({})", amount, wood ? "wood" : "food",
	                   target ? "merged" : "new pile");
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
	// Tree::GetWoodValue = life (1 for a fresh tree) * woodValue * scale * GLandBalance::Values[5] (1 by default).
	const auto type = registry.AllOf<Tree>(object) ? registry.Get<Tree>(object).type : registry.Get<DeadTree>(object).type;
	const auto& info = Locator::infoConstants::value().tree.at(static_cast<size_t>(type));
	auto wood = static_cast<uint32_t>(static_cast<float>(info.woodValue) * registry.Get<Transform>(object).scale.x);
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
