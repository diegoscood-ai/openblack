/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DiscipleMarks.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <optional>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <glm/geometric.hpp>

#include "ECS/Components/Disciple.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Sprite.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/DiscipleRules.h"
#include "ECS/MobileDrawing.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerAnimate.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDisciple.h"
#include "Graphics/Texture2D.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::ecs::disciple_marks
{
using namespace components;

namespace
{
/// The glow is drawn from the smoke sheet, its puffs eight to a row, the first fifteen played through in half a second
constexpr auto k_GlowSheet = entt::hashed_string("raw/smoke");
constexpr auto k_GlowSheetAlpha = entt::hashed_string("raw/smokea");
constexpr uint32_t k_GlowFrames = 15;
constexpr float k_GlowCycleMs = 500.0f;
/// Half-bright orange, added to what is behind
constexpr glm::vec4 k_GlowTint {193.0f / 255.0f, 129.0f / 255.0f, 25.0f / 255.0f, 128.0f / 255.0f};
/// The symbols are drawn from the leash sheet, eight to a row, in the symbol's yellow
constexpr auto k_SymbolSheet = entt::hashed_string("raw/leash");
constexpr auto k_SymbolSheetAlpha = entt::hashed_string("raw/leasha");
constexpr float k_SheetCells = 8.0f;
/// The symbol is as wide on the screen wherever it is
constexpr float k_SymbolPixels = 16.0f;
/// No sprite is drawn smaller than this
constexpr float k_LeastSpriteSize = 0.0001f;

/// The debug switch of the marks, in the debug hooks' store (Locator::debugHooks): shown until it is turned off
struct DiscipleMarksDebugState
{
	bool shown {true};
};

bool& Shown()
{
	if (!Locator::debugHooks::has_value())
	{
		std::fputs("disciple marks: no debug hooks in the locator (Locator::debugHooks)\n", stderr);
		std::abort();
	}
	return Locator::debugHooks::value().Get<DiscipleMarksDebugState>().shown;
}

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

/// The disciple's work, none for a villager that is no disciple
VillagerDisciple TypeOf(entt::entity villager)
{
	const auto* v = Entities().Valid(villager) ? Entities().TryGet<const Villager>(villager) : nullptr;
	if (v == nullptr || (v->flags & Villager::k_FlagDisciple) == 0)
	{
		return VillagerDisciple::None;
	}
	return static_cast<VillagerDisciple>(v->discipleType);
}

/// A disciple of a kind that glows (the disciple table's marker)
bool Glows(entt::entity villager)
{
	const auto type = TypeOf(villager);
	return type != VillagerDisciple::None && villager::DiscipleMarker(static_cast<uint8_t>(type)) != 0;
}

/// The disciple in the player's hand, or the one under the pointer when the hand holds none
std::optional<entt::entity> PersonShown()
{
	if (Locator::handGrabSystem::has_value())
	{
		if (const auto held = Locator::handGrabSystem::value().GetHeld(); held.has_value() && Glows(*held))
		{
			return held;
		}
	}
	if (Locator::pickingSystem::has_value())
	{
		if (const auto hovered = Locator::pickingSystem::value().GetPick().object; hovered.has_value() && Glows(*hovered))
		{
			return hovered;
		}
	}
	return std::nullopt;
}

glm::vec2 CellUv(uint32_t cell)
{
	return {static_cast<float>(cell % 8) / k_SheetCells, static_cast<float>(cell / 8) / k_SheetCells};
}

entt::entity MakeMark(entt::entity villager, bool symbol)
{
	auto& registry = Entities();
	const auto sheet = symbol ? k_SymbolSheet : k_GlowSheet;
	const auto alpha = symbol ? k_SymbolSheetAlpha : k_GlowSheetAlpha;
	const auto entity = registry.Create();
	registry.Assign<Sprite>(entity, Sprite {.texture = sheet.value(),
	                                        .uvMin = glm::vec2(0.0f),
	                                        .uvExtent = glm::vec2(1.0f / k_SheetCells),
	                                        .tint = symbol ? disciple::TintOf(disciple::k_SymbolColour) : k_GlowTint,
	                                        .additive = !symbol,
	                                        .facesCamera = true,
	                                        .alpha = alpha.value()});
	registry.Assign<Transform>(entity, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(0.0f));
	registry.Assign<DiscipleMark>(entity, DiscipleMark {.villager = villager, .symbol = symbol});
	return entity;
}

/// Every mark goes
void ClearMarks()
{
	auto& registry = Entities();
	std::vector<entt::entity> marks;
	registry.Each<const DiscipleMark>([&marks](entt::entity mark, const DiscipleMark&) { marks.push_back(mark); });
	for (const auto mark : marks)
	{
		registry.Destroy(mark);
	}
}
} // namespace

void SetShown(bool shown)
{
	Shown() = shown;
	if (!shown && Locator::entitiesRegistry::has_value())
	{
		ClearMarks();
	}
}

bool IsShown()
{
	return Shown();
}

void Update(const glm::vec3& eye, float screenWidth)
{
	if (!Shown() || !Locator::entitiesRegistry::has_value() || !Locator::resources::has_value())
	{
		return;
	}
	auto& registry = Entities();
	const auto& textures = Locator::resources::value().GetTextures();
	if (!textures.Contains(k_GlowSheet.value()) || !textures.Contains(k_GlowSheetAlpha.value()) ||
	    !textures.Contains(k_SymbolSheet.value()) || !textures.Contains(k_SymbolSheetAlpha.value()))
	{
		return;
	}
	// The puffs turn with the game's time
	float clock = 0.0f;
	if (Locator::time::has_value())
	{
		const auto& time = Locator::time::value();
		constexpr auto k_TurnMs = static_cast<float>(systems::TimeSystemInterface::k_TurnDuration.count());
		const double gameMs = (static_cast<double>(time.GetTurn()) + static_cast<double>(time.GetTurnFraction())) * k_TurnMs;
		clock = static_cast<float>(std::fmod(gameMs, static_cast<double>(k_GlowCycleMs)));
	}
	const auto frame = static_cast<uint32_t>(static_cast<float>(k_GlowFrames) / k_GlowCycleMs * clock);

	// Marks of villagers gone or no longer glowing go
	std::vector<entt::entity> gone;
	std::vector<entt::entity> marked;
	std::optional<entt::entity> symbolMark;
	registry.Each<const DiscipleMark>([&](entt::entity mark, const DiscipleMark& of) {
		if (of.symbol)
		{
			symbolMark = mark;
		}
		else if (!Glows(of.villager))
		{
			gone.push_back(mark);
		}
		else
		{
			marked.push_back(of.villager);
		}
	});
	for (const auto mark : gone)
	{
		registry.Destroy(mark);
	}
	std::vector<entt::entity> glowing;
	registry.Each<const Villager>([&glowing](entt::entity villager, const Villager& v) {
		if ((v.flags & Villager::k_FlagDisciple) != 0 && villager::DiscipleMarker(v.discipleType) != 0)
		{
			glowing.push_back(villager);
		}
	});
	for (const auto villager : glowing)
	{
		if (std::ranges::find(marked, villager) == marked.end())
		{
			MakeMark(villager, false);
		}
	}

	// Each glow over its villager's head as the villager is drawn this frame, the size of the villager, its puffs
	// turning; none while the villager is out of sight
	registry.Each<DiscipleMark, Sprite, Transform>([&registry, frame](DiscipleMark& of, Sprite& sprite, Transform& transform) {
		if (of.symbol)
		{
			return;
		}
		const auto& body = registry.Get<const Transform>(of.villager);
		const float size = std::max(body.scale.x, k_LeastSpriteSize);
		const bool hidden = registry.AllOf<LivingAction>(of.villager) &&
		                    villager_animate::IsHiddenByState(villager::GetState(of.villager, LivingAction::Index::Top));
		transform.position = disciple::GlowPoint(DrawnPosition(registry, of.villager), body.scale.x);
		// drawn over the villager's drawing pose (smooth between turns); the Transform keeps the game's
		of.drawPosition = disciple::GlowPoint(DrawnPosition(registry, of.villager, PoseOf::Drawing), body.scale.x);
		transform.scale = hidden ? glm::vec3(0.0f) : glm::vec3(size, size, 1.0f);
		sprite.uvMin = CellUv(frame);
	});

	// The symbol of the disciple in the hand or under it, over its glow
	const auto shown = PersonShown();
	const auto cell = shown.has_value() ? disciple::SymbolCell(TypeOf(*shown)) : std::nullopt;
	if (!cell.has_value())
	{
		if (symbolMark.has_value())
		{
			registry.Get<Transform>(*symbolMark).scale = glm::vec3(0.0f);
		}
		return;
	}
	const auto mark = symbolMark.has_value() ? *symbolMark : MakeMark(*shown, true);
	auto& transform = registry.Get<Transform>(mark);
	const auto& body = registry.Get<const Transform>(*shown);
	transform.position =
	    disciple::SymbolPoint(disciple::GlowPoint(DrawnPosition(registry, *shown), body.scale.x), body.scale.x);
	// drawn over the villager's drawing pose (smooth between turns); the Transform keeps the game's
	registry.Get<DiscipleMark>(mark).drawPosition = disciple::SymbolPoint(
	    disciple::GlowPoint(DrawnPosition(registry, *shown, PoseOf::Drawing), body.scale.x), body.scale.x);
	const float halfWidth = std::max(screenWidth * 0.5f, 1.0f);
	const float size =
	    std::max(glm::distance(eye, transform.position) * (k_SymbolPixels / halfWidth) * 0.5f, k_LeastSpriteSize);
	transform.scale = glm::vec3(size, size, 1.0f);
	registry.Get<Sprite>(mark).uvMin = CellUv(*cell);
	registry.Get<DiscipleMark>(mark).villager = *shown;
}
} // namespace openblack::ecs::disciple_marks
