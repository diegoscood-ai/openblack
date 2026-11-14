/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "InLoopLogoDraw.h"

#include <cstring>

#include <array>
#include <filesystem>
#include <limits>
#include <string>
#include <utility>

#include <L3DFile.h>
#include <bgfx/bgfx.h>
#include <entt/core/hashed_string.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>

#include "3D/Billboard.h"
#include "Camera/Camera.h"
#include "FileSystem/FileSystemInterface.h"
#include "Graphics/ArgbColour.h"
#include "Graphics/FrameBuffer.h"
#include "Graphics/GameFont.h"
#include "Graphics/RendererInterface.h"
#include "Graphics/Texture2D.h"
#include "Gui/Canvas.h"
#include "Gui/GameFont.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Visual/RenderModes.h"
#include "Visual/RenderPass.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
/// The sky's mesh, in the game's Data folder
constexpr std::string_view k_SkyMesh = "D_SKY.l3d";
/// The stars' texture and its alpha
constexpr entt::id_type k_StarTexture = entt::hashed_string("raw/gatheringtext").value();
constexpr entt::id_type k_StarAlpha = entt::hashed_string("raw/gatheringtexta").value();
/// The name's and the trademark's font, the game's own
constexpr std::string_view k_Font = "j0";
/// The view the sky and the stars are drawn in, and the one of the name, the trademark and the fade: both into the
/// logo's picture. Then the copy of the picture to the window
constexpr auto k_WorldView = RenderPass::Main;
constexpr auto k_OverlayView = RenderPass::ScreenOverlay;
constexpr auto k_CopyView = RenderPass::Interface;
/// The picture starts black and opaque
constexpr uint32_t k_Black = 0x000000FFu;
/// The stars' materials: made on their texture with no flags, one blended over what is behind it, one added to it
constexpr render_modes::Material k_StarBlended {render_modes::SpriteMode(false), 0};
constexpr render_modes::Material k_StarAdded {render_modes::SpriteMode(true), 0};

[[nodiscard]] glm::vec4 Rgba(uint32_t argb)
{
	return {static_cast<float>((argb >> 16u) & 0xFFu) / 255.0f, static_cast<float>((argb >> 8u) & 0xFFu) / 255.0f,
	        static_cast<float>(argb & 0xFFu) / 255.0f, static_cast<float>(argb >> 24u) / 255.0f};
}

/// The billboards' camera frame of the logo's camera
[[nodiscard]] billboard::CameraFrame FrameOf(const Camera& camera)
{
	return billboard::CameraFrame::From(camera);
}

/// Text in the font at a place and size, as the canvas's glyph quads
void AddText(gui::Canvas& canvas, const gui::GameFont& font, std::u16string_view text, glm::vec2 at, float size,
             glm::vec4 colour)
{
	std::vector<GameFont::Vertex> glyphs;
	font.Get().AddText(glyphs, std::u16string(text), at.x, at.y, size, colour);
	for (size_t i = 0; i + 5 < glyphs.size(); i += 6)
	{
		const auto& a = glyphs[i];
		const auto& b = glyphs[i + 2];
		canvas.DrawQuad({a.x, a.y}, {b.x, b.y}, {a.u, a.v}, {b.u, b.v}, colour, &font.GetTexture(), true);
	}
}
} // namespace

InLoopLogoDraw::InLoopLogoDraw() = default;
InLoopLogoDraw::~InLoopLogoDraw() = default;

std::vector<uint8_t> InLoopLogoDraw::SkyTexels(std::span<const uint16_t> texels)
{
	std::vector<uint8_t> rgba(texels.size() * 4);
	const auto widen = [](uint32_t five) { return static_cast<uint8_t>((five << 3u) | (five >> 2u)); };
	for (size_t i = 0; i < texels.size(); ++i)
	{
		const uint32_t texel = texels[i];
		rgba[i * 4] = widen((texel >> 10u) & 0x1Fu);
		rgba[i * 4 + 1] = widen((texel >> 5u) & 0x1Fu);
		rgba[i * 4 + 2] = widen(texel & 0x1Fu);
		rgba[i * 4 + 3] = 0xFF;
	}
	return rgba;
}

std::unique_ptr<InLoopLogoDraw> InLoopLogoDraw::Create()
{
	auto& fileSystem = Locator::filesystem::value();
	auto font = gui::GameFont::From(
	    resources::LoadGameFont(Locator::resources::value().GetFonts(), fileSystem.GetPath<filesystem::Path::Data>() / k_Font));
	const auto& textures = Locator::resources::value().GetTextures();
	if (!font || !textures.Contains(k_StarTexture) || !textures.Contains(k_StarAlpha))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The logo can't be shown without its font and the stars' texture");
		return nullptr;
	}
	auto draw = std::unique_ptr<InLoopLogoDraw>(new InLoopLogoDraw());
	draw->_font = std::make_unique<gui::GameFont>(std::move(*font));
	draw->_overlay = std::make_unique<gui::Canvas>(k_OverlayView);
	draw->_copy = std::make_unique<gui::Canvas>(k_CopyView);
	// the sky's alpha is its colour's: its texture is opaque
	constexpr std::array<uint8_t, 4> k_White {0xFF, 0xFF, 0xFF, 0xFF};
	draw->_white = std::make_unique<Texture2D>("logo/white");
	draw->_white->Create(1, 1, 1, TextureFormat::RGBA8, Wrapping::Repeat, Filter::Nearest,
	                     bgfx::copy(k_White.data(), static_cast<uint32_t>(k_White.size())));

	// The sky: its one submesh's primitives, each vertex at its place and texture point
	const auto path = fileSystem.GetPath<filesystem::Path::Data>() / k_SkyMesh;
	l3d::L3DFile file;
	if (fileSystem.Exists(path) &&
	    file.Open(resources::LoadOptionalBlob(Locator::resources::value().GetBlobs(), fileSystem.FindPath(path),
	                                          "the logo's sky")) == l3d::L3DResult::Success &&
	    !file.GetSubmeshHeaders().empty() && !file.GetSkins().empty())
	{
		Sky sky;
		const auto primitives = file.GetPrimitiveSpan(0);
		const auto vertices = file.GetVertexSpan(0);
		const auto indices = file.GetIndexSpan(0);
		uint32_t firstIndex = 0;
		uint32_t firstVertex = 0;
		for (const auto& primitive : primitives)
		{
			for (uint32_t i = 0; i < primitive.numTriangles * 3 && firstIndex + i < indices.size(); ++i)
			{
				sky.indices.push_back(static_cast<uint16_t>(indices[firstIndex + i] + firstVertex));
			}
			sky.twoSided = (primitive.material.cullMode & 1u) != 0;
			sky.tiling = (primitive.material.cullMode & 4u) != 0;
			firstIndex += primitive.numTriangles * 3;
			firstVertex += primitive.numVertices;
		}
		for (const auto& vertex : vertices)
		{
			sky.vertices.push_back({.position = {vertex.position.x, vertex.position.y, vertex.position.z},
			                        .uv = {vertex.texCoord.x, vertex.texCoord.y},
			                        .abgr = 0xFFFFFFFFu});
		}
		const auto& skin = file.GetSkins().front();
		std::vector<uint16_t> words(skin.texels.size());
		std::memcpy(words.data(), skin.texels.data(), words.size() * sizeof(uint16_t));
		const auto rgba = SkyTexels(words);
		sky.skin = std::make_unique<Texture2D>("logo/sky");
		sky.skin->Create(l3d::L3DTexture::k_Width, l3d::L3DTexture::k_Height, 1, TextureFormat::RGBA8, Wrapping::Repeat,
		                 Filter::Linear, bgfx::copy(rgba.data(), static_cast<uint32_t>(rgba.size())));
		draw->_sky = std::move(sky);
	}
	else
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The logo's sky {} could not be read; it is drawn without it",
		                   path.generic_string());
	}
	return draw;
}

void InLoopLogoDraw::MakeTarget(glm::u16vec2 resolution)
{
	if (_target && _targetSize == resolution)
	{
		return;
	}
	_target = std::make_unique<FrameBuffer>("InLoopLogo", resolution.x, resolution.y, TextureFormat::RGBA8,
	                                        TextureFormat::Depth24Stencil8);
	_targetSize = resolution;
	_cleared = false;
}

float InLoopLogoDraw::NameWidth(std::u16string_view name, float size) const
{
	return _font->GetWidth(name, size);
}

void InLoopLogoDraw::Draw(const Frame& frame)
{
	if (frame.resolution.x == 0 || frame.resolution.y == 0)
	{
		return;
	}
	MakeTarget(frame.resolution);
	const auto& shaders = Locator::rendererInterface::value().GetShaderManager();
	const auto& textures = Locator::resources::value().GetTextures();

	// The camera: the game's field of view, the logo's near clip
	Camera camera(frame.focus);
	const float aspect = static_cast<float>(frame.resolution.x) / static_cast<float>(frame.resolution.y);
	camera.SetProjectionMatrixPerspective(glm::degrees(frame.horizontalFov), aspect, in_loop_logo::k_NearClip, frame.farClip);
	camera.SetOrigin(frame.eye).SetFocus(frame.focus);
	const auto view = camera.GetViewMatrix(Camera::Interpolation::Current);
	const auto& projection = camera.GetProjectionMatrix(Camera::Projection::ReversedZ);

	// Only the depth is cleared, but for the picture's first frame
	const auto world = static_cast<bgfx::ViewId>(k_WorldView);
	_target->Bind(k_WorldView);
	bgfx::setViewRect(world, 0, 0, frame.resolution.x, frame.resolution.y);
	bgfx::setViewMode(world, bgfx::ViewMode::Sequential);
	bgfx::setViewClear(world, _cleared ? BGFX_CLEAR_DEPTH : (BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH), k_Black, 0.0f, 0);
	_cleared = true;
	bgfx::setViewTransform(world, glm::value_ptr(view), glm::value_ptr(projection));
	bgfx::touch(world);

	// The sky at the origin, in its colour through the global alpha table
	if (_sky.has_value())
	{
		const uint32_t abgr = argb_colour::ToAbgr(frame.skyArgb);
		for (auto& vertex : _sky->vertices)
		{
			vertex.abgr = abgr;
		}
		const render_modes::Material material {.mode = render_modes::Mode::Textured,
		                                       .flags = static_cast<uint8_t>((_sky->twoSided ? render_modes::k_TwoSided : 0u) |
		                                                                     (_sky->tiling ? render_modes::k_Tiling : 0u))};
		world_triangles::SubmitRaw(k_WorldView, _sky->vertices, _sky->indices, *_sky->skin, *_white, material, shaders, {},
		                           render_modes::Table::GlobalAlpha);
	}

	// The stars in their order, each run of one material drawn at once
	const auto cameraFrame = FrameOf(camera);
	const auto& starTexture = *textures.Handle(k_StarTexture);
	const auto& starAlpha = *textures.Handle(k_StarAlpha);
	std::vector<world_triangles::Vertex> vertices;
	std::vector<uint16_t> indices;
	const auto submit = [&](bool bright) {
		if (!indices.empty())
		{
			world_triangles::SubmitRaw(k_WorldView, vertices, indices, starTexture, starAlpha,
			                           bright ? k_StarAdded : k_StarBlended, shaders);
		}
		vertices.clear();
		indices.clear();
	};
	bool runBright = false;
	for (const auto& star : frame.stars)
	{
		if (star.bright != runBright)
		{
			submit(runBright);
			runBright = star.bright;
		}
		const billboard::Sprite sprite {.position = star.position,
		                                .size = star.size,
		                                .height = 1.0f,
		                                .angle = star.angle,
		                                .argb = star.argb,
		                                .cell = in_loop_logo::k_StarCell};
		const auto quad = billboard::SpriteQuad(sprite, cameraFrame);
		if (!quad.has_value())
		{
			continue;
		}
		const auto first = static_cast<uint16_t>(vertices.size());
		const uint32_t abgr = argb_colour::ToAbgr(star.argb);
		for (size_t corner = 0; corner < quad->corners.size(); ++corner)
		{
			vertices.push_back({.position = quad->corners.at(corner), .uv = quad->uv.at(corner), .abgr = abgr});
		}
		for (const int corner : billboard::k_SpriteTriangles)
		{
			indices.push_back(static_cast<uint16_t>(first + corner));
		}
	}
	submit(runBright);

	// The name, the trademark sign and the black fade over it all, into the picture
	_target->Bind(k_OverlayView);
	_overlay->Begin(frame.resolution);
	const glm::ivec2 screen(frame.resolution);
	const float nameSize = in_loop_logo::NameSize(screen);
	const auto namePlace = in_loop_logo::NamePlace(screen, NameWidth(frame.name, nameSize));
	const auto nameAlpha = static_cast<float>(static_cast<uint8_t>(frame.nameFade * 255.0f)) / 255.0f;
	AddText(*_overlay, *_font, frame.name, namePlace.position, namePlace.size, glm::vec4(1.0f, 1.0f, 1.0f, nameAlpha));
	if (frame.trademark)
	{
		const auto place = in_loop_logo::TrademarkPlace(screen);
		const glm::vec4 grey(200.0f / 255.0f, 200.0f / 255.0f, 200.0f / 255.0f,
		                     static_cast<float>(frame.trademarkAlpha) / 255.0f);
		AddText(*_overlay, *_font, u"TM", place.position, place.size, grey);
	}
	if (frame.fade > 0)
	{
		_overlay->DrawQuad({0.0f, 0.0f}, glm::vec2(frame.resolution), {0.0f, 0.0f}, {1.0f, 1.0f},
		                   Rgba(static_cast<uint32_t>(frame.fade) << 24u), nullptr);
	}
	_overlay->End();

	// The picture onto the window, as it is
	auto& renderer = Locator::rendererInterface::value();
	renderer.ConfigureView(k_CopyView, frame.resolution, k_Black);
	_copy->Begin(frame.resolution);
	// pixel for pixel: the canvas's half pixel taken back out
	const bool flipped = bgfx::getCaps()->originBottomLeft;
	const glm::vec2 half(gui::Canvas::HalfPixel() ? 0.5f : 0.0f);
	_copy->DrawQuad(-half, glm::vec2(frame.resolution) - half, {0.0f, flipped ? 1.0f : 0.0f}, {1.0f, flipped ? 0.0f : 1.0f},
	                glm::vec4(1.0f), &_target->GetColorAttachment());
	_copy->End();
}

void InLoopLogoDraw::DrawBlack(glm::u16vec2 resolution)
{
	ReleaseViews();
	auto& renderer = Locator::rendererInterface::value();
	renderer.ConfigureView(k_CopyView, resolution, k_Black);
	bgfx::touch(static_cast<bgfx::ViewId>(k_CopyView));
}

void InLoopLogoDraw::ReleaseViews()
{
	for (const auto view : {k_WorldView, k_OverlayView})
	{
		bgfx::setViewFrameBuffer(static_cast<bgfx::ViewId>(view), BGFX_INVALID_HANDLE);
	}
}
