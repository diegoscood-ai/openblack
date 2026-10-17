/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "ECS/Components/CreatureSkin.h"
#include "ECS/Registry.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "Graphics/RenderModes.h"
#include "Graphics/WorldTriangles.h"

namespace openblack::ecs::components
{
struct CreatureEyes;
struct CreatureHair;
} // namespace openblack::ecs::components

/// What the renderer needs to draw each creature: its body's mesh, pose and the meshes its shape is blended towards,
/// its eyes, its hair as ribbons, and its painted skins. Everything is read from the creature's components through the
/// const registry, so a land without a creature gains no storage and draws nothing.
namespace openblack::graphics::creature_draw
{

/// A mesh's bone count, or nothing while the mesh isn't loaded
using MeshBones = std::function<std::optional<size_t>(entt::id_type)>;
/// Whether a mesh is loaded
using HasMesh = std::function<bool(entt::id_type)>;
/// Where each drawn entity's row is in the frame's instances
using EntityInstances = std::unordered_map<entt::entity, ecs::systems::RenderContext::EntityInstance>;

/// The meshes a body is blended towards, evil or good, thin or fat and weak or strong, and how far towards each
struct MorphTargets
{
	std::array<entt::id_type, 3> meshes {};
	glm::vec3 weights {0.0f};
};

/// One creature's body this frame
struct Body
{
	entt::entity entity {entt::null};
	/// Its row in the frame's instances
	uint32_t instance {0};
	entt::id_type mesh {0};
	/// The pose it is drawn in, the mesh's own rest pose when empty
	std::span<const glm::mat4> bones;
	/// The shape it is blended to, for a creature whose body follows what it has become
	std::optional<MorphTargets> morph;
	/// How far it is drawn fizzed out of sight, 0 to 1, and how frozen, 0 to 1 (components::CreatureSpells)
	float fizz {0.0f};
	float freeze {0.0f};
	/// Whose painted skins it is drawn with: its own, or for the Creature Cave's copy of a creature, the creature's
	entt::entity paintedSkins {entt::null};
};

/// Every creature, and every copy of one in the Creature Cave (components::CreatureCopy, of its creature's species),
/// with a row in the frame's instances and a loaded mesh, by its row
[[nodiscard]] std::vector<Body> Bodies(const ecs::Registry& registry, const EntityInstances& instances,
                                       const MeshBones& meshBones);
/// The body drawn with a row, if one is
[[nodiscard]] const Body* Find(std::span<const Body> bodies, uint32_t instance);

/// A part of an eye: its mesh, where it is in the world, and for an eyelid the colour of the skin under it and how many
/// times the body's colour multiplies it: once for the first lid drawn, twice for the second, as the game keeps the
/// product on the eyelid from one lid to the next
struct EyePart
{
	entt::id_type mesh {0};
	glm::mat4 model {1.0f};
	std::optional<uint32_t> tint;
	uint32_t lidTimes {0};
};
/// The eyeballs and eyelids drawn this frame whose meshes are loaded, each eyeball before its eyelid
[[nodiscard]] std::vector<EyePart> Eyes(const ecs::components::CreatureEyes& eyes, const HasMesh& hasMesh);
/// An eyelid's colour: the skin's under it times the body's colour, `times` over (each channel trunc(a b / 255), alpha
/// 0xFF)
[[nodiscard]] uint32_t LidColour(uint32_t skin, uint32_t bodyColour, uint32_t times);

/// A row of the frame's instances: the model matrix's four columns, then the object's colour fields
using InstanceRow = std::array<glm::vec4, 5>;

/// The falling spell film's copy of the local player's creature (components::FallingCreature), drawn as its species
/// with its creature's painted skins, in the shape and pose it has; none without one or without its mesh loaded. It
/// has no row in the frame's instances (`instance` 0): it is drawn with a row of its own (FallingRow)
[[nodiscard]] std::optional<Body> FallingBody(const ecs::Registry& registry, const MeshBones& meshBones);
/// The falling copy's row: its model matrix, its colour set instead of the land's light (no haze) and its specular
[[nodiscard]] InstanceRow FallingRow(const glm::mat4& model, uint32_t colour, uint32_t specular);
/// An eyelid's colour on a body drawn in a set colour: each channel the skin's times the body's, over 255 (truncated),
/// alpha 0xFF
[[nodiscard]] uint32_t SetColourLid(uint32_t bodyColour, uint32_t skin);
/// The row an eye part is drawn with: the body's own; an eyelid's with its colour set instead of the land's light (no
/// haze) and the body's specular
[[nodiscard]] InstanceRow EyeRow(const glm::mat4& bodyRow, const glm::vec4& bodyColours, std::optional<uint32_t> lidColour,
                                 uint32_t specular);
/// An eye part's world matrix in the space of the body's row, so that the shader, which applies that row after the
/// part's own matrix, draws the part where it was placed. The row's w components carry other data (the fade, the
/// texture offset) and are dropped as the shader drops them.
[[nodiscard]] glm::mat4 InBodySpace(const glm::mat4& bodyRow, const glm::mat4& world);

/// What a mesh draw sets of the render states: the mode every primitive takes instead of its own, if one does, the
/// options added to each mode, the mode table and the object's alpha byte
struct PassState
{
	std::optional<render_modes::Mode> mode;
	render_modes::StateOptions options;
	render_modes::Table table {render_modes::Table::Normal};
	uint8_t globalAlpha {255};
};
/// One of the passes of a mesh fizzing out of sight, and what its shader is told (u_creatureSpellLook): for the depth,
/// how far the static has slid, 1, and the alpha the static must reach; for the body, 0, 0, 2 and the alpha it is
/// drawn at
struct FizzPass
{
	PassState state;
	glm::vec4 spellLook {0.0f};
};
/// A mesh fizzing out of sight at a level, as a byte, is drawn twice. First its depth alone, in the mode that writes
/// only Z where the alpha test passes, with `backFaces` culled whatever its materials say, where the static slid by
/// `scroll` reaches the level. Then its body in its own modes through the global alpha table, its alpha byte what the
/// level leaves, only where its depth equals that depth. `own` is how the mesh is drawn otherwise.
[[nodiscard]] std::array<FizzPass, 2> FizzPasses(const PassState& own, uint8_t level, glm::vec2 scroll,
                                                 render_modes::Cull backFaces);

/// Triangles in the world, two for each pair of ribbon corners
struct Strip
{
	std::vector<world_triangles::Vertex> vertices;
	std::vector<uint16_t> indices;
};
/// A creature's hair as ribbons facing an eye: the tufts drawn in the hair texture, and those drawn in their colour
/// alone. Each tuft's colour is its own in the land's light under the creature, then the land's colour and the haze
/// added, each 0 to 255.
struct Hair
{
	Strip textured;
	Strip plain;
};
[[nodiscard]] Hair BuildHair(const ecs::components::CreatureHair& hair, const glm::vec3& eye, const glm::ivec3& light,
                             const glm::ivec3& added);

/// Each creature's skins as painted, in textures the renderer owns. Painted again only when the creature's skins
/// change; a creature gone takes its textures with it. A skin with no texture, when no more can be made, keeps the
/// species' own.
template <typename Texture>
class PaintedSkins
{
public:
	/// A new skin texture, or none when no more can be made
	using Make = std::function<std::unique_ptr<Texture>()>;
	/// Copies the texels of a painted skin into its texture
	using Upload = std::function<void(Texture&, std::span<const uint16_t>)>;

	void Update(const ecs::Registry& registry, const Make& make, const Upload& upload)
	{
		std::vector<entt::entity> seen;
		registry.Each<const ecs::components::CreatureSkin>([&](entt::entity entity, const ecs::components::CreatureSkin& skin) {
			if (skin.skins.empty())
			{
				return;
			}
			seen.push_back(entity);
			auto& entry = _entries[entity];
			if (entry.painted && entry.revision == skin.revision && entry.skins.size() == skin.skins.size())
			{
				return;
			}
			entry.skins.resize(skin.skins.size());
			for (size_t i = 0; i < skin.skins.size(); ++i)
			{
				auto& [id, texture] = entry.skins.at(i);
				id = skin.skins.at(i).id;
				if (!texture)
				{
					texture = make();
				}
				if (texture)
				{
					upload(*texture, skin.skins.at(i).texels);
				}
			}
			entry.revision = skin.revision;
			entry.painted = true;
		});
		std::ranges::sort(seen);
		std::erase_if(_entries, [&seen](const auto& entry) { return !std::ranges::binary_search(seen, entry.first); });
	}

	/// The texture a creature's skin is drawn with in place of its mesh's, if it has one
	[[nodiscard]] const Texture* Find(entt::entity entity, uint32_t skinId) const
	{
		const auto entry = _entries.find(entity);
		if (entry == _entries.end())
		{
			return nullptr;
		}
		const auto& skins = entry->second.skins;
		const auto skin = std::ranges::find(skins, skinId, &std::pair<uint32_t, std::unique_ptr<Texture>>::first);
		return skin != skins.end() ? skin->second.get() : nullptr;
	}

	/// How many creatures have painted skins
	[[nodiscard]] size_t Size() const { return _entries.size(); }

	void Clear() { _entries.clear(); }

private:
	struct Entry
	{
		uint32_t revision {0};
		bool painted {false};
		std::vector<std::pair<uint32_t, std::unique_ptr<Texture>>> skins;
	};
	// Keyed by the whole entity, its version too, so a destroyed creature's entry can never be found for an entity that
	// reuses its index; Update drops every entry whose creature it did not see, a destroyed one included
	std::unordered_map<entt::entity, Entry> _entries;
};

} // namespace openblack::graphics::creature_draw
