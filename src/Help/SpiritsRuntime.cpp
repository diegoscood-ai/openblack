/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SpiritsRuntime.h"

#include <cmath>
#include <cstring>

#include <algorithm>
#include <exception>
#include <string>

#include <glm/geometric.hpp>
#include <glm/matrix.hpp>
#include <spdlog/spdlog.h>

#include "3D/FrameAnim.h"
#include "3D/L3DMesh.h"
#include "3D/ObjectMatrix.h"
#include "3D/LandIslandInterface.h"
#include "3D/TempleInteriorInterface.h"
#include "Audio/Audio.h"
#include "Audio/Services/Advisor.h"
#include "Camera/Camera.h"
#include "ECS/Components/Transform.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "FileSystem/FileSystemInterface.h"
#include "Game.h"
#include "GameClock.h"
#include "Graphics/Lh3dColour.h"
#include "Graphics/OverlayFrame.h"
#include "Help/HelpSystem.h"
#include "Locator.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;
using namespace openblack::help;
using namespace openblack::help::spirits;

namespace
{
std::unique_ptr<Runtime> g_Runtime;

/// The .hd names of fn_005C2A40 (0x915D20 good, 0x915D24 evil); the files on the disc are lower case
constexpr std::array<const char*, k_Dudes> k_Files = {"markgood.hd", "markevil.hd"};

/// HelpDude::Load 0x5C2194 through LHFile, then LoadRestSkeleton (the L3D0's bones, LH3DAnim::SetTransform 0x83A1D0)
void LoadAssets(const char* name, DudeAssets& out)
{
	std::string error;
	try
	{
		auto& fileSystem = Locator::filesystem::value();
		const auto bytes =
		    fileSystem.ReadAll(fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Data>() / "HelpSprite" / name));
		if (!help::LoadHelpDudeFile(bytes, out.file, &error) || !help::LoadRestSkeleton(out.file, out.rest, &error))
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "HelpSprite {}: {}", name, error);
			return;
		}
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "HelpSprite {}: {}", name, e.what());
		return;
	}
	std::memcpy(out.faceBones.data(), out.file.block2ECC.data(), sizeof(out.faceBones));
	out.data = DudeData::FromFile(out.file);
	out.loaded = true;
}

/// __ftol 0x7A1400
int32_t Ftol(float value)
{
	return static_cast<int32_t>(value);
}
} // namespace

glm::mat4 spirits::ModelOf(const glm::mat3& rows, const glm::vec3& position)
{
	return lh_matrix::Model(position, rows, glm::vec3(1.0f)); // the rows as they are (scale 1), the position
}

Runtime* spirits::Get()
{
	return g_Runtime.get();
}

void spirits::Start()
{
	g_Runtime = std::make_unique<Runtime>();
}

void spirits::Shutdown()
{
	g_Runtime.reset();
}

Runtime::Runtime()
{
	for (int i = 0; i < k_Dudes; ++i)
	{
		LoadAssets(k_Files.at(static_cast<size_t>(i)), _assets.at(static_cast<size_t>(i)));
	}
	RefreshView();
	// fn_005C2A40: both dudes fn_005C17D0(0 / 1), both at home (0x5C2CEA..0x5C2D16). Without a file the dude has no
	// clip and no mesh: the opcodes still run its states, nothing is drawn
	_control = std::make_unique<HelpDudeControl>(_assets.at(k_GoodDude).data, _assets.at(k_EvilDude).data, MakeQueries(),
	                                             _screen);
}

Runtime::~Runtime() = default;

void Runtime::RefreshView()
{
	if (Locator::windowing::has_value())
	{
		const auto size = Locator::windowing::value().GetSize();
		if (size.x > 0 && size.y > 0)
		{
			_screen = {static_cast<uint16_t>(size.x), static_cast<uint16_t>(size.y)};
		}
	}
	if (Locator::camera::has_value())
	{
		const auto& camera = Locator::camera::value();
		_frame = graphics::billboard::CameraFrame::From(camera);
		_lens = screen_point::LensOf(_screen.width, _screen.height, camera.GetHorizontalFieldOfView());
	}
}

Queries Runtime::MakeQueries()
{
	Queries q;
	// the random streams: unset, so game_random's (LocalRand, LocalFloatRand, crt::Random)
	q.isTalking = [](int dude) { return audio::advisor::IsTalking(dude); };
	q.talkedRecently = [](int dude) { return audio::advisor::TalkingOrJustStopped(dude); };
	q.sayActive = [](int dude) { return audio::advisor::Active(dude); };
	q.lipSync = [](int dude) -> std::optional<LipSyncFrame> {
		const auto frame = audio::advisor::LipSyncThisFrame(dude);
		if (!frame)
		{
			return std::nullopt;
		}
		return LipSyncFrame {frame->time, frame->playing, audio::advisor::LipSyncKey(dude).weights};
	};
	// LH3DTech's globals of the last UpdateCamera: g_camera, the W2C rotation, the lens and the near clip
	q.pointFromScreen = [this](glm::vec2 pixel, float depth) {
		return screen_point::PointFromScreen(_frame, _lens, Ftol(pixel.x), Ftol(pixel.y), depth);
	};
	q.worldToPixel = [this](const glm::vec3& p, bool force) -> std::optional<glm::vec2> {
		// fn_0081B450 (+ fn_0081B5F0 forced): (inferred) the clip flag 0x20 is "behind the eye", the others off the
		// screen; (approximate) the flag tests themselves are not read
		const auto projected = screen_point::Project(_frame, _lens, p);
		if (!projected)
		{
			return std::nullopt;
		}
		const glm::vec2 pixel = projected->pixel;
		if (!force && (pixel.x < 0.0f || pixel.y < 0.0f || pixel.x > static_cast<float>(_screen.width) ||
		               pixel.y > static_cast<float>(_screen.height)))
		{
			return std::nullopt;
		}
		return pixel;
	};
	q.projectPoint = [this](const glm::vec3& p) -> std::optional<ProjectedPoint> {
		const auto projected = screen_point::Project(_frame, _lens, p);
		if (!projected)
		{
			return std::nullopt;
		}
		return ProjectedPoint {Ftol(projected->pixel.x), Ftol(projected->pixel.y), projected->depth};
	};
	q.nearClip = [this]() { return _frame.nearZ; };
	q.cameraAxes = [this]() { return glm::mat3(_frame.right, _frame.up, _frame.forward); };
	q.cameraPosition = [this]() { return _frame.eye; };
	q.altitude = [](float x, float z) {
		return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt({x, z}) : 0.0f;
	};
	q.headAngles = [this](int dude, const glm::mat3& rows, const glm::vec3& position, const glm::vec3& target) {
		return HeadAngles(dude, rows, position, target);
	};
	q.fingertip = [this](int dude, const glm::mat3& rows, const glm::vec3& position) {
		return Fingertip(dude, rows, position);
	};
	// GetScriptGameThing / IsAvailable: a valid entity with a Transform; (x, y, z) world (openblack's y already has
	// GetAltitude in it) and Object::GetHeight vt+0x42C
	q.object = [](uint32_t object) -> std::optional<ObjectInfo> {
		if (object == 0 || !Locator::entitiesRegistry::has_value())
		{
			return std::nullopt;
		}
		const auto& registry = Locator::entitiesRegistry::value();
		const auto entity = static_cast<entt::entity>(object);
		if (!registry.Valid(entity) || !registry.AllOf<ecs::components::Transform>(entity))
		{
			return std::nullopt;
		}
		return ObjectInfo {registry.Get<const ecs::components::Transform>(entity).position, ecs::object::GetHeight(entity)};
	};
	return q;
}

void Runtime::Update()
{
	RefreshView();
	FrameInput input;
	// Draw3D 0x5C5ACE: g_delta_time x 0.001 (0x5C5AD4)
	input.dt = static_cast<float>(game_clock::FrameRealMs()) * 0.001f;
	// fn_005557E0: g_delta_time in the citadel, g_game_time_inc otherwise (inferred: openblack's temple interior)
	const bool citadel = Locator::temple::has_value() && Locator::temple::value().Active();
	input.frameMs = static_cast<int32_t>(game_clock::ClampedFrameMs(citadel));
	input.screen = _screen;
	if (const auto* game = Game::Instance(); game != nullptr)
	{
		input.mouse = game->GetMousePosition();
	}
	input.tickMs = audio::TickCount();
	if (const auto* helpSystem = help::Get(); helpSystem != nullptr)
	{
		input.wideScreen = helpSystem->GetWideScreen() != 0; // +0x45E8
	}
	_control->Update(input);

	for (int d = 0; d < k_Dudes; ++d)
	{
		const HelpDude& dude = _control->Dude(d);
		// FinishAnimStack 0x5C0610: the bones (hierarchy x M) for the draw
		EvaluatePose(d, dude.Layers().size(), ModelOf(dude.Rows(), dude.Position()), _bones.at(static_cast<size_t>(d)));
		UpdateTrail(d, game_clock::FrameRealMs());
		// HelpDudeControl::Say 0x5C36D0 reads +0x3514 for the voice's delay (audio::advisor::Say)
		audio::advisor::SetHover(d, dude.HoverX().value);
	}
	// (pending) HelpDude::PlaySoundFX 0x5C2800 for dude.Sounds(): the phase crossing rule and
	// GAudio::SamplePlayAnimEffect 0x42A4B0 on the InGame bank are not ported
	// (pending) the sentence's audio tags (+0x2F08, AudioTag::BuildAudioTags 0x42AE70): openblack's audio does not
	// read the cue / labl chunks of the HelpSprites.sad waves, so SetSentenceTags is never fed and no gesture fires
}

void Runtime::ProcessTurn()
{
	_control->ProcessTurn();
}

std::shared_ptr<const graphics::L3DMesh> Runtime::Mesh(int dude)
{
	auto& assets = _assets.at(static_cast<size_t>(dude));
	if (!assets.loaded)
	{
		return nullptr;
	}
	if (!assets.meshTried)
	{
		assets.meshTried = true;
		auto mesh = std::make_shared<graphics::L3DMesh>(k_Files.at(static_cast<size_t>(dude)));
		if (mesh->LoadFromBuffer(assets.file.mesh) && mesh->GetBoneMatrices().size() == assets.rest.parents.size())
		{
			assets.mesh = std::move(mesh);
		}
		else
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "HelpSprite {}: the L3D0 mesh does not load",
			                    k_Files.at(static_cast<size_t>(dude)));
		}
	}
	return assets.mesh;
}

void Runtime::EvaluatePose(int dude, size_t count, const glm::mat4& model, std::vector<glm::mat4>& world) const
{
	const auto& assets = _assets.at(static_cast<size_t>(dude));
	if (!assets.loaded || assets.rest.parents.empty())
	{
		world.clear();
		return;
	}
	const auto& rest = assets.rest;
	std::vector<glm::mat4> local = rest.local;
	const CAnim* stand = assets.data.clips[anim::Stand];
	// the fill of fn_00860E00's last argument 1: the stand clip's key 0, HelpDude +0x28 (0x5BB8BE)
	const CFrame* fill = stand != nullptr && !stand->frames.empty() ? stand->frames.data() : nullptr;
	const auto clipOf = [&assets](uint32_t i) { return i < k_AnimSlots ? assets.data.clips[i] : nullptr; };
	const auto& layers = _control->Dude(dude).Layers();
	for (size_t i = 0; i < count && i < layers.size(); ++i)
	{
		const AnimLayer& layer = layers[i];
		const CAnim* clip = clipOf(layer.clip);
		if (clip == nullptr)
		{
			continue;
		}
		switch (layer.kind)
		{
		case AnimLayer::Kind::Set:
			SetPose(*clip, layer.milliseconds, rest, fill, local);
			break;
		case AnimLayer::Kind::SetBlend:
		{
			// fn_005BC7D0: both poses, lerped per float in absolute bone space (0x839F10 / 0x83A020, inferred: the
			// hierarchy to absolute and back)
			const CAnim* other = clipOf(layer.clipB);
			if (other == nullptr)
			{
				SetPose(*clip, layer.milliseconds, rest, fill, local);
				break;
			}
			std::vector<glm::mat4> a = local;
			std::vector<glm::mat4> b = local;
			SetPose(*clip, layer.milliseconds, rest, fill, a);
			SetPose(*other, layer.millisecondsB, rest, fill, b);
			std::vector<glm::mat4> worldA;
			std::vector<glm::mat4> worldB;
			ComposeWorld(rest, a, glm::mat4(1.0f), worldA);
			ComposeWorld(rest, b, glm::mat4(1.0f), worldB);
			std::vector<glm::mat4> blended(worldA.size());
			for (size_t k = 0; k < worldA.size(); ++k)
			{
				blended[k] = worldA[k] + (worldB[k] - worldA[k]) * layer.blend;
			}
			for (size_t k = 0; k < blended.size(); ++k)
			{
				const uint32_t parent = rest.parents[k];
				local[k] = parent < k ? glm::inverse(blended[parent]) * blended[k] : blended[k];
			}
			break;
		}
		case AnimLayer::Kind::Add:
			if (layer.referenceKey < clip->frames.size())
			{
				ApplyAdditive(*clip, layer.milliseconds, clip->frames[layer.referenceKey], rest, local);
			}
			break;
		}
	}
	// (pending) fn_005C0310's eye bone scale (+0x2ECC..+0x2EE0 by face rec[3] / rec[4]: 1 but for the evil Normal 1.01
	// and Afraid 1.47) and fn_005BF9D0's pupil UVs on the eye bones' vertices: not in the layer list, not drawn
	ComposeWorld(rest, local, model, world);
}

std::optional<glm::vec2> Runtime::HeadAngles(int dude, const glm::mat3& rows, const glm::vec3& position,
                                             const glm::vec3& target) const
{
	// CalcHeadPos 0x5BFE00(M, target, head): the head bone's frame (fn_00839FA0 of this frame's pose so far under M)
	// with its origin moved to the middle of the two eye bones (+0x2ED8, +0x2ECC; 0x5BFEC8..0x5BFF3D), inverted
	// (LHMatrix::SetInverse 0x7FB290); the target in it, nothing if behind (local y < 0, 0x5BFFA5), normalised
	// (0x5BFFE5..0x5C000F); +0x34BC = atan2(z, y), +0x34C0 = asin(x), each clamped to +-1.0472 ([0x8C79E0] / [0x900CA4])
	// and x 0.477465 ([0x900CA0])
	const auto& assets = _assets.at(static_cast<size_t>(dude));
	std::vector<glm::mat4> world;
	EvaluatePose(dude, _control->Dude(dude).Layers().size(), ModelOf(rows, position), world);
	const uint32_t eyeA = assets.faceBones[3]; // +0x2ED8
	const uint32_t eyeB = assets.faceBones[0]; // +0x2ECC
	const uint32_t head = assets.faceBones[6]; // +0x2EE4
	if (eyeA >= world.size() || eyeB >= world.size() || head >= world.size())
	{
		return glm::vec2(0.0f);
	}
	glm::mat4 frame = world[head];
	frame[3] = glm::vec4((glm::vec3(world[eyeA][3]) + glm::vec3(world[eyeB][3])) * 0.5f, 1.0f);
	const glm::mat4 inverse(lh_matrix::Inverse(glm::mat4x3(frame))); // LHMatrix::SetInverse 0x7FB290
	glm::vec3 l = glm::vec3(inverse * glm::vec4(target, 1.0f));
	if (l.y < 0.0f)
	{
		return std::nullopt; // 0x5BFFA5: behind, +0x34BC / +0x34C0 keep their values
	}
	if (l.x != 0.0f || l.y != 0.0f || l.z != 0.0f)
	{
		l *= 1.0f / std::sqrt(l.x * l.x + l.y * l.y + l.z * l.z);
	}
	constexpr float k_Limit = 1.04719758f; // 0x3F860A92
	const float yaw = std::clamp(std::atan2(l.z, l.y), -k_Limit, k_Limit);
	const float pitch = std::clamp(std::asin(std::clamp(l.x, -1.0f, 1.0f)), -k_Limit, k_Limit);
	return glm::vec2(yaw, pitch) * 0.477465f;
}

glm::vec3 Runtime::Fingertip(int dude, const glm::mat3& rows, const glm::vec3& position) const
{
	// Update1 0x5BF1DC..0x5BF2FB: (bone matrix 0 x M).pos + r0 (+0x35C4 x +0x10) + r2 (+0x35C8 x +0x10)
	// (approximate operand mapping, spec_spirits_motion section 1.16)
	const auto& assets = _assets.at(static_cast<size_t>(dude));
	std::vector<glm::mat4> world;
	EvaluatePose(dude, _control->Dude(dude).Layers().size(), ModelOf(rows, position), world);
	const glm::vec3 root = world.empty() ? position : glm::vec3(world[0][3]);
	return root + rows[0] * (assets.data.fingerR0 * assets.data.modelSize) +
	       rows[2] * (assets.data.fingerR2 * assets.data.modelSize);
}

void Runtime::UpdateTrail(int dude, uint32_t deltaMs)
{
	// fn_005C3850 feeds fn_005B8F00 only for the dudes it draws (ctrl state != 0 and +0x35DC == 0) with
	// (+0x3514, +0x3544, +0x3574) and g_delta_time x 0.01
	const HelpDude& d = _control->Dude(dude);
	auto& trail = _trails.at(static_cast<size_t>(dude));
	const glm::vec3 point(d.HoverX().value, d.HoverY().value, d.DepthChannel().value);
	if (d.TrailResets() != trail.resets)
	{
		trail.resets = d.TrailResets(); // fn_005BBCD0: all 32 points at the current position
		trail.ring.fill(point);
		trail.accumulator = 0.0f;
	}
	if (_control->State(dude) == ControlState::Home || d.InWorld() != 0.0f)
	{
		return;
	}
	trail.accumulator += static_cast<float>(deltaMs) * 0.01f;
	if (trail.accumulator > 0.2f)
	{
		// (inferred) one point per call past 0.2, the accumulator taken down by 0.2
		trail.accumulator -= 0.2f;
		trail.ring.at(trail.head) = point;
		trail.head = (trail.head + 1) & (Trail::k_Points - 1);
	}
	// (pending) the 16 sparks at +0x180 (age, respawn at 16, drift): no draw path of them was found
}

std::pair<uint32_t, uint32_t> Runtime::WorldColour(const land_light::Sample& sample, float blend)
{
	if (blend == 0.0f)
	{
		return {0xFFFFFFFFu, 0u}; // 0x5BEA8F
	}
	// (approximate) the byte rule as spec_spirits_motion section 1.9 gives it, the code 0x5BE9B6..0x5BEA8F not re-read
	const auto w = static_cast<uint32_t>(Ftol(255.0f * Smooth(blend)));
	const uint32_t colour = land_light::LerpBytes(0xFFFFFFFFu, sample.diffuse, static_cast<int>(w)) | 0xFF000000u;
	const uint32_t specular = lh3d_colour::ScaleShr8_3KeepA(sample.specular, w);
	return {colour, specular};
}

void Runtime::FillOverlay(graphics::OverlayFrame& frame)
{
	// the order of the draw: Draw3D 0x5C5B26's dudes (the frame), then the FinishFrame callback 0x5C2E10's (the
	// overlay); each dude is in one of the two, so the halo clock moves once per drawn halo as before
	frame.spirits.clear();
	Draws(false, frame.spirits);
	Draws(true, frame.spirits);
	frame.spiritTrails.clear();
	for (const auto& v : TrailTriangles())
	{
		frame.spiritTrails.push_back(
		    {v.position.x, v.position.y, v.position.z, v.uv.x, v.uv.y, lh3d_colour::ToAbgr(v.argb)});
	}
}

void Runtime::Draws(bool overlay, std::vector<graphics::SpiritOverlay>& out)
{
	for (int d = 0; d < k_Dudes; ++d)
	{
		if (_control->State(d) == ControlState::Home)
		{
			continue;
		}
		const HelpDude& dude = _control->Dude(d);
		const float blend = dude.InWorld();
		if ((blend < 0.5f) != overlay) // fn_005C3920 / Draw3D 0x5C5B26 (0x5C2CDE)
		{
			continue;
		}
		const auto& assets = _assets.at(static_cast<size_t>(d));
		const auto& bones = _bones.at(static_cast<size_t>(d));
		graphics::SpiritOverlay draw;
		draw.dude = d;
		draw.overlay = overlay;
		draw.mesh = Mesh(d);
		draw.bones = bones; // a copy: the draw reads no live pose
		draw.alpha = static_cast<uint8_t>(std::clamp(dude.AlphaByte(), 0, 255));
		draw.inWorld = blend;
		if (blend != 0.0f)
		{
			// fn_00801C90 at the model's position (inferred: +0x3374), with the Renderer's land light table of the
			// frame (WorldColour there; white and 0 without a table)
			draw.landLightPoint = glm::vec2(dude.Position().x, dude.Position().z);
		}
		if (overlay)
		{
			// 0x5C0732..0x5C080C: Get3DPointFromScreen((0, H/2), -10), towards the eye by 2 blend while blend < 0.5
			glm::vec3 light = screen_point::PointFromScreen(_frame, _lens, 0, _screen.HalfHeight(), -10.0f);
			light += (_frame.eye - light) * (blend + blend);
			draw.lightPosition = light;
		}
		// LH3DSprite::Draw 0x840530 (mode A; nothing at or before the near plane) with the camera of the last Update,
		// two triangles per sprite. The sprites, the halo and the puff quads are built with the camera the spirits'
		// Update read (_frame / _lens), which may differ from the camera finally drawn this frame (as before)
		const auto addSprite = [this, &draw](const graphics::billboard::Sprite& sprite) {
			const auto quad = graphics::billboard::SpriteQuad(sprite, _frame);
			if (!quad)
			{
				return;
			}
			const uint32_t abgr = lh3d_colour::ToAbgr(sprite.argb);
			for (const int i : graphics::billboard::k_SpriteTriangles)
			{
				const auto& p = quad->corners.at(static_cast<size_t>(i));
				const auto& uv = quad->uv.at(static_cast<size_t>(i));
				draw.sprites.push_back({p.x, p.y, p.z, uv.x, uv.y, abgr});
			}
		};
		// the LH3DObject's position, vt+0x20(bone 0, 0, 1.0) in FinishAnimStack: halo and puff anchor (+0x14 / +0x38)
		const glm::vec3 anchor = bones.empty() ? dude.Position() : glm::vec3(bones[0][3]);
		// 0x5C0986..0x5C099F: +0x34DC != 0 and the alpha byte != 0 (only then [0xD15AB0] moves on)
		if (assets.file.haloScale != 0.0f && draw.alpha != 0)
		{
			graphics::billboard::Sprite halo;
			// (inferred) the object matrix is the identity at the anchor (angle 0, scale 1.0), so the offset is in world axes
			halo.position = anchor + assets.file.haloOffset;
			halo.size = assets.data.scale * assets.file.haloScale * dude.ModelScale() * 100.0f; // 0x5C0AB8..0x5C0AD7
			halo.height = 0.3f; // sprite +0x10
			const int32_t step = static_cast<int32_t>(
			    game_clock::ClampedFrameMs(Locator::temple::has_value() && Locator::temple::value().Active()));
			// 0x5C0A7A..0x5C0AAF: ([0xD15AB0] / 200) & 15 (x 0x51EB851F, sar 6)
			halo.cell = graphics::frame_anim::HelpSystemCell(_haloClockMs, step);
			halo.argb = static_cast<uint32_t>(draw.alpha) << 24 | 0xFFFFFFu;
			addSprite(halo); // the halo, then the puff (0x5C0986..0x5C0E95)
		}
		if (const auto& particles = dude.PuffParticles(); particles && dude.PuffRunning())
		{
			// 0x5C0AE5..0x5C0E95 with this frame's step of the logic (HelpDude::UpdateDraw): the alpha of the age before
			// the step, the size, the angle and the cell of the age after it, the position of vy before the drift
			// Convert3DToHover(obj +0x38, force 1) 0x5C0DCB: the anchor in hover space
			const auto hover = dude.WorldToHover(anchor, true).value_or(dude.Hover());
			const float fade = dude.PuffFade();
			const uint32_t mask = d == k_EvilDude ? 0x7F1F1Fu : 0xFFFFFFu; // +0x2C24 (0x5C0C0E..0x5C0C22)
			for (size_t i = 0; i < particles->size(); ++i)
			{
				const PuffParticle& p = particles->at(i);
				if (p.drawAlpha <= 0) // 0x5C0CDA
				{
					continue;
				}
				graphics::billboard::Sprite sprite;
				// (2 a) / 4 (cdq, and 3, sar 2: 0x5C0CEA..0x5C0CFC) << 24 + grey & mask
				sprite.argb = (static_cast<uint32_t>((p.drawAlpha * 2) / 4) << 24) + (p.grey & mask);
				// ((sizeBase - ((k age) fade) 0.5) + 1) 0.12, at least 0.0001 (0x5C0D19..0x5C0D51)
				float size = ((p.sizeBase - p.k * p.age * fade * 0.5f) + 1.0f) * 0.12f;
				if (size < 0.0001f)
				{
					size = 0.0001f;
				}
				sprite.size = size;
				sprite.angle = p.spin * p.age + static_cast<float>(i); // 0x5C0D73..0x5C0D87 (fiadd i)
				sprite.cell = graphics::frame_anim::SpriteCell(Ftol(p.age * 8.0f) & 15); // 0x5C0D91..0x5C0DB5
				// fn_005BD2A0(hx + vx 0.1, hy + vy 0.1, -(+0x3574) - 0.6, 0) (0x5C0DD0..0x5C0E15): the dude's depth
				// channel, not the particle's +8
				sprite.position = dude.HoverTo3D(hover.x + p.velocity.x * 0.1f, hover.y + p.drawVelocityY * 0.1f,
				                                 -dude.DepthChannel().value - 0.6f, false);
				addSprite(sprite);
			}
		}
		out.push_back(std::move(draw));
	}
}

std::vector<TrailVertex> Runtime::TrailTriangles() const
{
	std::vector<TrailVertex> out;
	for (int d = 0; d < k_Dudes; ++d)
	{
		const HelpDude& dude = _control->Dude(d);
		if (_control->State(d) == ControlState::Home || dude.InWorld() != 0.0f)
		{
			continue;
		}
		const auto& assets = _assets.at(static_cast<size_t>(d));
		const auto& trail = _trails.at(static_cast<size_t>(d));
		std::array<glm::vec3, Trail::k_Points> points {};
		for (size_t i = 0; i < points.size(); ++i)
		{
			points[i] = trail.ring.at((trail.head + i) & (Trail::k_Points - 1)); // oldest first
		}
		// fn_005B90C0: two vertices per point
		std::array<TrailVertex, 2 * Trail::k_Points> v {};
		const float u0 = d == k_GoodDude ? 1.0f : 0.0f;
		for (size_t i = 0; i < points.size(); ++i)
		{
			const glm::vec3 tangent = points[std::min(i + 1, points.size() - 1)] - points[i == 0 ? 0 : i - 1];
			const float len = glm::length(tangent);
			const glm::vec2 n = len > 0.0001f ? glm::vec2(tangent) / len : glm::vec2(0.0f);
			const float w = std::min(len > 0.0001f ? len + 0.2f : 0.0f, 0.6f);
			const float h = w * assets.data.scale / assets.data.nearDepth * 150.0f;
			const auto alpha = static_cast<uint32_t>(std::min(20.0f + 100.0f * w, 64.0f));
			const uint32_t argb = alpha << 24 | 0xFFFFFFu;
			const float vCoord = static_cast<float>(i) / 32.0f;
			const glm::vec3& p = points[i];
			v[2 * i] = {dude.HoverTo3D(p.x + n.y * h, p.y - n.x * h, p.z, false), {u0, vCoord}, argb};
			v[2 * i + 1] = {dude.HoverTo3D(p.x - n.y * h, p.y + n.x * h, p.z, false), {0.5f, vCoord}, argb};
		}
		// (inferred) the 62 triangles of the index list 0xD15788 as a strip (not read)
		for (size_t i = 0; i + 1 < points.size(); ++i)
		{
			for (const size_t k : {2 * i, 2 * i + 1, 2 * i + 2, 2 * i + 1, 2 * i + 3, 2 * i + 2})
			{
				out.push_back(v[k]);
			}
		}
	}
	return out;
}
