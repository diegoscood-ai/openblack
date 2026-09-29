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

#include <array>
#include <chrono>
#include <filesystem>
#include <memory>
#include <string_view>
#include <vector>

#include <SDL.h>
#include <bgfx/bgfx.h>
#include <glm/fwd.hpp>
#include <glm/mat4x4.hpp>

#include "Graphics/RenderPass.h"
#include "Graphics/RendererInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack
{
struct BgfxCallback;
class LandLightTable;
class Clouds;
class Foliage;
class Game;

namespace ecs
{
class Registry;
}

namespace graphics
{
class L3DSubMesh;
class Mesh;

class Renderer final: public RendererInterface
{
	/// Rebuilds the landscape light table for this frame and uploads it (256x1 RGBA8, point sampled)
	void UpdateLandLight() const;
	/// Bakes the static object shadows into the island-wide shadow texture
	void DrawStaticShadowPass(const DrawSceneDesc& drawDesc) const;
	/// The hand's dynamic shadow (CHand, LH3DComplexObject::CreateDynamicShadow): silhouette into a small texture
	void DrawHandShadowPass(const DrawSceneDesc& drawDesc) const;
	/// The sun (fn_0086C140, right after the sky dome) and its glare (fn_0086BB60, at the end of the frame)
	void DrawSun(graphics::RenderPass viewId, const Camera& camera, bool glare) const;
	/// The moon and its glow (LH3DAtmos::UpdateGame 0x8356E0, fn_0086A930, fn_0086A7F0)
	void DrawMoon(graphics::RenderPass viewId, const Camera& camera) const;
	/// The sky clouds (fn_005E25C0 / CloudInSky), back to front in the blended view
	void DrawClouds(graphics::RenderPass viewId, const Camera& camera) const;
	/// The mirrored held object and thrown objects in the reflection (DrawUnderWater, GLandscape::Draw 0x5E4905..)
	void DrawObjectReflections(graphics::RenderPass viewId) const;
	/// The hand's dynamic shadow on the objects under it (the Draw tail loop over ShadowInfo, fn_0080B050)
	void DrawHandShadowOnObjects() const;
	/// The fish farm shoals (fn_00824B90, before the sea): misc0.raw sprites lying on the water, mode 6; drawn
	/// mirrored into the reflection target, which is what shows through the sea here
	void DrawFishShoals(graphics::RenderPass viewId) const;
	/// Mod world.foliage: loads ModAssets/Foliage on first use, places the plants for the island and draws them
	void DrawFoliage(const DrawSceneDesc& desc) const;
	/// The water rings (fn_005E5100, after the landscape): flat smoke.raw sprites, mode 13
	void DrawWaterRings(graphics::RenderPass viewId) const;
	/// The villagers' ground blobs ("human shadow", fn_0081FFF0 / fn_0081FE50)
	void DrawHumanShadows(graphics::RenderPass viewId) const;
	/// FinishFrame (e) and (h): the cinema bars and the screen fade (fn_0081E590, fn_0086FEE0)
	void DrawScreenOverlay() const;
	/// A mesh with the celestial shader: model matrix, texture, colour, render state
	void DrawCelestialMesh(graphics::RenderPass viewId, const L3DMesh& mesh, const glm::mat4& model, const Texture2D& texture,
	                       const glm::vec4& colour, uint64_t state, const glm::vec4& celestial = glm::vec4(0.0f),
	                       const Texture2D* alpha = nullptr) const;

public:
	Renderer(uint32_t bgfxReset, std::unique_ptr<BgfxCallback>&& bgfxCallback) noexcept;
	~Renderer() noexcept final;

	[[nodiscard]] ShaderManager& GetShaderManager() const noexcept final;

	void ConfigureView(RenderPass viewId, glm::u16vec2 resolution, uint32_t clearColor) const noexcept final;

	void DrawScene(const DrawSceneDesc& drawDesc) const noexcept final;
	void DrawMesh(const L3DMesh& mesh, const L3DMeshSubmitDesc& desc, uint8_t subMeshIndex) const noexcept final;
	void Frame() noexcept final;
	void RequestScreenshot(const std::filesystem::path& filepath) noexcept final;
	[[nodiscard]] bool GetDebug() const noexcept final { return _bgfxDebug; }
	void SetDebug(bool value) noexcept final { _bgfxDebug = value; }
	[[nodiscard]] bool GetProfile() const noexcept final { return _bgfxProfile; }
	void SetProfile(bool value) noexcept final { _bgfxProfile = value; }

	void Reset(glm::u16vec2 resolution) const noexcept final;

private:
	void DrawFootprintPass(const DrawSceneDesc& drawDesc) const;
	void DrawSubMesh(const L3DMesh& mesh, const L3DSubMesh& subMesh, const L3DMeshSubmitDesc& desc, bool preserveState) const;
	void DrawPass(const DrawSceneDesc& desc) const;

	std::unique_ptr<ShaderManager> _shaderManager;
	std::unique_ptr<BgfxCallback> _bgfxCallback;
	mutable std::unique_ptr<LandLightTable> _landLight;
	mutable bgfx::TextureHandle _landLightTexture = BGFX_INVALID_HANDLE;
	mutable std::array<glm::vec4, 2> _hazeUniforms {}; ///< u_haze and u_hazeColour of the pass being drawn
	mutable float _sunGlare {0.0f};                     ///< [0xFA2778]: sun glare visibility 0..255, smoothed
	mutable std::unique_ptr<Clouds> _clouds;
	mutable std::unique_ptr<Foliage> _foliage;
	mutable bool _foliageLoadTried {false};
	mutable glm::u16vec2 _resolution {0, 0}; ///< of the main view
	mutable std::unique_ptr<FrameBuffer> _handShadowFrameBuffer;
	mutable glm::vec4 _handShadowBox {0.0f};    ///< xy: box minimum x/z, zw: 1 / size
	mutable glm::vec4 _handShadowParams {0.0f}; ///< x: opacity (max 8/15 x fade), y: ground height
	mutable std::vector<float> _cloudAlpha;          ///< per cloud 0..255 this frame
	mutable std::vector<uint8_t> _cloudShadowImage;  ///< sclouds.raw
	mutable std::vector<uint8_t> _cloudShadowCap;
	mutable bgfx::TextureHandle _cloudShadowTexture = BGFX_INVALID_HANDLE;
	mutable glm::u16vec2 _cloudShadowSize {0, 0};
	/// Moves the clouds, computes their colour / alpha and bakes their shadows into the luminosity cap texture
	void UpdateClouds() const;
	mutable glm::vec3 _cloudRgb {1.0f};
	uint32_t _bgfxReset;
	bool _bgfxDebug = false;
	bool _bgfxProfile = false;
	std::unique_ptr<Mesh> _plane;
};
} // namespace graphics
} // namespace openblack
