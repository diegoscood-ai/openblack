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
#include <memory>
#include <unordered_map>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "ECS/Fire/FireEffect.h"
#include "ECS/Fire/FireGraphicData.h"

namespace openblack::ecs::fire::sound
{
/// One of the two crackle slots: the fire playing in it and its camera distance
struct Slot
{
	FireEffect* fire {nullptr};
	float distance {0.0f};
};
} // namespace openblack::ecs::fire::sound

namespace openblack::ecs::systems
{

/// Fire: everything hotter than the air has a temperature (see fire/FireModel.h). Miracles, scripts and other fires heat
/// things; at their combustion temperature they burn, hurt themselves, heat what is round them and spread through a
/// forest or a town as one blaze, while water, the rain and the villagers beating them cool them. Burning things are
/// drawn with flames, steam as they cool and smoke as they go out, and crackle near the camera.
///
/// The fires themselves run in ecs::fire (ECS/Fire), which keeps everything it needs between calls here: the fires,
/// their graphics and their crackle.
class FireSystemInterface
{
public:
	virtual ~FireSystemInterface() = default;

	/// Once a game turn, newest fire first: each burns, cools, hurts its object and heats what is round it. A fire made
	/// during the walk waits for the next turn.
	virtual void ProcessTurn() = 0;
	/// Once a frame: the flames, steam and smoke move on by the game time
	virtual void Update(float seconds) = 0;
	/// A new land: no fires, no graphics, no crackle
	virtual void Reset() = 0;

	/// An object is made exactly this hot, setting it alight only when that is hotter than it is; an existing fire just
	/// takes the temperature, lower too. It never heats its source back.
	virtual void SetTemperature(entt::entity object, float temperature, entt::entity source) = 0;
	/// An object is set alight at a speed: its combustion temperature and that much more of twice it
	virtual void SetOnFire(entt::entity object, float speed) = 0;
	/// An object goes back to the air's temperature, putting it out
	virtual void PutOut(entt::entity object) = 0;
	/// An object held in a hand catches from the fires in its cell
	virtual void HeatHeldObject(entt::entity object) = 0;
	/// An object is picked up or thrown: it leaves its blaze, and the living flee it in the hand
	virtual void StartedMoving(entt::entity object, bool inHand) = 0;
	/// A script keeps an object from catching, or lets it catch without being hurt
	virtual void SetCanBeSetOnFire(entt::entity object, bool can) = 0;
	virtual void SetHurtByFire(entt::entity object, bool hurt) = 0;

	[[nodiscard]] virtual float GetTemperature(entt::entity object) const = 0;
	[[nodiscard]] virtual bool IsOnFire(entt::entity object) const = 0;
	/// Whether anything burns within a radius of a point: an object of the map cells round it, a worship site measured
	/// from its totem
	[[nodiscard]] virtual bool IsFireNear(const glm::vec3& point, float radius) const = 0;
	/// How charred an object is, 0..1
	[[nodiscard]] virtual float GetCharring(entt::entity object) const = 0;

	// What ecs::fire keeps between calls
	// The fires: every FireEffect, owned until the end of the turn it is deleted in, the list (newest first), the
	// look-ups by object and by handle, the next handle and the two process tags

	/// The object's fire, or null
	[[nodiscard]] virtual fire::FireEffect* FindByObject(entt::entity object) = 0;
	/// A fire by its handle while it is listed, or null
	[[nodiscard]] virtual fire::FireEffect* FindById(uint32_t id) = 0;
	/// Takes the next handle (never reused within a game)
	[[nodiscard]] virtual uint32_t TakeId() = 0;
	/// The tag of the next fire, which then goes back to 0
	[[nodiscard]] virtual uint8_t TakeCreateTag() = 0;
	/// The tag ProcessList processes
	[[nodiscard]] virtual uint8_t ProcessTag() const = 0;
	virtual void SetProcessTag(uint8_t tag) = 0;
	/// The fire is found by its object and its handle, put at the head of the list and kept until freed
	virtual fire::FireEffect& Insert(std::unique_ptr<fire::FireEffect> fire) = 0;
	/// The fire leaves the list (it stays owned)
	virtual void Unlist(const fire::FireEffect& fire) = 0;
	/// The object no longer finds a fire
	virtual void ForgetObject(entt::entity object) = 0;
	/// The handle no longer finds a fire
	virtual void ForgetId(uint32_t id) = 0;
	/// The fire is found by `to` instead of `from`
	virtual void MoveObject(entt::entity from, entt::entity to, fire::FireEffect& fire) = 0;
	/// Every listed fire, newest first
	[[nodiscard]] virtual const std::vector<fire::FireEffect*>& List() const = 0;
	/// The fires marked deleted are freed
	virtual void FreeDeleted() = 0;
	/// A land is loaded: no fires and both tags 0; the handles go on
	virtual void ClearFires() = 0;

	// The graphics, by fire handle, and whether their draw has been handed to the particle manager
	using Graphics = std::unordered_map<uint32_t, std::unique_ptr<fire::graphic::Graphic>>;
	/// Every fire's graphic, by its fire's handle
	[[nodiscard]] virtual Graphics& AllGraphics() = 0;
	/// The fire's graphic is this one (an older one goes)
	virtual void SetGraphic(uint32_t fire, std::unique_ptr<fire::graphic::Graphic> graphic) = 0;
	/// The fire's graphic goes
	virtual void EraseGraphic(uint32_t fire) = 0;
	/// A land is loaded: no graphics. The draw stays handed over
	virtual void ClearGraphics() = 0;
	/// The draw of the fires was handed to the particle manager (once per game)
	[[nodiscard]] virtual bool GraphicSourceAdded() const = 0;
	virtual void SetGraphicSourceAdded() = 0;

	// The crackle: the two slots, the farthest slot's distance and each fire's sound owner
	using Slots = std::array<fire::sound::Slot, 2>;
	/// The channels' owner of each fire that played: an audio object number
	using Owners = std::unordered_map<const fire::FireEffect*, uint32_t>;
	[[nodiscard]] virtual Slots& GetSlots() = 0;
	[[nodiscard]] virtual float MaxDistance() const = 0;
	virtual void SetMaxDistance(float distance) = 0;
	[[nodiscard]] virtual Owners& GetOwners() = 0;
};

} // namespace openblack::ecs::systems
