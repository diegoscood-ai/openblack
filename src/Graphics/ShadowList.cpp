/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ShadowList.h"

#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>

#include <LNDFile.h>
#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>
#include <stb_image_write.h>

#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "ECS/Animations.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/DynamicShadow.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "Graphics/Argb4444.h"
#include "Graphics/Haze.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;
using namespace openblack::graphics::shadow_list;

namespace
{
/// The hand's texture size and density while S5 waits (k_HandShadowAsOriginal): openblack's old 64 x 64 at full density
constexpr int k_HandTexels = k_HandShadowAsOriginal ? shadow_math::k_Texels : 64;
constexpr bool k_HandHalfRows = k_HandShadowAsOriginal;
constexpr bool k_HandHoldsInShadow = k_HandShadowAsOriginal;

/// fn_007FCE80 0x7FCE9D..0x7FCEC7 gives a ShadowInfo to the physics object that IsShadowOnTextureChroma (vt+0x94,
/// LH3DObject flags +4 bit 13, fn_007F98D0), IsShadowOnTexture (vt+0x84, bit 12, fn_007F98A0) or is animated
/// (vt+0x1AC): the casters of a static shadow (RenderingSystem's CastsStaticShadow) and the villagers and animals
bool CastsPhysicsShadow(const ecs::Registry& registry, entt::entity entity)
{
	using namespace ecs::components;
	// building fragments: SetShadowOnTexture(0) clears the 0x1000 that fn_007FCE80 needs
	if (registry.AnyOf<Fragment>(entity))
	{
		return false;
	}
	if (registry.AnyOf<Villager, Animal>(entity))
	{
		return true;
	}
	return registry.AnyOf<Fixed, MobileStatic, MobileObject, Tree, Abode, Feature, BigForest>(entity) &&
	       !registry.AnyOf<Pot, AnimatedStatic, DeadTree, Field, Creature, Hand, Alpha, TempleInteriorPart>(entity);
}

/// The instance matrix as a plain affine matrix: the columns' w carry other data (openblack-internals.md)
glm::mat4 InstanceMatrix(const glm::mat4& instance)
{
	glm::mat4 matrix = instance;
	matrix[0].w = 0.0f;
	matrix[1].w = 0.0f;
	matrix[2].w = 0.0f;
	matrix[3].w = 1.0f;
	return matrix;
}

/// One caster's part of the silhouette: its projected points and its primitives' triangles into them
struct Silhouette
{
	std::vector<glm::vec2> points;
	struct Primitive
	{
		size_t first;
		size_t count;
		bool bothFaces;
		bool halfRows;
	};
	std::vector<uint16_t> indices;
	std::vector<Primitive> primitives;
};

/// fn_00806F60 0x807109..0x807148 (the caster) or 0x8071D3..0x807249 (the held object): every sub-mesh with the LOD 0
/// bit (0x20000000, [0xC37D94] = 1) through fn_0087FA70 -> fn_00850900, each vertex by its bone (the skinned branch,
/// [0xFA93BC] & 1, 0x850B04) or the object's matrix
void ProjectCaster(const L3DMesh& mesh, const glm::mat4& instance, const glm::mat4* bones, size_t boneCount,
                   const shadow_math::Projection& projection, bool halfRows, shadow_math::Box& box, Silhouette& out)
{
	for (const auto& subMesh : mesh.GetSubMeshes())
	{
		if (subMesh->IsPhysics() || (subMesh->GetFlags().lodMask & 1) != 1)
		{
			continue;
		}
		const auto& local = subMesh->GetSkinLocalPositions();
		const auto& skin = subMesh->GetSkinBones();
		const auto& collision = subMesh->GetCollisionIndices();
		const size_t base = out.points.size();
		if (base + local.size() > 0xFFFF)
		{
			continue; // (port guard) the 16-bit indices of fn_00850CC0
		}
		for (size_t i = 0; i < local.size(); ++i)
		{
			const auto bone = i < skin.size() && skin[i] < boneCount ? skin[i] : 0;
			const auto matrix = bones != nullptr && boneCount > 0 ? instance * bones[bone] : instance;
			out.points.push_back(shadow_math::Project(projection, matrix, local[i], box));
		}
		// the file's triangles of each primitive (GetCollisionRanges: the hd-tweaks smoothing moves the primitives' own
		// offsets into its own index buffer)
		const auto& primitives = subMesh->GetPrimitives();
		const auto& ranges = subMesh->GetCollisionRanges();
		for (size_t p = 0; p < primitives.size() && p < ranges.size(); ++p)
		{
			const size_t first = out.indices.size();
			const auto end = std::min<size_t>(collision.size(), size_t {ranges[p].first} + ranges[p].second);
			for (size_t i = ranges[p].first; i < end; ++i)
			{
				out.indices.push_back(static_cast<uint16_t>(base + collision[i]));
			}
			// fn_00850CC0 0x850CC9..0x850CEE: both faces with the material's +5 & 1 or a mist caster ([0xEA1AE8] vt+0x1F8 =
			// IsMist, 1 only in Mist 0x55EB90; no mist gets a ShadowInfo)
			out.primitives.push_back({first, out.indices.size() - first, primitives[p].twoSided, halfRows});
		}
	}
}

/// The block states of this frame for fn_00874600: g_index_block -> +0x920 & 1 (fn_00877210's outcodes with the main
/// camera) and +0x9BC (|centre - camera|, 0x877C8A..0x877CCD)
struct Blocks
{
	std::unordered_map<int, shadow_math::BlockState> states;
	[[nodiscard]] shadow_math::BlockState At(int x, int z) const
	{
		const auto found = states.find(x * 4096 + z);
		return found != states.end() ? found->second : shadow_math::BlockState {};
	}
};

Blocks BuildBlocks(const LandIslandInterface& island, const FrameInputs& inputs)
{
	Blocks blocks;
	for (const auto& block : island.GetBlocks())
	{
		const auto& lnd = block.GetLndBlock();
		const float height = lnd ? static_cast<float>(static_cast<int32_t>(lnd->highestAltitude)) : 0.0f; // fild +0x924
		const auto mapPosition = block.GetMapPosition();
		const auto corners = haze::BlockCorners(mapPosition, height, inputs.landRef);
		// the centre: x, z + 80 ([0x8D060C]); y 0 with LandRef, else h 0.67 0.5 (0x877232..0x877292)
		const glm::vec3 centre(mapPosition.x + 80.0f, inputs.landRef ? 0.0f : height * 0.67f * 0.5f, mapPosition.y + 80.0f);
		const float dx = centre.x - inputs.camera.x;
		const float dy = centre.y - inputs.camera.y;
		const float dz = centre.z - inputs.camera.z;
		shadow_math::BlockState state;
		state.exists = true;
		state.visible = shadow_math::BlockVisible(corners, inputs.worldToClip, inputs.nearW);
		// (inferido: no visible effect) the original keeps an invisible block's old +0x9BC; here every block's is fresh
		state.distance = std::sqrt(dz * dz + dy * dy + dx * dx);
		const int bx = static_cast<int>(mapPosition.x / shadow_math::k_BlockSize);
		const int bz = static_cast<int>(mapPosition.y / shadow_math::k_BlockSize);
		blocks.states[bx * 4096 + bz] = state;
	}
	return blocks;
}

void Upload(ShadowInfo& shadow)
{
	const auto side = static_cast<uint16_t>(shadow.texels);
	if (!bgfx::isValid(shadow.texture))
	{
		// fn_0087FD50: fn_008379E0(0, 0xC4, ...) cleared to 0; material +5 = 0: no tiling (CLAMP, 0x87864D)
		shadow.texture = bgfx::createTexture2D(side, side, false, 1, bgfx::TextureFormat::R8,
		                                       BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP, nullptr);
		bgfx::setName(shadow.texture, "ShadowInfo");
	}
	const auto* memory = bgfx::alloc(static_cast<uint32_t>(side) * side);
	for (size_t i = 0; i < shadow.texels16.size() && i < memory->size; ++i)
	{
		memory->data[i] = argb4444::Expand(shadow.texels16[i]); // the ARGB4444 alpha nibble as D3D samples it
	}
	bgfx::updateTexture2D(shadow.texture, 0, 0, 0, 0, side, side, memory);
}

/// OPENBLACK_DUMP_SHADOWS=<dir>: each texture x 8 as a PNG, once per 300 frames
void Dump(const ShadowInfo& shadow, int frame, size_t index)
{
	const char* dir = std::getenv("OPENBLACK_DUMP_SHADOWS");
	if (dir == nullptr || frame % 300 != 0)
	{
		return;
	}
	constexpr int k_Zoom = 8;
	const int side = shadow.texels * k_Zoom;
	std::vector<uint8_t> pixels(static_cast<size_t>(side * side));
	for (int y = 0; y < side; ++y)
	{
		for (int x = 0; x < side; ++x)
		{
			pixels[static_cast<size_t>(y * side + x)] =
			    argb4444::Expand(shadow.texels16[static_cast<size_t>((y / k_Zoom) * shadow.texels + x / k_Zoom)]);
		}
	}
	const auto path = std::filesystem::path(dir) / ("shadow_" + std::to_string(frame) + "_" + std::to_string(index) + "_" +
	                                               std::to_string(static_cast<uint32_t>(shadow.caster)) + ".png");
	stbi_write_png(path.string().c_str(), side, side, 1, pixels.data(), side);
}
} // namespace

List::List() = default;

List::~List()
{
	Clear();
}

ShadowInfo& List::Add(entt::entity caster, Update update, LightKind light, bool onObjects, bool halfRows, int texels)
{
	ShadowInfo shadow;
	shadow.caster = caster;
	shadow.update = update;
	shadow.light = light;
	shadow.onObjects = onObjects;
	shadow.halfRows = halfRows;
	shadow.texels = texels;
	shadow.emitter = update == Update::Generic;
	shadow.texels16.assign(static_cast<size_t>(texels * texels), 0);
	return _shadows.emplace_front(std::move(shadow)); // [0xFAA7E0] = the new one, +4 = the old head (0x87FED6)
}

void List::Remove(entt::entity caster)
{
	_shadows.remove_if([caster](ShadowInfo& shadow) {
		if (shadow.caster != caster)
		{
			return false;
		}
		if (bgfx::isValid(shadow.texture))
		{
			bgfx::destroy(shadow.texture);
		}
		return true;
	});
}

void List::Clear()
{
	for (auto& shadow : _shadows)
	{
		if (bgfx::isValid(shadow.texture))
		{
			bgfx::destroy(shadow.texture);
		}
	}
	_shadows.clear();
}

void List::Frame(const FrameInputs& inputs)
{
	++_frame;
	if (!Locator::terrainSystem::has_value() || !Locator::rendereringSystem::has_value() ||
	    !Locator::entitiesRegistry::has_value())
	{
		Clear();
		return;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& renderCtx = Locator::rendereringSystem::value().GetContext();
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto& island = Locator::terrainSystem::value();

	// ---- the producers ----
	for (auto& shadow : _shadows)
	{
		shadow.seen = false;
	}
	const auto find = [this](entt::entity caster) -> ShadowInfo* {
		for (auto& shadow : _shadows)
		{
			if (shadow.caster == caster)
			{
				return &shadow;
			}
		}
		return nullptr;
	};
	const auto want = [&](entt::entity caster, Update update, LightKind light, bool onObjects, bool halfRows,
	                      int texels) -> ShadowInfo& {
		auto* shadow = find(caster);
		if (shadow == nullptr || shadow->light != light || shadow->texels != texels)
		{
			Remove(caster);
			shadow = &Add(caster, update, light, onObjects, halfRows, texels);
		}
		shadow->onObjects = onObjects;
		shadow->seen = true;
		return *shadow;
	};
	// the hand: CreateDynamicShadow (0x80C020) when [0xC3820C] != 0 (1 in the data, `01000000`); si+0xC stays 0
	if (Locator::handSystem::has_value())
	{
		const auto hand = Locator::handSystem::value().GetPlayerHands()[0];
		if (registry.Valid(hand))
		{
			auto& shadow = want(hand, Update::Complex, LightKind::Hand, true, k_HandHalfRows, k_HandTexels);
			// SetHeldObject vt+0x234 = fn_00816830 (0x816855) when IsG3DObjectDrawnInHand (vt+0x618, 0x46DC86): the
			// seeds that are only an effect in the hand have no mesh drawn (HandSpellSeed ShowSeedMesh), so no instance
			const auto held = Locator::handSystem::value().GetHeldObject();
			shadow.held = k_HandHoldsInShadow && held.has_value() && renderCtx.entityInstances.contains(*held) ? *held
			                                                                                                 : entt::null;
		}
	}
	// the flying physics objects (fn_00646FE0 -> fn_007FCE80; the resting proxies, PhysOb+0x174, are skipped)
	ecs::physics::PhysicsObjects::ForEach([&](const ecs::physics::PhysicsObject& object) {
		if (object.body.resting || !registry.Valid(object.entity) || !CastsPhysicsShadow(registry, object.entity))
		{
			return;
		}
		// fn_008745A0 0x8745C8: si+0xC = 1, holder+4 = 0
		want(object.entity, Update::Generic, LightKind::Vertical, false, false, shadow_math::k_Texels);
	});
	// the objects with a holder of their own (the launched boat: holder+4 = 1 0x5E11B6, si+0xC = 0 0x5E11BE)
	registry.Each<const ecs::components::DynamicShadow>(
	    [&](entt::entity entity, const ecs::components::DynamicShadow& dynamic) {
		    want(entity, Update::Generic, dynamic.useSun ? LightKind::Sun : LightKind::Vertical, dynamic.onObjects, false,
		         shadow_math::k_Texels);
	    });
	for (auto it = _shadows.begin(); it != _shadows.end();)
	{
		if (!it->seen)
		{
			if (bgfx::isValid(it->texture))
			{
				bgfx::destroy(it->texture);
			}
			it = _shadows.erase(it);
		}
		else
		{
			++it;
		}
	}
	if (_shadows.empty())
	{
		return;
	}

	// ---- the updates ----
	const auto blocks = BuildBlocks(island, inputs);
	const shadow_math::BlockAt blockAt = [&blocks](int x, int z) { return blocks.At(x, z); };
	const int cellLimit = static_cast<int>(island.GetCellsPerSide()) - 1; // 0x1FF in the original's 512 cells
	const auto poses = ecs::PosesByInstance(renderCtx);
	const bool trace = std::getenv("OPENBLACK_SHADOW_TRACE") != nullptr && _frame % 60 == 0;
	size_t index = 0;
	for (auto& shadow : _shadows)
	{
		++index;
		const auto instance = renderCtx.entityInstances.find(shadow.caster);
		if (instance == renderCtx.entityInstances.end() || !meshes.Contains(instance->second.meshId) ||
		    instance->second.index >= renderCtx.instanceUniforms.size())
		{
			shadow.active = false; // (inferido) not drawn this frame: like obj+0xAC of fn_00814FD0 (D-B1)
			continue;
		}
		shadow.active = true;
		const auto mesh = meshes.Handle(instance->second.meshId);
		const auto matrix = InstanceMatrix(renderCtx.instanceUniforms[instance->second.index]);
		const glm::vec3 position(matrix[3]); // obj+0x38..0x40, the drawn (interpolated, 0x7FCED2) matrix
		const auto* transform = registry.TryGet<const ecs::components::Transform>(shadow.caster);
		const float scale = transform != nullptr ? transform->scale.x : 1.0f;         // obj+0x44
		const float radius = ecs::object::MeshHalfDiagonal(instance->second.meshId); // mesh+0x30
		const float ground = island.GetHeightAt(glm::vec2(position.x, position.z));   // GetAltitude 0x874789

		// fn_00874850 0x874872 / fn_00814FD0 0x815002
		const float fade = shadow_math::Fade(position, ground, inputs.camera, scale, radius, blockAt, cellLimit);
		shadow.alpha = shadow.update == Update::Generic ? shadow_math::AlphaGeneric(fade, shadow.baseAlpha)
		                                                : shadow_math::AlphaComplex(fade, shadow.baseAlpha);
		if (shadow.alpha == 0) // 0x874898 / 0x815041
		{
			continue;
		}
		glm::vec3 light;
		switch (shadow.light)
		{
		case LightKind::Vertical:
			light = shadow_math::LightGeneric(position, false);
			break;
		case LightKind::Sun:
			light = shadow_math::LightGeneric(position, true);
			break;
		case LightKind::Hand:
			light = shadow_math::LightHand(position);
			break;
		case LightKind::Creature:
			light = position; // (no creature yet)
			break;
		}
		shadow.projection = shadow_math::MakeProjection(position, light);
		if (shadow.light == LightKind::Hand && !k_HandShadowAsOriginal)
		{
			// TODO(S5): the look of the hand's shadow before the list, until the original's is seen (D-U1): projected
			// from the light onto the ground under the hand, s = (ground - Ly) / (y - Ly) (the old
			// vs_dynamic_shadow_instanced). fn_00850900's t = -Ly / (h - Ly) with h = y - base gives the same with the base
			// at the ground and the light's y taken from it. The original's base is the hand's own y (0x8152B1, R12).
			shadow.projection.baseY = ground;
			shadow.projection.light.y = light.y - ground;
		}
		shadow.box = {};

		// the caster's bones: the hand's ([0xC37D9C] = CHand+0x47F0, 0x46CB1C), a posed model's (fn_00839980), or the
		// mesh's rest pose
		const glm::mat4* bones = nullptr;
		size_t boneCount = 0;
		if (mesh->IsBoned())
		{
			const std::vector<glm::mat4>* handBones = nullptr;
			if (shadow.light == LightKind::Hand && Locator::handSystem::has_value())
			{
				handBones = Locator::handSystem::value().GetBoneMatrices();
			}
			if (handBones != nullptr && handBones->size() == mesh->GetBoneMatrices().size())
			{
				bones = handBones->data();
				boneCount = handBones->size();
			}
			else
			{
				bones = mesh->GetBoneMatrices().data();
				auto count = static_cast<uint8_t>(std::min<size_t>(255, mesh->GetBoneMatrices().size()));
				ecs::UsePose(poses, instance->second.index, *mesh, bones, count);
				boneCount = count;
			}
		}
		Silhouette silhouette;
		ProjectCaster(*mesh, matrix, bones, boneCount, shadow.projection, shadow.halfRows, shadow.box, silhouette);

		// the held object: its own base y ([0xEA1AE8] = held, 0x807163), its pose, the same light and box, full density
		if (shadow.held != entt::null)
		{
			const auto heldInstance = renderCtx.entityInstances.find(shadow.held);
			if (heldInstance != renderCtx.entityInstances.end() && meshes.Contains(heldInstance->second.meshId) &&
			    heldInstance->second.index < renderCtx.instanceUniforms.size())
			{
				const auto heldMesh = meshes.Handle(heldInstance->second.meshId);
				const auto heldMatrix = InstanceMatrix(renderCtx.instanceUniforms[heldInstance->second.index]);
				auto heldProjection = shadow.projection;
				heldProjection.baseY = heldMatrix[3].y;
				const glm::mat4* heldBones = nullptr;
				uint8_t heldCount = 0;
				if (heldMesh->IsBoned())
				{
					heldBones = heldMesh->GetBoneMatrices().data();
					heldCount = static_cast<uint8_t>(std::min<size_t>(255, heldMesh->GetBoneMatrices().size()));
					ecs::UsePose(poses, heldInstance->second.index, *heldMesh, heldBones, heldCount);
				}
				ProjectCaster(*heldMesh, heldMatrix, heldBones, heldCount, heldProjection, false, shadow.box, silhouette);
			}
		}
		if (silhouette.points.empty() || !(shadow.box.x1 > shadow.box.x0) || !(shadow.box.z1 > shadow.box.z0))
		{
			shadow.alpha = 0; // (port guard) nothing to divide the grid by
			continue;
		}

		// fn_00806F60: the grid, the raster, the resolve (0x807601) and the baked fade (0x80769A)
		shadow_math::ToGrid(shadow.box, silhouette.points, shadow.texels);
		shadow_math::Coverage coverage(shadow.texels);
		for (const auto& primitive : silhouette.primitives)
		{
			shadow_math::RasterTriangles(silhouette.points,
			                             std::span<const uint16_t>(silhouette.indices).subspan(primitive.first, primitive.count),
			                             primitive.bothFaces, primitive.halfRows, coverage);
		}
		shadow_math::Resolve(coverage, shadow.texels16);
		shadow_math::BakeAlpha(shadow.texels16, shadow.alpha);

		// fn_00878350's t': H = GetAltitude(caster) with si+0x464, else si+0x18
		shadow.landT = shadow_math::LandT(shadow.projection.baseY, light.y, shadow.emitter ? ground : shadow.projection.baseY);
		Upload(shadow);
		Dump(shadow, _frame, index);
		if (trace)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("graphics"),
			                   "shadow {} caster {} light {} alpha {} fade {:.1f} box ({:.2f}, {:.2f})..({:.2f}, {:.2f}) kMin {:.1f} "
			                   "t' {:.5f} max n {} points {}",
			                   index, static_cast<uint32_t>(shadow.caster), static_cast<int>(shadow.light), shadow.alpha,
			                   fade, shadow.box.x0, shadow.box.z0, shadow.box.x1, shadow.box.z1, shadow.box.kMin,
			                   shadow.landT, *std::max_element(shadow.texels16.begin(), shadow.texels16.end()),
			                   silhouette.points.size());
		}
	}
}
