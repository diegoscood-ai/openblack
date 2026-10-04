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
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/Billboard.h"
#include "3D/LandLight.h"
#include "Camera/ScreenPoint.h"
#include "Help/CAnim.h"
#include "Help/HelpDudeFile.h"
#include "Help/Spirits.h"

// The two advisor spirits wired to the engine (spec_spirits.md P4, spec_spirits_motion.md sections 3, 4 and 6): the
// models of Data\HelpSprite\markgood.hd / markevil.hd (HelpDude::Load 0x5C2194, Init 0x5C2A4E), the HelpDudeControl of
// Help/Spirits.h with its Queries answered by the camera, the land, audio::advisor and game_random, and, once a frame,
// the anim layers turned into the skinning matrices (FinishAnimStack 0x5C0610) plus what fn_005C0700 and the trail
// callback fn_005C3850 draw, written once a frame into the frame's graphics::OverlayFrame before the draw (FillOverlay;
// Graphics/RendererSpirits.cpp draws only that copy). HelpSystem owns the control in the original (+0x10); here it is
// this module's, started next to help::Start.
namespace openblack::graphics
{
class L3DMesh;
struct OverlayFrame;
struct SpiritOverlay;
} // namespace openblack::graphics

namespace openblack::help::spirits
{

/// One .hd and what is built from it
struct DudeAssets
{
	HelpDudeFile file;
	RestSkeleton rest;
	DudeData data;
	bool loaded {false};
	/// +0x2ECC..+0x2EE4 (the 0x1C bytes of the .hd): eye bones 0..2 (+0x2ECC), 3..5 (+0x2ED8), head +0x2EE4
	std::array<uint32_t, 7> faceBones {};
	/// The LH3DMesh of the embedded L3D0 (LH3DMesh::Create 0x5C0FB7), made on the first FillOverlay; shared with the
	/// frame's OverlayFrame so the draw keeps it alive
	std::shared_ptr<const graphics::L3DMesh> mesh;
	bool meshTried {false};
};

/// The rainbow trail of a dude (HelpDude +0x28BC, fn_005B8F00 / fn_005B90C0)
struct Trail
{
	static constexpr size_t k_Points = 32;
	std::array<glm::vec3, k_Points> ring {}; ///< (hx, hy, depth channel), +0x348 the head
	size_t head {0};
	float accumulator {0.0f};                ///< +0x344
	uint32_t resets {0};                     ///< the logic's TrailResets() last seen
};

/// One vertex of the trail (Draw3DWorldTriangle 0x81C090 in the world, ARGB)
struct TrailVertex
{
	glm::vec3 position {0.0f};
	glm::vec2 uv {0.0f};
	uint32_t argb {0};
};

class Runtime
{
public:
	/// fn_005C2A40 Init: both .hd through Locator::filesystem (Data\HelpSprite), then the control (both dudes at home)
	Runtime();
	~Runtime();
	Runtime(const Runtime&) = delete;
	Runtime& operator=(const Runtime&) = delete;

	[[nodiscard]] HelpDudeControl& Control() { return *_control; }
	[[nodiscard]] const HelpDudeControl& Control() const { return *_control; }
	[[nodiscard]] bool Loaded(int dude) const { return _assets.at(static_cast<size_t>(dude)).loaded; }

	/// HelpSystem::Draw3D 0x5C59A0 -> HelpDudeControl::Process 0x5C3A30 (g_delta_time x 0.001, 0.4), once a frame;
	/// then the pose of each dude (FinishAnimStack 0x5C0610) and the trail ring (the puff particles are the logic's,
	/// HelpDude::PuffParticles)
	void Update();
	/// HelpSpirit::Process 0x5C5270 of both spirits, once a game turn
	void ProcessTurn();

	/// The LH3DMesh of a dude, made on first use (it needs the renderer); nullptr without the .hd
	[[nodiscard]] std::shared_ptr<const graphics::L3DMesh> Mesh(int dude);
	/// Motor M2's PreDraw (Game.cpp FillOverlayFrame, once a frame before DrawScene): the frame's spirits, Draws(false)
	/// then Draws(true), and the trails, copied into `frame` (its spirits and spiritTrails replaced)
	void FillOverlay(graphics::OverlayFrame& frame);
	/// fn_005C3850 -> fn_005B90C0 for each dude out of home and not in the world (+0x35DC == 0): 64 vertices each, in
	/// the order of the index list 0xD15788 (three per triangle)
	[[nodiscard]] std::vector<TrailVertex> TrailTriangles() const;

	/// The camera of the last Update (LH3DTech's globals are the last UpdateCamera's)
	[[nodiscard]] const graphics::billboard::CameraFrame& Frame() const { return _frame; }

	/// SetColorSpecular of Update1 step 9 (0x5BE9B6..0x5BEA8F): sb = smooth(+0x35DC), w = ftol(255 sb); colour = white
	/// lerped to the land diffuse by w (land_light::LerpBytes), specular = the land specular x w >> 8; sb 0: (white, 0).
	/// Pure: the Renderer applies it with its land light table (graphics::SpiritOverlay::landLightPoint)
	[[nodiscard]] static std::pair<uint32_t, uint32_t> WorldColour(const land_light::Sample& sample, float blend);

private:
	/// fn_005C0700 for the dudes the original draws this frame, appended to `out`: `overlay` the FinishFrame callback
	/// fn_005C3920 (in-world blend < 0.5), else Draw3D 0x5C5B26 (>= 0.5). Advances the halo clock [0xD15AB0]
	void Draws(bool overlay, std::vector<graphics::SpiritOverlay>& out);
	/// The anim layers 0..count of a dude applied to its rest locals, then composed under `model`
	void EvaluatePose(int dude, size_t count, const glm::mat4& model, std::vector<glm::mat4>& world) const;
	/// CalcHeadPos 0x5BFE00
	[[nodiscard]] std::optional<glm::vec2> HeadAngles(int dude, const glm::mat3& rows, const glm::vec3& position,
	                                                  const glm::vec3& target) const;
	/// The fingertip of Update1 step 16 (0x5BF1DC..0x5BF2FB)
	[[nodiscard]] glm::vec3 Fingertip(int dude, const glm::mat3& rows, const glm::vec3& position) const;
	Queries MakeQueries();
	void RefreshView();
	void UpdateTrail(int dude, uint32_t deltaMs);

	std::array<DudeAssets, k_Dudes> _assets;
	std::unique_ptr<HelpDudeControl> _control;
	std::array<std::vector<glm::mat4>, k_Dudes> _bones;
	std::array<Trail, k_Dudes> _trails {};
	int32_t _haloClockMs {0}; ///< [0xD15AB0], one for both
	graphics::billboard::CameraFrame _frame;
	screen_point::Lens _lens {320.0f, 240.0f, 1.0f, 4.0f / 3.0f};
	Screen _screen {};
};

/// The control of the running game (HelpSystem+0x10), nullptr before Start / after Shutdown
[[nodiscard]] Runtime* Get();
void Start();
void Shutdown();

/// The model matrix of the rows (+0x3350, row i in [i]) and the position (+0x3374): glm's columns are LH's rows
[[nodiscard]] glm::mat4 ModelOf(const glm::mat3& rows, const glm::vec3& position);

} // namespace openblack::help::spirits
