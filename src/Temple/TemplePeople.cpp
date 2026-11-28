/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TemplePeople.h"

#include <algorithm>
#include <array>
#include <optional>
#include <utility>

#include <entt/core/hashed_string.hpp>

#include "3D/L3DAnim.h"
#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/ObjectMatrix.h"
#include "Common/GameRandom.h"
#include "ECS/Animations.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "ECS/Villager/VillagerScript.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
/// The floors the worshippers walk, laid over the temple's rooms
constexpr auto k_WalkMeshId = entt::hashed_string("temple/interior/movement_l3d");
/// The game's seconds of a frame's milliseconds
constexpr float k_SecondsPerMillisecond = 0.001f;

/// The game's own random numbers, from this machine's seed
class GameDice final: public temple_people::Dice
{
public:
	explicit GameDice(GameRandomInterface& random)
	    : _random(random)
	{
	}
	uint32_t Below(int32_t n) override { return _random.LocalRand(n); }
	float UpTo(float x) override { return _random.LocalFloatRand(x); }

private:
	GameRandomInterface& _random;
};

std::optional<temple_people::WalkMesh> LoadWalkMesh()
{
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(k_WalkMeshId.value()))
	{
		return std::nullopt;
	}
	const auto mesh = meshes.Handle(k_WalkMeshId.value());
	if (mesh->GetSubMeshes().empty())
	{
		return std::nullopt;
	}
	// The triangles of the first primitive of the mesh's first submesh, as the file has its vertices
	const auto& subMesh = *mesh->GetSubMeshes().front();
	const auto& positions = subMesh.GetCollisionPositions();
	const auto& indices = subMesh.GetCollisionIndices();
	const auto& ranges = subMesh.GetCollisionRanges();
	if (ranges.empty())
	{
		return std::nullopt;
	}
	const auto [first, count] = ranges.front();
	std::vector<std::array<uint16_t, 3>> triangles;
	triangles.reserve(count / 3);
	for (size_t i = first; i + 2 < static_cast<size_t>(first) + count && i + 2 < indices.size(); i += 3)
	{
		triangles.push_back({indices[i], indices[i + 1], indices[i + 2]});
	}
	if (triangles.empty() || std::ranges::any_of(triangles, [&positions](const auto& triangle) {
		    return std::ranges::any_of(triangle, [&positions](uint16_t index) { return index >= positions.size(); });
	    }))
	{
		return std::nullopt;
	}
	return temple_people::BuildWalkMesh(positions, triangles);
}

/// The worshippers of each of the player's worship sites, by the site's place around the temple
std::vector<std::vector<entt::entity>> PlayersWorshippers(const ecs::Registry& registry, PlayerNames player)
{
	std::vector<std::vector<entt::entity>> bySite;
	bool found = false;
	registry.Each<const Temple, const CitadelWorship>([&](const Temple& temple, const CitadelWorship& worship) {
		if (temple.owner != player || found)
		{
			return;
		}
		found = true;
		for (const auto site : worship.sites)
		{
			if (const auto* worshipSite = site != entt::null ? registry.TryGet<const WorshipSite>(site) : nullptr)
			{
				bySite.push_back(worshipSite->villagers);
			}
		}
	});
	return bySite;
}
} // namespace

std::unique_ptr<TemplePeople> TemplePeople::Gather(PlayerNames player)
{
	if (!Locator::resources::has_value() || !Locator::gameRandom::has_value() || !Locator::entitiesRegistry::has_value())
	{
		return nullptr;
	}
	auto walkMesh = LoadWalkMesh();
	if (!walkMesh.has_value())
	{
		return nullptr;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	// A villager comes in while it is available: not being deleted, and not dying
	const auto available = [&registry](entt::entity villager) {
		return registry.Valid(villager) && registry.AllOf<Villager, Mesh, Transform>(villager) &&
		       ecs::villager::IsAvailable(villager);
	};
	const auto visitors = temple_people::ChooseVisitors(PlayersWorshippers(registry, player), available);

	auto people = std::make_unique<TemplePeople>();
	people->_mesh = std::move(*walkMesh);
	GameDice dice(Locator::gameRandom::value());
	for (const auto villager : visitors)
	{
		// Each comes in as the villager looks: its model, its size and its sex, and goes ahead of those in already
		people->_visitors.insert(people->_visitors.begin(),
		                         Visitor {
		                             .mesh = registry.Get<const Mesh>(villager).id,
		                             .scale = registry.Get<const Transform>(villager).scale.x,
		                             .man = registry.Get<const Villager>(villager).sex == Villager::Sex::MALE,
		                             .walker = temple_people::Arrive(people->_mesh, dice),
		                         });
	}
	return people;
}

void TemplePeople::Walk(uint32_t milliseconds)
{
	if (_visitors.empty())
	{
		return;
	}
	GameDice dice(Locator::gameRandom::value());
	const float seconds = static_cast<float>(milliseconds) * k_SecondsPerMillisecond;
	for (auto& visitor : _visitors)
	{
		temple_people::Step(visitor.walker, _mesh, seconds, dice);
	}
}

void TemplePeople::See(uint32_t milliseconds, std::span<const temple_people::Pass> draws, const glm::mat4& temple,
                       const glm::mat4& viewProjection, float nearClip, glm::vec2 resolution, bool shown)
{
	_seen.clear();
	if (_visitors.empty() || !shown)
	{
		return;
	}
	const auto& animations = Locator::resources::value().GetAnimations();
	const auto& meshes = Locator::resources::value().GetMeshes();
	for (auto& visitor : _visitors)
	{
		auto& walker = visitor.walker;
		const auto feet = glm::vec3(temple * glm::vec4(walker.position, 1.0f));
		if (!temple_people::Drawn(temple_people::FeetInClip(viewProjection, feet), nearClip, resolution) ||
		    !meshes.Contains(visitor.mesh))
		{
			continue;
		}
		// While seen, its clip plays on at each of the frame's draws of it and sets the speed it walks at
		const auto clip = temple_people::ClipOf(walker.gait, visitor.man);
		const auto clipId = ecs::ClipId(static_cast<uint32_t>(clip));
		const bool hasClip = animations.Contains(clipId);
		std::vector<uint32_t> roomClipPlaces;
		std::vector<uint32_t> reflectionClipPlaces;
		for (const auto pass : draws)
		{
			if (hasClip)
			{
				const auto animation = animations.Handle(clipId);
				walker.speed = temple_people::SpeedOf(animation->GetCycleDistance(), animation->GetPlayTime(), visitor.scale);
				walker.clipPlace = temple_people::AdvanceClipPlace(walker.clipPlace, milliseconds, animation->GetPlayTime(),
				                                                   animation->IsLooping());
			}
			(pass == temple_people::Pass::Reflection ? reflectionClipPlaces : roomClipPlaces).push_back(walker.clipPlace);
		}
		// Placed at its feet, turned to its facing and scaled to its size, as the game places an object
		const auto model = temple * affine::PlacementMatrix(walker.position, walker.facing, visitor.scale);
		_seen.push_back({.mesh = visitor.mesh,
		                 .model = model,
		                 .feet = feet,
		                 .clip = clip,
		                 .roomClipPlaces = std::move(roomClipPlaces),
		                 .reflectionClipPlaces = std::move(reflectionClipPlaces)});
	}
}
