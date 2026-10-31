/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Camera/CreatureInteractCameraModel.h"
#include "ECS/Components/HandOnCreature.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class CreatureHandSystem final: public CreatureHandSystemInterface
{
public:
	bool Grab(entt::entity creature) override;
	[[nodiscard]] bool MayHold(entt::entity creature) const override;
	std::optional<HandPose> Update(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, glm::vec2 cursor,
	                               float screenWidth, float seconds) override;
	[[nodiscard]] std::optional<Feedback> Release() override;
	[[nodiscard]] std::optional<HandPose> Pose() const override { return _pose; }
	[[nodiscard]] std::optional<entt::entity> GetCreature() const override;
	[[nodiscard]] std::optional<CreatureHit> CreatureAlong(const glm::vec3& rayOrigin,
	                                                       const glm::vec3& rayDirection) const override;
	[[nodiscard]] std::optional<entt::entity> CreatureUnderHand() const override { return _underHand; }
	void SetCreatureUnderHand(std::optional<entt::entity> creature) override { _underHand = creature; }
	[[nodiscard]] float GetFeedbackSum() const override;
	[[nodiscard]] float GetLastFeedbackSum() const override;
	bool Stroke(entt::entity creature, creature_feedback::BodyPart part) override;
	bool Slap(entt::entity creature, float heightShare, bool gentle, bool sweepsRight) override;
	[[nodiscard]] bool IsHeldByCommand() const override;
	void FollowWithCamera(entt::entity creature) override;
	void GiveCameraBack() override;
	void SetInteractionCamera(bool on) override;
	[[nodiscard]] bool InteractionCamera() const override { return _cameraOn; }
	[[nodiscard]] std::optional<creature_interact::Holder> CameraTakenFrom() const override
	{
		return _cameraModel != nullptr ? std::optional(_takenFrom) : std::nullopt;
	}
	[[nodiscard]] std::optional<creature_interact::Holder> LastTakenOff() const override { return _lastTakenOff; }

private:
	/// The hand on the creature by a command: the hold it already has on it, or a new one; none when it may not hold
	/// it, or holds another creature (the button has to let that one go first, so that its feedback reaches it)
	components::HandOnCreature* HoldByCommand(entt::entity creature);
	/// What a step of the hand on the creature goes on
	struct StepInput
	{
		/// Where the hand is beside the body, and where it touches the body if it does
		glm::vec3 point;
		std::optional<glm::vec3> touch;
		glm::vec2 cursor;
		float screenWidth;
		float stepMs;
		/// The body's drawn matrix and height this frame
		glm::mat4 drawn;
		float height;
	};
	/// A step of stroking and slapping, once a frame
	static void Step(components::HandOnCreature& contact, const StepInput& input);
	/// Where the hand was drawn by the last Update while held to a creature
	std::optional<HandPose> _pose;
	/// The creature the hand's pick found under the hand this frame
	std::optional<entt::entity> _underHand;
	/// The camera on the creature while the hand interacts with it, and where the player's camera was going when it
	/// took over
	CreatureInteractCameraModel* _cameraModel {nullptr};
	std::optional<creature_interact::View> _returnTo;
	/// The camera that had the view when the interaction took it
	creature_interact::Holder _takenFrom {creature_interact::Holder::PlayersCamera};
	/// The camera the end of the last hold that did not get the view took off
	std::optional<creature_interact::Holder> _lastTakenOff;
	/// The end of a hold that did not get the view: the camera that kept it is taken off
	void TakeOffTheCameraThatKeptTheView();
	/// The interaction has asked for the camera since the hand took hold (creature_interact::AsksForTheCamera)
	bool _cameraAsked {false};
	bool _cameraOn {true};
};

} // namespace openblack::ecs::systems
