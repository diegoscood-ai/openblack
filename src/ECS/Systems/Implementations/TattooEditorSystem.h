/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <memory>
#include <vector>

#include "Creature/CreatureFace.h"
#include "ECS/Systems/TattooEditorSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class TattooEditorSystem final: public TattooEditorSystemInterface
{
public:
	/// Where the creature's tattoos are read from and written to: its skin, painted again as they change
	class Skin
	{
	public:
		virtual ~Skin() = default;
		[[nodiscard]] virtual std::optional<creature_tattoo::Slots> Read(entt::entity creature) const = 0;
		virtual void Write(entt::entity creature, size_t slot, const creature_tattoo::Slot& tattoo) = 0;
		/// The creature room's copy of the creature, the one the editor shows, pulls a face for 1.8 seconds
		virtual void CopyPullsFace(creature_face::Face /*face*/) {}
		/// The tattoos put on as a drag's end puts them: the slots, a new block of the player's own pictures they show and
		/// the player's masks made again from them
		virtual void PutOn(entt::entity creature, const creature_tattoo::Slots& slots)
		{
			for (size_t i = 0; i < slots.size(); ++i)
			{
				Write(creature, i, slots.at(i));
			}
		}
		/// The player's masks made again from the creature's tattoos and block of pictures, and the creature painted again
		virtual void RemakeMasks(entt::entity /*creature*/) {}
		/// The body the editor showed from the Players page goes
		virtual void DestroyShown(entt::entity /*shown*/) {}
	};
	/// How long the shown copy pulls a face as a tattoo goes on or comes off
	static constexpr float k_TattooFaceMs = 1800.0f;

	/// The creatures' skins in the registry
	TattooEditorSystem();
	explicit TattooEditorSystem(std::unique_ptr<Skin> skin);

	void Open(entt::entity creature, const creature_tattoo_editor::Orbit& view) override;
	void OpenFromMenu(std::optional<entt::entity> playerCreature, entt::entity shown) override;
	void Close() override;
	[[nodiscard]] bool IsOpen() const override { return _open; }
	[[nodiscard]] std::optional<entt::entity> GetCreature() const override { return _creature; }
	[[nodiscard]] std::optional<entt::entity> GetShown() const override { return _shown; }
	[[nodiscard]] const creature_tattoo_editor::Orbit& GetView() const override { return _view; }
	[[nodiscard]] const creature_tattoo_editor::Session& GetSession() const override { return _session; }
	void Update(float milliseconds) override;
	void Steer(glm::ivec2 grab, glm::ivec2 pointer, float milliseconds) override;
	void Hover(std::span<const creature_tattoo_editor::SiteOnScreen> sites, glm::ivec2 pointer) override;
	[[nodiscard]] std::optional<uint8_t> GetHoveredSite() const override { return _hovered; }
	[[nodiscard]] std::optional<glm::ivec2> GetHoveredPoint() const override { return _hoveredPoint; }
	bool Drop(uint8_t design, const glm::u8vec3& colour) override;
	std::optional<creature_tattoo::Slot> Lift() override;
	creature_tattoo_editor::Accept Ok(bool networkGame) override;
	void Answered() override { _session.changed = false; }
	void Cancel() override;

private:
	/// The creatures wear the tattoos as they are now
	void Write(const creature_tattoo::Slots& slots);
	/// Whether a creature still has its skin
	[[nodiscard]] bool HasSkin(entt::entity creature) const;
	/// The creatures that wear the tattoos and still have their skin: the player's, and the body shown from the Players
	/// page
	[[nodiscard]] std::vector<entt::entity> Wearers() const;

	std::unique_ptr<Skin> _skin;
	bool _open {false};
	std::optional<entt::entity> _creature;
	std::optional<entt::entity> _shown;
	creature_tattoo_editor::Session _session;
	/// The view, kept from one opening to the next: the Players page's editor starts from the one the editor last had,
	/// at first the menu's own
	creature_tattoo_editor::Orbit _view {.eye = creature_tattoo_editor::k_MenuEye,
	                                     .focus = creature_tattoo_editor::k_MenuFocus,
	                                     .turnSpeed = 0.0f,
	                                     .tipSpeed = 0.0f};
	std::optional<uint8_t> _hovered;
	std::optional<glm::ivec2> _hoveredPoint;
};

} // namespace openblack::ecs::systems
