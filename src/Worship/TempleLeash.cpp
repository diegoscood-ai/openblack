/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TempleLeash.h"

#include <span>
#include <utility>
#include <vector>

#include <entt/entity/registry.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/LandMorph.h"
#include "3D/MapCoords.h"
#include "Audio/Audio.h"
#include "Audio/Game/Banks.h"
#include "Creature/CreatureSpells.h"
#include "Creature/LocalPlayer.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/LeashPost.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "Input/GamePackets.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"
#include "Worship/Citadel.h"
#include "Worship/LeashPosts.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
/// The special points of the heart's mesh, none without a mesh in the cache (tests)
std::span<const glm::mat4> SpecialPointsOf(const ecs::Registry& registry, entt::entity heart)
{
	const auto* mesh = registry.TryGet<const Mesh>(heart);
	if (mesh == nullptr || !Locator::resources::has_value())
	{
		return {};
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(mesh->id))
	{
		return {};
	}
	return meshes.Handle(mesh->id)->GetExtraMetrics();
}

/// The temple's matrix: its rotation by its scale, and its position
glm::mat4 MatrixOf(const Transform& transform)
{
	glm::mat4 matrix(transform.rotation);
	matrix[0] *= transform.scale.x;
	matrix[1] *= transform.scale.y;
	matrix[2] *= transform.scale.z;
	matrix[3] = glm::vec4(transform.position, 1.0f);
	return matrix;
}

/// The land's height under a point, read as the land is read for a temple's points: x and z through the map's fixed
/// point first. 0 with no land
float GroundUnder(glm::vec2 xz)
{
	// (the placeholder island has no materials and no heights)
	if (!Locator::terrainSystem::has_value() || Locator::terrainSystem::value().GetMaterialInfo().empty())
	{
		return 0.0f;
	}
	const auto lookup = [](float m) { return map_coords::ToMetres(map_coords::MetresToFixedForHandLookup(m)); };
	// (inferred) the island's height at a point stands for the original's altitude lookup, as for the hand
	return Locator::terrainSystem::value().GetHeightAt(glm::vec2(lookup(xz.x), lookup(xz.y)));
}

/// Where post `post` of the heart stands: the temple's special point through its matrix, on the land
glm::vec3 PostPoint(const ecs::Registry& registry, entt::entity heart, size_t post)
{
	return worship::leash_posts::PointOnLand(MatrixOf(registry.Get<const Transform>(heart)), SpecialPointsOf(registry, heart),
	                                         post, GroundUnder);
}

/// entt's on_destroy<TempleLeash>: the heart goes (Registry::Destroy from ecs::ToBeDeleted or the dead list), and its
/// posts go with it at once. The component is still there during the signal
void OnTempleLeashDestroyed(entt::registry& registry, entt::entity heart)
{
	const auto posts = registry.get<const TempleLeash>(heart).posts;
	for (const auto post : posts)
	{
		if (post != entt::null && registry.valid(post))
		{
			registry.destroy(post);
		}
	}
}
} // namespace

void worship::temple_leash::CreatePosts(entt::entity heart, const Draw& draw)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	if (!lookup.Valid(heart) || !lookup.AllOf<CitadelHeart>(heart) || lookup.AllOf<TempleLeash>(heart))
	{
		return;
	}
	// connected once per registry (entt's sink::connect disconnects the same listener first: idempotent)
	registry.OnDestroy<TempleLeash>().connect<&OnTempleLeashDestroyed>();
	const auto owner = lookup.Get<const Temple>(heart).owner;
	TempleLeash leashes;
	for (size_t post = 0; post < leash_posts::k_Count; ++post)
	{
		// the point; the post object and its collar stand there unturned, at scale 1
		const auto point = PostPoint(lookup, heart, post);
		const auto entity = registry.Create();
		registry.Assign<Transform>(entity, point, glm::mat3(1.0f), glm::vec3(1.0f));
		// the four draws, one after the other: the collar's scroll, its two turns, the smoke's frame clock
		const float scroll = draw(leash_posts::k_Draws[0].low, leash_posts::k_Draws[0].high);
		const float xAngle = draw(leash_posts::k_Draws[1].low, leash_posts::k_Draws[1].high);
		const float zAngle = draw(leash_posts::k_Draws[2].low, leash_posts::k_Draws[2].high);
		const float frame = draw(leash_posts::k_Draws[3].low, leash_posts::k_Draws[3].high);
		registry.AssignState<LeashPost>(entity, LeashPost {.index = static_cast<uint8_t>(post),
		                                                   .owner = owner,
		                                                   .heart = heart,
		                                                   .spin = leash_posts::SeedSpin(scroll, xAngle, zAngle, frame)});
		leashes.posts.at(post) = entity;
	}
	// nothing picked yet
	leashes.pick = leash_posts::k_NoPick;
	registry.AssignState<TempleLeash>(heart, leashes);
}

int32_t worship::temple_leash::Pick(entt::entity heart)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* leashes = registry.Valid(heart) ? registry.TryGet<const TempleLeash>(heart) : nullptr;
	return leashes != nullptr ? leashes->pick : leash_posts::k_NoPick;
}

LeashType worship::temple_leash::PickedType(entt::entity heart)
{
	return leash_posts::PickedType(Pick(heart));
}

void worship::temple_leash::SetPick(entt::entity heart, LeashType type)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!std::as_const(registry).Valid(heart) || !std::as_const(registry).AllOf<TempleLeash>(heart))
	{
		return;
	}
	auto& leashes = registry.Get<TempleLeash>(heart);
	leashes.pick = leash_posts::PickAfterSet(leashes.pick, type);
}

std::optional<int32_t> worship::temple_leash::PickOf(PlayerNames player)
{
	const auto heart = citadel::HeartOf(citadel::Of(player));
	if (heart == entt::null)
	{
		return std::nullopt;
	}
	return Pick(heart);
}

std::vector<entt::entity> worship::temple_leash::ShownPosts(entt::entity heart, const ecs::systems::LeashSystemInterface& leash)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	if (!registry.Valid(heart) || !registry.AllOf<TempleLeash, CitadelHeart, Temple>(heart))
	{
		return {};
	}
	const auto& posts = registry.Get<const TempleLeash>(heart).posts;
	const float built = registry.Get<const CitadelHeart>(heart).drawPercent;
	const auto creature = leash.PlayersCreature(registry.Get<const Temple>(heart).owner);
	std::vector<entt::entity> shown;
	for (size_t post = 0; post < posts.size(); ++post)
	{
		const bool knows = creature.has_value() && leash.Knows(*creature, leash_posts::TypeOf(post));
		if (leash_posts::Shown(built, creature.has_value(), knows) && registry.Valid(posts.at(post)))
		{
			shown.push_back(posts.at(post));
		}
	}
	return shown;
}

std::optional<glm::vec3> worship::temple_leash::PointNow(entt::entity post)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* leashPost = registry.Valid(post) ? registry.TryGet<const LeashPost>(post) : nullptr;
	if (leashPost == nullptr || !registry.Valid(leashPost->heart) || !registry.AllOf<CitadelHeart, Transform>(leashPost->heart))
	{
		return std::nullopt;
	}
	return PostPoint(registry, leashPost->heart, leashPost->index);
}

std::vector<worship::temple_leash::HandPost> worship::temple_leash::HandPosts(const ecs::systems::LeashSystemInterface& leash,
                                                                              PlayerNames local, bool handHidden)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	std::vector<entt::entity> hearts;
	registry.Each<const TempleLeash>([&hearts](entt::entity heart, const TempleLeash&) { hearts.push_back(heart); });
	std::vector<HandPost> felt;
	for (const auto heart : hearts)
	{
		const bool localTemple = registry.AllOf<Temple>(heart) && registry.Get<const Temple>(heart).owner == local;
		const auto pick = Pick(heart);
		for (const auto post : ShownPosts(heart, leash))
		{
			// the local player's picked post hangs on the hand: no collision while the hand is hidden
			const auto index = static_cast<int32_t>(registry.Get<const LeashPost>(post).index);
			if (handHidden && localTemple && pick == index)
			{
				continue;
			}
			if (const auto point = PointNow(post); point.has_value())
			{
				felt.push_back({post, *point});
			}
		}
	}
	return felt;
}

bool worship::temple_leash::InterfaceValidToTap(entt::entity post)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* leashPost = registry.Valid(post) ? registry.TryGet<const LeashPost>(post) : nullptr;
	return leashPost != nullptr && leash_posts::ValidToTap(leashPost->owner, creature::LocalPlayer());
}

uint32_t worship::temple_leash::InterfaceTap(entt::entity post, bool myInterface)
{
	if (!InterfaceValidToTap(post))
	{
		return 1;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto& leashPost = std::as_const(registry).Get<const LeashPost>(post);
	auto* leashes = std::as_const(registry).Valid(leashPost.heart) ? registry.TryGet<TempleLeash>(leashPost.heart) : nullptr;
	if (leashes == nullptr)
	{
		return 1;
	}
	const auto outcome = leash_posts::Tap(leashes->pick, leashPost.index, myInterface);
	if (outcome.click)
	{
		// the in-game bank's click, two-dimensional, of no owner
		audio::PlayOptions options;
		options.sample = {audio::Bank(audio::SfxBank::InGame), leash_posts::k_TapSample};
		options.owner = audio::Owner::None();
		options.is3D = false;
		audio::PlaySoundEffect(options);
	}
	leashes->pick = outcome.pick;
	if (outcome.sent.has_value())
	{
		game_packets::Push({.type = game_packets::Type::LeashType,
		                    .value = static_cast<int32_t>(*outcome.sent),
		                    .player = creature::LocalPlayer()});
	}
	return 1;
}

void worship::temple_leash::ApplyLeashType(PlayerNames player, LeashType type, ecs::systems::LeashSystemInterface* leash)
{
	const auto& lookup = std::as_const(Locator::entitiesRegistry::value());
	auto pet = leash != nullptr ? leash->PlayersCreature(player) : std::nullopt;
	if (pet.has_value() && !lookup.Valid(*pet))
	{
		pet.reset();
	}
	if (pet.has_value())
	{
		const auto* spells = lookup.TryGet<const CreatureSpells>(*pet);
		const auto under = [spells](CreatureReceiveSpellType spellType) {
			const auto spell = creature_spells::SpellOf(spellType);
			return spells != nullptr && spell.has_value() && spells->spells.IsActive(*spell);
		};
		if (leash_posts::LeashRefused(true, under(CreatureReceiveSpellType::CreatureReceiveSpellCompassionate),
		                              under(CreatureReceiveSpellType::CreatureReceiveSpellAngry)))
		{
			return;
		}
		// the player's leash: ours is their creature's, which takes the leash's moods as it changes
		leash->ChangeType(*pet, type);
	}
	// the temple's pick, the same as the tap's
	SetPick(citadel::HeartOf(citadel::Of(player)), type);
}

void worship::temple_leash::RegisterPacketHandler()
{
	game_packets::SetHandler(game_packets::Type::LeashType, [](const game_packets::Packet& packet) {
		ApplyLeashType(packet.player, static_cast<LeashType>(packet.value),
		               Locator::leashSystem::has_value() ? &Locator::leashSystem::value() : nullptr);
	});
}

void worship::temple_leash::DisconnectDeletionListener()
{
	if (Locator::entitiesRegistry::has_value())
	{
		Locator::entitiesRegistry::value().OnDestroy<TempleLeash>().disconnect<&OnTempleLeashDestroyed>();
	}
}
