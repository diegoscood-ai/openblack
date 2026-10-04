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
#include <bit>
#include <functional>
#include <optional>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "3D/Billboard.h"

namespace openblack
{
class L3DAnim;
}

namespace openblack::graphics
{
class L3DMesh;
struct IntroLightOverlay;
} // namespace openblack::graphics

/// JCMisc.cpp's `Intro` (research dev\documentacion\intro\spec_jc_specials.md): PLAY_JC_SPECIAL 0, 1, 2, 4, 5
/// (fn_005DF9C0), its per-frame update fn_005DF640 (fn_005E5CD0 0x5E6241), the grip of the intro hand
/// fn_005DFCE0 (GLandscape::Draw 0x5E4B35) and Intro::ReleaseAll 0x5DFC40. Special 6 (PetitNavire) is ecs/PetitNavire.h.
///
/// In FollowUs (Land 1): 0 makes the light that falls from the sky onto the Son ("the light", fn_00827F20, 20
/// LH3DSprites of misc0.raw in mode 13), 1 puts the drawn camera behind it (LH3DTech's debug camera mode 2), 2 gives the
/// camera back 4.5 s later. When the light lands the intro hand appears at the Son's spot and plays
/// Data\MISC\hand_intro2.anm ("Hand_Pick_Up_Swimmer", 766 ms, one shot); then 4 puts it at the boat with
/// Data\MISC\hand_intro.anm ("Hand_Put_Down_Swimmer", 3066 ms, looping), 5 plays it with the Son in its grip (the
/// SuperVillager's feature 7) and the second 5 jumps to 2379 ms and lets the Son go; at the end of the clip the hand goes.
namespace openblack::ecs::intro_special
{

/// [0xD19A28] (initialiser 0x5DF620): the Son's spot of FollowUs, where the light lands and the pick-up hand stands
inline constexpr glm::vec3 k_SonSpot {std::bit_cast<float>(0x44B10AC9u), 0.0f, std::bit_cast<float>(0x4500580Cu)};
/// [0xD19A38] (initialiser 0x5DF5F0): the put-down hand (special 4)
inline constexpr glm::vec3 k_HandSpot {std::bit_cast<float>(0x44BACC21u), std::bit_cast<float>(0xC0200000u),
                                       std::bit_cast<float>(0x4502B795u)};
/// [0xD19A18] (fn_005DF640 0x5DF75B..0x5DF76F, once): the pick-up hand in state 12, 1.5 below the Son's spot
inline constexpr glm::vec3 k_PickUpSpot {std::bit_cast<float>(0x44B10AC9u), std::bit_cast<float>(0xBFC00000u),
                                         std::bit_cast<float>(0x4500580Cu)};
/// fn_005DF9C0 0x5DFA3F..0x5DFA4F: the light's direction before it is normalised
inline constexpr glm::vec3 k_LightDirection {std::bit_cast<float>(0xBF800000u), std::bit_cast<float>(0xBE75C28Fu),
                                             std::bit_cast<float>(0xBF800000u)};
inline constexpr float k_HandScale = std::bit_cast<float>(0x3C03126Fu); ///< [0xBF2AF4] 0.008 (and [0xD19A14])
inline constexpr float k_PickUpYaw = std::bit_cast<float>(0x40490FDBu); ///< [0xBF2B00] pi (0x5DF830)
inline constexpr int32_t k_SeekMs = 0x94B;                              ///< 2379, the second special 5 (0x5DFBCE)
inline constexpr float k_GripLastMs = 1817.0f;                          ///< [0xBF2B08] (0x5DFD13)
inline constexpr float k_GripDrop = std::bit_cast<float>(0x3F266666u);  ///< [0xBF2B0C] 0.65 (0x5DFE9B)
inline constexpr int32_t k_PickUpGo = 0x3E8;                            ///< [0xD19CB0] = 1000 (0x5DF731)

/// fn_005DF9C0 0x5DF9C3..0x5DF9DD: the put-down hand's yaw, -pi/4 ([0x8C79E4]) - 15 ([0xBF2B04]) x pi/180
/// ([0x92B20C]), each step a float (the FPU at 24 bits)
[[nodiscard]] float PutDownYaw();

/// Object::AdvanceAnimTime's rule as fn_005DF640 writes it inline (0x5DF7EC..0x5DF803 / 0x5DF8A8..0x5DF8D2): n = time +
/// ms; a looping clip (+0x50 & 0x100) n % duration (idiv, signed), else min(n, duration - 1). `wrapped` when n < time
/// (0x5DF805 / 0x5DF8DA jge)
[[nodiscard]] int32_t AdvanceTime(int32_t time, uint32_t milliseconds, int32_t duration, bool looping, bool& wrapped);

/// The light of special 0 (fn_00827F20, 0x34 bytes in JCMisc's [0xD19C88]): 20 LH3DSprites (Create(20, 1) 0x8404A0:
/// SetToZero 0x8404F0 then +0x28 |= 0x80, the sprite's own material) of CreateMaterial(13, misc0.raw [0xEA1A90])
/// [0xD19C8C] (render_modes::materials::k_Misc0Additive)
namespace light
{
inline constexpr int k_Sprites = 20;
inline constexpr float k_Far = std::bit_cast<float>(0xC57A0000u);      ///< [0x9A3950] -4000: the start, along -direction
inline constexpr float k_Speed = std::bit_cast<float>(0x3EE66666u);    ///< [0x9A3958] 0.45 units a millisecond
inline constexpr float k_Travel = std::bit_cast<float>(0x45796000u);   ///< [0x9A3954] 3990: then it has arrived
inline constexpr float k_Spacing = std::bit_cast<float>(0x40C00000u);  ///< [0x8AB35C] 6: sprite i trails 6 i behind
inline constexpr float k_Spin = std::bit_cast<float>(0x3924B5BEu);     ///< [0x9A395C] sprite 19 turns 0.00015708 rad/ms
inline constexpr int32_t k_ZAlwaysAfterMs = 0x202D;                    ///< 8237: ZFUNC ALWAYS after it (0x828659)
inline constexpr float k_CameraBack = std::bit_cast<float>(0x43160000u); ///< [0x8CC7E8] 150 behind the head
inline constexpr float k_CameraUp = std::bit_cast<float>(0x41F00000u);   ///< [0x8BF51C] 30 above
inline constexpr int32_t k_HoldMs = 500;                              ///< [0xC383E0] (0x828519)
inline constexpr int32_t k_FadeMs = 1000;                             ///< [0xC383E4] (0x828412)
inline constexpr int32_t k_FadeFloor = 50;                            ///< [0xC383E8] the last alpha (0x828429)

/// +0x28
enum class State : int32_t
{
	Falling = 0, ///< 20 sprites along the beam
	Hold = 1,    ///< 500 ms, sprite 0 three times
	Fade = 2,    ///< 1000 ms, sprite 0's alpha 255 -> 50
	Done = 4,    ///< nothing drawn; fn_005DF640 deletes it
};

struct Light
{
	std::array<graphics::billboard::Sprite, k_Sprites> sprites; ///< +0x00 (LH3DSprite*, stride 0x34)
	glm::vec3 head {0.0f};      ///< +0x04: where the beam's front is
	glm::vec3 start {0.0f};     ///< +0x10: 4000 up the beam from the target
	glm::vec3 direction {0.0f}; ///< +0x1C: normalised (InverseSquareRoot 0x841170)
	State state {State::Falling}; ///< +0x28
	int32_t elapsed {0};        ///< +0x2C: ms of the fall (the draw's g_game_time_inc)
	bool arrived {false};       ///< +0x30
};

/// ?Random@@YAMMM@Z 0x81D180 (game_random::crt::Random): replaceable for the tests
using RandomFn = std::function<float(float, float)>;

/// fn_00827F20(target, direction, material) 0x827F20..0x8282CF
[[nodiscard]] Light Create(const glm::vec3& target, const glm::vec3& direction, const RandomFn& random);
/// fn_00828350(out) 0x828350: d = (float)elapsed x 0.45, past 3990 it is 3990 and +0x30 = 1; start + direction d
glm::vec3 HeadPosition(Light& light);
/// 0x828760(out) (special 1): HeadPosition into +0x04, copied to `out` (the debug camera's position [0xEA1B58])
glm::vec3 Start(Light& light);

/// What one call of the light's Z-object callback 0x8283D0 draws
struct Frame
{
	/// LH3DSprite::Draw 0x840530 in order (screen sprites, mode A, each with its near test)
	std::vector<graphics::billboard::Sprite> drawn;
	/// SetRenderState(ZFUNC 0x17, ALWAYS 8) around the draws (0x82847C / 0x82866C), LESSEQUAL 4 back after
	bool depthAlways {false};
	/// the falling state's write of the debug camera's position [0xEA1B58] (0x8286D7..0x828746)
	std::optional<glm::vec3> cameraPosition;
};
/// The Z-object callback 0x8283D0 (queued by fn_00828300 with key |+0x04 - g_camera|^2, SumOrder::XYZ). `timer` is the
/// global [0xEB9A78] of the hold and the fade; `milliseconds` g_game_time_inc [0xEA9EC0] (0 while paused: then the
/// falling state draws no Random, the other two still draw two)
void Draw(Light& light, uint32_t milliseconds, int32_t& timer, const RandomFn& random, Frame& out);
} // namespace light

/// fn_005DFCE0 (0x5DFD03..0x5DFEB2) as a pure function: nothing past 1817 ms; else the hand posed by GetPose 0x839980
/// at `time` with its own matrix (`model`, LH3DObject +0x14), bone `bone` (the EBone block's +0x304) times the EBone's
/// matrix 0 (fn_007FAE60: the EBone point taken through the bone), its y less 0.65. (pending) a bone outside the pose
/// (the original reads past the buffer) gives nothing
[[nodiscard]] std::optional<glm::vec3> GripPoint(const graphics::L3DMesh& mesh, const L3DAnim& clip, int32_t time,
                                                 const glm::mat4& model, const std::array<float, 12>& eBoneMatrix,
                                                 int32_t bone);

/// fn_005DF9C0 (from GScript::PlayJCSpecial 0x708ED0, table 0x708F74): 0, 1, 2, 4, 5. 6 is PetitNavire (CHLApi.cpp),
/// 3 ScriptGFX 0x828DB0 (pending: not in the game's scripts), others nothing
void Play(int32_t special);
/// fn_005DF640 then, when it queued the light, its callback 0x8283D0 (one Z object a frame; drawn in the main view's
/// queue). `milliseconds` = g_game_time_inc of the frame (game_clock::FrameGameMs)
void Update(uint32_t milliseconds);
/// GLandscape::Draw 0x5E4B30..0x5E4BEE: the point the SuperVillagers with feature 7 are drawn at while [0xD19C34] (the
/// first special 5 to the second, or special 0) is set; called before Update (the original reads the hand as the last
/// frame left it). (approximate) when fn_005DFCE0 writes nothing the original reads a stale stack slot: the last point
/// written is kept
[[nodiscard]] std::optional<glm::vec3> Grip();
/// The light's Z object of this frame for the draw (Graphics/OverlayFrame.h), before DrawScene
void FillFrame(graphics::IntroLightOverlay& out);
/// IS_PLAYING_JC_SPECIAL(13) 0x708FE9: [0xD19C94], set when the pick-up clip wraps (fn_005DF640 0x5DF807), never cleared
[[nodiscard]] bool Finished13();
/// Intro::ReleaseAll 0x5DFC40 (THING_JC_SPECIAL 18 0x70910E, CleanGameForScriptReboot 0x6EBBFB): the light, the hand
/// (object, mesh, anim), the material, [0xD19CA4] = 0, the debug camera off ([0xEA9EC8] = 0), state -1
void ReleaseAll();

} // namespace openblack::ecs::intro_special
