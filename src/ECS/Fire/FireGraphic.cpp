/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FireGraphic.h"

#include <cmath>

#include <algorithm>
#include <list>
#include <memory>
#include <random>
#include <unordered_map>
#include <array>
#include <vector>

#include <spdlog/spdlog.h>

#include "3D/FrameAnim.h"
#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandIslandInterface.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Life.h"
#include "ECS/Registry.h"
#include "ECS/Weather/Weather.h"
#include "FireEffect.h"
#include "FireObjectTraits.h"
#include "Locator.h"
#include "PSys/PSysManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::fire;

namespace
{
constexpr float k_FlameLife = 4.3f;  // 0x999664
constexpr float k_FlameFadeIn = 1.0f; // 0x999660
constexpr float k_PuffLife = 3.0f;   // 0x999680 / 0x99969C
constexpr uint32_t k_BurstTurns = 30; // 0x999694 / 0x9996B4

/// FireGraphic::SpritePos (0x48 bytes, vtable 0x9998D4)
struct SpritePos
{
	glm::vec3 position {0.0f}; ///< +0x1C local (flames) or world (puffs)
	float scale {1.0f};        ///< +0x28
	float age {0.0f};          ///< +0x2C
	uint8_t alpha {255};       ///< +0x30
	int index {0};             ///< +0x34 the mesh triangle
	glm::vec3 velocity {0.0f}; ///< +0x38
	float baseScale {1.0f};    ///< +0x44
};

struct Graphic
{
	entt::entity object {entt::null};
	/// +0xB5: bit 0 the 3D object morphs with the land, bit 1 flames, bit 2 smoke when it goes out, bit 3 steam while cooled, bit 4 light map
	uint8_t flags {0x1E};
	float flameAccumulator {0.0f}; ///< +0x38
	int flameCount {0};            ///< +0x3C
	bool veryHotDone {false};      ///< +0xA8
	float steamAccumulator {0.0f}; ///< +0x40
	int steamCount {0};            ///< +0x44
	uint32_t steamStart {0};       ///< +0x48
	float steamTemperature {0.0f}; ///< +0x4C
	float smokeAccumulator {0.0f}; ///< +0x50
	int smokeCount {0};            ///< +0x54
	uint32_t smokeStart {0};       ///< +0x58
	glm::vec3 smokeLocal {0.0f};   ///< +0x5C
	int smokeIndex {0};            ///< +0x68
	int maxFlames {0};             ///< +0x6C GetPSysFireMaxFlames
	float localScale {1.0f};       ///< +0x70 GetPSysFireLocalFlameScale
	float scaleMultiplier {1.0f};  ///< +0xA4
	std::list<SpritePos> flames;   ///< +0xB8, newest first
	std::list<SpritePos> steam;    ///< +0xC0
	std::list<SpritePos> smoke;    ///< +0xC8
};

std::unordered_map<uint32_t, std::unique_ptr<Graphic>> g_Graphics;
uint32_t g_Turn = 0;
std::mt19937 g_Random(0x5EED); // (aproximado: not GRand::LocalRand 0x6DE590's generator nor its seed)
bool g_SourceAdded = false;

/// GRand::LocalRand / LocalFloatRand (aproximado: another generator, so the sequences differ from the original)
float LocalFloatRand(float max)
{
	return std::uniform_real_distribution<float>(0.0f, 1.0f)(g_Random) * max;
}
int LocalRand(int count)
{
	return count > 0 ? static_cast<int>(g_Random() % static_cast<uint32_t>(count)) : 0;
}

psys::Creator MakeCreator(const char* texture, bool additive, float originY, float stretch)
{
	psys::Creator creator;
	creator.kind = psys::Creator::Kind::Sprite;
	creator.className = "FireGraphic";
	creator.texture = texture;
	// the frame is the cell (the sprite flags' low 6 bits, 0x7323B7..0x7323CA), 8 cells a row (LH3DSprite +0x30, 8 by
	// SetToZero 0x8404F0); (inferido) that S_Fire is drawn 8 x 8 like S_SpriteSheet3. The flame cells (fmod(.., 32) +
	// 32, frame_anim::FireCell) need 64 frames: PSysFrameIndex then keeps them as they are
	creator.spritesPerRow = 8;
	creator.numFrames = 64;
	creator.additive = additive;
	creator.originY = originY;
	creator.stretch = stretch;
	return creator;
}

/// The materials 0xDA09EC (S_Fire, mode 13), 0xDA09F0 (S_SpriteSheet3, 13), 0xDA09F4 (S_SpriteSheet3, 6); the shared
/// LH3DSprite 0xDA09E8 has size 1 and height 2 (Create 0x73130F / 0x731316). Each flame sets +0x18 = 0 and +0x1C =
/// height x size x -1 ([0x8AB678]) = -2 x size (0x732382..0x732395): LH3DSprite::Draw subtracts it, so the flame's
/// local y runs from 0 to 4 x size on the screen's up and its base stays on the point. As a PSys creator that is
/// SpriteOriginY = -1 (oy = OriginY x size x stretch).
const psys::Creator& FlameCreator()
{
	static const auto creator = MakeCreator("S_Fire", true, -1.0f, 2.0f);
	return creator;
}
/// (aproximado) The steam and smoke are drawn centred (origin 0). The original's fn_007323F0 / fn_007324E0 never write
/// +0x18 / +0x1C of the shared sprite 0xDA09E8, so they keep the oy = -2 x size of the last flame drawn with it:
/// fn_00732200 draws a fire's flames (fn_00732220, newest first, so the oldest last), then its steam, then its smoke,
/// and a fire with no flame takes the last flame of the fire drawn before it. In the original they sit 2 x that size
/// higher on the screen's up. Not ported: psys::manager::DrawAtom has no per-atom origin, and the order of the fires
/// between them is not read
const psys::Creator& SteamCreator()
{
	static const auto creator = MakeCreator("S_SpriteSheet3", true, 0.0f, 2.0f);
	return creator;
}
const psys::Creator& SmokeCreator()
{
	static const auto creator = MakeCreator("S_SpriteSheet3", false, 0.0f, 2.0f);
	return creator;
}

const graphics::L3DMesh* MeshOf(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* mesh = registry.TryGet<const components::Mesh>(object);
	if (mesh == nullptr || !Locator::resources::has_value())
	{
		return nullptr;
	}
	auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(mesh->id))
	{
		return nullptr;
	}
	return &*meshes.Handle(mesh->id);
}

/// Object::GetPSysFireLocalRndFlamePos 0x732770 (the plain object path, fn_00590460): a random point of a random
/// triangle of the mesh, in the mesh's own coordinates; trees x 0.5. False without a mesh. (inferido: fn_00590460's
/// sampling is not read step by step: the triangle choice, the a + b > 1 fold and the box-centre fallback are the
/// port's)
bool LocalRandomFlamePosition(entt::entity object, glm::vec3& position, int& index)
{
	const auto* mesh = MeshOf(object);
	if (mesh == nullptr)
	{
		position = glm::vec3(0.0f);
		return false;
	}
	// the LOD 0 triangles (inf: fn_0080B2A0 counts the drawn mesh's triangles)
	std::vector<std::array<glm::vec3, 3>> triangles;
	for (const auto& subMesh : mesh->GetSubMeshes())
	{
		if ((subMesh->GetFlags().lodMask & 1) == 0 && mesh->GetSubMeshes().size() > 1)
		{
			continue;
		}
		const auto& p = subMesh->GetCollisionPositions();
		const auto& indices = subMesh->GetCollisionIndices();
		for (size_t i = 0; i + 2 < indices.size(); i += 3)
		{
			if (indices[i] < p.size() && indices[i + 1] < p.size() && indices[i + 2] < p.size())
			{
				triangles.push_back({p[indices[i]], p[indices[i + 1]], p[indices[i + 2]]});
			}
		}
	}
	if (triangles.empty())
	{
		position = mesh->GetBoundingBox().Center();
		index = 0;
	}
	else
	{
		index = LocalRand(static_cast<int>(triangles.size()));
		float a = LocalFloatRand(1.0f);
		float b = LocalFloatRand(1.0f);
		if (a + b > 1.0f)
		{
			a = 1.0f - a;
			b = 1.0f - b;
		}
		const auto& t = triangles[static_cast<size_t>(index)];
		position = t[0] + (t[1] - t[0]) * a + (t[2] - t[0]) * b;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.AnyOf<components::Tree, components::DeadTree>(object))
	{
		position *= 0.5f; // IsAnyKindOfTree
	}
	return true;
}

/// Object::GetPSysFireWorldFlamePos 0x732660: the object's matrix x the local point
glm::vec3 WorldFlamePosition(entt::entity object, const glm::vec3& local)
{
	const auto* transform = Locator::entitiesRegistry::value().TryGet<const components::Transform>(object);
	if (transform == nullptr)
	{
		return local;
	}
	return transform->position + transform->rotation * (transform->scale * local);
}

glm::vec3 ObjectPosition(entt::entity object)
{
	const auto* transform = Locator::entitiesRegistry::value().TryGet<const components::Transform>(object);
	return transform != nullptr ? transform->position : glm::vec3(0.0f);
}

float LandAt(float x, float z)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)) : 0.0f;
}

/// fn_007317F0: the flames
void UpdateFlames(Graphic& graphic, const FireEffect& fire, float dt)
{
	const float fraction = fire.FireFraction();
	if (fraction != 0.0f)
	{
		const auto maximum = static_cast<float>(graphic.maxFlames);
		graphic.flameAccumulator += maximum / k_FlameLife * fraction * dt;
		if ((fire.flags & FireEffect::VeryHot) != 0 && !graphic.veryHotDone)
		{
			graphic.veryHotDone = true;
			graphic.flameAccumulator += maximum;
		}
		if ((fire.flags & FireEffect::JustIgnited) != 0)
		{
			graphic.flameAccumulator += static_cast<float>((graphic.maxFlames + 1) / 2);
		}
	}
	while (static_cast<float>(graphic.flameCount) < graphic.flameAccumulator)
	{
		++graphic.flameCount;
		SpritePos flame;
		if (LocalRandomFlamePosition(graphic.object, flame.position, flame.index))
		{
			graphic.flames.push_front(flame);
		}
	}
	const float fadeOut = 1.0f / (k_FlameLife - k_FlameFadeIn);
	for (auto it = graphic.flames.begin(); it != graphic.flames.end();)
	{
		it->age += dt;
		if (it->age > k_FlameLife)
		{
			it = graphic.flames.erase(it);
			continue;
		}
		it->scale = (fraction + 1.0f) * 0.5f * graphic.localScale; // 0.5 [0x8AA3B4] (fn_007317F0 0x7319D0)
		float alpha = it->age < k_FlameFadeIn ? (1.0f / k_FlameFadeIn) * it->age : 1.0f - (it->age - k_FlameFadeIn) * fadeOut;
		alpha = std::clamp(alpha, 0.0f, 1.0f) * 250.0f; // 250 [0x8C7B2C] (0x731A34)
		it->alpha = static_cast<uint8_t>(std::clamp(alpha, 0.0f, 255.0f));
		++it;
	}
}

/// The puffs of fn_00731AB0 (steam: while cooled, white) and fn_00731E50 (smoke: once it went out, grey): 4 a second for
/// 30 turns, grow from 0.2 to 2.6 x their scale and fade from `alpha0` in 3 s, drift with half the wind and rise
void UpdatePuffs(std::list<SpritePos>& puffs, float dt, float alpha0, float rise, const glm::vec3& at)
{
	// fn_00771B10(FireGraphic +0x98, 1): the wind at the object (ECS/Weather)
	const glm::vec3 wind = weather::GetWindAt(at, true);
	const float inverseLife = 1.0f / k_PuffLife;
	for (auto it = puffs.begin(); it != puffs.end();)
	{
		it->age += dt;
		if (it->age > k_PuffLife)
		{
			it = puffs.erase(it);
			continue;
		}
		const float t = inverseLife * it->age;
		// 2.6 / 0.2: steam [0x999688] / [0x999684] (0x731D2D), smoke [0x9996A8] / [0x9996A4] (0x73209A)
		it->scale = ((2.6f - 0.2f) * t + 0.2f) * it->baseScale;
		it->alpha = static_cast<uint8_t>(std::lrint((0.0f - alpha0) * t + alpha0)); // fistp
		// fn_00731AB0 0x731D71..0x731DB9 (smoke 0x7320DE..0x732126): 0.5 [0x999674], 0.1 [0x999678]
		const glm::vec3 pull = (wind * 0.5f - it->velocity) * 0.1f * dt;
		it->velocity += pull;
		it->velocity.y += rise * dt;
		it->position += it->velocity * (dt * it->baseScale);
		++it;
	}
}

SpritePos NewPuff(const Graphic& graphic, const glm::vec3& world, int index)
{
	SpritePos puff;
	puff.position = world;
	puff.index = index;
	puff.baseScale = graphic.scaleMultiplier * graphic.localScale;
	const float x = LocalFloatRand(2.0f) - 1.0f;
	const float z = LocalFloatRand(2.0f) - 1.0f;
	puff.velocity = glm::vec3(x * 1.0f, 0.0f, z * 1.0f);
	return puff;
}

void UpdateSteam(Graphic& graphic, const FireEffect& fire, float dt)
{
	if (graphic.steamStart == 0)
	{
		if ((fire.flags & FireEffect::Cooling) != 0 && fire.temperature > 75.0f && // 75 [0x999638] (0x731AD9)
		    fire.temperature > graphic.steamTemperature)
		{
			graphic.steamStart = g_Turn;
			graphic.steamCount = 0;
			graphic.steamAccumulator = 0.0f;
			graphic.steamTemperature = fire.temperature;
			// fn_007314E0: the sizzle (GGlobal +0x3AC, options 0x35) TODO(sound)
		}
	}
	else if (g_Turn > graphic.steamStart + k_BurstTurns)
	{
		graphic.steamStart = 0;
	}
	else
	{
		graphic.steamAccumulator += 4.0f * dt; // 4 [0x999670] (0x731B50)
		while (static_cast<float>(graphic.steamCount) < graphic.steamAccumulator)
		{
			++graphic.steamCount;
			glm::vec3 local;
			int index = 0;
			if (LocalRandomFlamePosition(graphic.object, local, index))
			{
				graphic.steam.push_front(NewPuff(graphic, WorldFlamePosition(graphic.object, local), index));
			}
		}
	}
	UpdatePuffs(graphic.steam, dt, 100.0f, 1.0f, ObjectPosition(graphic.object)); // alpha [0x999690], rise [0x99967C]
}

void UpdateSmoke(Graphic& graphic, const FireEffect& fire, float dt)
{
	if (graphic.smokeStart == 0)
	{
		if ((fire.flags & FireEffect::JustExtinguished) != 0)
		{
			graphic.smokeStart = g_Turn;
			graphic.smokeCount = 0;
			graphic.smokeAccumulator = 0.0f;
			LocalRandomFlamePosition(graphic.object, graphic.smokeLocal, graphic.smokeIndex);
		}
	}
	else if (g_Turn > graphic.smokeStart + k_BurstTurns)
	{
		graphic.smokeStart = 0;
	}
	else
	{
		graphic.smokeAccumulator += 4.0f * dt; // 4 [0x9996A0] (0x731ED7)
		while (static_cast<float>(graphic.smokeCount) < graphic.smokeAccumulator)
		{
			++graphic.smokeCount;
			graphic.smoke.push_front(
			    NewPuff(graphic, WorldFlamePosition(graphic.object, graphic.smokeLocal), graphic.smokeIndex));
		}
	}
	UpdatePuffs(graphic.smoke, dt, 180.0f, 2.0f, ObjectPosition(graphic.object)); // alpha [0x9996B0], rise [0x9996B8]
}

/// fn_00732200 through the renderer's PSys sprite path: one Z object per fire at the object (fn_007325D0)
void CollectFires(std::vector<psys::manager::Drawable>& out)
{
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto& [id, graphic] : g_Graphics)
	{
		const auto* transform = registry.TryGet<const components::Transform>(graphic->object);
		if (transform == nullptr)
		{
			continue;
		}
		psys::manager::Drawable drawable {transform->position, {}};
		// fn_00732220: flames, orange 0xFF713C, cell = int(fmod(-25 age, 32) + 32) (fn_007321B0, frame_anim::FireCell;
		// the age is SpritePos +0x2C, cell 32 at age 0)
		const float rockGround = (graphic->flags & 1) != 0 ? LandAt(transform->position.x, transform->position.z) : 0.0f;
		for (const auto& flame : graphic->flames)
		{
			auto position = WorldFlamePosition(graphic->object, flame.position);
			if ((graphic->flags & 1) != 0)
			{
				position.y += LandAt(position.x, position.z) - rockGround;
			}
			const auto frame = static_cast<float>(graphics::frame_anim::FireCell(flame.age));
			drawable.atoms.push_back({&FlameCreator(), position, glm::mat3(1.0f), flame.scale * graphic->scaleMultiplier,
			                          2.0f, static_cast<float>(flame.alpha), frame, {0xFF, 0x71, 0x3C}});
		}
		// fn_007323F0 / fn_007324E0: white steam (additive), grey smoke (alpha 6), cell = int(fmod(25 age, 32))
		// (SteamGetOffsetFromAge 0x7321E0 at 0x7324B2 and fn_0073250A 0x7325A2, frame_anim::SteamCell)
		for (const auto& puff : graphic->steam)
		{
			const auto frame = static_cast<float>(graphics::frame_anim::SteamCell(puff.age));
			drawable.atoms.push_back({&SteamCreator(), puff.position, glm::mat3(1.0f), puff.scale, 2.0f,
			                          static_cast<float>(puff.alpha), frame, {0xFF, 0xFF, 0xFF}});
		}
		for (const auto& puff : graphic->smoke) // grey: or 0x707070 (0x732593)
		{
			const auto frame = static_cast<float>(graphics::frame_anim::SteamCell(puff.age));
			drawable.atoms.push_back({&SmokeCreator(), puff.position, glm::mat3(1.0f), puff.scale, 2.0f,
			                          static_cast<float>(puff.alpha), frame, {0x70, 0x70, 0x70}});
		}
		if (!drawable.atoms.empty())
		{
			out.push_back(std::move(drawable));
		}
	}
}
} // namespace

void graphic::Create(FireEffect& fire)
{
	if (MeshOf(fire.object) == nullptr)
	{
		return; // no 3D object (Object +0x40)
	}
	if (!g_SourceAdded)
	{
		g_SourceAdded = true;
		psys::manager::AddDrawableSource(&CollectFires);
	}
	auto graphic = std::make_unique<Graphic>();
	graphic->object = fire.object;
	auto& registry = Locator::entitiesRegistry::value();
	// fn_00731460 defaults bits 1-4; bit 0 = the 3D object (Object +0x40, Game3DObject) IsMorphWithLand (its vt 0x1F0,
	// 0x7311FB; bw1-decomp LH3DObject.h); GetFireGPHXDrawn (vt 0x5E8) may clear them: MagicFireBall (0, 1, 0, 0).
	// (inf: openblack's MorphWithTerrain component stands for IsMorphWithLand)
	uint8_t flags = 0x1E;
	if (registry.AllOf<components::MorphWithTerrain>(fire.object))
	{
		flags |= 1;
	}
	if (fire::traits::IsObjectInMap(fire.object) == false)
	{
		flags = static_cast<uint8_t>((flags & 0xE1) | 0x04); // MagicFireBall 0x682DF0: only the smoke
	}
	graphic->flags = flags;
	// GetPSysFireMaxFlames 0x732A30: trees 2; else max(2D radius, height) < 3 ? 2 : 7
	const float radius = traits::Radius(fire.object);
	const float height = traits::Height(fire.object);
	const float largest = radius > height ? radius : height;
	const bool tree = registry.AnyOf<components::Tree, components::DeadTree>(fire.object);
	graphic->maxFlames = tree ? 2 : (largest < 3.0f ? 2 : 7);
	// GetPSysFireLocalFlameScale 0x732950: trees 0.2 x height, the rest 0.5 x height (impressive objects and citadel
	// parts 0.3, not ported)
	graphic->localScale = (tree ? 0.2f : 0.5f) * height;
	// TODO(M5): the light map S_LMFireBall (bit 4, Object +0x24 bit 1 and 2D radius > 2; GJBitmap 0xDA0960)
	g_Graphics[fire.id] = std::move(graphic);
}

void graphic::Destroy(FireEffect& fire)
{
	g_Graphics.erase(fire.id);
}

void graphic::Update(float seconds)
{
	for (auto& [id, graphic] : g_Graphics)
	{
		const auto* fire = Get(id);
		if (fire == nullptr || fire->object == entt::null)
		{
			continue;
		}
		// fn_00731560. (aproximado: the original updates a FireGraphic only when it is drawn, FireEffect::Draw 0x730330,
		// with a catch-up after 10 turns; here every fire is updated, so off-screen fires keep their flames and puffs)
		if ((graphic->flags & 2) != 0)
		{
			UpdateFlames(*graphic, *fire, seconds);
		}
		if ((graphic->flags & 8) != 0)
		{
			UpdateSteam(*graphic, *fire, seconds);
		}
		if ((graphic->flags & 4) != 0)
		{
			UpdateSmoke(*graphic, *fire, seconds);
		}
	}
}

void graphic::SetTurn(uint32_t turn)
{
	g_Turn = turn;
	// OPENBLACK_FIRE_TRACE: what each burning object is drawing, so that the flames can be checked without a screenshot
	if (turn % 20 != 0 || !TraceEnabled())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto& [id, graphic] : g_Graphics)
	{
		const auto* transform = registry.TryGet<const components::Transform>(graphic->object);
		const glm::vec3 position = transform != nullptr ? transform->position : glm::vec3(0.0f);
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "Fire: graphic of fire {} object {} at ({:.1f}, {:.1f}, {:.1f}): {} of {} flames (first scale "
		                   "{:.2f} alpha {}), {} steam, {} smoke",
		                   id, static_cast<int>(graphic->object), position.x, position.y, position.z, graphic->flames.size(),
		                   graphic->maxFlames, graphic->flames.empty() ? 0.0f : graphic->flames.front().scale,
		                   graphic->flames.empty() ? 0 : graphic->flames.front().alpha, graphic->steam.size(),
		                   graphic->smoke.size());
	}
}

void graphic::Clear()
{
	g_Graphics.clear();
}

std::optional<glm::u8vec3> graphic::TreeDrawColour(entt::entity object)
{
	const auto* fire = Find(object);
	if (fire == nullptr)
	{
		return std::nullopt;
	}
	// fn_0074B3A0: 50 by default, above 0.9 life max(50, 255 - (1 - life) x 2550), capped by the lighting global
	// 0xC22FA0 (Tree::PreDraw: 200 + 55 x the sun, inf: 255 here); the colour x that >> 8
	const float life = life::LifeOf(object);
	int grey = 50;
	if (life > 0.9f)
	{
		const float value = 255.0f - (1.0f - life) * 2550.0f;
		grey = value < 50.0f ? 50 : static_cast<int>(value);
	}
	grey = std::min(grey, 255);
	const auto g = static_cast<uint8_t>(grey);
	// the render mode 230 + heat x 25 / 255 (the glow material, OverrideRenderMode) is not ported
	return glm::u8vec3(g, g, g);
}

uint8_t graphic::CharringGrey(const FireEffect& fire)
{
	const int charring = static_cast<int>(fire.charring * 255.0f) & 0xFF;
	return static_cast<uint8_t>(255 - (charring * 175) / 256);
}

glm::u8vec3 graphic::CharringGlow(const FireEffect& fire, float turnTime)
{
	// VLNoise 0x590C30 at 0.6 x (turn + fraction) + (address & 0xFFFF): a smooth noise (inf, the id instead of the address)
	const float x = 0.6f * turnTime + static_cast<float>(fire.id & 0xFFFF);
	// (aproximado: two sines instead of VLNoise 0x590C30; the formula and its coefficients are the port's)
	const float noise = std::sin(x * 1.7f) * 0.6f + std::sin(x * 3.1f + 1.3f) * 0.4f;
	float value = 0.001f * fire.temperature * (1.0f + 0.2f * noise);
	value = std::clamp(value, 0.0f, 1.0f);
	const int c = static_cast<int>((1.0f - fire.charring) * value * 255.0f) & 0xFF;
	return glm::u8vec3(static_cast<uint8_t>(c * 180 / 256), static_cast<uint8_t>(c * 60 / 256), 0);
}
