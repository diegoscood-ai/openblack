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
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <SDL.h>
#include <bgfx/bgfx.h>
#include <entt/entity/fwd.hpp>
#include <glm/fwd.hpp>
#include <glm/mat4x4.hpp>

#include "3D/Billboard.h"
#include "3D/Clouds.h"
#include "ECS/ChimneySmoke.h"
#include "ECS/Weather/Rain.h"
#include "Graphics/Haze.h"
#include "Graphics/Mists.h"
#include "Graphics/RenderPass.h"
#include "PSys/PSysManager.h"
#include "PSys/Rules/SurfRevol.h"
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
class PhysicsShadows;
class Mesh;
class GameFont;

class Renderer final: public RendererInterface
{
	/// Rebuilds the landscape light table for this frame and uploads it (256x1 RGBA8, point sampled)
	void UpdateLandLight() const;
	/// Bakes the static object shadows into the island-wide shadow texture
	void DrawStaticShadowPass(const DrawSceneDesc& drawDesc) const;
	/// The rivers' footprints (ECS/Rivers): river2.l3d into the footprint target (drawRiverBeds) or river.l3d into the
	/// land alpha target with MIN blending
	void DrawRiverFootprints(bgfx::ViewId viewId, bool channel) const;
	void DrawLandAlphaPass(const DrawSceneDesc& drawDesc) const;
	/// The hand's dynamic shadow (CHand, LH3DComplexObject::CreateDynamicShadow): silhouette into a small texture
	void DrawHandShadowPass(const DrawSceneDesc& drawDesc) const;
	/// One particle effect's sprites, in the back-to-front list (RendererPSys.cpp)
	void DrawPSysEffect(const psys::manager::Drawable& effect, const Camera& camera, RenderPass viewId) const;
	/// The chain ribbons of the particle effects (lightning forks, gesture trail; fn_0067B3F0, RendererChain.cpp), each
	/// with the Z-sorter key of the effect that owns it: the original draws the ribbon inside that effect's single Z
	/// object (fn_006798B0 0x6798DD -> fn_0067B370 "draw now"), so it shares its key
	std::vector<std::pair<float, uint32_t>> CollectPSysChains(const Camera& camera) const;
	/// One ribbon of _frameChains, from the back-to-front list
	void DrawPSysChain(RenderPass viewId, const Camera& camera, uint32_t index) const;
	/// The ribbons of this frame, filled by CollectPSysChains
	mutable std::vector<psys::Effect::DrawChain> _frameChains;
	/// The effects' surfaces of revolution (ZR_SurfRevol: the teleport pool, the dispensers' discs;
	/// RendererSurfRevol.cpp), each with the Z-sorter key of the effect it belongs to: the surface is one of its atoms,
	/// drawn inside the effect's single Z object (PSysManager::AddDrawing 0x6797D0 -> 0x67CBA0)
	std::vector<std::pair<float, uint32_t>> CollectPSysSurfaces(const Camera& camera) const;
	/// One surface of _frameSurfaces, from the back-to-front list
	void DrawPSysSurface(RenderPass viewId, uint32_t index) const;
	/// The surfaces of this frame, filled by CollectPSysSurfaces
	mutable std::vector<psys::surf_revol::Surface> _frameSurfaces;
	/// LH3DAtmos::Render3D 0x836250: the raining tiles (weather::rain::CollectTiles), each one Z object of its own
	/// (fn_008341B0, NewZObject call 0x83427F) with its Z-sorter key and its index in _frameRain (RendererRain.cpp)
	std::vector<std::pair<float, uint32_t>> CollectRain(const Camera& camera) const;
	/// One tile of _frameRain, as the Z-sorter's callback 0x833F80 (fn_00834370): its streaks
	void DrawRainTile(RenderPass viewId, uint32_t index) const;
	/// The tiles of this frame, filled by CollectRain
	mutable std::vector<weather::rain::Tile> _frameRain;
	/// The sun (fn_0086C140, right after the sky dome) and its glare (fn_0086BB60, at the end of the frame)
	void DrawSun(graphics::RenderPass viewId, const Camera& camera, bool glare) const;
	/// The moon and its glow (LH3DAtmos::UpdateGame 0x8356E0, fn_0086A930, fn_0086A7F0)
	/// @param mirrored in the reflection pass: the mirrored glow and the moon's DrawUnderWater (fn_0086B010 0x86B61D)
	void DrawMoon(graphics::RenderPass viewId, const Camera& camera, bool mirrored = false) const;
	/// The sea: the screen rows of fn_00879930, or the level-0 quad of fn_0087A090 (RendererSea.cpp)
	void DrawSea(const DrawSceneDesc& desc) const;
	/// The reflection target follows the main view's size (RendererSea.cpp)
	void UpdateReflectionTarget() const;
	/// The hand's glow on the water at night (0x5E4D89, fn_005E3F70), into the reflection target (RendererSea.cpp)
	void DrawHandWaterGlow(graphics::RenderPass viewId) const;
	/// The sky clouds (fn_005E25C0 / CloudInSky) are LH3DMists: 0x5E2813 calls their vt+0x100, LH3DMist::AddDrawing
	/// 0x7FA7F0, which queues the ones on screen (NewZObject call 0x7FA87B). Their Z-sorter keys and indices, and the
	/// counters of those advanced
	std::vector<std::pair<float, uint32_t>> CollectClouds(const Camera& camera) const;
	/// One cloud, as the Z-sorter's callback 0x7FA980 (LH3DMist::Draw fn_007FA300, effect branch)
	void DrawCloud(graphics::RenderPass viewId, const Camera& camera, uint32_t index) const;
	/// LH3DMist::AddDrawing 0x7FA7F0: the mists on screen (the map's and the ones of mists::Submit) with their Z-sorter
	/// key and their index in _frameMists; the map mists' counters advanced (once per frame)
	std::vector<std::pair<float, uint32_t>> CollectMists(const Camera& camera) const;
	/// One mist of _frameMists, as the Z-sorter's callback 0x7FA980 (fn_007FA300)
	void DrawMist(graphics::RenderPass viewId, const Camera& camera, uint32_t index) const;
	/// The mists of this frame, filled by CollectMists
	mutable std::vector<mists::MistDesc> _frameMists;
	/// LH3DSmoke::AddDrawing 0x7F8D30 for every Abode with a chimney on screen (Abode::Draw 0x516288): the smoke's
	/// state and puffs advanced (fn_007F8E00 simulates while it draws), its Z-sorter key and its index in _frameSmoke
	/// (RendererSmoke.cpp)
	std::vector<std::pair<float, uint32_t>> CollectChimneySmoke(const Camera& camera) const;
	/// One smoke of _frameSmoke: its visible puffs in their order 0..9 (LH3DSprite::Draw 0x840530, material g_smoke_mat)
	void DrawChimneySmoke(graphics::RenderPass viewId, const Camera& camera, uint32_t index) const;
	/// The puffs of every smoke of this frame, filled by CollectChimneySmoke
	mutable std::vector<std::vector<ecs::chimney_smoke::DrawnPuff>> _frameSmoke;
	/// The mirrored held object and thrown objects in the reflection (DrawUnderWater, GLandscape::Draw 0x5E4905..)
	void DrawObjectReflections(graphics::RenderPass viewId) const;
	/// The missionaries' boat hull in the reflection (PetitNavire::PreDraw 0x5DFF20: DrawUnderWater in 0xFF303070), and
	/// the boat's sprites: the wake and the SmokyStuff puffs, smoke material mode 6 (RendererBoat.cpp)
	void DrawBoatReflection(graphics::RenderPass viewId) const;
	/// Each of the boat's LH3DSprites goes to the Z-sorter on its own (LH3DSprite::AddDrawing 0x840C70, NewZObject call
	/// 0x840CB3; the wake from PetitNavire::PostDraw 0x5E08D0, the puffs from fn_00823F70 0x82411A): their keys and
	/// indices in _frameBoatSprites
	std::vector<std::pair<float, uint32_t>> CollectBoatSprites(const Camera& camera) const;
	/// One sprite of _frameBoatSprites, as the Z-sorter's callback LH3DSprite::Draw 0x840530
	void DrawBoatSprite(graphics::RenderPass viewId, uint32_t index) const;
	/// A boat sprite of this frame: its quad (none at or before the near plane, 0x840585) and its colour
	struct BoatSpriteDraw
	{
		std::optional<billboard::Quad> quad;
		uint32_t argb {0};
	};
	mutable std::vector<BoatSpriteDraw> _frameBoatSprites;
	/// DrawCutByPlane (animated fn_00811C70, static fn_0080C050) of an entity's model: keep -1 the part under y = 0, 1
	/// the part over it; lit 90 + N.L in argb (0xAARRGGBB); mirrored in y = 0 for the reflection target (RendererCut.cpp)
	void DrawCutByPlane(graphics::RenderPass viewId, entt::entity entity, int8_t keep, uint32_t argb, bool mirrored) const;
	/// The parts under the water of the objects with components::CutByPlane, before the sea (GLandscape::Draw 4d-4e)
	void DrawCutBelowWater(graphics::RenderPass viewId) const;
	/// The parts above the water of the objects whose owner draws them cut (components::CutByPlane::drawAbove: the
	/// sharks' Whale::Draw 0x774E10), in the colour of the land light table[255]; the normal pass skips those instances
	void DrawCutAboveWater(graphics::RenderPass viewId) const;
	/// the instance indices (RenderContext::entityInstances) that DrawCutAboveWater draws instead of the normal pass
	[[nodiscard]] std::unordered_set<uint32_t> CutAboveInstances() const;
	/// The hand's dynamic shadow on the objects under it (the Draw tail loop over ShadowInfo, fn_0080B050)
	void DrawHandShadowOnObjects() const;
	/// The fish farm shoals (fn_00824B90, before the sea): misc0.raw sprites lying on the water, mode 6; drawn
	/// mirrored into the reflection target, which is what shows through the sea here
	void DrawFishShoals(graphics::RenderPass viewId) const;
	/// The fish puzzle's nets of floats (FishPlot), cut by the plane: keep -1 the part under the water (fn_00829BC0, into
	/// the reflection target, mirrored), 1 the part over it (fn_00829B50) (RendererFishPlot.cpp)
	void DrawFishPlots(graphics::RenderPass viewId, int8_t keep) const;
	/// Mod world.foliage: loads Mods/world.foliage on first use, places the plants for the island and draws them
	void DrawFoliage(const DrawSceneDesc& desc) const;
	/// The water rings (fn_005E5100, after the landscape): flat smoke.raw sprites, mode 13
	void DrawWaterRings(graphics::RenderPass viewId) const;
	/// The villagers' ground blobs ("human shadow", fn_0081FFF0 / fn_0081FE50)
	void DrawHumanShadows(graphics::RenderPass viewId) const;
	/// FinishFrame (e) and (h): the cinema bars and the screen fade (fn_0081E590, fn_0086FEE0)
	void DrawScreenOverlay() const;
	/// HelpSystem::Draw3D -> CameraHelp::DrawKeyOrMouse 0x447EA0: the tooltip next to the hand (the amount in the hand)
	void DrawHandToolTip(const Camera& camera) const;
	mutable std::unique_ptr<GameFont> _font; ///< Data\j0, font 0 of the tooltips
	mutable bool _fontLoadTried {false};
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
	/// The *Static program (one model matrix) for the object programs, used for meshes without bones
	[[nodiscard]] const ShaderProgram* StaticVariant(const ShaderProgram* program) const;
	mutable std::unordered_map<const ShaderProgram*, const ShaderProgram*> _staticVariants;
	/// The variant with 32 bones of an object program, for the posed villagers and animals (drawn one by one)
	[[nodiscard]] const ShaderProgram* BonesVariant32(const ShaderProgram* program) const;
	mutable std::unordered_map<const ShaderProgram*, const ShaderProgram*> _bonesVariants32;
	void DrawPass(const DrawSceneDesc& desc) const;

	std::unique_ptr<ShaderManager> _shaderManager;
	std::unique_ptr<BgfxCallback> _bgfxCallback;
	mutable std::unique_ptr<LandLightTable> _landLight;
	mutable bgfx::TextureHandle _landLightTexture = BGFX_INVALID_HANDLE;
	mutable std::array<glm::vec4, 2> _hazeUniforms {}; ///< u_haze and u_hazeColour of the pass being drawn
	mutable haze::Params _haze;                         ///< the haze of the pass being drawn (graphics::haze::Frame)
	mutable float _sunGlare {0.0f};                     ///< [0xFA2778]: sun glare visibility 0..255, smoothed
	mutable std::unique_ptr<Clouds> _clouds;
	mutable std::unique_ptr<Foliage> _foliage;
	mutable std::string _foliageLoadKey; ///< what _foliage was loaded with (its modules), empty: not tried yet
	mutable glm::u16vec2 _resolution {0, 0}; ///< of the main view
	mutable std::unique_ptr<FrameBuffer> _handShadowFrameBuffer;
	/// The physics objects' shadows on the land (fn_007FCE80)
	std::unique_ptr<PhysicsShadows> _physicsShadows;
	mutable glm::vec4 _handShadowBox {0.0f};    ///< xy: box minimum x/z, zw: 1 / size
	mutable glm::vec4 _handShadowParams {0.0f}; ///< x: opacity (max 8/15 x fade), y: ground height
	mutable std::vector<float> _cloudAlpha;          ///< per cloud 0..255 this frame
	mutable std::vector<uint8_t> _cloudShadowImage;  ///< sclouds.raw
	/// This frame's land cells (land_light::Texels: the stamps and the night lights in them), the cell map's layout
	mutable bgfx::TextureHandle _landCellsTexture = BGFX_INVALID_HANDLE;
	/// The instances of the fish puzzle nets' floats (RendererFishPlot.cpp), made on first use
	mutable bgfx::DynamicVertexBufferHandle _fishPlotInstances = BGFX_INVALID_HANDLE;
	mutable uint32_t _fishPlotCapacity {0};
	mutable glm::u16vec2 _landCellsSize {0, 0};
	/// Moves the clouds and computes their colour / alpha; then this frame's land cells (land_light): the stamps of
	/// fn_0086D360 with the clouds' shadows among them, the night lights, uploaded to _landCellsTexture
	void UpdateClouds() const;
	mutable glm::vec3 _cloudRgb {1.0f};
	mutable SkyAlignment _skyAlignment;       ///< [0xBF3378], moved towards the target every frame
	mutable uint32_t _cloudsGeneration {0};  ///< Clouds::GetLandscapeGeneration of _clouds
	mutable float _cloudMilliseconds {0.0f}; ///< this frame's game time step for the clouds' animation counters
	uint32_t _bgfxReset;
	bool _bgfxDebug = false;
	bool _bgfxProfile = false;
	std::unique_ptr<Mesh> _plane;
};
} // namespace graphics
} // namespace openblack
