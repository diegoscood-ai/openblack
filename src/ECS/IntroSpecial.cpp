/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "IntroSpecial.h"

#include <cmath>

#include <exception>
#include <string>

#include <glm/vec4.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DAnim.h"
#include "3D/L3DMesh.h"
#include "3D/ObjectMatrix.h"
#include "3D/SkeletalPose.h"
#include "Camera/ScriptCamera.h"
#include "Common/GameRandom.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/SuperVillager.h"
#include "FileSystem/FileSystemInterface.h"
#include "Graphics/OverlayFrame.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::ecs::intro_special
{
using namespace components;
using graphics::billboard::Sprite;

// ---- the pure parts ------------------------------------------------------------------------------------------------

float PutDownYaw()
{
	const float step = std::bit_cast<float>(0x41700000u) * std::bit_cast<float>(0x3C8EFA35u); // 15 x [0x92B20C]
	return std::bit_cast<float>(0xBF490FDBu) - step;                                       // fsubr [0x8C79E4]
}

int32_t AdvanceTime(int32_t time, uint32_t milliseconds, int32_t duration, bool looping, bool& wrapped)
{
	const auto n = static_cast<int32_t>(static_cast<uint32_t>(time) + milliseconds); // lea eax, [edi + esi]
	int32_t result = n;
	if (looping)
	{
		// cdq; idiv [anim+0x20]: the signed remainder. (pending) a clip of 0 ms divides by zero in the original
		result = duration > 0 ? n % duration : 0;
	}
	else if (duration - 1 < n) // cmp ecx (duration - 1), eax; jl
	{
		result = duration - 1;
	}
	wrapped = result < time; // jge: the time went back
	return result;
}

namespace light
{
namespace
{
constexpr float k_TwoPi = std::bit_cast<float>(0x40C90FDBu);   ///< Random(0, 2 pi) (0x828136, 0x8284A1, 0x8285DA)
constexpr float k_SizeStep = std::bit_cast<float>(0x3D4CCCCDu); ///< [0x8C7BD4] 0.05
constexpr float k_SizeBase = std::bit_cast<float>(0x40400000u); ///< [0x8C2C50] 3
constexpr float k_MinSize = std::bit_cast<float>(0x38D1B717u);  ///< [0x8BF518] 1e-4, SetSize's floor
constexpr float k_Jitter = std::bit_cast<float>(0x40000000u);   ///< Random(-2, 2) (0x8285F1 / 0x8285F6)
constexpr float k_TailBack = std::bit_cast<float>(0xC0C00000u); ///< [0x9A394C] -6: sprite 19 behind the start
constexpr float k_StreakSize = std::bit_cast<float>(0x43AF0000u);   ///< [0x9A3948] 350 (0x82829F)
constexpr float k_StreakHeight = std::bit_cast<float>(0x3D4CCCCDu); ///< 0.05 (0x8282A9): half height 17.5 ([0x9A3944])
constexpr float k_StreakAngle = std::bit_cast<float>(0xBEC90FDBu);  ///< -pi / 8 (0x828257)
constexpr float k_GlowSize = std::bit_cast<float>(0x42A00000u);     ///< [0x8D060C] 80, sprite 18 (0x8281AB)

/// LH3DSprite::SetSize as inlined (0x828134..0x82814E): the origin times new / old, then the size. The light's origins
/// stay 0, so only the size changes
void SetSize(Sprite& sprite, float size)
{
	const float ratio = size / sprite.size;
	sprite.origin.x = ratio * sprite.origin.x;
	sprite.origin.y = ratio * sprite.origin.y;
	sprite.size = size;
}

/// 0x828108..0x82812E: float(15 (19 - i)) x 0.05 (+ jitter) + 3, at least 1e-4 (fcom; test ah, 1)
float Floor(float size)
{
	return size < k_MinSize ? k_MinSize : size;
}
} // namespace

Light Create(const glm::vec3& target, const glm::vec3& direction, const RandomFn& random)
{
	Light light;
	// 0x827F41..0x827F87: (x^2 + y^2) + z^2 (z, y, x loaded), InverseSquareRoot 0x841170, each component times it
	const float lengthSquared = (direction.x * direction.x + direction.y * direction.y) + direction.z * direction.z;
	const float inverse = lh_matrix::InverseSquareRoot(lengthSquared);
	light.direction = {inverse * direction.x, inverse * direction.y, inverse * direction.z};
	// 0x827F9A..0x827FF8: start = target + direction x -4000 ([0x9A3950]); +0x04 and +0x10 both
	light.start = {target.x + light.direction.x * k_Far, target.y + light.direction.y * k_Far,
	               target.z + light.direction.z * k_Far};
	light.head = light.start;
	// 0x828000..0x8281E3: LH3DSprite::Create(20, 1), then per sprite
	for (int i = 0; i < k_Sprites; ++i)
	{
		auto& sprite = light.sprites.at(static_cast<size_t>(i));
		sprite = Sprite {}; // SetToZero 0x8404F0 (+0x28 bit 0x80: the material +0x2C, set at 0x828027)
		// 0x82804D..0x8280A3: start + direction x (float(-i) x 6)
		const float k = static_cast<float>(-i) * k_Spacing;
		sprite.position = {light.direction.x * k + light.start.x, light.direction.y * k + light.start.y,
		                   light.direction.z * k + light.start.z};
		const auto steps = static_cast<float>((19 - i) * 15);
		float size = steps * k_SizeStep + k_SizeBase;
		if (i > 5) // 0x82806D cmp ecx, 5; jle
		{
			sprite.cell = 50; // 0x8280AE..0x8280B7: (+0x28 & ~0xD) | 0x32
			size = size + size; // fadd st(0), st(0) 0x8280DD
		}
		else
		{
			sprite.cell = 48; // 0x8280F0..0x8280F9: (+0x28 & ~0xF) | 0x30
		}
		SetSize(sprite, Floor(size));
		sprite.angle = random(0.0f, k_TwoPi); // 0x828151
		// 0x82815C..0x828186: alpha (19 - i) 255 / 20 (imul 0x66666667, sar 3: truncated), white
		sprite.argb = (static_cast<uint32_t>((19 - i) * 255 / 20) << 24) | 0xFFFFFFu;
		if (i == 18) // 0x828183: the glow at the head, 80 wide, alpha 0x28
		{
			sprite.argb = 0x28FFFFFFu;
			SetSize(sprite, k_GlowSize);
			sprite.position = light.start;
		}
	}
	// 0x8281E9..0x8282B9: sprite 19, a thin streak 350 x 17.5 (half sizes) 6 behind the start, turned -pi / 8
	auto& streak = light.sprites.at(19);
	streak.position = {light.direction.x * k_TailBack + light.start.x, light.direction.y * k_TailBack + light.start.y,
	                   light.direction.z * k_TailBack + light.start.z};
	streak.angle = k_StreakAngle;
	streak.argb = 0x0EFFFFFFu; // 0x828269
	streak.cell = 49;          // 0x828280..0x828286: (+0x28 & ~0xE) | 0x31
	SetSize(streak, k_StreakSize);
	streak.height = k_StreakHeight; // SetHeight: the origin's y times 17.5 / old half height (0)
	light.state = State::Falling;    // 0x8282BC..0x8282C4: +0x28, +0x2C, +0x30 = 0
	light.elapsed = 0;
	light.arrived = false;
	return light;
}

glm::vec3 HeadPosition(Light& light)
{
	float d = static_cast<float>(light.elapsed) * k_Speed; // fild; fmul [0x9A3958]
	if (!(d <= k_Travel))                                  // fcom [0x9A3954]; test ah, 0x41; jne
	{
		light.arrived = true; // +0x30 = 1 (0x828369)
		d = k_Travel;
	}
	return {d * light.direction.x + light.start.x, d * light.direction.y + light.start.y,
	        d * light.direction.z + light.start.z};
}

glm::vec3 Start(Light& light)
{
	light.head = HeadPosition(light);
	return light.head;
}

void Draw(Light& light, uint32_t milliseconds, int32_t& timer, const RandomFn& random, Frame& out)
{
	out.drawn.clear();
	out.depthAlways = false;
	out.cameraPosition.reset();
	const auto msSigned = static_cast<int32_t>(milliseconds);
	if (light.state == State::Done) // 0x8283E2
	{
		return;
	}
	if (light.state == State::Falling) // 0x828500
	{
		const glm::vec3 head = Start(light); // fn_00828350(+0x04)
		if (light.arrived)                   // 0x82850E: hold for 500 ms
		{
			light.state = State::Hold;
			timer = k_HoldMs;
		}
		for (int i = 0; i < k_Sprites; ++i)
		{
			auto& sprite = light.sprites.at(static_cast<size_t>(i));
			if (i == 19) // 0x82852D: the streak turns, fild g_game_time_inc x [0x9A395C] + angle
			{
				sprite.angle = static_cast<float>(msSigned) * k_Spin + sprite.angle;
			}
			sprite.position = head;
			if (i >= 18) // 0x828556: 18 and 19 stay at the head
			{
				continue;
			}
			const float k = static_cast<float>(-i) * k_Spacing; // 0x828572..0x8285CE
			sprite.position = {light.direction.x * k + head.x, light.direction.y * k + head.y,
			                   light.direction.z * k + head.z};
			if (milliseconds == 0) // 0x8285D6: no draws while the game time stands
			{
				continue;
			}
			sprite.angle = random(0.0f, k_TwoPi); // 0x8285E1
			if (i >= 5)                           // 0x8285E6
			{
				continue;
			}
			// 0x8285F1..0x828649: the five front sprites flicker, (float(15 (19 - i)) x 0.05 + Random(-2, 2)) + 3
			const float jitter = random(-k_Jitter, k_Jitter);
			const float steps = static_cast<float>((19 - i) * 15);
			SetSize(sprite, Floor((steps * k_SizeStep + jitter) + k_SizeBase));
		}
		// 0x828656..0x8286CE: ZFUNC ALWAYS around the 20 draws once the fall has run 8237 ms
		out.depthAlways = light.elapsed > k_ZAlwaysAfterMs;
		out.drawn.assign(light.sprites.begin(), light.sprites.end());
		light.elapsed = static_cast<int32_t>(static_cast<uint32_t>(light.elapsed) + milliseconds); // 0x8286D4
		// 0x8286D7..0x828746: the debug camera's position, 150 back up the beam and 30 above
		glm::vec3 camera = HeadPosition(light);
		const glm::vec3 back {light.direction.x * k_CameraBack, light.direction.y * k_CameraBack,
		                      light.direction.z * k_CameraBack};
		camera.x = camera.x - back.x;
		camera.z = camera.z - back.z;
		camera.y = (camera.y - back.y) + k_CameraUp;
		out.cameraPosition = camera;
		return;
	}
	auto& glow = light.sprites.at(0);
	if (light.state == State::Hold) // 0x8283F0..0x828418
	{
		timer = static_cast<int32_t>(static_cast<uint32_t>(timer) - milliseconds);
		if (timer < 0) // jns
		{
			light.state = State::Fade;
			timer = k_FadeMs;
		}
	}
	if (light.state == State::Fade) // 0x82841E..0x828470: ftol((timer / 1000) x (255 - 50) + 50)
	{
		const float fraction = static_cast<float>(timer) / static_cast<float>(k_FadeMs);
		const float alpha = fraction * static_cast<float>(0xFF - k_FadeFloor) + static_cast<float>(k_FadeFloor);
		glow.argb = (static_cast<uint32_t>(static_cast<int32_t>(alpha)) << 24) | 0xFFFFFFu;
		timer = static_cast<int32_t>(static_cast<uint32_t>(timer) - milliseconds);
		if (timer < 0)
		{
			light.state = State::Done;
		}
	}
	// 0x828473..0x8284FB: ZFUNC ALWAYS; sprite 0 three times, a new Random(0, 2 pi) angle after the first and second
	out.depthAlways = true;
	out.drawn.push_back(glow);
	glow.angle = random(0.0f, k_TwoPi);
	out.drawn.push_back(glow);
	glow.angle = random(0.0f, k_TwoPi);
	out.drawn.push_back(glow);
}
} // namespace light

std::optional<glm::vec3> GripPoint(const graphics::L3DMesh& mesh, const L3DAnim& clip, int32_t time,
                                   const glm::mat4& model, const std::array<float, 12>& eBoneMatrix, int32_t bone)
{
	// 0x5DFD05..0x5DFD1E: fild the cycle time, fcomp 1817; test ah, 0x41 (nothing past it)
	if (!(static_cast<float>(time) <= k_GripLastMs))
	{
		return std::nullopt;
	}
	// 0x5DFDA1..0x5DFDD2: GetPose 0x839980(bones [0xC37D9C], mesh, time, +0x14): every bone in the world
	std::vector<glm::mat4> pose;
	graphics::ComputePose(mesh, clip, static_cast<float>(time), pose);
	if (bone < 0 || static_cast<size_t>(bone) >= pose.size())
	{
		return std::nullopt; // (pending) the original indexes the buffer with it all the same
	}
	// 0x5DFE65..0x5DFE8A: bones[EBone +0x304] copied, fn_007FAE60(copy, EBone matrix 0): its translation is the EBone
	// point (cells 9..11) through the bone
	const glm::vec4 point = model * pose.at(static_cast<size_t>(bone)) *
	                        glm::vec4(eBoneMatrix.at(9), eBoneMatrix.at(10), eBoneMatrix.at(11), 1.0f);
	return glm::vec3(point.x, point.y - k_GripDrop, point.z); // 0x5DFE93..0x5DFEB2
}

// ---- the game's Intro ----------------------------------------------------------------------------------------------

namespace
{
/// The intro hand, LH3DObject::Create(2) (an animated object) in [0xD19C98] with vt+0x58(1) (obj+4 bit 0x20 when
/// [0xC38224]: pending, as in spec_hd_models.md section 8)
struct HandObject
{
	entt::entity entity {entt::null};
	/// [0xD19C9C] LH3DMesh::CreateFromHD("Data\\MISC\\hand_intro.l3d", 0) 0x8067F0 (0xBF32B4)
	const super_villager::HdModel* model {nullptr};
	/// [0xD19CA0] fn_00839900(file) (0: none); the clip's id in the animation manager
	entt::id_type clip {0};
	/// the object's cycle time (vt+0x188 / +0x18C, LH3DAnimatedObject +0x84)
	int32_t time {0};
	/// the last SetPosition (vt+0x20 = LH3DObject::SetPosition 0x423140: T(p) Ry(-a) S(s))
	glm::vec3 position {0.0f};
	float yaw {0.0f};
	float scale {1.0f};
};

struct Runtime
{
	int32_t state {-1};               ///< [0xBF2AFC] (.data 0xFFFFFFFF)
	std::optional<light::Light> beam;  ///< [0xD19C88]: the light
	bool material {false};            ///< [0xD19C8C]: CreateMaterial(13, misc0) made (no GPU object in openblack)
	bool cameraFollow {false};        ///< [0xD19C90]: the debug camera's focus kept on the Son's spot each frame
	bool finished13 {false};          ///< [0xD19C94]
	HandObject hand;
	bool playing {false};             ///< [0xD19CA4]
	float pickUpYaw {0.0f};           ///< [0xD19CA8]: never written (0)
	int32_t pickUpWait {0};           ///< [0xD19CB0]: 1000 once the light is gone, then the pick-up clip runs
	bool followSon {false};           ///< [0xD19C34]
	int32_t lightTimer {0};           ///< [0xEB9A78] (bss)
	/// this frame: the light's Z object (fn_00828300) and its callback's draws
	bool lightQueued {false};
	glm::vec3 lightKey {0.0f};
	light::Frame lightFrame;
	/// the last grip fn_005DFCE0 wrote (the stack slot GLandscape::Draw reads)
	std::optional<glm::vec3> grip;
};
Runtime s_Intro;

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

float CrtRandom(float a, float b)
{
	return game_random::crt::Random(a, b); // ?Random@@YAMMM@Z 0x81D180, the CRT rand() stream
}

/// fn_00839900(name): Data\MISC\<file>, loaded once into the animation manager. (approximate) the original frees the
/// clip and reads the file again at each special 4; the data is the same
entt::id_type LoadClip(const char* file)
{
	const auto id = resources::HashIdentifier(std::string("misc/") + file);
	auto& animations = Locator::resources::value().GetAnimations();
	if (animations.Contains(id))
	{
		return id;
	}
	try
	{
		const auto path = Locator::filesystem::value().GetPath<filesystem::Path::Misc>() / file;
		animations.Load(id, resources::L3DAnimLoader::FromDiskTag {}, path);
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Intro: cannot load {}: {}", file, e.what());
		return 0;
	}
	return id;
}

const L3DAnim* Clip(entt::id_type id)
{
	if (id == 0)
	{
		return nullptr;
	}
	const auto& animations = Locator::resources::value().GetAnimations();
	return animations.Contains(id) ? &*animations.Handle(id) : nullptr;
}

/// LH3DObject::Create(2) 0x80B4D0 + vt+0x58(1) + CreateFromHD + SetMesh vt+0xF4 (0x5DFB00..0x5DFB3B / 0x5DF67B..
/// 0x5DF6C1): an entity with no Mesh yet (Update gives it one on the frames the hand is drawn)
void CreateHand()
{
	auto& hand = s_Intro.hand;
	hand = HandObject {};
	hand.model = super_villager::LoadHdModel("hand_intro", "hand_intro");
	auto& registry = Entities();
	hand.entity = registry.Create();
	registry.Assign<Transform>(hand.entity, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	auto& animation = registry.Assign<SkeletalAnimation>(hand.entity);
	animation.speed = 0.0f; // the time is the Intro's (vt+0x188), it does not run by itself
}

/// vt+0x180 SetCurrentAnim
void SetClip(entt::id_type clip)
{
	s_Intro.hand.clip = clip;
	if (s_Intro.hand.entity != entt::null && Entities().Valid(s_Intro.hand.entity))
	{
		auto& animation = Entities().Get<SkeletalAnimation>(s_Intro.hand.entity);
		animation.clip = clip;
		animation.hasClip = clip != 0;
	}
}

/// LH3DObject::SetPosition 0x423140 (vt+0x20)
void SetPosition(const glm::vec3& position, float yaw, float scale)
{
	auto& hand = s_Intro.hand;
	hand.position = position;
	hand.yaw = yaw;
	hand.scale = scale;
}

/// vt+0x4 (0x5DF8DE / 0x5DFC70), fn_00839970 and LH3DMesh::Release 0x806D00. (approximate) the mesh and the clip stay in
/// their managers
void FreeHand()
{
	auto& hand = s_Intro.hand;
	if (hand.entity != entt::null && Locator::entitiesRegistry::has_value() && Entities().Valid(hand.entity))
	{
		Entities().Destroy(hand.entity);
		Entities().SetDirty();
	}
	hand = HandObject {};
}

/// The draw of this frame: vt+0xA0(1) (obj+4 bit 0x10000, pending) and vt+0x108 (fn_00812170, the animated object's
/// Draw, at once in fn_005E5CD0's turn) at `time`; on the other frames the hand is not drawn
void ShowHand(bool drawn, int32_t time)
{
	auto& hand = s_Intro.hand;
	if (hand.entity == entt::null || !Entities().Valid(hand.entity))
	{
		return;
	}
	auto& registry = Entities();
	const bool hasMesh = registry.AllOf<Mesh>(hand.entity);
	const bool wantMesh = drawn && hand.model != nullptr;
	if (wantMesh && !hasMesh)
	{
		registry.Assign<Mesh>(hand.entity, hand.model->mesh, static_cast<int8_t>(0), static_cast<int8_t>(0));
	}
	else if (!wantMesh && hasMesh)
	{
		registry.Remove<Mesh>(hand.entity);
	}
	auto& transform = registry.Get<Transform>(hand.entity);
	const auto rotation = lh_matrix::AngleY(hand.yaw);
	if (transform.position != hand.position || transform.rotation != rotation || transform.scale.x != hand.scale)
	{
		transform.position = hand.position;
		transform.rotation = rotation;
		transform.scale = glm::vec3(hand.scale);
		registry.SetDirty();
	}
	registry.Get<SkeletalAnimation>(hand.entity).time = static_cast<float>(time);
}

/// fn_00828300: the light's Z object for this frame, keyed at its head as it is now
void QueueLight()
{
	s_Intro.lightQueued = true;
	s_Intro.lightKey = s_Intro.beam->head;
}
} // namespace

void Play(int32_t special)
{
	auto& intro = s_Intro;
	switch (special)
	{
	case 0: // 0x5DF9EE
		intro.state = 0;
		intro.material = true; // CreateMaterial(13, [0xEA1A90]) once
		// new 0x34 (JCMisc.cpp:0x3FF); a light still there is dropped without its delete, as in the original
		intro.beam = light::Create(k_SonSpot, k_LightDirection, CrtRandom);
		intro.followSon = false; // 0x5DFA5C (0x5DFA72 when the new fails)
		return;
	case 1: // 0x5DFA7D, state 0 only
		if (intro.state != 0)
		{
			return;
		}
		script_camera::SetDebugCameraFocus(k_SonSpot); // 0xEA1B68 = [0xD19A28]
		intro.cameraFollow = true;                     // [0xD19C90] = 1
		script_camera::SetDebugCameraMode(2);          // [0xEA9EC8] = 2
		if (intro.beam.has_value())                   // (approximate) the original calls 0x828760 on a null light too
		{
			script_camera::SetDebugCameraPosition(light::Start(*intro.beam)); // 0x828760(&0xEA1B58)
		}
		return;
	case 2: // 0x5DFAD5, state 0 only
		if (intro.state != 0)
		{
			return;
		}
		intro.cameraFollow = false;
		script_camera::SetDebugCameraMode(0);
		return;
	case 4: // 0x5DFAF5
	{
		const bool had = intro.hand.entity != entt::null;
		if (!had)
		{
			CreateHand();
		}
		// else 0x5DFB44: fn_00839970 frees the clip in [0xD19CA0]
		SetClip(LoadClip("hand_intro.anm")); // 0xBF32D0, vt+0x180
		intro.hand.time = 0;                 // vt+0x188(0) 0x5DFB81
		SetPosition(k_HandSpot, PutDownYaw(), k_HandScale); // vt+0x20 0x5DFBA0
		intro.state = 4;
		return;
	}
	case 5: // 0x5DFBB2, state 4 only
		if (intro.state != 4)
		{
			return;
		}
		if (intro.playing) // 0x5DFBC6: the jump to 2379 ms, the Son let go
		{
			intro.hand.time = k_SeekMs;
			intro.followSon = false;
			return;
		}
		intro.playing = true; // 0x5DFBE4..0x5DFBEE
		intro.followSon = true;
		return;
	default: // 3, 6 and above: not here (GScript::PlayJCSpecial's table)
		return;
	}
}

void Update(uint32_t milliseconds)
{
	auto& intro = s_Intro;
	intro.lightQueued = false;
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	switch (intro.state)
	{
	case 0: // 0x5DF92D: the light falls
		if (!intro.beam.has_value())
		{
			break;
		}
		if (intro.beam->state == light::State::Done) // 0x5DF937: deleted (fn_008282E0 + delete)
		{
			intro.beam.reset();
			break;
		}
		QueueLight();                                  // 0x5DF95D
		if (intro.beam->state == light::State::Hold) // 0x5DF968: landed last frame, the pick-up hand comes
		{
			intro.state = 12;
		}
		if (intro.cameraFollow) // 0x5DF978..0x5DF99C
		{
			script_camera::SetDebugCameraFocus(k_SonSpot);
		}
		break;
	case 4: // 0x5DF865: the put-down hand
	{
		ShowHand(true, intro.hand.time);
		if (!intro.playing)
		{
			break;
		}
		const auto* clip = Clip(intro.hand.clip);
		bool wrapped = false;
		const int32_t next = clip != nullptr ? AdvanceTime(intro.hand.time, milliseconds, clip->GetDurationMs(),
		                                                   clip->IsLooping(), wrapped)
		                                     : intro.hand.time;
		if (wrapped) // 0x5DF8DC..0x5DF910: all of it goes (not [0xD19C34])
		{
			FreeHand();
			intro.playing = false;
			intro.state = -1;
			break;
		}
		intro.hand.time = next; // vt+0x188 0x5DF921
		break;
	}
	case 12: // 0x5DF66F: the pick-up hand
	{
		if (intro.hand.entity == entt::null)
		{
			intro.pickUpWait = 0; // 0x5DF680 (and [0xD19CAC] = 0, never read)
			CreateHand();
			SetClip(LoadClip("hand_intro2.anm")); // 0xBF3298
			SetPosition(k_SonSpot, intro.pickUpYaw, k_HandScale); // 0x5DF704, written again below
		}
		if (intro.beam.has_value())
		{
			if (intro.beam->state == light::State::Done) // 0x5DF711: the light goes, the clip may run
			{
				intro.beam.reset();
				intro.pickUpWait = k_PickUpGo;
			}
			else
			{
				QueueLight(); // 0x5DF73D
			}
		}
		if (intro.pickUpWait != 0) // 0x5DF7B0
		{
			const auto* clip = Clip(intro.hand.clip);
			bool wrapped = false;
			const int32_t next = clip != nullptr ? AdvanceTime(intro.hand.time, milliseconds, clip->GetDurationMs(),
			                                                   clip->IsLooping(), wrapped)
			                                     : intro.hand.time;
			if (wrapped)
			{
				intro.finished13 = true; // 0x5DF807 (hand_intro2.anm is one shot: never, see the spec)
			}
			else
			{
				intro.hand.time = next; // 0x5DF81B
			}
		}
		SetPosition(k_PickUpSpot, k_PickUpYaw, k_HandScale); // 0x5DF83B ([0xD19A14] = 0.008 once)
		ShowHand(true, intro.hand.time);                     // vt+0xA0(1), vt+0x108
		break;
	}
	default:
		break;
	}
	if (intro.state != 4 && intro.state != 12)
	{
		ShowHand(false, intro.hand.time); // drawn only by states 4 and 12 (state 0 keeps an old hand undrawn)
	}
	// the light's Z object is drawn in the main view's queue: its callback 0x8283D0 runs once this frame. Its CRT
	// Random draws (0x8284A8, 0x8284C0, 0x8285E1, 0x8285FB) happen here: (approximate, pending Motor m2c2): the original
	// draws in the Z drain, after the clouds, night lights and smoke; moves to the end of Renderer::PreDraw with Motor's
	// m2c2
	if (intro.lightQueued)
	{
		light::Draw(*intro.beam, milliseconds, intro.lightTimer, CrtRandom, intro.lightFrame);
		if (intro.lightFrame.cameraPosition.has_value())
		{
			script_camera::SetDebugCameraPosition(*intro.lightFrame.cameraPosition); // [0xEA1B58]
		}
	}
}

std::optional<glm::vec3> Grip()
{
	auto& intro = s_Intro;
	if (!intro.followSon) // GLandscape::Draw 0x5E4BCE
	{
		return std::nullopt;
	}
	const auto& hand = intro.hand;
	// 0x5DFCE0..0x5DFD9F: a hand, [0xD19CA4], its time <= 1817, its mesh with an EBone block (flag 0x200000)
	if (hand.entity != entt::null && intro.playing && hand.model != nullptr && hand.model->host.has_value())
	{
		const auto& meshes = Locator::resources::value().GetMeshes();
		const auto* clip = Clip(hand.clip);
		if (clip != nullptr && meshes.Contains(hand.model->mesh))
		{
			const auto model = lh_matrix::Model(hand.position, lh_matrix::AngleY(hand.yaw), glm::vec3(hand.scale));
			const auto& host = *hand.model->host;
			if (const auto point = GripPoint(*meshes.Handle(hand.model->mesh), *clip, hand.time, model,
			                                 host.matrices.at(0), host.bones.at(0));
			    point.has_value())
			{
				intro.grip = point;
			}
		}
	}
	return intro.grip;
}

void FillFrame(graphics::IntroLightOverlay& out)
{
	out.active = s_Intro.lightQueued;
	out.sprites.clear();
	if (!out.active)
	{
		return;
	}
	out.keyPoint = s_Intro.lightKey;
	out.depthAlways = s_Intro.lightFrame.depthAlways;
	out.sprites = s_Intro.lightFrame.drawn;
}

bool Finished13()
{
	return s_Intro.finished13;
}

void ReleaseAll()
{
	auto& intro = s_Intro;
	intro.beam.reset(); // 0x5DFC41..0x5DFC5E
	FreeHand();          // 0x5DFC64..0x5DFC9D: the object, the mesh, the clip
	intro.material = false; // 0x5DFCA3..0x5DFCBE
	intro.playing = false;  // [0xD19CA4] = 0
	script_camera::SetDebugCameraMode(0); // [0xEA9EC8] = 0
	intro.state = -1;
	intro.lightQueued = false;
	// kept, as in the original: [0xD19C34], [0xD19C90], [0xD19C94], [0xD19CB0], the light timer
}

} // namespace openblack::ecs::intro_special
