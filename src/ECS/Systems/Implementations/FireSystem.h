/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/FireSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// The fires, their graphics and their crackle, kept for the whole game; each land empties them (Reset)
class FireSystem final: public FireSystemInterface
{
public:
	void ProcessTurn() override;
	void Update(float seconds) override;
	void Reset() override;

	void SetTemperature(entt::entity object, float temperature, entt::entity source) override;
	void SetOnFire(entt::entity object, float speed) override;
	void PutOut(entt::entity object) override;
	void HeatHeldObject(entt::entity object) override;
	void StartedMoving(entt::entity object, bool inHand) override;
	void SetCanBeSetOnFire(entt::entity object, bool can) override;
	void SetHurtByFire(entt::entity object, bool hurt) override;

	[[nodiscard]] float GetTemperature(entt::entity object) const override;
	[[nodiscard]] bool IsOnFire(entt::entity object) const override;
	[[nodiscard]] bool IsFireNear(const glm::vec3& point, float radius) const override;
	[[nodiscard]] float GetCharring(entt::entity object) const override;

	[[nodiscard]] fire::FireEffect* FindByObject(entt::entity object) override;
	[[nodiscard]] fire::FireEffect* FindById(uint32_t id) override;
	[[nodiscard]] uint32_t TakeId() override;
	[[nodiscard]] uint8_t TakeCreateTag() override;
	[[nodiscard]] uint8_t ProcessTag() const override;
	void SetProcessTag(uint8_t tag) override;
	fire::FireEffect& Insert(std::unique_ptr<fire::FireEffect> fire) override;
	void Unlist(const fire::FireEffect& fire) override;
	void ForgetObject(entt::entity object) override;
	void ForgetId(uint32_t id) override;
	void MoveObject(entt::entity from, entt::entity to, fire::FireEffect& fire) override;
	[[nodiscard]] const std::vector<fire::FireEffect*>& List() const override;
	void FreeDeleted() override;
	void ClearFires() override;

	[[nodiscard]] Graphics& AllGraphics() override;
	void SetGraphic(uint32_t fire, std::unique_ptr<fire::graphic::Graphic> graphic) override;
	void EraseGraphic(uint32_t fire) override;
	void ClearGraphics() override;
	[[nodiscard]] bool GraphicSourceAdded() const override;
	void SetGraphicSourceAdded() override;

	[[nodiscard]] Slots& GetSlots() override;
	[[nodiscard]] float MaxDistance() const override;
	void SetMaxDistance(float distance) override;
	[[nodiscard]] Owners& GetOwners() override;

private:
	// The crackle
	Slots _slots {};
	float _maxDistance {0.0f};
	Owners _owners;

	// The graphics
	Graphics _graphics;
	bool _graphicSourceAdded {false};

	// The fires: every one owned; the list order (newest first) is _list
	std::vector<std::unique_ptr<fire::FireEffect>> _pool;
	std::vector<fire::FireEffect*> _list;
	std::unordered_map<entt::entity, fire::FireEffect*> _byObject;
	std::unordered_map<uint32_t, fire::FireEffect*> _byId;
	uint32_t _nextId {1};
	/// The tag ProcessList processes (0 every turn) and the tag of the next fire (0 after each)
	uint8_t _processTag {0};
	uint8_t _createTag {0};
};
} // namespace openblack::ecs::systems
