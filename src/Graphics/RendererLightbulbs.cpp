/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The renderer is one of the locator's implementations
#define LOCATOR_IMPLEMENTATIONS

// The light bulbs the creatures show over their heads as they learn (Creature/CreatureLightbulb.h), in the world view:
// once a frame, from how the frame aged each bulb, its glow, its bulb and its text are made. The bulb is drawn at once
// with the land's objects, the glow goes to the transparency queue as a sprite, and the text is drawn with the numbers
// that float up from things, over the world. Nothing inside the temple, where the world is not drawn.

#include <cstdint>
#include <cstring>

#include <algorithm>
#include <array>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include <bgfx/bgfx.h>
#include <entt/core/hashed_string.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include "3D/AllMeshes.h"
#include "3D/Billboard.h"
#include "3D/L3DMesh.h"
#include "Camera/Camera.h"
#include "Creature/CreatureLightbulb.h"
#include "ECS/Components/CreatureLightbulbs.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "Graphics/ArgbColour.h"
#include "Graphics/GlowShellLook.h"
#include "Graphics/RegionOnScreen.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/WorldTriangles.h"
#include "Graphics/ZSort.h"
#include "Locator.h"
#include "Renderer.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Visual/RenderModes.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
constexpr entt::id_type k_Smoke = entt::hashed_string("raw/smoke").value();
constexpr entt::id_type k_SmokeAlpha = entt::hashed_string("raw/smokea").value();

/// The light bulb of the mesh pack
entt::id_type BulbMesh()
{
	return resources::HashIdentifier(MeshId::ObjectLightBulb);
}
} // namespace

void Renderer::CollectLightbulbs(const DrawSceneDesc& drawDesc) const
{
	_frameLightbulbs.clear();
	if (drawDesc.camera == nullptr || !Locator::entitiesRegistry::has_value() || !Locator::resources::has_value())
	{
		return;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto bulbMesh = BulbMesh();
	const bool hasMesh = meshes.Contains(bulbMesh);
	const auto eye = drawDesc.camera->GetOrigin();
	const auto viewProjection = drawDesc.camera->GetViewProjectionMatrix();
	const float nearPlane = billboard::CameraFrame::From(*drawDesc.camera).nearZ;
	const glm::vec2 resolution {static_cast<float>(_resolution.x), static_cast<float>(_resolution.y)};
	const float halfDiagonal = hasMesh ? ecs::object::MeshHalfDiagonal(bulbMesh) : 0.0f;
	std::as_const(Locator::entitiesRegistry::value())
	    .Each<const ecs::components::CreatureLightbulbs>([&](entt::entity /*creature*/,
	                                                         const ecs::components::CreatureLightbulbs& bulbs) {
		    const auto count = std::min(bulbs.looks.size(), bulbs.bulbs.size());
		    for (size_t i = 0; i < count; ++i)
		    {
			    const auto& look = bulbs.looks[i];
			    LightbulbDraw draw {
			        // the smoke's first frame, at the glow's size, yellow at the glow's alpha
			        .glow = billboard::Sprite {.position = look.glow,
			                                   .size = look.glowSize,
			                                   .argb = creature_lightbulb::GlowArgb(look),
			                                   .cell = 0},
			        .bulb = creature_lightbulb::BulbModel(look),
			        .bulbRgb = creature_lightbulb::BulbRgb(look),
			        .bulbShown = false,
			        .text = bulbs.bulbs[i].text,
			        .textPoint = std::nullopt,
			        .alpha = look.alpha,
			    };
			    // the bulb is drawn while its box is on the screen and it is not too far for its size
			    if (hasMesh)
			    {
				    const auto& box = meshes.Handle(bulbMesh)->GetBoundingBox();
				    draw.bulbShown =
				        !glow_shell::TooFar(zsort::Key(look.bulb, eye, zsort::SumOrder::XYZ), halfDiagonal, look.bulbScale) &&
				        region_on_screen::BoxInView(viewProjection, box, draw.bulb);
			    }
			    // the text's point, wherever it falls across the screen, as long as it is beyond the near plane
			    if (!draw.text.empty() && resolution.x > 0.0f && resolution.y > 0.0f)
			    {
				    draw.textPoint =
				        creature_lightbulb::ScreenPoint(viewProjection * glm::vec4(look.text, 1.0f), nearPlane, resolution);
			    }
			    _frameLightbulbs.push_back(std::move(draw));
		    }
	    });
}

void Renderer::DrawLightbulbs(RenderPass viewId) const
{
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto bulbMesh = BulbMesh();
	if (!meshes.Contains(bulbMesh))
	{
		return;
	}
	for (const auto& draw : _frameLightbulbs)
	{
		if (!draw.bulbShown)
		{
			continue;
		}
		// the bulb's own instance row: its matrix, no fade, its own colour in place of the land light and no specular,
		// lit by the model light (fifth column)
		std::array<glm::vec4, 5> row {draw.bulb[0], draw.bulb[1], draw.bulb[2], draw.bulb[3], glm::vec4(0.0f)};
		row[0].w = 0.0f;
		row[1].w = 0.0f;
		argb_colour::PackInstanceColour(row[4], draw.bulbRgb);
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
		DrawMesh(*meshes.Handle(bulbMesh), submit, std::numeric_limits<uint8_t>::max());
	}
}

void Renderer::DrawLightbulbGlow(RenderPass viewId, const Camera& camera, uint32_t index) const
{
	const auto& textures = Locator::resources::value().GetTextures();
	if (index >= _frameLightbulbs.size() || !textures.Contains(k_Smoke) || !textures.Contains(k_SmokeAlpha))
	{
		return;
	}
	const auto& glow = _frameLightbulbs[index].glow;
	// a sprite facing the screen (billboard::Screen), nothing at or before the near plane
	const auto quad = billboard::SpriteQuad(glow, billboard::CameraFrame::From(camera));
	if (!quad.has_value())
	{
		return;
	}
	std::array<world_triangles::Vertex, 4> vertices {};
	for (size_t k = 0; k < vertices.size(); ++k)
	{
		vertices.at(k) = {.position = quad->corners.at(k), .uv = quad->uv.at(k), .abgr = world_triangles::ToAbgr(glow.argb)};
	}
	constexpr std::array<uint16_t, 6> k_Indices {0, 1, 2, 0, 2, 3}; // billboard::k_SpriteTriangles
	// smoke.raw and its alpha, mode 6
	world_triangles::SubmitRaw(viewId, vertices, k_Indices, *textures.Handle(k_Smoke), *textures.Handle(k_SmokeAlpha),
	                           render_modes::materials::k_Smoke, *_shaderManager);
}

void Renderer::AddLightbulbTexts(const GameFont& font, float clipTop, float clipBottom,
                                 std::vector<GameFont::Vertex>& glyphs) const
{
	constexpr float k_Size = creature_lightbulb::k_TextSize;
	constexpr float k_Shadow = creature_lightbulb::k_ShadowOffset;
	const auto rgba = [](uint32_t rgb, float alpha) {
		return glm::vec4(static_cast<float>((rgb >> 16u) & 0xFFu) / 255.0f, static_cast<float>((rgb >> 8u) & 0xFFu) / 255.0f,
		                 static_cast<float>(rgb & 0xFFu) / 255.0f, alpha);
	};
	for (const auto& draw : _frameLightbulbs)
	{
		if (!draw.textPoint.has_value())
		{
			continue;
		}
		const auto at = creature_lightbulb::TextPlace(*draw.textPoint, font.GetStringWidth(draw.text, k_Size));
		const float alpha = static_cast<float>(draw.alpha) / 255.0f;
		// a black shadow a pixel right and down, then the text in yellow, both at the bulb's alpha
		font.AddText(glyphs, draw.text, at.x + k_Shadow, at.y + k_Shadow, k_Size,
		             rgba(creature_lightbulb::k_ShadowColour, alpha), clipTop, clipBottom);
		font.AddText(glyphs, draw.text, at.x, at.y, k_Size, rgba(creature_lightbulb::k_TextColour, alpha), clipTop, clipBottom);
	}
}
