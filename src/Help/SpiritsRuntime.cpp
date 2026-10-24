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
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <exception>
#include <string>

#include <entt/core/hashed_string.hpp>
#include <glm/matrix.hpp>
#include <spdlog/spdlog.h>

#include "3D/FrameAnim.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/ObjectMatrix.h"
#include "Audio/Audio.h"
#include "Audio/Game/Banks.h"
#include "Camera/Camera.h"
#include "Common/TruncateToInt.h"
#include "ECS/Components/Transform.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/ScriptStateInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Game.h"
#include "GameClock.h"
#include "Graphics/ArgbColour.h"
#include "Graphics/OverlayFrame.h"
#include "Help/AdvisorVoices.h"
#include "Help/HelpSystem.h"
#include "Input/GameCursor.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;
using namespace openblack::help;
using namespace openblack::help::spirits;

namespace
{
/// What this module keeps between calls (Locator::scriptState)
struct SpiritsRuntimeState
{
	std::unique_ptr<Runtime> runtime {};
};

SpiritsRuntimeState& SpiritsRuntimeData()
{
	return openblack::Locator::scriptState::value().Get<SpiritsRuntimeState>();
}

/// The .hd names of the good and evil spirits; the files on the disc are lower case
constexpr std::array<const char*, k_Dudes> k_Files = {"markgood.hd", "markevil.hd"};

/// Loads a spirit's .hd file through the file system, then LoadRestSkeleton (the L3D0's bones)
void LoadAssets(const char* name, DudeAssets& out)
{
	std::string error;
	try
	{
		auto& fileSystem = Locator::filesystem::value();
		const auto& bytes =
		    resources::LoadBlob(Locator::resources::value().GetBlobs(),
		                        fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Data>() / "HelpSprite" / name));
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
	out.data = DudeData::FromFile(out.file);
	out.loaded = true;
}

/// The advisors' voices (Locator::helpTextSystem); stops with a message when there are none
help::AdvisorVoices& TheAdvisorVoices()
{
	if (!Locator::helpTextSystem::has_value())
	{
		std::fputs("spirits: no help texts in the locator (Locator::helpTextSystem)\n", stderr);
		std::abort();
	}
	return Locator::helpTextSystem::value().GetVoices();
}
} // namespace

glm::mat4 spirits::ModelOf(const glm::mat3& rows, const glm::vec3& position)
{
	return affine::Model(position, rows, glm::vec3(1.0f)); // the rows as they are (scale 1), the position
}

Runtime* spirits::Get()
{
	// nothing without the script state (the unit tests that make no services)
	return openblack::Locator::scriptState::has_value() ? SpiritsRuntimeData().runtime.get() : nullptr;
}

void spirits::Start()
{
	SpiritsRuntimeData().runtime = std::make_unique<Runtime>();
}

void spirits::Shutdown()
{
	SpiritsRuntimeData().runtime.reset();
}

Runtime::Runtime()
{
	for (int i = 0; i < k_Dudes; ++i)
	{
		LoadAssets(k_Files.at(static_cast<size_t>(i)), _assets.at(static_cast<size_t>(i)));
	}
	RefreshView();
	// Both dudes are created at home. Without a file the dude has no
	// clip and no mesh: the opcodes still run its states, nothing is drawn
	_control = std::make_unique<AdvisorSpiritController>(_assets.at(k_GoodDude).data, _assets.at(k_EvilDude).data,
	                                                     MakeQueries(), _screen);
}

Runtime::~Runtime()
{
	if (!Locator::audio::has_value())
	{
		return;
	}
	for (const uint32_t owner : _soundOwner)
	{
		if (owner != 0)
		{
			audio::UnregisterObject(owner);
		}
	}
}

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
	// the random streams: unset, so game_random's
	q.isTalking = [](int dude) { return TheAdvisorVoices().IsTalking(dude); };
	q.talkedRecently = [](int dude) { return TheAdvisorVoices().TalkingOrJustStopped(dude); };
	q.sayActive = [](int dude) { return TheAdvisorVoices().IsActive(dude); };
	q.updateSentence = [](int dude) { TheAdvisorVoices().UpdateSaySentence(dude); };
	q.lipSync = [](int dude, float dt) { return TheAdvisorVoices().ApplyLipSync(dude, dt); };
	// the camera of the last view refresh: the eye, the world to camera rotation, the lens and the near clip
	q.pointFromScreen = [this](glm::vec2 pixel, float depth) {
		return screen_point::PointFromScreen(_frame, _lens, TruncateToInt(pixel.x), TruncateToInt(pixel.y), depth);
	};
	q.worldToPixel = [this](const glm::vec3& p, bool force) -> std::optional<glm::vec2> {
		// (inferred) one clip flag is "behind the eye", the others off the
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
		return ProjectedPoint {TruncateToInt(projected->pixel.x), TruncateToInt(projected->pixel.y), projected->depth};
	};
	q.nearClip = [this]() { return _frame.nearZ; };
	q.cameraAxes = [this]() { return glm::mat3(_frame.right, _frame.up, _frame.forward); };
	q.cameraPosition = [this]() { return _frame.eye; };
	// the advisors' anim sounds: the in-game bank, and each sound played as the update finds it
	q.soundBank = []() { return Locator::audioState::has_value() && audio::Bank(audio::SfxBank::InGame) != audio::k_NoBank; };
	q.playAnimSound = [this](int dude, const AnimSound& sound) { PlayAnimSound(dude, sound); };
	q.altitude = [](float x, float z) {
		return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt({x, z}) : 0.0f;
	};
	q.headAngles = [this](int dude, const glm::mat3& rows, const glm::vec3& position, const glm::vec3& target) {
		return HeadAngles(dude, rows, position, target);
	};
	q.fingertip = [this](int dude, const glm::mat3& rows, const glm::vec3& position) {
		return Fingertip(dude, rows, position);
	};
	// a valid entity with a Transform; (x, y, z) world (openblack's y already has the altitude in it) and the
	// object's height
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
	// the real frame time in seconds
	input.dt = static_cast<float>(game_clock::FrameRealMs()) * 0.001f;
	// the trails move on by the whole frame time in ms x 0.01
	input.trailDt = static_cast<float>(game_clock::FrameRealMs()) * 0.01f;
	// the real frame time inside the citadel, where the game is paused and its time step is 0, the game time step
	// otherwise
	input.frameMs = static_cast<int32_t>(game_clock::ClampedFrameMs(game_clock::IsInsideCitadel()));
	input.screen = _screen;
	if (Locator::inputState::has_value())
	{
		input.mouse = openblack::input::GameCursor();
	}
	input.tickMs = audio::TickCount();
	if (const auto* helpSystem = help::Get(); helpSystem != nullptr)
	{
		input.wideScreen = helpSystem->GetWideScreen() != 0;
	}
	_control->Update(input);

	for (int d = 0; d < k_Dudes; ++d)
	{
		const AdvisorSpirit& dude = _control->Dude(d);
		// the bones (hierarchy x M) for the draw
		EvaluatePose(d, dude.Layers().size(), ModelOf(dude.Rows(), dude.Position()), _bones.at(static_cast<size_t>(d)));
		// the voice's delay reads the hover x (help::AdvisorVoices::Say)
		TheAdvisorVoices().SetHover(d, dude.HoverX().value);
	}
}

void Runtime::PlayAnimSound(int dude, const AnimSound& sound)
{
	if (!Locator::audio::has_value() || !Locator::audioState::has_value())
	{
		return;
	}
	const auto index = static_cast<size_t>(dude);
	auto& owner = _soundOwner.at(index);
	if (owner == 0)
	{
		owner = audio::NewObjectId();
	}
	// The sound is not tracked, so its point is asked once, as it starts: the dude's position when it was found
	_soundPoint.at(index) = sound.position;
	audio::RegisterObject(owner, [this, index]() -> std::optional<glm::vec3> { return _soundPoint.at(index); });
	audio::PlayAnimationEffect(audio::Owner::Object(owner), sound.distance, sound.key,
	                           static_cast<audio::AnimAction>(sound.action), audio::Bank(audio::SfxBank::InGame), false, 0.0f,
	                           0.0f);
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
		// Kept in the mesh cache, read from the dude's bytes the first time a game asks for it
		const std::string name = k_Files.at(static_cast<size_t>(dude));
		const auto id = entt::hashed_string(("help/spirit/" + name).c_str()).value();
		std::shared_ptr<graphics::L3DMesh> mesh;
		if (Locator::resources::has_value())
		{
			auto& meshes = Locator::resources::value().GetMeshes();
			try
			{
				if (!meshes.Contains(id))
				{
					meshes.Load(id, resources::L3DLoader::FromBufferTag {}, name, assets.file.mesh);
				}
				mesh = meshes.Handle(id).handle();
			}
			catch (const std::exception&)
			{
				mesh.reset();
			}
		}
		if (mesh != nullptr && mesh->GetBoneMatrices().size() == assets.rest.parents.size())
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
	const SpiritAnimClip* stand = assets.data.clips[anim::k_Stand];
	// the fill for bones a clip lacks: the stand clip's key 0
	const SpiritAnimKey* fill = stand != nullptr && !stand->frames.empty() ? stand->frames.data() : nullptr;
	const auto clipOf = [&assets](uint32_t i) { return i < k_AnimSlots ? assets.data.clips[i] : nullptr; };
	const auto& layers = _control->Dude(dude).Layers();
	for (size_t i = 0; i < count && i < layers.size(); ++i)
	{
		const AnimLayer& layer = layers[i];
		if (layer.kind == AnimLayer::Kind::ScaleEyes)
		{
			ScaleEyes(assets.data.faceBones, layer.blend, layer.blendB, local);
			continue;
		}
		const SpiritAnimClip* clip = clipOf(layer.clip);
		if (clip == nullptr)
		{
			continue;
		}
		switch (layer.kind)
		{
		case AnimLayer::Kind::Set:
			SetPose(*clip, layer.milliseconds, rest, layer.fill ? fill : nullptr, local);
			break;
		case AnimLayer::Kind::SetBlend:
		{
			// both poses, each without fill, lerped per float in absolute bone space (inferred: the hierarchy to
			// absolute and back)
			const SpiritAnimClip* other = clipOf(layer.clipB);
			if (other == nullptr)
			{
				SetPose(*clip, layer.milliseconds, rest, nullptr, local);
				break;
			}
			std::vector<glm::mat4> a = local;
			std::vector<glm::mat4> b = local;
			SetPose(*clip, layer.milliseconds, rest, nullptr, a);
			SetPose(*other, layer.millisecondsB, rest, nullptr, b);
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
		case AnimLayer::Kind::ScaleEyes:
			break;
		}
	}
	// (pending) the pupil UVs on the eye bones' vertices: not drawn
	ComposeWorld(rest, local, model, world);
}

std::optional<glm::vec2> Runtime::HeadAngles(int dude, const glm::mat3& rows, const glm::vec3& position,
                                             const glm::vec3& target) const
{
	// The head bone's frame (this frame's pose so far under M) with its origin moved to the middle of the two eye
	// bones, inverted; the target in it, nothing if behind (local y < 0), normalised; yaw = atan2(z, y),
	// pitch = asin(x), each clamped to +-1.0472 (60 degrees) and x 0.477465
	const auto& assets = _assets.at(static_cast<size_t>(dude));
	std::vector<glm::mat4> world;
	EvaluatePose(dude, _control->Dude(dude).Layers().size(), ModelOf(rows, position), world);
	const uint32_t eyeA = assets.data.faceBones[3];
	const uint32_t eyeB = assets.data.faceBones[0];
	const uint32_t head = assets.data.faceBones[6];
	if (eyeA >= world.size() || eyeB >= world.size() || head >= world.size())
	{
		return glm::vec2(0.0f);
	}
	glm::mat4 frame = world[head];
	frame[3] = glm::vec4((glm::vec3(world[eyeA][3]) + glm::vec3(world[eyeB][3])) * 0.5f, 1.0f);
	const glm::mat4 inverse(affine::Inverse(glm::mat4x3(frame)));
	glm::vec3 l = glm::vec3(inverse * glm::vec4(target, 1.0f));
	if (l.y < 0.0f)
	{
		return std::nullopt; // behind: the head angles stay at 0
	}
	if (l.x != 0.0f || l.y != 0.0f || l.z != 0.0f)
	{
		l *= 1.0f / std::sqrt(l.x * l.x + l.y * l.y + l.z * l.z);
	}
	constexpr float k_Limit = 1.04719758f; // 60 degrees
	const float yaw = std::clamp(std::atan2(l.z, l.y), -k_Limit, k_Limit);
	const float pitch = std::clamp(std::asin(std::clamp(l.x, -1.0f, 1.0f)), -k_Limit, k_Limit);
	return glm::vec2(yaw, pitch) * 0.477465f;
}

glm::vec3 Runtime::Fingertip(int dude, const glm::mat3& rows, const glm::vec3& position) const
{
	// (bone matrix 0 x M).pos + r0 (fingertipOffsetRow0 x model size) + r2 (fingertipOffsetRow2 x model size)
	// (approximate operand mapping)
	const auto& assets = _assets.at(static_cast<size_t>(dude));
	std::vector<glm::mat4> world;
	EvaluatePose(dude, _control->Dude(dude).Layers().size(), ModelOf(rows, position), world);
	const glm::vec3 root = world.empty() ? position : glm::vec3(world[0][3]);
	return root + rows[0] * (assets.data.fingertipOffsetRow0 * assets.data.modelSize) +
	       rows[2] * (assets.data.fingertipOffsetRow2 * assets.data.modelSize);
}


std::pair<uint32_t, uint32_t> Runtime::WorldColour(const land_light::Sample& sample, float blend)
{
	if (blend == 0.0f)
	{
		return {0xFFFFFFFFu, 0u};
	}
	// the byte rule as the original's: white lerped to the land diffuse by w, the land specular scaled by it
	const auto w = static_cast<uint32_t>(TruncateToInt(255.0f * Smooth(blend)));
	const uint32_t colour = land_light::LerpBytes(0xFFFFFFFFu, sample.diffuse, static_cast<int>(w)) | 0xFF000000u;
	const uint32_t specular = argb_colour::ScaleRgbShift8KeepAlpha(sample.specular, w);
	return {colour, specular};
}

void Runtime::FillOverlay(graphics::OverlayFrame& frame)
{
	// the order of the draw: the world's dudes (the frame), then the end of frame callback's (the
	// overlay); each dude is in one of the two, so the halo clock moves once per drawn halo as before
	frame.spirits.clear();
	Draws(false, frame.spirits);
	Draws(true, frame.spirits);
	frame.spiritTrails.clear();
	for (const auto& v : TrailTriangles())
	{
		frame.spiritTrails.push_back({v.position.x, v.position.y, v.position.z, v.uv.x, v.uv.y, argb_colour::ToAbgr(v.argb)});
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
		const AdvisorSpirit& dude = _control->Dude(d);
		const float blend = dude.InWorld();
		if ((blend < 0.5f) != overlay)
		{
			continue;
		}
		const auto& assets = _assets.at(static_cast<size_t>(d));
		const auto& bones = _bones.at(static_cast<size_t>(d));
		graphics::SpiritOverlay draw {
		    .advisor = d,
		    .overlay = overlay,
		    .mesh = Mesh(d),
		    .bones = bones, // a copy: the draw reads no live pose
		    .alpha = static_cast<uint8_t>(std::clamp(dude.AlphaByte(), 0, 255)),
		    .inWorld = blend,
		};
		// The pupils once the face has set them: the first eye's on its first bone, the second's on its first
		if (const auto pupils = dude.Pupils(); pupils)
		{
			for (size_t eye = 0; eye < 2; ++eye)
			{
				draw.pupils.at(eye) = glm::vec4(static_cast<float>(assets.data.faceBones.at(eye * 3)), pupils->at(eye), 1.0f);
			}
		}
		draw.pupilCentre = glm::vec4(assets.data.pupilCentre, d == k_EvilDude ? 1.0f : 0.0f, 0.0f);
		if (blend != 0.0f)
		{
			// the land light at the model's position (inferred), with the Renderer's land light table of the
			// frame (WorldColour there; white and 0 without a table)
			draw.landLightPoint = glm::vec2(dude.Position().x, dude.Position().z);
		}
		if (overlay)
		{
			// the point from screen (0, H/2) at depth -10, towards the eye by 2 blend while blend < 0.5
			glm::vec3 light = screen_point::PointFromScreen(_frame, _lens, 0, _screen.HalfHeight(), -10.0f);
			light += (_frame.eye - light) * (blend + blend);
			draw.lightPosition = light;
		}
		// a screen sprite (mode A; nothing at or before the near plane) with the camera of the last Update,
		// two triangles per sprite. The sprites, the halo and the puff quads are built with the camera the spirits'
		// Update read (_frame / _lens), which may differ from the camera finally drawn this frame (as before)
		const auto addSprite = [this, &draw](const graphics::billboard::Sprite& sprite) {
			const auto quad = graphics::billboard::SpriteQuad(sprite, _frame);
			if (!quad)
			{
				return;
			}
			const uint32_t abgr = argb_colour::ToAbgr(sprite.argb);
			for (const int i : graphics::billboard::k_SpriteTriangles)
			{
				const auto& p = quad->corners.at(static_cast<size_t>(i));
				const auto& uv = quad->uv.at(static_cast<size_t>(i));
				draw.sprites.push_back({p.x, p.y, p.z, uv.x, uv.y, abgr});
			}
		};
		// the object's position, set from bone 0 (angle 0, scale 1.0) with the bones: halo and puff anchor
		const glm::vec3 anchor = bones.empty() ? dude.Position() : glm::vec3(bones[0][3]);
		// a non-zero halo scale and alpha byte (only then the halo clock moves on)
		if (assets.file.haloScale != 0.0f && draw.alpha != 0)
		{
			graphics::billboard::Sprite halo;
			// (inferred) the object matrix is the identity at the anchor (angle 0, scale 1.0), so the offset is in world axes
			halo.position = anchor + assets.file.haloOffset;
			halo.size = assets.data.scale * assets.file.haloScale * dude.ModelScale() * 100.0f;
			halo.height = 0.3f;
			const int32_t step = static_cast<int32_t>(game_clock::ClampedFrameMs(game_clock::IsInsideCitadel()));
			// (halo clock / 200) & 15
			halo.cell = graphics::frame_anim::HelpSystemCell(_haloClockMs, step);
			halo.argb = static_cast<uint32_t>(draw.alpha) << 24 | 0xFFFFFFu;
			addSprite(halo); // the halo, then the puff
		}
		if (const auto& particles = dude.PuffParticles(); particles && dude.PuffRunning())
		{
			// with this frame's step of the logic: the alpha of the age before the step, the size, the angle and the
			// cell of the age after it, the position of vy before the drift
			// the anchor in hover space (forced)
			const auto hover = dude.WorldToHover(anchor, true).value_or(dude.Hover());
			const float fade = dude.PuffFade();
			const uint32_t mask = d == k_EvilDude ? 0x7F1F1Fu : 0xFFFFFFu; // the evil puff is dark red
			for (size_t i = 0; i < particles->size(); ++i)
			{
				const PuffParticle& p = particles->at(i);
				if (p.drawAlpha <= 0)
				{
					continue;
				}
				graphics::billboard::Sprite sprite;
				// (2 a) / 4 (signed division) << 24 + grey & mask
				sprite.argb = (static_cast<uint32_t>((p.drawAlpha * 2) / 4) << 24) + (p.grey & mask);
				// ((sizeBase - ((k age) fade) 0.5) + 1) 0.12, at least 0.0001
				float size = ((p.sizeBase - p.k * p.age * fade * 0.5f) + 1.0f) * 0.12f;
				if (size < 0.0001f)
				{
					size = 0.0001f;
				}
				sprite.size = size;
				sprite.angle = p.spin * p.age + static_cast<float>(i);
				sprite.cell = graphics::frame_anim::SpriteCell(TruncateToInt(p.age * 8.0f) & 15);
				// hover to 3D of (hx + vx 0.1, hy + vy 0.1, -depth - 0.6): the dude's depth
				// channel, not the particle's own
				sprite.position = dude.HoverTo3D(hover.x + p.velocity.x * 0.1f, hover.y + p.drawVelocityY * 0.1f,
				                                 -dude.DepthChannel().value - 0.6f, false);
				addSprite(sprite);
			}
		}
		out.push_back(std::move(draw));
	}
}

std::vector<WorldTrailVertex> Runtime::TrailTriangles() const
{
	std::vector<WorldTrailVertex> out;
	const auto indices = TrailIndices();
	for (int d = 0; d < k_Dudes; ++d)
	{
		const AdvisorSpirit& dude = _control->Dude(d);
		// only a dude out and wholly near the screen draws its trail
		if (_control->State(d) == ControlState::Home || dude.InWorld() != 0.0f)
		{
			continue;
		}
		const auto strip = dude.TrailStrip();
		std::array<WorldTrailVertex, 2 * Trail::k_Points> vertices {};
		for (size_t i = 0; i < strip.size(); ++i)
		{
			const auto& v = strip.at(i);
			vertices.at(i) = {dude.HoverTo3D(v.hover.x, v.hover.y, v.hover.z, false), v.uv, v.argb};
		}
		for (const uint16_t k : indices)
		{
			out.push_back(vertices.at(k));
		}
	}
	return out;
}
