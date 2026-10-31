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

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "ECS/Fire/FireEffect.h"
#include "ECS/Systems/FireSystemInterface.h"

namespace openblack::test
{
/// The game's fires as a test sets them: an object burns at the temperature Burn gives it, and nothing else is ever
/// listed, made or freed. Inject it with Locator::fireSystem::emplace<FakeFires>()
class FakeFires final: public ecs::systems::FireSystemInterface
{
public:
	/// The object has a fire at this temperature
	ecs::fire::FireEffect& Burn(entt::entity object, float temperature)
	{
		auto fire = std::make_unique<ecs::fire::FireEffect>();
		fire->object = object;
		fire->temperature = temperature;
		auto& made = *fire;
		_fires[object] = std::move(fire);
		return made;
	}

	void ProcessTurn() override {}
	void Update(float /*seconds*/) override {}
	void Reset() override
	{
		_fires.clear();
		_forgotten.clear();
	}
	void SetTemperature(entt::entity object, float temperature, entt::entity source) override
	{
		temperaturesSet.emplace_back(object, temperature);
		temperatureSources.push_back(source);
	}
	void SetOnFire(entt::entity object, float speed) override { setOnFire.emplace_back(object, speed); }
	void PutOut(entt::entity object) override { putOut.push_back(object); }
	void HeatHeldObject(entt::entity /*object*/) override {}
	void StartedMoving(entt::entity /*object*/, bool /*inHand*/) override {}
	void MoveFire(entt::entity /*from*/, entt::entity /*to*/) override {}
	void CopyFire(entt::entity /*from*/, entt::entity /*to*/) override {}
	void SetCanBeSetOnFire(entt::entity object, bool can) override { canBeSetOnFire.emplace_back(object, can); }
	void SetHurtByFire(entt::entity object, bool hurt) override { hurtByFire.emplace_back(object, hurt); }
	[[nodiscard]] float GetTemperature(entt::entity object) const override
	{
		const auto found = _fires.find(object);
		return found != _fires.end() ? found->second->temperature : 0.0f;
	}
	[[nodiscard]] bool IsOnFire(entt::entity object) const override
	{
		const auto found = _fires.find(object);
		return found != _fires.end() && found->second->IsOnFire();
	}
	[[nodiscard]] bool IsFireNear(const glm::vec3& point, float radius) const override
	{
		fireNearAsked.emplace_back(point, radius);
		return fireNear;
	}
	[[nodiscard]] float GetCharring(entt::entity /*object*/) const override { return 0.0f; }
	[[nodiscard]] ecs::fire::FireEffect* FindByObject(entt::entity object) override
	{
		const auto found = _fires.find(object);
		return found != _fires.end() ? found->second.get() : nullptr;
	}
	[[nodiscard]] ecs::fire::FireEffect* FindById(uint32_t /*id*/) override { return nullptr; }
	[[nodiscard]] uint32_t TakeId() override { return 0; }
	[[nodiscard]] uint8_t TakeCreateTag() override { return 0; }
	[[nodiscard]] uint8_t CreateTag() const override { return 0; }
	void SetCreateTag(uint8_t /*tag*/) override {}
	[[nodiscard]] uint8_t ProcessTag() const override { return 0; }
	void SetProcessTag(uint8_t /*tag*/) override {}
	ecs::fire::FireEffect& Insert(std::unique_ptr<ecs::fire::FireEffect> fire) override
	{
		auto& made = *fire;
		_fires[fire->object] = std::move(fire);
		return made;
	}
	void Unlist(const ecs::fire::FireEffect& /*fire*/) override {}
	void ForgetObject(entt::entity object) override
	{
		// the fire stays alive, as the game's list keeps it, until the deleted fires are freed
		if (const auto found = _fires.find(object); found != _fires.end())
		{
			_forgotten.push_back(std::move(found->second));
			_fires.erase(found);
		}
	}
	void ForgetId(uint32_t /*id*/) override {}
	void MoveObject(entt::entity /*from*/, entt::entity /*to*/, ecs::fire::FireEffect& /*fire*/) override {}
	[[nodiscard]] const std::vector<ecs::fire::FireEffect*>& List() const override { return _list; }
	void FreeDeleted() override { _forgotten.clear(); }
	void ClearFires() override
	{
		_fires.clear();
		_forgotten.clear();
	}
	[[nodiscard]] Graphics& AllGraphics() override { return _graphics; }
	void SetGraphic(uint32_t /*fire*/, std::unique_ptr<ecs::fire::graphic::Graphic> /*graphic*/) override {}
	void EraseGraphic(uint32_t /*fire*/) override {}
	void ClearGraphics() override {}
	[[nodiscard]] bool GraphicSourceAdded() const override { return false; }
	void SetGraphicSourceAdded() override {}
	[[nodiscard]] Slots& GetSlots() override { return _slots; }
	[[nodiscard]] float MaxDistance() const override { return 0.0f; }
	void SetMaxDistance(float /*distance*/) override {}
	[[nodiscard]] Owners& GetOwners() override { return _owners; }

	/// Every temperature an object was made, in order, and the source given with it; nothing else follows from them
	std::vector<std::pair<entt::entity, float>> temperaturesSet;
	std::vector<entt::entity> temperatureSources;
	/// Every object set alight, with its speed, and every one put out, in order
	std::vector<std::pair<entt::entity, float>> setOnFire;
	std::vector<entt::entity> putOut;
	/// Every object a script let be hurt or set alight, or not, in order
	std::vector<std::pair<entt::entity, bool>> hurtByFire;
	std::vector<std::pair<entt::entity, bool>> canBeSetOnFire;
	/// Every point and radius asked whether a fire is near, and the answer given
	mutable std::vector<std::pair<glm::vec3, float>> fireNearAsked;
	bool fireNear {false};

private:
	std::map<entt::entity, std::unique_ptr<ecs::fire::FireEffect>> _fires;
	std::vector<std::unique_ptr<ecs::fire::FireEffect>> _forgotten;
	std::vector<ecs::fire::FireEffect*> _list;
	Graphics _graphics;
	Slots _slots {};
	Owners _owners;
};
} // namespace openblack::test
