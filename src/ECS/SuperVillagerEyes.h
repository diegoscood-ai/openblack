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
#include <functional>
#include <span>
#include <string>
#include <vector>

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>

/// The mesh eyes of the intro's SuperVillagers (ECS/SuperVillager.h; research dev\documentacion\intro\spec_hd_models.md,
/// sections 3 and 8): an eyeball sphere and four eyelids from Data\MISC\Eyes, put on bone 8 of the HD mesh through its
/// EBone block, with a random blink, a squint and a glance shared by every SuperVillager. The original's object is
/// `Eyes` (0x54 bytes): ctor fn_00884100, update and draw fn_00883560, lids fn_00883EA0, dtor fn_00884570.
///
/// The pure part (timers, matrices, shade, the L3D byte patches) has no Locator and is tested in
/// test/test_super_villager_eyes.cpp; the entities part draws them as six mesh entities.
namespace openblack::ecs::components
{
/// One of the eye objects: Transform + Mesh + ObjectColour (+ UvScroll for the eyeballs), moved every frame
struct SuperVillagerEye
{
	entt::entity owner {entt::null};
	/// Eyes +0x3C.. slot: 0 r_up, 1 r_down, 2 l_up, 3 l_down; the eyeball (+0x4C) is one object drawn twice by the original
	/// (0x883B22 eye 0, 0x883E6B eye 1): two entities here, 4 right and 5 left
	uint8_t slot {0};
	entt::id_type mesh {0};
};
} // namespace openblack::ecs::components

namespace openblack::ecs::super_villager::eyes
{

/// Eyes +0x20..+0x38, one per SuperVillager
struct Timers
{
	float glance {0.0f};      ///< +0x20, the eyeballs' yaw (radians)
	bool blinking {false};    ///< +0x24
	int32_t field28 {0};      ///< +0x28, zeroed when a blink ends (0x8835AA); no reader found
	int32_t untilBlinkMs {0}; ///< +0x2C, 0 at creation: the first frame with time blinks
	float closure {0.0f};     ///< +0x30, the lids, 0 open .. 1 shut
	int32_t blinkMs {0};      ///< +0x34
	int32_t holdMs {200};     ///< +0x38, 200 at creation (0x884153)
};

/// The globals every Eyes reads and moves (one step per Eyes per frame)
struct Shared
{
	float glanceTarget {0.0f}; ///< [0xFAA7E8]
	float squint {0.0f};       ///< [0xFAA7EC]
	uint32_t squintMs {0};     ///< [0xFAA7F0]
};

/// ?Random@@YAMMM@Z 0x81D180 (game_random::crt::Random); the tests give their own
using RandomFn = std::function<float(float, float)>;

/// fn_00883560 0x88356A..0x883712, before the matrices: closure = 0; the blink (Random(100, 200) when one ends,
/// Random(1000, 5000) when one starts), the glance towards the shared target at 0.7 rad/s (a new Random(-0.25, 0.25)
/// target when it is there), the shared squint (Random(0, 0.3) every 300 ms of this counter) as the closure's floor
void Step(Timers& timers, Shared& shared, int32_t milliseconds, const RandomFn& random);

/// The 180-degree matrix of 0x883758..0x883802: c = cos(pi), s = sin(pi) of the double 3.1415927410125732, rows (-c, 0,
/// -s), (0, 1, 0), (-s, 0, c) = (1, 0, -s), (0, 1, 0), (-s, 0, -1): a mirror of z (det -1), not a turn
[[nodiscard]] glm::mat4 Mirror();
/// An EBone block's matrix (L3DEBone::matrices[k]: 3 rows then the position) as glm (its rows are glm's columns)
[[nodiscard]] glm::mat4 EBoneMatrix(const std::array<float, 12>& cells);
/// The eye k matrix of 0x883712..0x883802 (0x883BD6..0x883C07 for k = 1): v R E_k bone in rows, the bone in the world
/// (the original's camera-space bone x [0xEA9DE0], the camera to world) = boneWorld E_k R in glm
[[nodiscard]] glm::mat4 EyeMatrix(const glm::mat4& boneWorld, const std::array<float, 12>& eBoneCells);
/// 0x883A98..0x883B1A: the eyeball turned by the glance about its own Y (rows 0 and 2, lh_matrix::RotateY), when the
/// glance is not 0; c stored as a float (0x883AB5, eye 1 0x883DF6), s on the FPU stack
[[nodiscard]] glm::mat4 Glance(const glm::mat4& eye, float glance);
/// fn_00883EA0: rows 1 and 2 turned by closure x 0.47 ([0x8C7A4C], upper) or closure x -0.35 ([0x9A3D80], lower):
/// r1' = c r1 - s r2, r2' = c r2 + s r1 (lh_matrix::RotateX), c stored as a float (0x883ECF / 0x883F72), s on the stack
[[nodiscard]] glm::mat4 Lid(const glm::mat4& eye, float closure, bool upper);
/// 0x883865..0x8839CE (eye 1: 0x883C0C..0x883D5B): the table 0xC3A1D8 (type x 16 + eye x 8) gives the yaw a and pitch b
/// of the eye's lighting normal n = (sin a cos b, -sin b, -cos a cos b); n M normalised (InverseSquareRoot 0x841170, the
/// exe's sum orders, which differ between the two eyes), dot the normalised default sun [0xEA1C88], I = fistp(255 dot)
/// (model_light::Intensity)
[[nodiscard]] int ShadeIntensity(int type, int eye, const glm::mat3& eyeRows);
/// Eyes ctor 0x88446B: the iris cell of misc0, u = k / 8 with k = {4, 3, 5}[type] ([0xC3A208]), v = 0.75
[[nodiscard]] float IrisU(int type);
/// The file of slot 0..4 (r_up, r_down, l_up, l_down, eye_ball) for an eye type (0x88415F..0x884393): types 0 and 1 the
/// same files, type 2 the "*2" ones. No extension
[[nodiscard]] std::string File(int type, int slot);

/// In-memory edits of an L3D file (the layout of components/l3d L3DFile: header dwords 3 / 4 submeshes, 14 / 15 skins;
/// submesh +4 / +8 the primitives' offsets; primitive +8 the material's skin id, +16 / +20 the vertices; vertex 32 bytes,
/// the uv at +12). False when the file does not have what is asked
namespace l3d_patch
{
/// The first material's texture: primitive 0 of submesh 0 (the original's mesh +0x10 [0] +8 [0] +8, 0x88440B..0x884415)
[[nodiscard]] bool FirstSkinId(const std::vector<uint8_t>& file, uint32_t& skinId);
bool SetFirstSkinId(std::vector<uint8_t>& file, uint32_t skinId);
/// 0x8844D9..0x884533: every vertex uv of every primitive doubled (fadd st, st)
bool DoubleUvs(std::vector<uint8_t>& file);
/// The embedded skin with that id (its id dword and 256 x 256 texels), empty when it has none
[[nodiscard]] std::vector<uint8_t> FindSkin(const std::vector<uint8_t>& file, uint32_t skinId);
/// The file gets that skin as its only one, appended after its data (the skins of the HD files also come after the size
/// the header gives); the other offsets are absolute and stay
bool AppendSkin(std::vector<uint8_t>& file, std::span<const uint8_t> skin);
} // namespace l3d_patch

/// What an Eyes needs of its host's HD file (ECS/SuperVillager.cpp loads it)
struct HostModel
{
	std::string name;                       ///< "nors_man", for the lids' mesh ids
	std::array<std::array<float, 12>, 2> matrices {}; ///< EBone matrices 0 and 1
	std::array<int32_t, 2> bones {};        ///< EBone +0x304 / +0x308
	uint32_t skinId {0};                    ///< its first material's texture
	std::vector<uint8_t> skin;              ///< that skin's bytes, for the lids
};

/// The six eye entities of one Eyes (slots 0..3 the lids, 4 / 5 the eyeballs)
using Objects = std::array<entt::entity, 6>;

/// One SuperVillager's eyes (SuperVillager +0x1C)
struct Eyes
{
	int32_t type {0}; ///< +0: 0 man, 1 woman, 2 boy
	std::array<std::array<float, 12>, 2> matrices {};
	std::array<int32_t, 2> bones {};
	Timers timers;
	Objects objects {entt::null, entt::null, entt::null, entt::null, entt::null, entt::null};
};

/// fn_00884100: the six eye entities (meshes loaded once into the mesh manager: the eyeball with the misc0 texture, the
/// lids with the host's skin and doubled uv)
[[nodiscard]] Eyes Create(entt::entity host, int32_t type, const HostModel& model);
/// The same with the five meshes given (slots 0..4; 0: none): the entities only, no loading
[[nodiscard]] Eyes Create(entt::entity host, int32_t type, const HostModel& model, const std::array<entt::id_type, 5>& meshes);
/// fn_00884570: the entities go (whatever the host's state: ECS/SuperVillager keeps their ids in its list entry, so that
/// a host destroyed while it is a SuperVillager does not leave them drawn)
void Destroy(Objects& objects);
/// Not drawn this frame (no Mesh): the host has no mesh or no pose, or fn_00825400's CheckRegionOnScreen failed
void Hide(const Eyes& eyes);
/// fn_00883560 for one SuperVillager on screen: Step, then each eye's matrix from the host's body matrix
/// (ecs::DrawnBodyModel, the turned copy) and drawn pose (ecs::DrawnPose, the cross-fade), its shade (the host's land
/// light +0x4C / +0x50 x the eye's own intensity) and the eyeball and lids moved. Hidden while the host has no mesh or
/// no pose
void Update(Eyes& eyes, Shared& shared, entt::entity host, int32_t milliseconds);

} // namespace openblack::ecs::super_villager::eyes
