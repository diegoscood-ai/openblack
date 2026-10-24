/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The particles' tab of the Magic window: starts any particle type or file at the hand or where the camera looks, and
// lists the running effects

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <imgui.h>

#include "3D/LandIslandInterface.h"
#include "Camera/Camera.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHighlight.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Town/TownBelief.h"
#include "Locator.h"
#include "Magic.h"
#include "MagicModel.h"
#include "Particles/ParticleTypes.h"

using namespace openblack;
using namespace openblack::debug::gui;
using namespace openblack::debug::magic_window;

namespace
{
constexpr float k_MaxMagnitude = 10.0f;
constexpr float k_MaxHeight = 50.0f;
constexpr int k_PlayerCount = 8;
constexpr float k_MaxCloseAfter = 30.0f;
constexpr int k_MaxTargets = 10;
constexpr std::array k_DrawPaths {particles::DrawPath::Sorted, particles::DrawPath::Queued, particles::DrawPath::Immediate};
} // namespace

glm::vec3 Magic::SpawnPoint() const noexcept
{
	glm::vec3 point(0.0f);
	if (_spawnAt == SpawnAt::Hand && Locator::handSystem::has_value())
	{
		for (const auto& hand : Locator::handSystem::value().GetPlayerHandPositions())
		{
			if (hand.has_value())
			{
				return *hand + glm::vec3(0.0f, _spawnHeight, 0.0f);
			}
		}
	}
	if (Locator::camera::has_value())
	{
		point = Locator::camera::value().GetFocus();
	}
	if (Locator::terrainSystem::has_value())
	{
		point.y = Locator::terrainSystem::value().GetHeightAt({point.x, point.z});
	}
	return point + glm::vec3(0.0f, _spawnHeight, 0.0f);
}

void Magic::DrawParticles() noexcept
{
	if (!Locator::particleSystem::has_value())
	{
		ImGui::TextUnformatted("No particle system");
		return;
	}
	auto& particles = Locator::particleSystem::value();
	if (_particleFiles.empty())
	{
		_particleFiles = particles.GetFileNames();
	}

	ImGui::Checkbox("By file", &_byFile);
	if (_byFile)
	{
		const auto preview = _particleFiles.empty() ? std::string("no files")
		                                            : _particleFiles.at(static_cast<size_t>(std::clamp(
		                                                  _particleFile, 0, static_cast<int>(_particleFiles.size()) - 1)));
		if (ImGui::BeginCombo("File", preview.c_str()))
		{
			for (int i = 0; i < static_cast<int>(_particleFiles.size()); ++i)
			{
				if (ImGui::Selectable(_particleFiles.at(static_cast<size_t>(i)).c_str(), i == _particleFile))
				{
					_particleFile = i;
				}
			}
			ImGui::EndCombo();
		}
	}
	else if (ImGui::BeginCombo("Type", ParticleTypeLabel(_particleType).c_str()))
	{
		for (size_t i = 0; i < particles::k_ParticleTypeCount; ++i)
		{
			const auto type = static_cast<ParticleType>(i);
			ImGui::BeginDisabled(particles::ParticleTypeFile(type).empty());
			if (ImGui::Selectable(ParticleTypeLabel(type).c_str(), type == _particleType))
			{
				_particleType = type;
			}
			ImGui::EndDisabled();
		}
		ImGui::EndCombo();
	}

	ImGui::RadioButton("At what the camera looks at", reinterpret_cast<int*>(&_spawnAt),
	                   static_cast<int>(SpawnAt::CameraFocus));
	ImGui::SameLine();
	ImGui::RadioButton("At the hand", reinterpret_cast<int*>(&_spawnAt), static_cast<int>(SpawnAt::Hand));
	ImGui::SliderFloat("Height", &_spawnHeight, 0.0f, k_MaxHeight, "%.1f");
	ImGui::SliderFloat("Magnitude", &_magnitude, 0.0f, k_MaxMagnitude, "%.2f");
	ImGui::SliderInt("Player", &_particlePlayer, 0, k_PlayerCount - 1);
	ImGui::SliderFloat("Close after", &_closeAfter, 0.0f, k_MaxCloseAfter, _closeAfter > 0.0f ? "%.1f s" : "never");
	ImGui::Checkbox("Synced random numbers", &_synced);
	if (ImGui::BeginCombo("Drawn", DrawPathName(_drawPath).data()))
	{
		for (const auto path : k_DrawPaths)
		{
			if (ImGui::Selectable(DrawPathName(path).data(), path == _drawPath))
			{
				_drawPath = path;
			}
		}
		ImGui::EndCombo();
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Sorted: each sprite, model, mist and ribbon in its own place among what blends.\n"
		                  "Queued: the whole effect at its origin, drawn in its own order, as some spot visuals are.\n"
		                  "Immediate: drawn just after the hand, as the miracle in the hand is.");
	}
	ImGui::SliderInt("Targets", &_targets, 0, k_MaxTargets, _targets > 0 ? "%d nearest" : "none");
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("The creatures and villagers nearest the effect are given to it to act on, as the heal "
		                  "miracle's chakra is given the people it heals.");
	}

	const bool byFile = _byFile && !_particleFiles.empty();
	ImGui::BeginDisabled(!byFile && particles::ParticleTypeFile(_particleType).empty());
	if (ImGui::Button("Spawn"))
	{
		const auto point = SpawnPoint();
		const auto file = byFile ? _particleFiles.at(static_cast<size_t>(
		                               std::clamp(_particleFile, 0, static_cast<int>(_particleFiles.size()) - 1)))
		                         : std::string();
		const auto id = byFile ? particles.Start(file, point, _magnitude, _synced)
		                       : particles.Start(_particleType, point, _magnitude, _synced);
		if (id != ecs::systems::ParticleSystemInterface::k_NoEffect)
		{
			particles.SetPlayer(id, _particlePlayer);
			particles.SetDrawPath(id, _drawPath);
			GiveNearestTargets(id, point);
			if (_closeAfter > 0.0f)
			{
				_timedEffects.push_back({.id = id, .secondsLeft = _closeAfter});
			}
		}
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	bool paused = particles.IsPaused();
	if (ImGui::Checkbox("Pause", &paused))
	{
		particles.SetPaused(paused);
	}
	ImGui::SameLine();
	if (ImGui::Button("Close all"))
	{
		for (const auto& effect : particles.GetEffects())
		{
			particles.CloseDown(effect.id);
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Delete all"))
	{
		for (const auto& effect : particles.GetEffects())
		{
			particles.Delete(effect.id);
		}
	}
	DrawBeliefSprites();
	ImGui::Separator();
	DrawParticleStats();
	DrawRunningEffects();
}

void Magic::DrawBeliefSprites() noexcept
{
	if (!ImGui::CollapsingHeader("Belief sprites"))
	{
		return;
	}
	ImGui::Text("%zu queued", ecs::town_belief::BeliefSpriteCount());
	ImGui::SliderInt("Belief", &_beliefAmount, 0, 200);
	ImGui::ColorEdit3("Colour", _beliefColour.data());
	if (ImGui::Button("Queue at the camera target"))
	{
		glm::vec3 point(0.0f);
		if (Locator::camera::has_value())
		{
			point = Locator::camera::value().GetFocus();
		}
		if (Locator::terrainSystem::has_value())
		{
			point.y = Locator::terrainSystem::value().GetHeightAt({point.x, point.z});
		}
		const auto channel = [this](size_t i) {
			return static_cast<uint32_t>(std::clamp(_beliefColour.at(i), 0.0f, 1.0f) * 255.0f + 0.5f);
		};
		Locator::particleSystem::value().AddBeliefSprite(
		    {.position = point, .amount = _beliefAmount, .colour = channel(0) << 16 | channel(1) << 8 | channel(2)});
	}
}

void Magic::DrawParticleStats() const noexcept
{
	const auto stats = Locator::particleSystem::value().GetDrawStats();
	ImGui::Text("Drawn: %zu sprites, %zu ribbons, %zu models, %zu mists, %zu light maps, from %zu effects", stats.sprites,
	            stats.chains, stats.meshes, stats.mists, stats.lightStamps, stats.effects);
	// the golden showers effects draw over their objects (the gate stones' stars while a script highlights them)
	ImGui::Text("Golden showers over objects: %zu", ecs::script_highlight::DrawnShowers());
}

void Magic::GiveNearestTargets(uint32_t effect, const glm::vec3& origin) const noexcept
{
	if (_targets <= 0 || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<TargetCandidate> candidates;
	const auto add = [&candidates, &origin](entt::entity entity, const ecs::components::Transform& transform) {
		candidates.push_back({.distance = glm::distance(transform.position, origin), .entity = entity});
	};
	registry.Each<const ecs::components::Creature, const ecs::components::Transform>(
	    [&add](entt::entity entity, const ecs::components::Creature&, const ecs::components::Transform& transform) {
		    add(entity, transform);
	    });
	registry.Each<const ecs::components::Villager, const ecs::components::Transform>(
	    [&add](entt::entity entity, const ecs::components::Villager&, const ecs::components::Transform& transform) {
		    add(entity, transform);
	    });
	for (const auto target : TargetsInGivingOrder(std::move(candidates), static_cast<size_t>(_targets)))
	{
		Locator::particleSystem::value().AddTarget(effect, target);
	}
}

void Magic::DrawRunningEffects() noexcept
{
	auto& particles = Locator::particleSystem::value();
	const auto effects = particles.GetEffects();
	ImGui::Text("%zu running", effects.size());
	if (!ImGui::BeginTable("ParticleEffects", 7, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY))
	{
		return;
	}
	ImGui::TableSetupColumn("File");
	ImGui::TableSetupColumn("Drawn");
	ImGui::TableSetupColumn("Age");
	ImGui::TableSetupColumn("Atoms");
	ImGui::TableSetupColumn("Groups");
	ImGui::TableSetupColumn("State");
	ImGui::TableSetupColumn("");
	ImGui::TableHeadersRow();
	// Changed once the walk is done
	std::optional<uint32_t> toClose;
	std::optional<uint32_t> toDelete;
	std::optional<std::pair<uint32_t, particles::DrawPath>> toRedraw;
	for (const auto& effect : effects)
	{
		ImGui::PushID(static_cast<int>(effect.id));
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(effect.file.c_str());
		if (!effect.unportedClasses.empty() && ImGui::IsItemHovered())
		{
			std::string list = "Not run yet:";
			for (const auto& name : effect.unportedClasses)
			{
				list += "\n  " + name;
			}
			ImGui::SetTooltip("%s", list.c_str());
		}
		ImGui::TableNextColumn();
		// Click to draw it the next way
		if (ImGui::SmallButton(DrawPathName(effect.path).data()))
		{
			const auto next = (static_cast<size_t>(effect.path) + 1) % k_DrawPaths.size();
			toRedraw = {effect.id, k_DrawPaths.at(next)};
		}
		ImGui::TableNextColumn();
		ImGui::Text("%.1f s", static_cast<double>(effect.age));
		ImGui::TableNextColumn();
		if (effect.targets > 0)
		{
			ImGui::Text("%zu (%zu to act on)", effect.atoms, effect.targets);
		}
		else
		{
			ImGui::Text("%zu", effect.atoms);
		}
		ImGui::TableNextColumn();
		ImGui::Text("%zu", effect.collections);
		ImGui::TableNextColumn();
		if (effect.closing)
		{
			ImGui::TextUnformatted("closing");
		}
		else if (effect.secondsLeft.has_value())
		{
			ImGui::Text("%.1f s left", static_cast<double>(*effect.secondsLeft));
		}
		else
		{
			ImGui::TextUnformatted(effect.ownedBySpell ? "miracle" : "running");
		}
		ImGui::TableNextColumn();
		if (ImGui::SmallButton("Close"))
		{
			toClose = effect.id;
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Delete"))
		{
			toDelete = effect.id;
		}
		ImGui::PopID();
	}
	ImGui::EndTable();
	if (toRedraw)
	{
		particles.SetDrawPath(toRedraw->first, toRedraw->second);
	}
	if (toClose)
	{
		particles.CloseDown(*toClose);
	}
	if (toDelete)
	{
		particles.Delete(*toDelete);
	}
}

void Magic::UpdateTimedEffects(float seconds) noexcept
{
	if (!Locator::particleSystem::has_value() || _timedEffects.empty())
	{
		return;
	}
	auto& particles = Locator::particleSystem::value();
	std::erase_if(_timedEffects, [&particles, seconds](TimedEffect& timed) {
		if (!particles.IsRunning(timed.id))
		{
			return true;
		}
		timed.secondsLeft -= seconds;
		if (timed.secondsLeft > 0.0f)
		{
			return false;
		}
		particles.CloseDown(timed.id);
		return true;
	});
}
