/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PlayerCreature.h"

#include <cmath>

#include <algorithm>
#include <exception>
#include <numbers>
#include <string>
#include <utility>
#include <vector>

#include <MindFile.h>
#include <PhysiqueFile.h>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "Creature/CreatureMind.h"
#include "Creature/CreatureMindFileBody.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureSize.h"
#include "Creature/CreatureSpells.h"
#include "Creature/LocalPlayer.h"
#include "Debug/StateHash.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureAutoscale.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureDrawPose.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureFriends.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureSizeLimits.h"
#include "ECS/Components/CreatureSkin.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Resources/CreatureFileWriter.h"
#include "Resources/Loaders.h"
#include "Resources/ResourcesInterface.h"
#include "ScriptHeaders/ScriptEnums.h"

namespace openblack::ecs::player_creature
{

std::optional<std::filesystem::path> ProfileMindPath(std::string_view file, const std::filesystem::path& mindFolder)
{
	if (file.empty())
	{
		return std::nullopt;
	}
	return mindFolder / file;
}

bool ProfileHasCreature()
{
	const auto& fileSystem = Locator::filesystem::value();
	const auto path =
	    ProfileMindPath(Locator::config::value().profileCreatureFile, fileSystem.GetPath<filesystem::Path::CreatureMind>(true));
	return path.has_value() && fileSystem.Exists(*path);
}

namespace
{
/// What the two DEV_FUNCTION values the land scripts use on the player's creature are called
enum class DevFunctionId : int32_t
{
	RopeLeashEnabled = 2,
	OtherLeashesEnabled = 3,
};

bool IsCreature(const Registry& registry, entt::entity thing)
{
	return thing != entt::null && registry.Valid(thing) && registry.AllOf<components::Creature>(thing);
}

/// The plan for a creature of a species, with the body the file describes, at a point on the ground
LoadPlan PlanOf(const creature_mind_body::Body& body, CreatureType species, glm::vec2 pointXZ)
{
	return LoadPlan {
	    .species = species,
	    .size = body.size,
	    .alignment = body.alignment,
	    .strength = body.strength,
	    .position = {pointXZ.x, 0.0f, pointXZ.y},
	    .fatness = body.fatness,
	    .previousFatness = body.previousFatness,
	    .needs = body.needs,
	};
}

/// The planned creature, made for its owner with the mind the cache keeps, on the ground at the plan's point
entt::entity MakeFromMind(const LoadPlan& plan, PlayerNames owner, entt::id_type mindId)
{
	using archetypes::CreatureArchetype;
	auto body = CreatureArchetype::StartBody(plan.species);
	body.strength = plan.strength;
	if (plan.alignment.has_value())
	{
		body.alignment = *plan.alignment;
	}
	auto position = plan.position;
	position.y = Locator::terrainSystem::value().GetHeightAt(glm::vec2(position.x, position.z));
	// (inferred) the creature faces the way the script creatures do
	const float yAngle = std::numbers::pi_v<float>;
	const auto creature = CreatureArchetype::Create(position, owner, plan.species, mindId, yAngle,
	                                                plan.size.value_or(CreatureArchetype::StartScale(plan.species)), body);
	auto* physiology = Locator::creaturePhysiologySystem::has_value() ? &Locator::creaturePhysiologySystem::value() : nullptr;
	RestoreBody(Locator::entitiesRegistry::value(), physiology, creature, plan);
	return creature;
}

/// The mind file through the mind cache: its id there and what the cache keeps of it, none when it was not read
std::pair<entt::id_type, const creature::CreatureMind*> LoadMind(const std::string& file, const std::filesystem::path& path)
{
	auto& minds = Locator::resources::value().GetCreatureMinds();
	const auto loaded = minds.Load(file, resources::CreatureMindLoader::FromDiskTag {}, path);
	const auto mindId = loaded.first->first;
	const auto handle = minds.Handle(mindId);
	return {mindId, handle ? &*handle : nullptr};
}
} // namespace

std::optional<LoadPlan> PlanLoad(bool playerHasCreature, const creature::CreatureMind* mind, glm::vec2 pointXZ)
{
	if (playerHasCreature || mind == nullptr || !mind->Loaded())
	{
		return std::nullopt;
	}
	const auto body = creature_mind_body::FromMindFile(mind->data);
	if (!body.has_value())
	{
		return std::nullopt;
	}
	// the middle of the point's cell, as the original's map coordinates of a cell
	const auto centre = [](float metres) {
		return map_coords::ToMetres(map_coords::CellOf(map_coords::ToFixed(metres)) * map_coords::k_FixedPerCell +
		                            map_coords::k_FixedPerCell / 2);
	};
	return PlanOf(*body, body->species, glm::vec2(centre(pointXZ.x), centre(pointXZ.y)));
}

entt::entity LoadMyCreature(glm::vec2 pointXZ)
{
	const auto player = PlayerNames::PLAYER_ONE;
	const bool hasCreature =
	    Locator::leashSystem::has_value() && Locator::leashSystem::value().PlayersCreature(player).has_value();
	const auto& file = Locator::config::value().profileCreatureFile;
	auto& fileSystem = Locator::filesystem::value();
	const auto path = ProfileMindPath(file, fileSystem.GetPath<filesystem::Path::CreatureMind>(true));
	if (hasCreature || !path.has_value())
	{
		return entt::null;
	}
	const auto [mindId, mind] = LoadMind(file, *path);
	const auto plan = PlanLoad(false, mind, pointXZ);
	if (!plan.has_value())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "LOAD_MY_CREATURE: no creature in {}", path->generic_string());
		return entt::null;
	}
	return MakeFromMind(*plan, player, mindId);
}

void RestoreBody(Registry& registry, systems::CreaturePhysiologySystemInterface* physiology, entt::entity creature,
                 const LoadPlan& plan)
{
	if (!IsCreature(std::as_const(registry), creature))
	{
		return;
	}
	auto& self = registry.Get<components::Creature>(creature);
	self.fatness = plan.fatness;
	if (auto* morph = registry.TryGet<components::CreatureMorph>(creature))
	{
		morph->shownFatness = creature_mind_body::LoadedShownFatness(plan.previousFatness, plan.fatness);
		morph->drawn = creature_morph::FromAttributes(self.alignment, morph->shownFatness, self.strength,
		                                              archetypes::CreatureArchetype::SpeciesStrength(self.species));
	}
	if (physiology != nullptr)
	{
		if (const auto needs = physiology->NeedsOf(creature))
		{
			physiology->SetNeeds(creature, creature_mind_body::WithSavedNeeds(*needs, plan.needs));
		}
	}
}

std::optional<LoadPlan> PlanScriptLoad(int32_t type, const creature::CreatureMind* mind, glm::vec2 pointXZ)
{
	if (mind == nullptr || !mind->Loaded() || type < 0)
	{
		return std::nullopt;
	}
	// the script's type is a row of the game's creature tables
	const auto species = creature_mind_body::SpeciesFromRow(static_cast<uint32_t>(type));
	if (!species.has_value())
	{
		return std::nullopt;
	}
	// x and z as the fixed point keeps them
	return PlanOf(creature_mind_body::FromMindFile(mind->data, *species), *species,
	              glm::vec2(map_coords::Quantise(pointXZ.x), map_coords::Quantise(pointXZ.y)));
}

void SettleScriptLoaded(systems::LeashSystemInterface& leash, Registry& registry, entt::entity creature)
{
	leash.SetLeashable(creature, true);
	leash.SetKnown(creature, LeashType::Rope, true);
	leash.SetKnown(creature, LeashType::Evil, true);
	leash.SetKnown(creature, LeashType::Good, true);
	SetDevelopmentStage(registry, creature, k_LastDevelopmentStage);
}

entt::entity ScriptLoadCreature(int32_t type, std::string_view file, PlayerNames player, glm::vec2 pointXZ)
{
	if (Locator::leashSystem::has_value() && Locator::leashSystem::value().PlayersCreature(player).has_value())
	{
		// the original says so and loads the new one all the same
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "LOAD_CREATURE: Player has already his creature loaded, ");
	}
	// the folder joined with the file's name, as for the profile's; no name is no file
	const auto path = ProfileMindPath(file, Locator::filesystem::value().GetPath<filesystem::Path::CreatureMind>(true));
	if (!path.has_value())
	{
		return entt::null;
	}
	const auto [mindId, mind] = LoadMind(std::string(file), *path);
	const auto plan = PlanScriptLoad(type, mind, pointXZ);
	if (!plan.has_value())
	{
		// openblack's own line: the original says nothing here
		SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "LOAD_CREATURE: no creature of type {} in {}", type,
		                   path->generic_string());
		return entt::null;
	}
	const auto creature = MakeFromMind(*plan, player, mindId);
	if (Locator::leashSystem::has_value())
	{
		SettleScriptLoaded(Locator::leashSystem::value(), Locator::entitiesRegistry::value(), creature);
	}
	return creature;
}

std::optional<creaturemind::MindFileData> ToMindFile(const Registry& registry,
                                                     const systems::CreatureMindSystemInterface& minds,
                                                     const systems::CreaturePhysiologySystemInterface* physiology,
                                                     const systems::LeashSystemInterface* leash, entt::entity creature)
{
	if (!IsCreature(registry, creature))
	{
		return std::nullopt;
	}
	auto mind = minds.SaveMind(creature);
	if (!mind.has_value())
	{
		return std::nullopt;
	}
	const auto& self = registry.Get<const components::Creature>(creature);
	const auto* morph = registry.TryGet<const components::CreatureMorph>(creature);
	const auto* tattoos = registry.TryGet<const components::CreatureTattoos>(creature);
	creature_mind_body::LiveBody body {
	    .fatness = self.fatness,
	    .shownFatness = morph != nullptr ? morph->shownFatness : self.fatness,
	    .needs = physiology != nullptr ? physiology->NeedsOf(creature) : std::nullopt,
	    .fightHealth = GetCreatureProperty(registry, physiology, creature, script::ObjectPropertyType::CreatureFightHealth)
	                       .value_or(components::CreatureFightHealth {}.health),
	    .inDevScript = self.inDevScript,
	};
	if (leash != nullptr)
	{
		body.leashes = creature_mind_body::Leashes {
		    .evil = leash->Knows(creature, LeashType::Evil),
		    .rope = leash->Knows(creature, LeashType::Rope),
		    .good = leash->Knows(creature, LeashType::Good),
		};
	}
	if (tattoos != nullptr)
	{
		body.tattoos = tattoos->slots;
	}
	return creature_mind_body::ToMindFile(std::move(*mind), body);
}

bool SaveMyCreature()
{
	if (!Locator::leashSystem::has_value() || !Locator::creatureMindSystem::has_value() ||
	    !Locator::entitiesRegistry::has_value())
	{
		return false;
	}
	const auto mine = Locator::leashSystem::value().PlayersCreature(PlayerNames::PLAYER_ONE);
	const auto& file = Locator::config::value().profileCreatureFile;
	auto& fileSystem = Locator::filesystem::value();
	const auto mindFolder = fileSystem.GetPath<filesystem::Path::CreatureMind>(true);
	const auto path = ProfileMindPath(file, mindFolder);
	if (!mine.has_value() || !path.has_value())
	{
		return false;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* physiology =
	    Locator::creaturePhysiologySystem::has_value() ? &Locator::creaturePhysiologySystem::value() : nullptr;
	const auto mind =
	    ToMindFile(registry, Locator::creatureMindSystem::value(), physiology, &Locator::leashSystem::value(), *mine);
	if (!mind.has_value())
	{
		return false;
	}
	auto& minds = Locator::resources::value().GetCreatureMinds();
	if (!resources::SaveCreatureMind(fileSystem, minds, file, *path, *mind))
	{
		return false;
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The creature is saved to {}", path->generic_string());
	// the physique beside it, the skin's lists kept from the file it replaces
	const auto physiquePath = mindFolder / ("Physique" + file);
	std::optional<creaturemind::PhysiqueFileData> previous;
	if (fileSystem.Exists(physiquePath))
	{
		try
		{
			previous = creaturemind::ReadPhysique(fileSystem.ReadAll(physiquePath));
		}
		catch (const std::exception&)
		{
			previous.reset();
		}
	}
	const auto& body = registry.Get<const components::Creature>(*mine);
	const creature_mind_body::BodyNow now {
	    .speciesRow = static_cast<uint32_t>(creature::InfoRow(body.species)),
	    // the size it is drawn at, as the pen shrink takes it this turn (smaller in its temple's pen)
	    .size = PenSizeOf(registry, *mine).value_or(body.size),
	    .strength = body.strength,
	    .fatness = body.fatness,
	    .alignment = mind->alignment.value_or(body.alignment),
	};
	resources::SaveCreaturePhysique(fileSystem, physiquePath, creature_mind_body::ToPhysique(now, previous));
	return true;
}

void RegisterStateHash()
{
	state_hash::Register("creature", [](state_hash::Hasher& h) {
		if (Locator::entitiesRegistry::has_value())
		{
			HashCreatures(h, std::as_const(Locator::entitiesRegistry::value()));
		}
	});
}

void HashCreatures(state_hash::Hasher& h, const Registry& registry)
{
	registry.Each<const components::Creature>([&h, &registry](entt::entity entity, const components::Creature& creature) {
		h.U32(static_cast<uint32_t>(entity));
		h.U32(static_cast<uint32_t>(creature.owner));
		h.U32(static_cast<uint32_t>(creature.species));
		h.U32(creature.leashable ? 1u : 0u);
		h.U32(creature.inDevScript ? 1u : 0u);
		h.Float(creature.alignment);
		h.Float(creature.fatness);
		h.Float(creature.strength);
		h.Float(creature.size);
		const auto* mind = registry.TryGet<const components::CreatureMindState>(entity);
		h.U32(mind != nullptr ? mind->developmentPhase : 0xFFFFFFFFu);
		const auto* leash = registry.TryGet<const components::CreatureLeash>(entity);
		h.U32(leash != nullptr && leash->home.has_value() ? 1u : 0u);
		h.Vec3(leash != nullptr ? leash->home.value_or(glm::vec3(0.0f)) : glm::vec3(0.0f));
	});
}

std::optional<entt::entity> PlayersCreature(const systems::LeashSystemInterface& leash, PlayerNames player)
{
	return leash.PlayersCreature(player);
}

glm::vec3 HomeOnGround(glm::vec3 point, float groundHeight)
{
	return {map_coords::ToMetres(map_coords::ToFixed(point.x)), groundHeight,
	        map_coords::ToMetres(map_coords::ToFixed(point.z))};
}

void SetHome(systems::LeashSystemInterface& leash, const Registry& registry, entt::entity thing, glm::vec3 home)
{
	if (IsCreature(registry, thing))
	{
		leash.SetHome(thing, home);
	}
}

std::optional<glm::vec3> TemplePenPoint(const components::Transform& temple, std::span<const glm::mat4> specialPoints)
{
	if (specialPoints.size() <= k_TemplePenPoint)
	{
		return std::nullopt;
	}
	const auto local = glm::vec3(specialPoints[k_TemplePenPoint][3]);
	return temple.position + (temple.rotation * (local * temple.scale));
}

void FollowTemplePens(systems::LeashSystemInterface& leash, const Registry& registry,
                      const std::function<std::optional<glm::vec3>(PlayerNames)>& penOf,
                      const std::function<float(glm::vec2)>& groundAt)
{
	std::vector<std::pair<entt::entity, PlayerNames>> creatures;
	registry.Each<const components::Creature>([&creatures](entt::entity entity, const components::Creature& creature) {
		creatures.emplace_back(entity, creature.owner);
	});
	for (const auto& [entity, owner] : creatures)
	{
		if (const auto pen = penOf(owner); pen.has_value())
		{
			auto home = HomeOnGround(*pen, 0.0f);
			home.y = groundAt(glm::vec2(home.x, home.z));
			leash.SetHome(entity, home);
		}
	}
}

void FollowTemplePens()
{
	if (!Locator::leashSystem::has_value() || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto penOf = [&registry](PlayerNames owner) {
		std::optional<glm::vec3> pen;
		registry.Each<const components::Temple, const components::Transform>(
		    [&registry, &pen, owner](entt::entity entity, const components::Temple& temple,
		                             const components::Transform& transform) {
			    if (pen.has_value() || temple.owner != owner)
			    {
				    return;
			    }
			    // a temple being built or rebuilt keeps no creature yet
			    if (const auto* build = registry.TryGet<const components::CitadelPartBuild>(entity);
			        build != nullptr && (build->buildFlags & components::CitadelPartBuild::k_Built) == 0)
			    {
				    return;
			    }
			    const auto* mesh = registry.TryGet<const components::Mesh>(entity);
			    if (mesh == nullptr || !Locator::resources::has_value())
			    {
				    return;
			    }
			    const auto& meshes = Locator::resources::value().GetMeshes();
			    if (meshes.Contains(mesh->id))
			    {
				    pen = TemplePenPoint(transform, meshes.Handle(mesh->id)->GetExtraMetrics());
			    }
		    });
		return pen;
	};
	const auto groundAt = [](glm::vec2 point) {
		return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(point) : 0.0f;
	};
	FollowTemplePens(Locator::leashSystem::value(), registry, penOf, groundAt);
}

bool BetweenPenWalls(glm::vec2 temple, float templeYAngle, glm::vec2 point)
{
	const float first = templeYAngle + k_PenWallAngle;
	const float second = first + k_PenWallsApart;
	const float dx = point.x - temple.x;
	const float dz = point.y - temple.y;
	return std::cos(first) * dz - std::sin(first) * dx >= 0.0f && std::sin(second) * dx - std::cos(second) * dz >= 0.0f;
}

float PenDrawnSize(float size, float distanceToPen, bool betweenWalls)
{
	if (!betweenWalls || !(distanceToPen <= k_PenOuterRadius))
	{
		return size;
	}
	const float t = std::clamp(distanceToPen, k_PenInnerRadius, k_PenOuterRadius);
	return k_PenDrawnSize + (size - k_PenDrawnSize) * (t - k_PenInnerRadius) / (k_PenOuterRadius - k_PenInnerRadius);
}

std::optional<float> PenSizeOf(const Registry& registry, entt::entity entity)
{
	const auto& creature = registry.Get<const components::Creature>(entity);
	const auto* transform = registry.TryGet<const components::Transform>(entity);
	const auto* leash = registry.TryGet<const components::CreatureLeash>(entity);
	std::optional<float> drawn;
	if (transform == nullptr || leash == nullptr || !leash->home.has_value())
	{
		return drawn;
	}
	registry.Each<const components::Temple, const components::Transform, const components::CitadelWorship>(
	    [&](entt::entity temple, const components::Temple& owner, const components::Transform& place,
	        const components::CitadelWorship& worship) {
		    if (drawn.has_value() || owner.owner != creature.owner)
		    {
			    return;
		    }
		    if (const auto* build = registry.TryGet<const components::CitadelPartBuild>(temple);
		        build != nullptr && (build->buildFlags & components::CitadelPartBuild::k_Built) == 0)
		    {
			    return;
		    }
		    const auto at = glm::vec2(transform->position.x, transform->position.z);
		    const float size =
		        PenDrawnSize(creature.size, gutils::GetDistanceInMetres(transform->position, *leash->home),
		                     BetweenPenWalls(glm::vec2(place.position.x, place.position.z), worship.heartYAngle, at));
		    if (size != creature.size)
		    {
			    drawn = size;
		    }
	    });
	return drawn;
}

void ShrinkInPens()
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	std::vector<entt::entity> creatures;
	lookup.Each<const components::Creature>(
	    [&creatures](entt::entity entity, const components::Creature&) { creatures.push_back(entity); });
	for (const auto entity : creatures)
	{
		if (lookup.TryGet<const components::Transform>(entity) == nullptr ||
		    lookup.TryGet<const components::CreatureDrawPose>(entity) == nullptr)
		{
			continue;
		}
		const auto& creature = lookup.Get<const components::Creature>(entity);
		std::optional<glm::vec3> drawn;
		const auto size = PenSizeOf(lookup, entity);
		if (size.has_value())
		{
			drawn = glm::vec3(archetypes::CreatureArchetype::DrawnScale(creature.species, *size));
		}
		auto& pose = registry.Get<components::CreatureDrawPose>(entity);
		pose.scale = drawn;
		pose.size = size;
	}
}

bool SetAutoscale(Registry& registry, entt::entity thing, bool enabled, float factor)
{
	if (!IsCreature(std::as_const(registry), thing))
	{
		return false;
	}
	registry.AssignOrReplaceState<components::CreatureAutoscale>(
	    thing, components::CreatureAutoscale {.enabled = enabled, .factor = factor});
	return true;
}

void Autoscale(Registry& registry, std::optional<entt::entity> localCreature, std::optional<entt::entity> held)
{
	const auto& lookup = std::as_const(registry);
	if (!localCreature.has_value() || !IsCreature(lookup, *localCreature))
	{
		return;
	}
	std::vector<entt::entity> creatures;
	lookup.Each<const components::Creature, const components::CreatureAutoscale>(
	    [&creatures, held](entt::entity entity, const components::Creature&, const components::CreatureAutoscale& autoscale) {
		    if (autoscale.enabled && held != entity)
		    {
			    creatures.push_back(entity);
		    }
	    });
	for (const auto entity : creatures)
	{
		// the local creature's size is read for each step, so a creature following its own size sees the last step
		const auto& local = lookup.Get<const components::Creature>(*localCreature);
		const auto* localSpells = lookup.TryGet<const components::CreatureSpells>(*localCreature);
		const float localBase =
		    localSpells != nullptr ? creature_spells::SizeBeforeSpells(localSpells->spells, local.size) : local.size;
		auto& creature = registry.Get<components::Creature>(entity);
		const float size = creature_size::AutoscaleStep(creature.size, localBase,
		                                                lookup.Get<const components::CreatureAutoscale>(entity).factor);
		if (lookup.AllOf<components::CreatureSpells>(entity))
		{
			creature_spells::SetSizeBeforeSpells(registry.Get<components::CreatureSpells>(entity).spells, creature.size, size);
		}
		else
		{
			creature.size = size;
		}
	}
}

void Autoscale()
{
	if (!Locator::leashSystem::has_value() || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	// the creature the hand holds stands in for the one whose turn skips the step (approximate)
	std::optional<entt::entity> held;
	if (Locator::creatureHandSystem::has_value())
	{
		held = Locator::creatureHandSystem::value().GetCreature();
	}
	Autoscale(Locator::entitiesRegistry::value(), PlayersCreature(Locator::leashSystem::value(), creature::LocalPlayer()),
	          held);
}

bool AddFriend(std::vector<entt::entity>& friends, entt::entity other)
{
	if (std::ranges::find(friends, other) != friends.end())
	{
		return false;
	}
	friends.insert(friends.begin(), other);
	return true;
}

bool ForceFriends(Registry& registry, entt::entity first, entt::entity second, bool enable)
{
	if (!IsCreature(std::as_const(registry), first) || !IsCreature(std::as_const(registry), second))
	{
		return false;
	}
	if (!enable)
	{
		return true;
	}
	// one after the other, so a creature made its own friend is in its list once
	for (const auto& [to, other] : {std::pair {first, second}, std::pair {second, first}})
	{
		auto* friends = registry.TryGet<components::CreatureFriends>(to);
		if (friends == nullptr)
		{
			friends = &registry.AssignState<components::CreatureFriends>(to);
		}
		AddFriend(friends->friends, other);
	}
	return true;
}

bool IsCreatureProperty(script::ObjectPropertyType property)
{
	using enum script::ObjectPropertyType;
	return property == Strength || property == Alignment || (property >= CreatureWarmth && property <= CreatureMaxSize);
}

float ClampProperty(float value, float lowest, float highest)
{
	if (!(value >= lowest))
	{
		return lowest;
	}
	return value > highest ? highest : value;
}

std::optional<float> GetCreatureProperty(const Registry& registry, const systems::CreaturePhysiologySystemInterface* physiology,
                                         entt::entity thing, script::ObjectPropertyType property)
{
	if (!IsCreature(registry, thing) || !IsCreatureProperty(property))
	{
		return std::nullopt;
	}
	using enum script::ObjectPropertyType;
	const auto& creature = registry.Get<const components::Creature>(thing);
	switch (property)
	{
	case Strength:
		return creature.strength;
	case Alignment:
		return creature.alignment;
	case CreatureFatness:
		return creature.fatness;
	case CreatureFightHealth:
		if (const auto* fighting = registry.TryGet<const components::CreatureFighting>(thing))
		{
			return fighting->fighter.health;
		}
		if (const auto* kept = registry.TryGet<const components::CreatureFightHealth>(thing))
		{
			return kept->health;
		}
		return components::CreatureFightHealth {}.health;
	case CreatureMinSize:
	case CreatureMaxSize:
	{
		// the sizes the small and big spells take it to
		const auto* own = registry.TryGet<const components::CreatureSizeLimits>(thing);
		const auto limits = own != nullptr ? own->limits : creature_spells::SizeLimits {};
		return property == CreatureMinSize ? limits.smallest : limits.largest;
	}
	default:
		break;
	}
	const auto needs = physiology != nullptr ? physiology->NeedsOf(thing) : std::nullopt;
	if (!needs.has_value())
	{
		return std::nullopt;
	}
	switch (property)
	{
	case CreatureWarmth:
		return needs->warmth;
	case CreatureEnergy:
		return needs->energy;
	case CreatureItchiness:
		return needs->itchiness;
	case CreatureAmountOfPoo:
		return needs->poo;
	case CreatureExhaustion:
		return needs->exhaustion;
	case CreatureDehydration:
		return needs->dehydration;
	default:
		return std::nullopt;
	}
}

bool SetCreatureProperty(Registry& registry, systems::CreaturePhysiologySystemInterface* physiology, entt::entity thing,
                         script::ObjectPropertyType property, float value)
{
	if (!IsCreature(std::as_const(registry), thing) || !IsCreatureProperty(property))
	{
		return false;
	}
	using enum script::ObjectPropertyType;
	auto& creature = registry.Get<components::Creature>(thing);
	switch (property)
	{
	case Strength:
		creature.strength = value;
		return true;
	case Alignment:
		creature.alignment = value;
		return true;
	case CreatureFatness:
		creature.fatness = value;
		return true;
	case CreatureFightHealth:
		if (auto* fighting = registry.TryGet<components::CreatureFighting>(thing))
		{
			fighting->fighter.health = value;
			return true;
		}
		registry.AssignOrReplaceState<components::CreatureFightHealth>(thing,
		                                                               components::CreatureFightHealth {.health = value});
		return true;
	case CreatureMinSize:
	case CreatureMaxSize:
	{
		auto* own = registry.TryGet<components::CreatureSizeLimits>(thing);
		auto& limits = (own != nullptr ? *own : registry.AssignState<components::CreatureSizeLimits>(thing)).limits;
		(property == CreatureMinSize ? limits.smallest : limits.largest) = value;
		return true;
	}
	default:
		break;
	}
	auto needs = physiology != nullptr ? physiology->NeedsOf(thing) : std::nullopt;
	if (!needs.has_value())
	{
		return false;
	}
	switch (property)
	{
	case CreatureWarmth:
		needs->warmth = ClampProperty(value, -1.0f, 1.0f);
		break;
	case CreatureEnergy:
		needs->energy = ClampProperty(value, 0.0f, 1.0f);
		break;
	case CreatureItchiness:
		needs->itchiness = value;
		break;
	case CreatureAmountOfPoo:
		needs->poo = value;
		break;
	case CreatureExhaustion:
		needs->exhaustion = value;
		break;
	case CreatureDehydration:
		needs->dehydration = value;
		break;
	default:
		return false;
	}
	physiology->SetNeeds(thing, *needs);
	return true;
}

float SizeForHeight(float height)
{
	return height * k_SizePerHeight;
}

bool SetCreatureScale(Registry& registry, entt::entity thing, float scale)
{
	if (!IsCreature(std::as_const(registry), thing))
	{
		return false;
	}
	// the 3D body first, at the size kept within its limits, then the creature's own size as given
	auto& creature = registry.Get<components::Creature>(thing);
	if (auto* transform = registry.TryGet<components::Transform>(thing))
	{
		transform->scale =
		    glm::vec3(archetypes::CreatureArchetype::DrawnScale(creature.species, creature_morph::ClampScale(scale)));
	}
	creature.size = scale;
	return true;
}

bool SetCreatureHeight(Registry& registry, entt::entity thing, float height)
{
	if (!IsCreature(std::as_const(registry), thing))
	{
		return false;
	}
	// the size only: the drawn matrix waits for the body's next resize
	registry.Get<components::Creature>(thing).size = SizeForHeight(height);
	return true;
}

void SetDevelopmentStage(Registry& registry, entt::entity thing, int32_t stage)
{
	if (!IsCreature(std::as_const(registry), thing) || stage < 0 || stage > k_LastDevelopmentStage)
	{
		return;
	}
	if (auto* mind = registry.TryGet<components::CreatureMindState>(thing))
	{
		mind->developmentPhase = static_cast<uint32_t>(stage);
	}
}

bool SetName(Registry& registry, entt::entity thing, std::u16string name)
{
	if (!IsCreature(std::as_const(registry), thing))
	{
		return false;
	}
	if (auto* mind = registry.TryGet<components::CreatureMindState>(thing))
	{
		if (mind->learnt.has_value() && mind->pendingFile == nullptr)
		{
			mind->learnt->name = std::move(name);
			mind->scriptName.reset();
		}
		else
		{
			mind->scriptName = std::move(name);
		}
	}
	return true;
}

bool DevFunction(systems::LeashSystemInterface& leash, int32_t function, PlayerNames player)
{
	if (function != std::to_underlying(DevFunctionId::RopeLeashEnabled) &&
	    function != std::to_underlying(DevFunctionId::OtherLeashesEnabled))
	{
		return false;
	}
	const auto creature = leash.PlayersCreature(player);
	if (!creature.has_value())
	{
		return true;
	}
	if (function == std::to_underlying(DevFunctionId::RopeLeashEnabled))
	{
		leash.SetKnown(*creature, LeashType::Rope, true);
		return true;
	}
	leash.SetKnown(*creature, LeashType::Evil, true);
	leash.SetKnown(*creature, LeashType::Good, true);
	leash.SetLeashable(*creature, true);
	return true;
}

void SetInDevScript(Registry& registry, entt::entity thing, bool inDevScript)
{
	if (IsCreature(std::as_const(registry), thing))
	{
		registry.Get<components::Creature>(thing).inDevScript = inDevScript;
	}
}

} // namespace openblack::ecs::player_creature
