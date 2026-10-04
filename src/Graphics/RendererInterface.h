/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <filesystem>
#include <memory>
#include <optional>
#include <string_view>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

#include "GraphicsHandle.h"
#include "InstanceDesc.h"
#include "RenderModes.h"
#include "RenderPass.h"
#include "SeaPass.h"

#include "../EngineConfig.h"

namespace openblack
{
class Camera;
class Profiler;
class Sky;
class Ocean;
} // namespace openblack

namespace openblack::ecs
{
class Registry;
}

namespace openblack::help
{
enum class TextFont : uint8_t;
}

namespace openblack::graphics
{
class L3DMesh;
class FrameBuffer;
class ShaderManager;
class ShaderProgram;
struct OverlayFrame;

class RendererInterface
{
public:
	struct DrawSceneDesc
	{
		const Camera* camera;
		const graphics::FrameBuffer* frameBuffer;
		const ecs::Registry& entities;
		/// What the end of the frame draws over the scene (Graphics/OverlayFrame.h), filled before the draw
		const graphics::OverlayFrame& overlay;
		uint32_t time;
		float timeOfDay;
		float bumpMapStrength;
		float smallBumpMapStrength;
		graphics::RenderPass viewId;
		bool drawSky;
		bool drawWater;
		bool drawIsland;
		bool drawEntities;
		bool drawSprites;
		bool drawBoundingBoxes;
		bool wireframe;
	};

	struct L3DMeshSubmitDesc
	{
		graphics::RenderPass viewId;
		const graphics::ShaderProgram* program;
		/// What the draw adds to every primitive's mode (render_modes::State): Z func, alpha written, MSAA, a cull for the
		/// whole mesh (the sky, the hand's shadow); without one, each primitive's material culls (+5 bit 0)
		render_modes::StateOptions options;
		/// g_set_render_mode_data [0xECA618]: GlobalAlpha (0xC387C8) for an object drawn with its own alpha
		render_modes::Table table {render_modes::Table::Normal};
		/// with the table 0xC387C8: the object's alpha byte (the diffuse [0xC37D8C] >> 24), the ALPHAREF of modes 9 / 15
		uint8_t globalAlpha {255};
		/// every primitive drawn in this mode instead of its own, alpha test included (the PSys additive atoms: 13,
		/// SetMaterialProperties 0x57E120; the hand's shadow on the objects: 6, fn_0080B050 0x80B06A..0x80B08B)
		std::optional<render_modes::Mode> mode;
		uint32_t rgba;
		const glm::mat4* modelMatrices;
		uint8_t matrixCount;
		std::unique_ptr<const graphics::InstanceDesc> instanceDesc;
		uint32_t instanceStart;
		uint32_t instanceCount;
		bool isSky;
		bool drawAll; ///< For use in the mesh viewer
		bool morphWithTerrain;
		float lightBoost {1.0f}; ///< model colour multiplier (the hand: 1.5, CHand::AddDrawing 0x46D135)
		bool noHaze {false};     ///< no distance haze (the hand: CHand::AddDrawing never calls fn_007FEB30)
		uint8_t landLightMode {0}; ///< land_light::ObjectMode: how the model takes the land light (vs_object)
		uint8_t blendFilter {0}; ///< 0: every primitive, 1: the opaque ones only, 2: the blended ones only
		/// A draw of the pass under the sea (graphics::sea_pass): DrawUnderWater (vt+0x118) in a constant or the last
		/// Draw's colour, DrawCutByPlane (vt+0x11C), the plane kept and whether it is mirrored back
		sea_pass::SeaDraw sea {};
		/// A projected shadow drawn on the object (programs *ShadowInstanced, shadow_list): its texture (si+0x45C), its
		/// box (x0, z0, 1 / (x1 - x0), 1 / (z1 - z0)) and the code 0x400 (d.x, d.z, the least k)
		std::optional<graphics::TextureHandle> dynamicShadow;
		glm::vec4 dynamicShadowBox {0.0f};
		glm::vec4 dynamicShadowCull {0.0f};
	};

	static std::unique_ptr<RendererInterface> Create(GraphicsBackend backend, bool vsync) noexcept;

	virtual ~RendererInterface() noexcept = default;

	virtual void ConfigureView(graphics::RenderPass viewId, glm::u16vec2 resolution, uint32_t clearColor) const noexcept = 0;
	virtual void Reset(glm::u16vec2 resolution) const noexcept = 0;
	/// (openblack engine) a new land: what the draw would load lazily (chimney smoke alpha, the foliage mod)
	virtual void PreloadForLand() const noexcept = 0;
	/// (openblack engine) the frame's writes of the draw (sky type, dome, land light, light, shadows, clouds), on the
	/// logic side, just before DrawScene, which then only reads
	virtual void PreDraw(const DrawSceneDesc& drawDesc) const noexcept = 0;
	virtual void DrawScene(const DrawSceneDesc& drawDesc) const noexcept = 0;
	/// The size of the Main view (ConfigureView), what the overlays are laid out for; 0 x 0 before it.
	/// Called from the logic side (the OverlayFrame filled before the draw): read-only, no bgfx calls
	[[nodiscard]] virtual glm::u16vec2 GetResolution() const noexcept = 0;
	/// GatheringText::GetStringWidth 0x831130 of a HelpText font (f1 / f3 fall back to j0, table 0xECCD08): the width
	/// of the text at that size, 0 without the font. For the layouts made before the draw (Graphics/OverlayFrame.h).
	/// Called from the logic side: read-only, no bgfx calls; the font's first-use load (GameFontAt) must move before
	/// the game or go through gpu::Submit (pending, Motor M3)
	[[nodiscard]] virtual float MeasureText(help::TextFont font, std::u16string_view text,
	                                        float size) const noexcept = 0;
	virtual void Frame() noexcept = 0;
	virtual void RequestScreenshot(const std::filesystem::path& filepath) noexcept = 0;
	[[nodiscard]] virtual bool GetDebug() const noexcept = 0;
	virtual void SetDebug(bool value) noexcept = 0;
	[[nodiscard]] virtual bool GetProfile() const noexcept = 0;
	virtual void SetProfile(bool value) noexcept = 0;

	// TODO: Remove this function. All renderables should be drawn through RenderingSystem with Components
	virtual void DrawMesh(const L3DMesh& mesh, const L3DMeshSubmitDesc& desc, uint8_t subMeshIndex) const noexcept = 0;
	// TODO: Should shader manager be available through Locator as a service?
	[[nodiscard]] virtual graphics::ShaderManager& GetShaderManager() const noexcept = 0;
};

} // namespace openblack::graphics
