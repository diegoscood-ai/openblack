/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// The temples' leash posts in the world view (Graphics/LeashPostDraw.h, worship::leash_posts): once a frame the spin of
// every post shown moves on and its collar and smoke are made, then each goes to the transparency queue on its own,
// its smoke first. Nothing inside the temple, where the world is not drawn.

#include <cstdint>
#include <cstring>

#include <array>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include <bgfx/bgfx.h>
#include <entt/core/hashed_string.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include "3D/AffineMatrix.h"
#include "3D/Billboard.h"
#include "3D/FrameAnim.h"
#include "3D/L3DMesh.h"
#include "3D/LandLight.h"
#include "3D/LandLightTable.h"
#include "3D/ObjectMatrix.h"
#include "Camera/Camera.h"
#include "Creature/LocalPlayer.h"
#include "ECS/Components/LeashPost.h"
#include "ECS/Components/NotDrawn.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "GameClock.h"
#include "Graphics/LeashPostDraw.h"
#include "Graphics/RenderModes.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/WorldTriangles.h"
#include "Locator.h"
#include "Renderer.h"
#include "Resources/ResourcesInterface.h"
#include "Resources/SharedAssets.h"
#include "Worship/LeashPosts.h"
#include "Worship/TempleLeash.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
constexpr entt::id_type k_Smoke = entt::hashed_string("raw/smoke").value();
constexpr entt::id_type k_SmokeAlpha = entt::hashed_string("raw/smokea").value();

/// The player's hand as the picked collar hangs on it: its root bone in the world (the hand's matrix by its first
/// bone), the hand entity's scale and whether it is hidden (components::NotDrawn); none without the hand service
std::optional<leash_post_draw::Hand> PlayerHand(const ecs::Registry& registry)
{
	if (!Locator::handSystem::has_value())
	{
		return std::nullopt;
	}
	const auto& hands = Locator::handSystem::value();
	const auto entity = hands.GetPlayerHands()[0];
	if (!registry.Valid(entity) || !registry.AllOf<ecs::components::Transform>(entity))
	{
		return std::nullopt;
	}
	leash_post_draw::Hand hand;
	// the y scale: the x is mirrored for a right-handed hand
	hand.scale = registry.Get<const ecs::components::Transform>(entity).scale.y;
	hand.hidden = registry.AllOf<ecs::components::NotDrawn>(entity);
	if (const auto* bones = hands.GetBoneMatrices(); bones != nullptr && !bones->empty())
	{
		hand.root = hands.GetHandMatrix() * bones->front();
	}
	return hand;
}
} // namespace

void Renderer::CollectLeashPosts() const
{
	_frameLeashPosts.clear();
	if (!Locator::entitiesRegistry::has_value() || !Locator::leashSystem::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	std::vector<entt::entity> hearts;
	lookup.Each<const ecs::components::TempleLeash>(
	    [&hearts](entt::entity heart, const ecs::components::TempleLeash&) { hearts.push_back(heart); });
	if (hearts.empty())
	{
		return;
	}
	// the frame's game time, stopped while the game is paused (as the mists and the chimney smoke)
	const bool paused = !Locator::time::has_value() || game_clock::IsPaused();
	const float seconds = worship::leash_posts::FrameSeconds(paused ? 0u : game_clock::FrameGameMs());
	const auto hand = PlayerHand(lookup);
	// the land light table's last entry of this frame (UpdateLandLight, PreDraw), white without the land's light
	const uint32_t landLight = IsLandLit() ? land_light::FullLight(*_landLight) : 0xFFFFFFFFu;
	const auto local = creature::LocalPlayer();
	const auto& leash = Locator::leashSystem::value();
	for (const auto heart : hearts)
	{
		const bool localTemple =
		    lookup.AllOf<ecs::components::Temple>(heart) && lookup.Get<const ecs::components::Temple>(heart).owner == local;
		const auto pick = worship::temple_leash::Pick(heart);
		for (const auto entity : worship::temple_leash::ShownPosts(heart, leash))
		{
			// the spin moves on for every post shown, the hidden hand's picked one too
			auto& post = registry.Get<ecs::components::LeashPost>(entity);
			worship::leash_posts::Step(post.spin, seconds);
			const bool pickedHere = localTemple && pick == static_cast<int32_t>(post.index);
			const auto model = affine::Model(lookup.Get<const ecs::components::Transform>(entity));
			if (auto drawn = leash_post_draw::Build(post, model, pickedHere, hand, landLight); drawn.has_value())
			{
				_frameLeashPosts.push_back(*drawn);
			}
		}
	}
}

void Renderer::DrawLeashPostSmoke(RenderPass viewId, const Camera& camera, uint32_t index) const
{
	const auto& textures = Locator::resources::value().GetTextures();
	if (index >= _frameLeashPosts.size() || !textures.Contains(k_Smoke) || !textures.Contains(k_SmokeAlpha))
	{
		return;
	}
	const auto& post = _frameLeashPosts[index];
	// a sprite facing the screen (billboard::Screen), nothing at or before the near plane
	const auto quad = billboard::SpriteQuad(post.smoke, billboard::CameraFrame::From(camera));
	if (!quad.has_value())
	{
		return;
	}
	std::array<world_triangles::Vertex, 4> vertices {};
	for (size_t k = 0; k < vertices.size(); ++k)
	{
		vertices.at(k) = {
		    .position = quad->corners.at(k), .uv = quad->uv.at(k), .abgr = world_triangles::ToAbgr(post.smoke.argb)};
	}
	constexpr std::array<uint16_t, 6> k_Indices {0, 1, 2, 0, 2, 3}; // billboard::k_SpriteTriangles
	// smoke.raw and its alpha, mode 6; the picked post's in mode 13 (SRCALPHA / ONE)
	world_triangles::SubmitRaw(viewId, vertices, k_Indices, *textures.Handle(k_Smoke), *textures.Handle(k_SmokeAlpha),
	                           post.additive ? render_modes::materials::k_SmokeAdditive : render_modes::materials::k_Smoke,
	                           *_shaderManager);
}

void Renderer::DrawLeashPostCollar(RenderPass viewId, uint32_t index) const
{
	const auto& meshes = Locator::resources::value().GetMeshes();
	constexpr auto k_Mesh = resources::shared_assets::k_LeashCollarMesh.value();
	if (index >= _frameLeashPosts.size() || !meshes.Contains(k_Mesh))
	{
		return;
	}
	const auto& post = _frameLeashPosts[index];
	// the collar's own instance row: its matrix, no fade, its texture offset in the w of the second column
	// (frame_anim::PackUvOffset), and the land light alone for its colour (fifth column 0)
	std::array<glm::vec4, 5> row {post.collar[0], post.collar[1], post.collar[2], post.collar[3], glm::vec4(0.0f)};
	row[0].w = 0.0f;
	row[1].w = frame_anim::PackUvOffset(post.uv.x, post.uv.y);
	constexpr auto k_Stride = static_cast<uint16_t>(sizeof(row));
	if (bgfx::getAvailInstanceDataBuffer(1, k_Stride) < 1)
	{
		return; // (openblack guard) no room left this frame
	}
	bgfx::InstanceDataBuffer instance {};
	bgfx::allocInstanceDataBuffer(&instance, 1, k_Stride);
	std::memcpy(instance.data, row.data(), sizeof(row));
	const glm::mat4 identity(1.0f);
	L3DMeshSubmitDesc submit {};
	submit.viewId = viewId;
	submit.program = _shaderManager->GetShader("ObjectInstanced");
	submit.options = render_modes::k_ModelPass;
	submit.modelMatrices = &identity;
	submit.matrixCount = 1;
	submit.transientInstance = &instance;
	// (inferred) the dynamic lighting the collar is made with: the objects' own light, the land's at its origin and the
	// engine's one light
	DrawMesh(*meshes.Handle(k_Mesh), submit, std::numeric_limits<uint8_t>::max());
}
