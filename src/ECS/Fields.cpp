/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Fields.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <vector>

#include <glm/vec3.hpp>

#include <glm/geometric.hpp>

#include "Common/GameRandom.h"
#include "Camera/Camera.h"
#include "EngineConfig.h"
#include "InfoConstants.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/FishFarms.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Weather/Weather.h"
#include "Enums.h"
#include "Locator.h"

using namespace openblack::ecs::components;

namespace
{
/// Tree::PreDraw 0x74A7C0: 16 phases advancing at 1.06-2.12 rad/s, the speeds drawn again (Random(1, 2)) every 2 s
struct WindSwaySlots
{
	// 0xDA3A4C, past .data's raw end (0xC3D000), so 0 until the first draw: no sway for the first 2 s
	std::array<float, 16> speed {};
	std::array<float, 16> phase {};
	std::array<float, 16> lean {};
	float sinceSpeeds {0.0f}; ///< [0xDA579C] += g_game +0x205D48 (0x74A7D6), back to 0 once over 2000 (0x74A879)

	void Update(float milliseconds)
	{
		sinceSpeeds += milliseconds;
		if (sinceSpeeds > 2000.0f) // cmp 0x7D0; jle (0x74A7F4)
		{
			for (auto& s : speed)
			{
				s = openblack::game_random::crt::Random(1.0f, 2.0f); // 0x74A805, slot by slot
			}
			sinceSpeeds = 0.0f;
		}
		for (size_t i = 0; i < 16; ++i)
		{
			phase[i] += milliseconds * speed[i] * 0.00106061f;
			lean[i] = -0.03f * std::cos(phase[i]);
		}
	}
};

WindSwaySlots g_windSway;

/// BlendColor 0x5284C0: k = 0 gives a, 255 gives b, per channel (a (255 - k) + b k) / 255 truncated
glm::u8vec3 BlendColour(int k, glm::ivec3 a, glm::ivec3 b)
{
	k = std::clamp(k, 0, 255);
	return glm::u8vec3((a * (255 - k) + b * k) / 255);
}
} // namespace

glm::u8vec3 openblack::ecs::FieldDrawColour(const Field& field)
{
	// the startup constants 0x528440 / 0x528470 / 0x5284A0
	constexpr glm::ivec3 k_Olive(121, 145, 25);
	constexpr glm::ivec3 k_LightGreen(170, 212, 67);
	constexpr glm::ivec3 k_White(255, 255, 255);
	if (field.growth < Field::k_AgeGrowth)
	{
		return BlendColour(static_cast<int>(255.0f * (1.0f - field.food / Field::k_TotalFood)), k_Olive, k_LightGreen);
	}
	if (field.growth < Field::k_AgeRecolt)
	{
		return BlendColour(static_cast<int>(255.0f * (field.growth - Field::k_AgeGrowth) / (Field::k_AgeRecolt - Field::k_AgeGrowth)),
		                   k_Olive, k_White);
	}
	return glm::u8vec3(k_White);
}

float openblack::ecs::WindSway(uint32_t slot)
{
	return g_windSway.lean.at(slot & 15u);
}

void openblack::ecs::ProcessFieldsTurn(uint32_t turn)
{
	auto& registry = Locator::entitiesRegistry::value();
	const float speed = Locator::config::value().fieldGrowthMultiplier;
	registry.Each<Field>([&](entt::entity entity, Field& field) {
		// Abode::Process 0x404440 first (0x529026): nothing visible for a field (abode_villagers::RunsAbodeProcess
		// leaves fields out: Field::ReduceLife 0x52A0A0 does no damage). (approximate) its +0xB0 / +0xB9 are not
		// advanced (no reader found)
		// 0x52902B..0x529047: (turn g_game +0x205A40 + +0x11C) % 10, unsigned div
		if ((turn + field.turnOffset) % 10 != 0)
		{
			return;
		}
		// 0x52904F..0x529057: Object::IsOnFire 0x637CC0 (a fire effect at +0x44 that is on fire)
		if (fire::IsOnFire(entity))
		{
			return;
		}
		const auto& info = fields::InfoOf(field);
		// 0x52905D..0x52907E: (float)crops < timesToSow (fild; fcomp +0x128; test ah, 1)
		if (static_cast<float>(field.crops) < info.timesToSow)
		{
			return;
		}
		// 0x529084..0x529095: past ripe (fcomp +0x124; test ah, 0x41; je: growth > ageRecolt)
		if (!(field.growth <= info.ageRecolt))
		{
			return;
		}
		// 0x5290A1: MapCoords::GetAlignment 0x6057B0 of +0x14; 0x5290BF..0x5290C5: GClimate::IsRaining 0x7714B0 of its
		// GetLHPoint 0x605C40 (only x and z are read; UpdateFields moves the Transform's y for the sinking)
		const auto at = map_coords::ToWorld(object::MapCoordsOf(entity));
		const float alignment = effects::alignment::LandAlignmentAt(at);
		const bool raining = weather::IsRainingAt(at);
		// (openblack) mod world.crops: d times its speed (1 without the mod)
		const float d = fields::GrowthStep(field.growth, info, alignment, raining) * speed;
		// 0x52911C..0x529126: growth += d; 0x529133..0x52914F: food += d x totalFoodInField (+0x130) / ageRecolt
		field.growth += d;
		field.food += d * info.totalFoodInField / info.ageRecolt;
		// 0x52912C / 0x529159: IsUnripe 0x5298D0 twice on the same growth, the first must be true and the second false
		// for the town pulse (+0x5E8 = 1, +0x5EC = 0 at 0x529162..0x52917E): unreachable, not ported
	});
}

bool openblack::ecs::ApplyWaterSpellToField(entt::entity entity)
{
	auto* field = Locator::entitiesRegistry::value().TryGet<Field>(entity);
	if (field == nullptr)
	{
		return false;
	}
	// Field::ApplyWaterSpell 0x528F78..0x528FEA (info = Field +0x120, the runtime GFieldTypeInfo: +0x124 ageRecolt,
	// +0x128 timesToSow, +0x130 totalFoodInField, +0x150 effectOfWaterSpell; the 6 info.dat rows are the same, so the
	// first row's effectOfWaterSpell stands for the field's own)
	if (!(static_cast<float>(field->crops) > static_cast<float>(Field::k_TimesToSow)))
	{
		// fild crops; fcomp timesToSow; jne 0x528FF9 (crops <= timesToSow): crops = ftol(timesToSow + 1)
		field->crops = static_cast<uint8_t>(Field::k_TimesToSow + 1);
		return true;
	}
	if (field->growth > Field::k_AgeRecolt)
	{
		return true; // je 0x529010: past ripe, nothing
	}
	const float water = Locator::infoConstants::value().fieldType.at(0).effectOfWaterSpell;
	field->growth += water;
	// Field::IsUnripe 0x5298D0 is called here and its result dropped
	field->food += water * Field::k_TotalFood / Field::k_AgeRecolt;
	return true;
}

bool openblack::ecs::IsFieldRipe(entt::entity entity)
{
	const auto* field = Locator::entitiesRegistry::value().TryGet<const Field>(entity);
	return field != nullptr && field->growth >= Field::k_AgeRecolt;
}

int32_t openblack::ecs::RemoveFieldFood(entt::entity entity, float amount)
{
	return fields::RemoveFood(entity, amount);
}

void openblack::ecs::UpdateFields(float seconds)
{
	// the wind the ripe fields (and trees) sway in, once per frame like GLandscape::Draw -> Tree::PreDraw
	g_windSway.Update(seconds * 1000.0f);
	auto& registry = Locator::entitiesRegistry::value();
	registry.Each<Field, Transform, const Mesh>([&](entt::entity entity, Field& field, Transform& transform, const Mesh& mesh) {
		// Mod world.crops, standing in for the farmers (Villager::FarmerPlantsCrop sows a crop at a time): they would
		// take what the hand leaves (a ripe field only clears when asked for more than it has, and the hand's halved,
		// truncated amounts leave the last food unit there for good) and sow the field again. So a ripe field with less
		// than it takes to be drawn (25) is cleared, and an empty field is sown again at once.
		if (Locator::config::value().fieldsWithoutFarmers)
		{
			if (field.growth >= Field::k_AgeRecolt && field.food < 25.0f)
			{
				field.food = 0.0f;
				field.crops = 0;
				field.growth = 0.0f;
			}
			if (field.crops < Field::k_TimesToSow)
			{
				field.crops = Field::k_TimesToSow;
			}
		}
		// v = food / 350 - 1, eased over 1 s; y += 2 v scale half height (Field::Draw 0x5287B9..0x5287D3: the mesh's
		// +0x28 read inline, the half height, so the mesh level)
		auto* sink = registry.TryGet<PileSink>(entity);
		if (sink == nullptr)
		{
			const float height = ecs::object::MeshHalfHeight(mesh.id);
			sink = &registry.Assign<PileSink>(entity, transform.position.y, height);
		}
		// mod world.foliage, fields = wheat: the mesh is only the far view of the plants, whole and tinted by the
		// growth (Foliage::UpdateFields), so it doesn't sink with the food
		const auto& config = Locator::config::value();
		const bool plants = config.foliageFields && config.foliageDensity > 0.0f;
		const float v = plants ? 0.0f : field.food / Field::k_TotalFood - 1.0f;
		if (std::abs(v - field.sinkTarget) > 1e-4f || !field.sinkStarted)
		{
			field.sinkTarget = v;
			field.sinkStarted = true;
			field.sink.SetDestinationWithSpeedAndTime(v, 0.0f, 1.0f);
		}
		field.sink.Update(seconds);
		float shown = field.sink.value;
		float alpha = 1.0f;
		if (shown < -0.8f)
		{
			// alpha byte (+0x4F) = ((v + 0.8) 2 + 1) 255, v held at -0.8
			alpha = std::clamp((shown + 0.8f) * 2.0f + 1.0f, 0.0f, 1.0f);
			shown = -0.8f;
		}
		// not drawn at all below a quarter of the growing age or 25 food
		if (!plants && (field.growth < 0.25f * Field::k_AgeGrowth || field.food < 25.0f))
		{
			alpha = 0.0f;
		}
		// mod world.foliage, fields = wheat: the plants of 3D/Foliage stand in for the mesh (still there for the hand);
		// far away, where the plants shrink into the ground (the last fifth of the draw distance), the mesh fades in
		// (nothing while unsown or emptied)
		if (plants)
		{
			alpha = field.crops >= Field::k_TimesToSow && field.food >= 1.0f ? 1.0f : 0.0f;
			float far = 0.0f;
			if (Locator::camera::has_value())
			{
				const float away = glm::distance(Locator::camera::value().GetOrigin(), transform.position);
				far = std::clamp((away - 0.8f * config.foliageDistance) / (0.2f * config.foliageDistance), 0.0f, 1.0f);
			}
			alpha = std::min(alpha, far);
		}
		// fld [g3d + 0x44]; fmul [m + 0x28]; fmul v; fadd st0, st0 (0x5287C2..0x5287D1)
		const float sunk = transform.scale.y * sink->height * shown;
		sink->offset.SetPosition(sunk + sunk);
		transform.position.y = sink->baseY + sink->offset.value;
		auto* fade = registry.TryGet<Alpha>(entity);
		if (alpha < 1.0f && fade == nullptr)
		{
			registry.Assign<Alpha>(entity, alpha);
		}
		else if (alpha < 1.0f)
		{
			fade->value = alpha;
		}
		else if (fade != nullptr)
		{
			registry.Remove<Alpha>(entity);
		}
	});
}

// ---- Field.cpp of runblack.exe W120: the farmers and the field functions the villager side calls (Fields.h) --------

namespace openblack::ecs::fields
{
namespace
{
/// [0x8AB6E4] = 5.0: RandomFarmPoint's 5 - GameFloatRand(10) (0x52899C / 0x5289BA), IsTouching's +- 5 (0x5292B3)
constexpr float k_FarmPointHalf = 5.0f;
/// The push 0x41200000 (10.0) of RandomFarmPoint's two GameFloatRand (0x52898A / 0x5289AC)
constexpr float k_FarmPointRange = 10.0f;

// The FPU runs at 24-bit precision (fn_007DEE00: and word [ebp-2], 0xFCFF at 0x7DEE0D): every x87 fadd / fsub / fmul
// / fdiv / fiadd / fidiv below is rounded to float, so they are float operations here, one per statement (no fused
// multiply-add); fild, fld and fcom are exact

Field* FieldOf(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(entity) ? registry.TryGet<Field>(entity) : nullptr;
}

/// The field's +0x14 MapCoords. UpdateFields moves the Transform's y with the food (the draw's sink, Field::Draw),
/// which the original's +0x1C altitude never sees: the altitude is taken from PileSink::baseY (the y before the sink)
/// when the field has one
map_coords::MapCoords PositionOf(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const Transform>(entity);
	const auto* sink = registry.TryGet<const PileSink>(entity);
	if (transform == nullptr || sink == nullptr)
	{
		return object::MapCoordsOf(entity);
	}
	return map_coords::FromWorld(glm::vec3(transform->position.x, sink->baseY, transform->position.z));
}

/// entt's on_destroy<Field> (Registry::Destroy from ecs::ToBeDeleted, Remove, Reset): Field::ToBeDeleted 0x5280F0 ->
/// Abode::ToBeDeleted 0x402C6F -> vt +0x910 DeleteDependancys 0x528100. The component is still there during the signal
void OnFieldDestroyed(entt::registry& /*registry*/, entt::entity entity)
{
	DeleteDependancys(entity);
}

/// Connected before the first farmer joins (a field without farmers has nothing to release); entt's sink::connect
/// disconnects the same listener first, so it is connected once per registry
void ConnectFieldDeletion()
{
	Locator::entitiesRegistry::value().OnDestroy<Field>().connect<&OnFieldDestroyed>();
}
} // namespace

void DisconnectDeletionListeners()
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	Locator::entitiesRegistry::value().OnDestroy<Field>().disconnect<&OnFieldDestroyed>();
	fish_farms::DisconnectDeletionListener();
}

void ReleaseWorker(entt::entity villager)
{
	// SetTopState(163) (vt +0x8E8, push 0xA3: 0x52813F / 0x52C6C2). (openblack guard) a villager already gone (a
	// registry Reset clears the pools one after another) is not called
	if (Locator::entitiesRegistry::value().Valid(villager))
	{
		villager::SetTopState(villager, VillagerStates::DecideWhatToDo);
	}
}

const GFieldTypeInfo& InfoOf(const Field& field)
{
	return Locator::infoConstants::value().fieldType.at(static_cast<size_t>(field.type));
}

float GrowthStep(float growth, const GFieldTypeInfo& info, float alignment, bool raining)
{
	// 0x5290A6..0x5290BB: fmul 0.5; fadd 1; fadd st0, st0; fstp a
	const float a = (alignment * 0.5f + 1.0f) * 2.0f;
	// 0x5290CA..0x5290E4: fld growth; fcomp ageGrowth (+0x120); test ah, 1 (below: growing)
	const bool growing = growth < info.ageGrowth;
	if (raining)
	{
		// 0x5290E6..0x5290F7: +0x140 effectRainWhenGrowing / +0x144 effectRainWhenRipening
		return a * (growing ? info.effectRainWhenGrowing : info.effectRainWhenRipening);
	}
	// 0x5290FF..0x529110: +0x138 effectSunWhenGrowing / +0x13C effectSunWhenRipening
	return a * (growing ? info.effectSunWhenGrowing : info.effectSunWhenRipening);
}

float GetPercentFull(entt::entity entity)
{
	const auto* field = FieldOf(entity);
	if (field == nullptr)
	{
		return 0.0f;
	}
	// 0x529501..0x529517: fild (u8 crops); fdiv timesToSow (+0x128)
	return static_cast<float>(field->crops) / InfoOf(*field).timesToSow;
}

int GetFieldActivity(entt::entity entity)
{
	const auto* field = FieldOf(entity);
	if (field == nullptr)
	{
		return 0;
	}
	// 0x529358..0x529365: GetPercentFull < 1 -> 1
	if (GetPercentFull(entity) < 1.0f)
	{
		return 1;
	}
	// 0x52937A..0x529387: growth >= ageGrowth (+0x120; test ah, 1; jne) -> 2
	return field->growth < InfoOf(*field).ageGrowth ? 0 : 2;
}

float GetDesireToBeFarmed(entt::entity entity)
{
	const auto* field = FieldOf(entity);
	if (field == nullptr)
	{
		return 0.0f;
	}
	// 0x5293A6..0x5293AB: a fire effect at +0x44 (the pointer, not IsOnFire)
	if (fire::Find(entity) != nullptr)
	{
		return 0.0f;
	}
	// 0x5293B1..0x5293BC: IsFunctional (vt +0xD4) != 1
	if (!abode_queries::IsFunctional(entity))
	{
		return 0.0f;
	}
	const auto& info = InfoOf(*field);
	// 0x5293C2..0x529413: fild qword {+0xD8, 0}; fidiv dword maxFarmerInFarm (+0x134, read signed), rounded to float;
	// fcom 1; test ah, 1; jne: below 1 (or unordered) kept, else 1; a = 1 - that (fsub, fstp dword)
	const auto maximum = static_cast<float>(static_cast<int32_t>(info.maxFarmerInFarm));
	float share = static_cast<float>(field->farmers.size()) / maximum;
	if (!(share < 1.0f) && !std::isnan(share))
	{
		share = 1.0f;
	}
	const float a = 1.0f - share;
	// 0x529419..0x529438: p = GetPercentFull below 1, else 1
	const float full = GetPercentFull(entity);
	const float p = full < 1.0f ? full : 1.0f;
	const int activity = GetFieldActivity(entity);
	if (activity == 2)
	{
		// 0x52944E..0x529476: growth below ageRecolt (+0x124; test ah, 1; jne) -> 0, else a
		return field->growth < info.ageRecolt ? 0.0f : a;
	}
	if (activity == 1)
	{
		// 0x529480..0x529493: fld 1; fsub p; fmul a; fmul a; fmul a, each rounded to float
		float d = 1.0f - p;
		d = d * a;
		d = d * a;
		d = d * a;
		return d;
	}
	return 0.0f;
}

bool PlantCrop(entt::entity entity)
{
	auto* field = FieldOf(entity);
	// 0x5291A0..: (float)crops < timesToSow -> ++crops, 1; the position argument is not read
	if (field == nullptr || !(static_cast<float>(field->crops) < InfoOf(*field).timesToSow))
	{
		return false;
	}
	++field->crops;
	return true;
}

bool IsStillSowing(entt::entity entity)
{
	const auto* field = FieldOf(entity);
	// GetPlantCropPos 0x529210: (float)crops < timesToSow
	return field != nullptr && static_cast<float>(field->crops) < InfoOf(*field).timesToSow;
}

map_coords::MapCoords RandomFarmPoint(entt::entity entity)
{
	auto pos = PositionOf(entity);
	// 0x528997 ("Field.cpp" 0x164) for x, then 0x5289B5 (0x165) for z: fsubr [0x8AB6E4] = 5 - r
	const float r1 = k_FarmPointHalf - game_random::GameFloatRand(k_FarmPointRange);
	const float r2 = k_FarmPointHalf - game_random::GameFloatRand(k_FarmPointRange);
	// 0x5289C0..0x528A1B: ftol((pos x 10 x 2^-16 + r) x 65536 / 10) on each axis; the altitude (+8) copied
	pos.x = map_coords::ToFixedGUtils(map_coords::ToMetres(pos.x) + r1);
	pos.z = map_coords::ToFixedGUtils(map_coords::ToMetres(pos.z) + r2);
	return pos;
}

bool RipeFarmPoint(entt::entity entity, map_coords::MapCoords& out)
{
	const auto* field = FieldOf(entity);
	// fn_00529240: growth < ageRecolt -> 0, no draw
	if (field == nullptr || field->growth < InfoOf(*field).ageRecolt)
	{
		return false;
	}
	out = RandomFarmPoint(entity);
	return true;
}

map_coords::MapCoords GetArrivePos(entt::entity entity)
{
	// 0x529330: the position +0x14
	return PositionOf(entity);
}

bool IsTouching(entt::entity entity, const map_coords::MapCoords& pos)
{
	if (FieldOf(entity) == nullptr)
	{
		return false;
	}
	const auto f = map_coords::ToMetres(PositionOf(entity));
	const auto p = map_coords::ToMetres(pos);
	// 0x5292B1..0x5292D3 on x: below fx - 5 (test ah, 1; jne) -> 0; not below fx + 5 (test ah, 1; je) -> 0
	if (p.x < f.x - k_FarmPointHalf || !(p.x < f.x + k_FarmPointHalf))
	{
		return false;
	}
	// 0x5292F3..0x529315: the same on z (+0x18)
	return !(p.y < f.y - k_FarmPointHalf) && p.y < f.y + k_FarmPointHalf;
}

int32_t RemoveFood(entt::entity entity, float amount)
{
	auto* field = FieldOf(entity);
	if (field == nullptr)
	{
		return 0;
	}
	const auto& info = InfoOf(*field);
	// 0x5295A8..0x5295BA: fcomp 0; test ah, 0x40 (equal, or unordered) -> 0
	if (!(field->food < 0.0f || field->food > 0.0f))
	{
		return 0;
	}
	// 0x5295C0..0x5295E1: (float)crops < timesToSow -> 0
	if (static_cast<float>(field->crops) < info.timesToSow)
	{
		return 0;
	}
	// 0x5295E7..0x5295EB: k = ftol(amount)
	const int32_t k = map_coords::FtoL(amount);
	// 0x5295F0..0x529624: unripe (growth < ageRecolt, test ah, 1): cost = ftol(amount x ratioBeforeRipe (+0x14C) + k)
	// (fmul, then fiadd dword k: each rounded to float; (approximate) a k above 2^24 is rounded to float before the
	// add); ripe: k
	int32_t cost = k;
	if (field->growth < info.ageRecolt)
	{
		const float product = amount * info.ratioBeforeRipe;
		const float sum = product + static_cast<float>(k);
		cost = map_coords::FtoL(sum);
	}
	// 0x529626..0x52963D: fild qword {cost, 0} (the cost read as unsigned); fcom food; test ah, 1
	const auto unsignedCost = static_cast<double>(static_cast<uint32_t>(cost));
	if (unsignedCost < static_cast<double>(field->food))
	{
		// 0x5296CF..0x5296E8: fld food; fsub st(1); fstp food; EAX = k
		field->food = static_cast<float>(static_cast<double>(field->food) - unsignedCost);
		return k;
	}
	// 0x529643..0x529649: Object::SetTemperature 0x639A60 (0, NULL)
	fire::SetTemperature(entity, 0.0f, entt::null);
	field = FieldOf(entity);
	// 0x52964E..0x52966A: with a town (GetTown vt +0x48): +0x5E8 = 1, +0x5EC = 0
	if (const auto town = TownOf(entity); town != entt::null)
	{
		auto& t = Locator::entitiesRegistry::value().Get<Town>(town);
		t.buildPulse = 1;
		t.buildPulsePrevious = 0;
	}
	// 0x529670..0x5296A5: unripe (read again): food = 0, ftol(amount x ratioBeforeRipe)
	if (field->growth < info.ageRecolt)
	{
		field->food = 0.0f;
		const float product = amount * info.ratioBeforeRipe; // 0x529693 fmul, rounded to float
		return map_coords::FtoL(product);
	}
	// 0x5296A8..0x5296CC: ftol(food), then food = crops = growth = 0
	const int32_t left = map_coords::FtoL(field->food);
	field->food = 0.0f;
	field->crops = 0;
	field->growth = 0.0f;
	return left;
}

float GetFoodValue(entt::entity entity)
{
	const auto* field = FieldOf(entity);
	// 0x529700: growth < ageRecolt ? 0 : food
	if (field == nullptr || field->growth < InfoOf(*field).ageRecolt)
	{
		return 0.0f;
	}
	return field->food;
}

void AddFarmer(entt::entity entity, entt::entity villager)
{
	auto* field = FieldOf(entity);
	if (field == nullptr)
	{
		return;
	}
	// 0x5283E3..0x5283FB: already in the list -> nothing; 0x5283FD: a null villager -> nothing
	if (std::find(field->farmers.begin(), field->farmers.end(), villager) != field->farmers.end() ||
	    villager == entt::null)
	{
		return;
	}
	// 0x528401..0x528420: a new node at the head, +0xD8++ (the deletion listener connected first)
	ConnectFieldDeletion();
	field->farmers.insert(field->farmers.begin(), villager);
}

void RemoveFarmer(entt::entity entity, entt::entity villager)
{
	if (auto* field = FieldOf(entity); field != nullptr)
	{
		// 0x52836C..0x5283B3: every node of the villager unlinked and freed, +0xD8-- each
		std::erase(field->farmers, villager);
	}
	// 0x528362 (not in the list) / 0x5283B8 / 0x5283C8 (empty list): villager +0x118 = 0, always
	// TODO(Personas): villager::SetTargetThing(villager, entt::null); // fn_00528340 0x528362 / 0x5283B8 / 0x5283C8
}

bool HasFarmer(entt::entity entity, entt::entity villager)
{
	const auto* field = FieldOf(entity);
	return field != nullptr &&
	       std::find(field->farmers.begin(), field->farmers.end(), villager) != field->farmers.end();
}

uint32_t FarmerCount(entt::entity entity)
{
	const auto* field = FieldOf(entity);
	return field != nullptr ? static_cast<uint32_t>(field->farmers.size()) : 0u;
}

entt::entity TownOf(entt::entity entity)
{
	const auto* field = FieldOf(entity);
	if (field == nullptr || field->town < 0)
	{
		return entt::null;
	}
	// 0x528960: +0x118; openblack keeps the Town::id (FieldArchetype, CREATE_TOWN_FIELD)
	auto& registry = Locator::entitiesRegistry::value();
	const auto& towns = registry.Context().towns;
	const auto it = towns.find(static_cast<uint32_t>(field->town));
	const bool found = it != towns.end() && registry.Valid(it->second) && registry.AllOf<Town>(it->second);
	return found ? it->second : entt::null;
}

std::vector<entt::entity> TownFields(entt::entity town)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* t = town != entt::null && registry.Valid(town) ? registry.TryGet<const Town>(town) : nullptr;
	if (t == nullptr)
	{
		return {};
	}
	std::vector<entt::entity> list;
	registry.Each<const Field>([&](entt::entity entity, const Field& field) {
		if (field.town >= 0 && static_cast<uint32_t>(field.town) == t->id)
		{
			list.push_back(entity);
		}
	});
	// the ctor's head insertion (0x527E64..0x527E75): newest first
	std::sort(list.begin(), list.end(),
	          [](entt::entity a, entt::entity b) { return object_index::Of(a) > object_index::Of(b); });
	return list;
}

bool IsField(entt::entity thing)
{
	return FieldOf(thing) != nullptr;
}

void DeleteDependancys(entt::entity entity)
{
	const auto* field = FieldOf(entity);
	if (field == nullptr)
	{
		return;
	}
	// 0x528104..0x528117: v = the head's villager (none: nothing)
	auto villager = field->farmers.empty() ? entt::entity {entt::null} : field->farmers.front();
	while (villager != entt::null)
	{
		// 0x528119..0x52813B: before the call, the villager after v's first node in the list as it is now (none: 0)
		auto next = entt::entity {entt::null};
		if (const auto* f = FieldOf(entity); f != nullptr)
		{
			const auto it = std::find(f->farmers.begin(), f->farmers.end(), villager);
			if (it != f->farmers.end() && std::next(it) != f->farmers.end())
			{
				next = *std::next(it);
			}
		}
		// 0x52813D..0x528146: SetTopState(163) (its exit ExitFarming 0x75A2A0 -> RemoveFarmer unlinks it)
		ReleaseWorker(villager);
		// 0x52814E: villager +0x118 = 0
		// TODO(Personas): villager::SetTargetThing(villager, entt::null); // Field::DeleteDependancys 0x52814E
		villager = next;
	}
	// 0x528158..0x5281C3 (out of the town's +0x780, Town::SetTownArea), 0x5281F1..0x52826C (out of g_game +0x205C04):
	// nothing to do in openblack (TownFields / the components are the lists; town_placement recomputes the rectangle
	// when it is read). 0x5281CA..0x5281DC RemoveMapObject: ecs::ToBeDeleted's generic part. 0x5281E2..0x5281EC the
	// fence (+0xC8, fn_00528CA0): not ported (no fences, the builder 0x528B80 has no caller)
}
} // namespace openblack::ecs::fields
