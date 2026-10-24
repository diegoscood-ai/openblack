/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureDraw.h"

#include <cmath>

#include <limits>

#include <glm/common.hpp>
#include <glm/matrix.hpp>

#include "Creature/CreatureFizzLook.h"
#include "Creature/CreatureHair.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureTurnPose.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureCopy.h"
#include "ECS/Components/CreatureHair.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/FallingCreature.h"
#include "ECS/Components/Mesh.h"
#include "Graphics/ArgbColour.h"

using namespace openblack;
using namespace openblack::graphics;
using namespace openblack::ecs::components;

namespace
{
/// An opaque colour, each channel 0 to 255, as 0xAARRGGBB
uint32_t OpaqueArgb(const glm::ivec3& colour)
{
	const auto byte = [](int channel) { return static_cast<uint32_t>(std::clamp(channel, 0, 255)); };
	return 0xFF000000u | (byte(colour.r) << 16u) | (byte(colour.g) << 8u) | byte(colour.b);
}

/// One strand's ribbon added to a strip, two triangles between each pair of its points. False when the strip has no
/// room left for it.
bool AddStrand(creature_draw::Strip& strip, std::span<const glm::vec3> points, const glm::vec3& eye, float halfWidth,
               uint32_t abgr)
{
	if (points.size() < 2)
	{
		return true;
	}
	const size_t first = strip.vertices.size();
	const size_t count = points.size() * 2;
	if (first + count > std::numeric_limits<uint16_t>::max())
	{
		return false;
	}
	std::vector<creature_hair::RibbonVertex> corners(count);
	creature_hair::BuildRibbon(points, eye, halfWidth, corners);
	for (const auto& corner : corners)
	{
		strip.vertices.push_back({.position = corner.position, .uv = corner.uv, .abgr = abgr});
	}
	for (size_t i = 0; i + 1 < points.size(); ++i)
	{
		const auto a = static_cast<uint16_t>(first + (i * 2));
		for (const auto offset : {0, 1, 2, 2, 1, 3})
		{
			strip.indices.push_back(static_cast<uint16_t>(a + offset));
		}
	}
	return true;
}
} // namespace

std::vector<creature_draw::Body> creature_draw::Bodies(const ecs::Registry& registry, const EntityInstances& instances,
                                                       const MeshBones& meshBones)
{
	using MeshComponent = ecs::components::Mesh; // not graphics::Mesh, which this namespace finds first
	std::vector<Body> bodies;
	const auto add = [&](entt::entity entity, CreatureType species, entt::entity paintedSkins) {
		const auto instance = instances.find(entity);
		if (instance == instances.end())
		{
			return;
		}
		const auto meshId = instance->second.meshId;
		const auto boneCount = meshBones(meshId);
		if (!boneCount.has_value())
		{
			return;
		}
		Body body {.entity = entity, .instance = instance->second.index, .mesh = meshId, .paintedSkins = paintedSkins};
		// The pose the body is drawn with (creature_turn_pose::BonesDrawn), if it fits the mesh; else the mesh's rest pose
		if (const auto* animation = registry.TryGet<const CreatureAnimation>(entity);
		    animation != nullptr && *boneCount > 0 && creature_turn_pose::BonesDrawn(*animation).size() == *boneCount)
		{
			body.bones = creature_turn_pose::BonesDrawn(*animation);
		}
		// The shape: towards the meshes of the species that are loaded, the base mesh standing in for the others
		if (const auto* morph = registry.TryGet<const CreatureMorph>(entity); morph != nullptr)
		{
			const auto meshes = creature_morph::MeshesOf(species, morph->drawn,
			                                             [&meshBones](entt::id_type id) { return meshBones(id).has_value(); });
			body.morph = MorphTargets {
			    .meshes = {meshes.evilGood, meshes.thinFat, meshes.weakStrong},
			    .weights = glm::abs(glm::vec3(morph->drawn.evilGood, morph->drawn.thinFat, morph->drawn.weakStrong)),
			};
		}
		// How the spells on it leave it drawn
		if (const auto* spells = registry.TryGet<const ecs::components::CreatureSpells>(entity); spells != nullptr)
		{
			body.fizz = spells->fizz;
			body.freeze = spells->freeze;
		}
		bodies.push_back(body);
	};
	registry.Each<const Creature, const MeshComponent>(
	    [&add](entt::entity entity, const Creature& creature, const MeshComponent& /*mesh*/) {
		    add(entity, creature.species, entity);
	    });
	// the Creature Cave's copy of a creature, drawn as its creature, with its skins
	registry.Each<const CreatureCopy>(
	    [&add](entt::entity entity, const CreatureCopy& copy) { add(entity, copy.species, copy.source); });
	std::ranges::sort(bodies, {}, &Body::instance);
	return bodies;
}

std::optional<creature_draw::Body> creature_draw::FallingBody(const ecs::Registry& registry, const MeshBones& meshBones)
{
	std::optional<Body> found;
	registry.Each<const FallingCreature>([&](entt::entity entity, const FallingCreature& copy) {
		const auto* morph = registry.TryGet<const CreatureMorph>(entity);
		const auto drawn = morph != nullptr ? morph->drawn : creature_morph::Morph {};
		const auto meshes =
		    creature_morph::MeshesOf(copy.species, drawn, [&meshBones](entt::id_type id) { return meshBones(id).has_value(); });
		const auto boneCount = meshBones(meshes.base);
		if (!boneCount.has_value())
		{
			return;
		}
		Body body {.entity = entity, .instance = 0, .mesh = meshes.base, .paintedSkins = copy.source};
		// its pose, if it fits the mesh; else the mesh's rest pose
		if (const auto* animation = registry.TryGet<const CreatureAnimation>(entity);
		    animation != nullptr && *boneCount > 0 && creature_turn_pose::BonesDrawn(*animation).size() == *boneCount)
		{
			body.bones = creature_turn_pose::BonesDrawn(*animation);
		}
		if (morph != nullptr)
		{
			body.morph = MorphTargets {
			    .meshes = {meshes.evilGood, meshes.thinFat, meshes.weakStrong},
			    .weights = glm::abs(glm::vec3(drawn.evilGood, drawn.thinFat, drawn.weakStrong)),
			};
		}
		found = body;
	});
	return found;
}

creature_draw::InstanceRow creature_draw::FallingRow(const glm::mat4& model, uint32_t colour, uint32_t specular)
{
	InstanceRow row {model[0], model[1], model[2], model[3], glm::vec4(0.0f)};
	argb_colour::PackInstanceColour(row[4], colour);
	argb_colour::PackInstanceSpecular(row[4], specular);
	return row;
}

uint32_t creature_draw::SetColourLid(uint32_t bodyColour, uint32_t skin)
{
	uint32_t lid = 0xFF000000u;
	for (const uint32_t shift : {16u, 8u, 0u})
	{
		const uint32_t c = (bodyColour >> shift) & 0xFFu;
		const uint32_t s = (skin >> shift) & 0xFFu;
		lid |= ((c * s) / 255u) << shift;
	}
	return lid;
}

const creature_draw::Body* creature_draw::Find(std::span<const Body> bodies, uint32_t instance)
{
	const auto found = std::ranges::lower_bound(bodies, instance, {}, &Body::instance);
	return found != bodies.end() && found->instance == instance ? &*found : nullptr;
}

std::vector<creature_draw::EyePart> creature_draw::Eyes(const CreatureEyes& eyes, const HasMesh& hasMesh)
{
	std::vector<EyePart> parts;
	const bool eyeball = hasMesh(CreatureEyes::k_EyeballMeshId);
	const bool eyelid = hasMesh(CreatureEyes::k_EyelidMeshId);
	uint32_t lids = 0;
	for (const auto& eye : eyes.drawn)
	{
		if (eyeball && eye.eyeball.has_value())
		{
			parts.push_back({.mesh = CreatureEyes::k_EyeballMeshId, .model = *eye.eyeball, .tint = std::nullopt});
		}
		if (eyelid && eye.eyelid.has_value())
		{
			parts.push_back(
			    {.mesh = CreatureEyes::k_EyelidMeshId, .model = *eye.eyelid, .tint = eyes.lidColour, .lidTimes = ++lids});
		}
	}
	return parts;
}

uint32_t creature_draw::LidColour(uint32_t skin, uint32_t bodyColour, uint32_t times)
{
	for (uint32_t i = 0; i < times; ++i)
	{
		skin = argb_colour::MultiplyRgbOpaque(skin, bodyColour);
	}
	return skin;
}

creature_draw::InstanceRow creature_draw::EyeRow(const glm::mat4& bodyRow, const glm::vec4& bodyColours,
                                                 std::optional<uint32_t> lidColour, uint32_t specular)
{
	InstanceRow row {bodyRow[0], bodyRow[1], bodyRow[2], bodyRow[3], bodyColours};
	if (lidColour.has_value())
	{
		argb_colour::PackInstanceColour(row[4], *lidColour);
		argb_colour::PackInstanceSpecular(row[4], specular);
	}
	return row;
}

glm::mat4 creature_draw::InBodySpace(const glm::mat4& bodyRow, const glm::mat4& world)
{
	// In double, so that the eye keeps its millimetres a few kilometres from the land's origin
	const glm::dmat4 row(glm::dvec4(glm::dvec3(bodyRow[0]), 0.0), glm::dvec4(glm::dvec3(bodyRow[1]), 0.0),
	                     glm::dvec4(glm::dvec3(bodyRow[2]), 0.0), glm::dvec4(glm::dvec3(bodyRow[3]), 1.0));
	return glm::mat4(glm::inverse(row) * glm::dmat4(world));
}

std::array<creature_draw::FizzPass, 2> creature_draw::FizzPasses(const PassState& own, uint8_t level, glm::vec2 scroll,
                                                                 render_modes::Cull backFaces)
{
	// The depth: only Z, where the static over the skin reaches the level
	FizzPass depth {.state = own};
	depth.state.mode = render_modes::Mode::ChromaDepthOnly;
	depth.state.options.cull = backFaces;
	depth.spellLook = {scroll, 1.0f, static_cast<float>(creature_fizz_look::StaticThreshold(level)) / 255.0f};
	// The body: its own modes through the global alpha table, at the alpha byte the level leaves, over that depth alone
	FizzPass body {.state = own};
	body.state.options.zFunc = render_modes::ZFunc::Equal;
	body.state.table = render_modes::Table::GlobalAlpha;
	body.state.globalAlpha = static_cast<uint8_t>(std::numeric_limits<uint8_t>::max() - level);
	body.spellLook = {0.0f, 0.0f, 2.0f, creature_fizz_look::BodyAlpha(level)};
	return {depth, body};
}

creature_draw::Hair creature_draw::BuildHair(const CreatureHair& hair, const glm::vec3& eye, const glm::ivec3& light,
                                             const glm::ivec3& added)
{
	Hair out;
	for (const auto& group : hair.groups)
	{
		if (!group.shown)
		{
			continue;
		}
		const auto abgr = world_triangles::ToAbgr(OpaqueArgb(creature_hair::StrandColour(group.colour, light, added)));
		auto& strip = group.textured ? out.textured : out.plain;
		for (const auto& strand : group.strands)
		{
			if (!AddStrand(strip, strand.positions, eye, group.halfWidth, abgr))
			{
				break;
			}
		}
	}
	return out;
}
