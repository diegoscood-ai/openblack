/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// The gesture icons (Magic/Gestures/GestureIcons.h): each one Z object of the frame's transparency queue at key 0, whose
// callback first resets the depth of the box above the icon (a rectangle in mode 18, ZFUNC ALWAYS, writing only the
// depth) and draws the icon's model there (Magic/Gestures/GestureIconModels.h) with the models' camera on the world's
// lens, every vertex moved by the model's offset on the screen and held on it: a leash, the bubble, or a seed's graphic
// (Magic/Gestures/GestureIconSeeds.h: its mesh, the sprites of its holder effect, its power-up bands); then, with ZFUNC
// ALWAYS and no Z write, its symbol (a cell of S_Gesture<page>.raw, mode 6, two-sided and tiling) and its sparkles (a
// cell of S_SpriteSheet2.raw, mode 13), sprites in the plane of the screen.
// Everything comes from the frame's graphics::OverlayFrame (filled before DrawScene): nothing here reads the game.

#include <cstdint>
#include <cstring>

#include <array>
#include <limits>
#include <span>
#include <utility>
#include <vector>

#include <bgfx/bgfx.h>
#include <entt/core/hashed_string.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/matrix.hpp>

#include "3D/Billboard.h"
#include "3D/FrameAnim.h"
#include "Camera/Camera.h"
#include "Camera/ScreenPoint.h"
#include "Graphics/ArgbColour.h"
#include "Graphics/GestureIconEffect.h"
#include "Graphics/OverlayFrame.h"
#include "Graphics/RenderModes.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/WorldTriangles.h"
#include "Locator.h"
#include "Magic/Gestures/GestureIconModels.h"
#include "Renderer.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
/// (openblack) the depth the box above an icon is reset to: far behind its model, along the view. Only that model is
/// tested against it before the frame's end clears the depth, so its value is not seen
constexpr float k_DepthBoxDistance = 1000.0f;

/// The models' camera: at the eye, looking at the focus, on the world camera's lens
glm::mat4 IconModelView()
{
	namespace gestures = magic::gestures;
	return glm::lookAt(gestures::k_IconModelEye, gestures::k_IconModelFocus, glm::vec3(0.0f, 1.0f, 0.0f));
}

/// The models' camera as the sprites face it: its axes, its view and the world lens's near plane
billboard::CameraFrame IconCameraFrame(const Camera& world)
{
	billboard::CameraFrame frame = billboard::CameraFrame::From(world);
	frame.eye = magic::gestures::k_IconModelEye;
	frame.view = IconModelView();
	frame.inverseView = glm::inverse(frame.view);
	frame.worldToCamera = glm::mat3(frame.view);
	// the view's rows: right, up and forward
	frame.right = glm::vec3(frame.view[0][0], frame.view[1][0], frame.view[2][0]);
	frame.up = glm::vec3(frame.view[0][1], frame.view[1][1], frame.view[2][1]);
	frame.forward = glm::vec3(frame.view[0][2], frame.view[1][2], frame.view[2][2]);
	const auto basis = glm::mat3(frame.inverseView);
	frame.mist = glm::mat3(basis[0], -basis[2], basis[1]);
	return frame;
}
} // namespace

void Renderer::DrawGestureIconDepthBox(RenderPass viewId, const Camera& camera, const GestureIconOverlay& icon,
                                       const Texture2D& anyTexture) const
{
	if (_resolution.x == 0 || _resolution.y == 0)
	{
		return;
	}
	// the corners through the view's own view and projection, at the far depth, back on their pixels
	const auto toWorld =
	    screen_point::ClipToWorldOf(camera.GetViewMatrix(Camera::Interpolation::Current), camera.GetProjectionMatrix());
	const glm::vec2 screen(_resolution);
	std::array<world_triangles::Vertex, 4> vertices {};
	for (size_t k = 0; k < vertices.size(); ++k)
	{
		const auto pixel = icon.depthBox.at(k);
		const glm::vec2 clip(pixel.x / screen.x * 2.0f - 1.0f, 1.0f - pixel.y / screen.y * 2.0f);
		// its colour 0, no texture: mode 18 writes no colour, only the depth where its alpha test (ALPHAREF 0) passes
		vertices.at(k) = {
		    .position = screen_point::WorldAtDepth(toWorld, clip, k_DepthBoxDistance), .uv = {0.0f, 0.0f}, .abgr = 0u};
	}
	constexpr std::array<uint16_t, 6> k_Indices {0, 1, 3, 1, 2, 3};
	constexpr render_modes::Material k_DepthOnly {render_modes::Mode::ChromaDepthOnly, render_modes::k_TwoSided};
	render_modes::StateOptions options;
	options.zFunc = render_modes::ZFunc::Always;
	world_triangles::SubmitRaw(viewId, vertices, k_Indices, anyTexture, anyTexture, k_DepthOnly, *_shaderManager, options);
}

void Renderer::DrawGestureIconModel(RenderPass viewId, const Camera& camera, const GestureIconModelOverlay& model,
                                    glm::vec2 screenOffset) const
{
	const auto& meshes = Locator::resources::value().GetMeshes();
	constexpr auto k_Stride = static_cast<uint16_t>(sizeof(std::array<glm::vec4, 5>));
	if (!meshes.Contains(model.mesh) || bgfx::getAvailInstanceDataBuffer(1, k_Stride) < 1)
	{
		return;
	}
	const auto* program = _shaderManager->GetShader("ObjectIconInstanced");
	// the models' camera on the world's lens, and the model's move on the screen (vs_object.sc, USE_DRAW_CAMERA)
	const glm::mat4 viewProjection = camera.GetProjectionMatrix() * IconModelView();
	program->SetUniformValue("u_drawViewProj", glm::value_ptr(viewProjection));
	const glm::vec4 offset(screenOffset, 0.0f, 0.0f);
	program->SetUniformValue("u_drawScreenOffset", glm::value_ptr(offset));
	// its row: its matrix, its alpha as the fade of the first column's w, the texture's offset in the second's, and its
	// colour set (or none: the land's light where it stands) with its specular; a model not lit takes that colour as it
	// is
	std::array<glm::vec4, 5> row {model.matrix[0], model.matrix[1], model.matrix[2], model.matrix[3], glm::vec4(0.0f)};
	row[0].w = 1.0f - static_cast<float>(model.alpha) / 255.0f;
	row[1].w = frame_anim::PackUvOffset(model.uv.x, model.uv.y);
	if (!model.landLight)
	{
		argb_colour::PackInstanceColour(row[4], model.colour);
	}
	argb_colour::PackInstanceSpecular(row[4], model.specular);
	if (!model.lit)
	{
		argb_colour::PackInstanceUnlit(row[4]);
	}
	bgfx::InstanceDataBuffer instance {};
	bgfx::allocInstanceDataBuffer(&instance, 1, k_Stride);
	std::memcpy(instance.data, row.data(), sizeof(row));
	const glm::mat4 identity(1.0f);
	L3DMeshSubmitDesc submit {};
	submit.viewId = viewId;
	submit.program = program;
	// drawn with its own alpha through the global alpha table, else the normal one; no haze this near the models' camera
	submit.options = model.fading ? render_modes::StateOptions {.msaa = true} : render_modes::k_ModelPass;
	submit.table = model.fading ? render_modes::Table::GlobalAlpha : render_modes::Table::Normal;
	submit.globalAlpha = model.alpha;
	submit.noHaze = true;
	submit.modelMatrices = &identity;
	submit.matrixCount = 1;
	submit.transientInstance = &instance;
	DrawMesh(*meshes.Handle(model.mesh), submit, std::numeric_limits<uint8_t>::max());
}

void Renderer::DrawGestureIconEffect(RenderPass viewId, const Camera& camera, const GestureIconEffect& effect,
                                     glm::vec2 screenOffset) const
{
	if (_resolution.x == 0 || _resolution.y == 0 || effect.sprites.empty())
	{
		return;
	}
	// the sprites face the models' camera; each corner then goes to the screen through it, is moved there as the
	// model's vertices are, and is put back into the world at its depth, so that the world's view draws it on that pixel
	const auto frame = IconCameraFrame(camera);
	const glm::dmat4 iconToClip = glm::dmat4(camera.GetProjectionMatrix()) * glm::dmat4(frame.view);
	const auto toWorld =
	    screen_point::ClipToWorldOf(camera.GetViewMatrix(Camera::Interpolation::Current), camera.GetProjectionMatrix());
	const glm::vec2 screen(_resolution);
	const auto place = [&](const glm::vec3& corner) {
		return magic::gestures::IconPointInWorld(iconToClip, toWorld.inverseViewProjection, screenOffset, screen, corner);
	};
	DrawParticleSprites(effect.sprites, frame, viewId, place);
}

void Renderer::DrawGestureIcon(RenderPass viewId, const Camera& camera, const GestureIconOverlay& icon) const
{
	constexpr std::array<std::pair<entt::id_type, entt::id_type>, 2> k_Symbols = {{
	    {entt::hashed_string("raw/S_Gesture0").value(), entt::hashed_string("raw/S_Gesture0a").value()},
	    {entt::hashed_string("raw/S_Gesture1").value(), entt::hashed_string("raw/S_Gesture1a").value()},
	}};
	constexpr entt::id_type k_Sparkle = entt::hashed_string("raw/S_SpriteSheet2").value();
	constexpr entt::id_type k_SparkleAlpha = entt::hashed_string("raw/S_SpriteSheet2a").value();
	if (icon.page >= k_Symbols.size())
	{
		return;
	}
	const auto& textures = Locator::resources::value().GetTextures();
	const auto& [symbol, symbolAlpha] = k_Symbols.at(icon.page);
	// the box above it first, its depth reset, and its model there
	if (textures.Contains(symbol))
	{
		DrawGestureIconDepthBox(viewId, camera, icon, *textures.Handle(symbol));
	}
	for (const auto& model : icon.models)
	{
		DrawGestureIconModel(viewId, camera, model, icon.screenOffset);
	}
	if (icon.effect != nullptr)
	{
		DrawGestureIconEffect(viewId, camera, *icon.effect, icon.screenOffset);
	}
	for (const auto& band : icon.bands)
	{
		DrawGestureIconModel(viewId, camera, band, icon.screenOffset);
	}
	const auto frame = billboard::CameraFrame::From(camera);
	// ZFUNC ALWAYS and ZWRITEENABLE 0 around the symbol and the sparkles
	render_modes::StateOptions options;
	options.zFunc = render_modes::ZFunc::Always;
	options.zWrite = false;
	// each sprite's four corners and the sprite draw's triangles (0,1,2),(0,2,3), one indexed draw per material
	const auto submit = [&](std::span<const billboard::Sprite> sprites, entt::id_type texture, entt::id_type alpha,
	                        const render_modes::Material& material) {
		if (!textures.Contains(texture) || !textures.Contains(alpha))
		{
			return;
		}
		std::vector<world_triangles::Vertex> vertices;
		std::vector<uint16_t> indices;
		for (const auto& sprite : sprites)
		{
			const auto quad = billboard::SpriteQuad(sprite, frame);
			if (!quad.has_value())
			{
				continue; // at or before the near plane
			}
			const uint32_t abgr = argb_colour::ToAbgr(sprite.argb);
			const auto first = static_cast<uint16_t>(vertices.size()); // at most 3 x 4
			for (size_t corner = 0; corner < quad->corners.size(); ++corner)
			{
				vertices.push_back({quad->corners.at(corner), quad->uv.at(corner), abgr});
			}
			for (const int corner : billboard::k_SpriteTriangles)
			{
				indices.push_back(static_cast<uint16_t>(first + corner));
			}
		}
		if (vertices.empty())
		{
			return;
		}
		world_triangles::SubmitRaw(viewId, vertices, indices, *textures.Handle(texture), *textures.Handle(alpha), material,
		                           *_shaderManager, options);
	};
	submit(std::span(&icon.symbol, 1), symbol, symbolAlpha, render_modes::materials::k_GestureSymbol);
	submit(icon.sparkles, k_Sparkle, k_SparkleAlpha, render_modes::materials::k_GestureSparkle);
}
