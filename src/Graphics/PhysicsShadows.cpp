/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PhysicsShadows.h"

#include <cstring>

#include <algorithm>
#include <limits>

#include <bgfx/bgfx.h>
#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandIslandInterface.h"
#include "Camera/Camera.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Creature.h"
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
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "Graphics/FrameBuffer.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/IndexBuffer.h"
#include "Graphics/RenderPass.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/VertexBuffer.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
constexpr float k_LightHeight = 15000.0f; // 0x9A3C10: obj.pos + (0, 15000, 0), holder+4 = 0 (not the sun)
constexpr uint16_t k_AtlasTexels = PhysicsShadows::k_Texels * PhysicsShadows::k_SlotsPerRow;

/// fn_007FCE80 gives a shadow to what casts a static shadow (IsShadowOnTexture / IsShadowOnTextureChroma, the list of
/// RenderingSystem.cpp's CastsStaticShadow) or is animated (villagers, animals)
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

glm::vec3 ToWorld(const glm::mat4& instance, const glm::vec3& point)
{
	// the columns' w carry other data (openblack-internals.md)
	return glm::vec3(instance[0]) * point.x + glm::vec3(instance[1]) * point.y + glm::vec3(instance[2]) * point.z +
	       glm::vec3(instance[3]);
}
} // namespace

PhysicsShadows::PhysicsShadows() = default;
PhysicsShadows::~PhysicsShadows() = default;

void PhysicsShadows::Update(const Camera& camera)
{
	_shadows.clear();
	if (!Locator::terrainSystem::has_value() || !Locator::rendereringSystem::has_value())
	{
		return;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& renderCtx = Locator::rendereringSystem::value().GetContext();
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto& island = Locator::terrainSystem::value();
	const auto cameraOrigin = camera.GetOrigin();

	ecs::physics::PhysicsObjects::ForEach([&](const ecs::physics::PhysicsObject& object) {
		// resting proxies are skipped (the byte elem+0x19C = PhysOb+0x174 that fn_00646FE0 tests)
		if (_shadows.size() >= k_MaxShadows || object.body.resting ||
		    !registry.Valid(object.entity) || !CastsPhysicsShadow(registry, object.entity))
		{
			return;
		}
		const auto found = renderCtx.entityInstances.find(object.entity);
		if (found == renderCtx.entityInstances.end() || !meshes.Contains(found->second.meshId) ||
		    found->second.index >= renderCtx.instanceUniforms.size())
		{
			return;
		}
		const auto mesh = meshes.Handle(found->second.meshId);
		const auto& transform = registry.Get<const ecs::components::Transform>(object.entity);
		const auto& instance = renderCtx.instanceUniforms[found->second.index];
		const glm::vec3 position = transform.position;

		// the tight box of the projected vertices (fn_00850900); f = Ly / (Ly - Wy) is 1 within 1/15000 per unit of
		// height, so the projection is straight down. Boned meshes (rigid skin in bone space) use their box corners.
		glm::vec2 minimum(std::numeric_limits<float>::max());
		glm::vec2 maximum(std::numeric_limits<float>::lowest());
		const auto add = [&minimum, &maximum](const glm::vec3& world) {
			minimum = glm::min(minimum, glm::vec2(world.x, world.z));
			maximum = glm::max(maximum, glm::vec2(world.x, world.z));
		};
		if (mesh->IsBoned())
		{
			const auto box = mesh->GetBoundingBox();
			for (int corner = 0; corner < 8; ++corner)
			{
				add(ToWorld(instance, glm::vec3((corner & 1) != 0 ? box.maxima.x : box.minima.x,
				                                (corner & 2) != 0 ? box.maxima.y : box.minima.y,
				                                (corner & 4) != 0 ? box.maxima.z : box.minima.z)));
			}
		}
		else
		{
			for (const auto& subMesh : mesh->GetSubMeshes())
			{
				if (subMesh->IsPhysics() || (subMesh->GetFlags().lodMask & 1) != 1)
				{
					continue;
				}
				for (const auto& point : subMesh->GetCollisionPositions())
				{
					add(ToWorld(instance, point));
				}
			}
		}
		if (maximum.x <= minimum.x || maximum.y <= minimum.y)
		{
			return;
		}

		// fn_00874600: 255 up to 50 radii between the camera and the ground point under the object, 0 at 80
		const float radius = 0.5f * glm::length(mesh->GetBoundingBox().Size()) * transform.scale.x;
		const float ground = island.GetHeightAt(glm::vec2(position.x, position.z));
		const float q = glm::distance(cameraOrigin, glm::vec3(position.x, ground, position.z)) / std::max(radius, 0.001f);
		const float fade = q < 50.0f ? 1.0f : std::max(0.0f, 1.0f - (q - 50.0f) / 30.0f);
		if (fade <= 0.0f)
		{
			return;
		}
		_shadows.push_back({object.entity, found->second.meshId, found->second.index,
		                    glm::vec4(minimum, 1.0f / (maximum - minimum)),
		                    glm::vec4(position + glm::vec3(0.0f, k_LightHeight, 0.0f), position.y), fade});
	});

	for (size_t i = 0; i < _shadows.size(); ++i)
	{
		const auto column = static_cast<float>(i % k_SlotsPerRow);
		const auto row = static_cast<float>(i / k_SlotsPerRow);
		const float size = 1.0f / static_cast<float>(k_SlotsPerRow);
		_boxes[i] = _shadows[i].box;
		_slots[i] = glm::vec4(column * size, row * size, size, _shadows[i].fade);
	}
}

void PhysicsShadows::Draw(const ShaderManager& shaders)
{
	if (_shadows.empty())
	{
		return;
	}
	if (!_silhouettes)
	{
		// 4 x 2 subsamples per texel (fn_00806F60: gx = 128 (x' - x0) / (x1 - x0), gz = 64 (z' - z0) / (z1 - z0))
		_silhouettes = std::make_unique<FrameBuffer>("PhysicsShadowSilhouettes", k_AtlasTexels * 4, k_AtlasTexels * 2,
		                                             TextureFormat::R8);
		_resolved = std::make_unique<FrameBuffer>("PhysicsShadows", k_AtlasTexels, k_AtlasTexels, TextureFormat::R8);
	}
	const auto& renderCtx = Locator::rendereringSystem::value().GetContext();
	const auto& meshes = Locator::resources::value().GetMeshes();

	// 1. Silhouettes (1 bit per subsample)
	{
		const auto viewId = static_cast<bgfx::ViewId>(RenderPass::PhysicsShadow);
		_silhouettes->Bind(RenderPass::PhysicsShadow);
		bgfx::setViewClear(viewId, BGFX_CLEAR_COLOR, 0x00000000);
		bgfx::setViewRect(viewId, 0, 0, k_AtlasTexels * 4, k_AtlasTexels * 2);
		bgfx::touch(viewId);
		const auto* program = shaders.GetShader("DynamicShadowInstanced");
		constexpr uint64_t k_State = BGFX_STATE_WRITE_R | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_ONE) |
		                             BGFX_STATE_BLEND_EQUATION(BGFX_STATE_BLEND_EQUATION_MAX);
		for (size_t i = 0; i < _shadows.size(); ++i)
		{
			const auto& shadow = _shadows[i];
			const auto mesh = meshes.Handle(shadow.meshId);
			const auto& skins = mesh->GetSkins();
			const glm::mat4 identity(1.0f);
			const auto* matrices = mesh->IsBoned() ? mesh->GetBoneMatrices().data() : &identity;
			const auto matrixCount = mesh->IsBoned() ? static_cast<uint16_t>(mesh->GetBoneMatrices().size()) : uint16_t {1};
			for (const auto& subMesh : mesh->GetSubMeshes())
			{
				// submeshes with 0x20000000 (LOD 0)
				if (subMesh->IsPhysics() || (subMesh->GetFlags().lodMask & 1) != 1)
				{
					continue;
				}
				for (const auto& prim : subMesh->GetPrimitives())
				{
					// chroma casters (trees) are alpha tested like their static shadow (fn_008815D0); the original also
					// blurs those 2 x 2 afterwards (0x807635), not done here
					const Texture2D* texture = nullptr;
					if (const auto skin = skins.find(prim.skinID); skin != skins.end())
					{
						texture = skin->second.get();
					}
					const glm::vec4 u_shadowParams = {prim.thresholdAlpha ? prim.alphaCutoutThreshold : 0.0f,
					                                  texture != nullptr ? 1.0f : 0.0f, 0.0f, 0.0f};
					program->SetUniformValue("u_shadowParams", &u_shadowParams);
					if (texture != nullptr)
					{
						program->SetTextureSampler("s_diffuse", 0, *texture);
					}
					program->SetUniformValue("u_shadowLight", &shadow.light);
					program->SetUniformValue("u_shadowBox", &shadow.box);
					program->SetUniformValue("u_shadowSlot", &_slots[i]);
					bgfx::setTransform(matrices, matrixCount);
					bgfx::setInstanceDataBuffer(toBgfx(renderCtx.instanceUniformBuffer), shadow.instance, 1);
					if (subMesh->GetMesh().IsIndexed())
					{
						subMesh->GetMesh().GetIndexBuffer().Bind(prim.indicesCount, prim.indicesOffset);
					}
					subMesh->GetMesh().GetVertexBuffer().Bind();
					bgfx::setState(k_State);
					bgfx::submit(viewId, toBgfx(program->GetRawHandle()));
				}
			}
		}
	}

	// 2. Subsample count (fn_00880FC0: table 0xFA95C4[mask] = popcount << 12, texels 1..30 only)
	{
		struct Vertex
		{
			float x, y, z;
			float u, v;
			uint32_t abgr;
		};
		bgfx::VertexLayout layout;
		layout.begin()
		    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
		    .end();
		if (bgfx::getAvailTransientVertexBuffer(6, layout) < 6)
		{
			return;
		}
		bgfx::TransientVertexBuffer buffer;
		bgfx::allocTransientVertexBuffer(&buffer, 6, layout);
		const Vertex quad[6] = {{-1, -1, 0.5f, 0, 0, ~0u}, {1, -1, 0.5f, 1, 0, ~0u}, {1, 1, 0.5f, 1, 1, ~0u},
		                        {-1, -1, 0.5f, 0, 0, ~0u}, {1, 1, 0.5f, 1, 1, ~0u},  {-1, 1, 0.5f, 0, 1, ~0u}};
		std::memcpy(buffer.data, quad, sizeof(quad));

		const auto viewId = static_cast<bgfx::ViewId>(RenderPass::PhysicsShadowResolve);
		_resolved->Bind(RenderPass::PhysicsShadowResolve);
		bgfx::setViewClear(viewId, BGFX_CLEAR_COLOR, 0x00000000);
		bgfx::setViewRect(viewId, 0, 0, k_AtlasTexels, k_AtlasTexels);
		const glm::mat4 identity(1.0f);
		bgfx::setViewTransform(viewId, glm::value_ptr(identity), glm::value_ptr(identity));
		const auto* program = shaders.GetShader("PhysicsShadowResolve");
		const glm::vec4 u_resolve = {static_cast<float>(k_AtlasTexels), static_cast<float>(PhysicsShadows::k_Texels),
		                             0.0f, 0.0f};
		program->SetUniformValue("u_resolve", &u_resolve);
		program->SetTextureSampler("s_diffuse", 0, _silhouettes->GetColorAttachment());
		bgfx::setVertexBuffer(0, &buffer);
		bgfx::setState(BGFX_STATE_WRITE_R);
		bgfx::submit(viewId, toBgfx(program->GetRawHandle()));
	}
}

void PhysicsShadows::BindTerrain(const ShaderProgram& program, bool enabled) const
{
	const glm::vec4 u_physicsShadowCount = {enabled && _resolved ? static_cast<float>(_shadows.size()) : 0.0f, 0.0f, 0.0f,
	                                        0.0f};
	program.SetUniformValue("u_physicsShadowCount", &u_physicsShadowCount);
	program.SetUniformValue("u_physicsShadowBox", _boxes.data(), static_cast<uint16_t>(k_MaxShadows));
	program.SetUniformValue("u_physicsShadowSlot", _slots.data(), static_cast<uint16_t>(k_MaxShadows));
	if (_resolved)
	{
		program.SetTextureSampler("s9_physicsShadow", 9, _resolved->GetColorAttachment());
	}
}
