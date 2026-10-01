/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <filesystem>
#include <memory>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

#include "InstanceDesc.h"
#include "RenderPass.h"

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

namespace openblack::graphics
{
class L3DMesh;
class FrameBuffer;
class ShaderManager;
class ShaderProgram;

class RendererInterface
{
public:
	struct DrawSceneDesc
	{
		const Camera* camera;
		const graphics::FrameBuffer* frameBuffer;
		const ecs::Registry& entities;
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
		bool cullBack;
		bool wireframe;
	};

	struct L3DMeshSubmitDesc
	{
		graphics::RenderPass viewId;
		const graphics::ShaderProgram* program;
		uint64_t state;
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
		uint8_t blendFilter {0}; ///< 0: every primitive, 1: the opaque ones only, 2: the blended ones only
		float unlitColour {-1.0f}; ///< >= 0: unlit grey instead of the land light (reflections)
		bool landColourOnly {false}; ///< the land light colour and specular without vertex lighting or haze (reflections)
		bool clipBelowSea {false}; ///< discard the fragments below y = 0 (reflections: only the part above the water)
		/// DrawCutByPlane (animated fn_00811C70, static fn_0080C050): -1 keeps y <= 0 (the plane (0, -1, 0, 0), the part
		/// under the water), 1 keeps y >= 0 ((0, 1, 0, 0)); lit per vertex 90 + N.L in cutColour (fn_00858BA0)
		int8_t cutByPlane {0};
		uint32_t cutColour {0xFFFFFFFFu}; ///< 0xAARRGGBB, the colour of SetColorSpecular (obj+0x4C) for cutByPlane
		bool mirrorInSea {false}; ///< drawn mirrored in y = 0 (the parts under the water, into the reflection target)
		/// The dynamic shadow drawn on the object (programs *ShadowInstanced): texture, box and opacity
		const graphics::Texture2D* dynamicShadow {nullptr};
		glm::vec4 dynamicShadowBox {0.0f};
		glm::vec4 dynamicShadowParams {0.0f};
	};

	static std::unique_ptr<RendererInterface> Create(GraphicsBackend backend, bool vsync) noexcept;

	virtual ~RendererInterface() noexcept = default;

	virtual void ConfigureView(graphics::RenderPass viewId, glm::u16vec2 resolution, uint32_t clearColor) const noexcept = 0;
	virtual void Reset(glm::u16vec2 resolution) const noexcept = 0;
	virtual void DrawScene(const DrawSceneDesc& drawDesc) const noexcept = 0;
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
