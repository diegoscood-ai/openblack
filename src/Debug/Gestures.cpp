/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Gestures.h"

#include <algorithm>
#include <string>

#include <glm/vec2.hpp>

#include "Camera/CameraShake.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "GesturesModel.h"
#include "Locator.h"
#include "Magic/Gestures/GestureIconModels.h"
#include "Magic/Gestures/GestureIconSeeds.h"
#include "Magic/Gestures/GestureIcons.h"
#include "Magic/Gestures/GestureInput.h"
#include "Magic/Gestures/GestureMatch.h"
#include "Magic/Gestures/PowerUpSystem.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;
using namespace openblack::debug::gui;
using namespace openblack::debug::gestures_window;
namespace gestures = openblack::magic::gestures;

namespace
{
constexpr ImU32 k_PathColour = IM_COL32(255, 255, 255, 160);
constexpr ImU32 k_CornerColour = IM_COL32(255, 220, 60, 255);
constexpr ImU32 k_StartColour = IM_COL32(80, 255, 80, 255);
constexpr ImU32 k_EndColour = IM_COL32(255, 80, 80, 255);
constexpr ImU32 k_MatchColour = IM_COL32(60, 200, 255, 220);
/// How wide a template is drawn, in pixels
constexpr float k_DrawnSize = 320.0f;

/// The game window's size in pixels, which the mouse positions are measured in
glm::vec2 WindowSize()
{
	if (Locator::windowing::has_value())
	{
		const auto size = Locator::windowing::value().GetSize();
		return {static_cast<float>(size.x), static_cast<float>(size.y)};
	}
	return {ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y};
}

/// From the game window's pixels to ImGui's
ImVec2 Display(float x, float y)
{
	const auto window = WindowSize();
	const auto& display = ImGui::GetIO().DisplaySize;
	const float scaleX = window.x > 0.0f ? display.x / window.x : 1.0f;
	const float scaleY = window.y > 0.0f ? display.y / window.y : 1.0f;
	return {x * scaleX, y * scaleY};
}

ImU32 KeyPointColour(KeyPoint key)
{
	switch (key)
	{
	case KeyPoint::Start:
		return k_StartColour;
	case KeyPoint::Corner:
		return k_CornerColour;
	case KeyPoint::End:
		return k_EndColour;
	default:
		return 0u;
	}
}
} // namespace

Gestures::Gestures() noexcept
    : Window("Gestures", ImVec2(460.0f, 520.0f))
{
}

void Gestures::Draw() noexcept
{
	ImGui::Checkbox("Draw the hand's path over the screen", &_overlay);
	if (_overlay)
	{
		DrawOverlay();
	}
	DrawState();
	ImGui::Separator();
	DrawTools();
}

void Gestures::DrawOverlay() const noexcept
{
	const auto& path = gestures::State().system;
	auto* list = ImGui::GetForegroundDrawList();
	for (int i = 1; i < path.Count(); ++i)
	{
		const auto& from = path.At(i - 1);
		const auto& to = path.At(i);
		list->AddLine(Display(from.sx, from.sz), Display(to.sx, to.sz), k_PathColour, 2.0f);
	}
	for (int i = 0; i < path.Count(); ++i)
	{
		const auto& sample = path.At(i);
		if (const auto colour = KeyPointColour(KeyPointOf(sample.flags)); colour != 0u)
		{
			list->AddCircleFilled(Display(sample.sx, sample.sz), 5.0f, colour);
		}
	}

	// The gestures the path matches now, by its newest sample
	if (path.Count() == 0)
	{
		return;
	}
	const auto& templates = gestures::Templates();
	const auto ratio = gestures::sampling::ScreenRatio();
	const auto matches = MatchesNow(templates, gestures::BuildFromSystem(path, ratio), ratio);
	auto position = Display(path.At(path.Count() - 1).sx, path.At(path.Count() - 1).sz);
	position.x += 10.0f;
	for (const auto& match : matches)
	{
		const auto label = std::string(GestureName(match.gesture)) + (match.mirrored ? " mirrored" : "");
		list->AddText(position, k_MatchColour, label.c_str());
		position.y += ImGui::GetTextLineHeight();
	}
}

void Gestures::DrawState() noexcept
{
	const auto& state = gestures::State();
	const auto& templates = gestures::Templates();
	const auto ratio = gestures::sampling::ScreenRatio();
	const auto keyPoints = gestures::BuildFromSystem(state.system, ratio);
	ImGui::Text("%zu templates, %d points recorded, %d key points", templates.size(), static_cast<int>(state.system.Count()),
	            static_cast<int>(keyPoints.count));
	// a changed Gestures.jty's templates of more than 80 keypoints, and those whose turns run past the list's end
	{
		const auto isLong = [](const gestures::GestureData& t) { return t.count > gestures::k_MaxSamples; };
		// the matcher reads every turn but the last keypoint's
		const auto pastTheList = [](const gestures::GestureData& t) {
			return t.count > gestures::k_MaxSamples + 1 && t.laterTurns.size() < t.count - gestures::k_MaxSamples - 1;
		};
		if (const auto longOnes = std::ranges::count_if(templates, isLong); longOnes > 0)
		{
			ImGui::Text("%d templates of more than 80 key points, %d never taken (their turns run past the list)",
			            static_cast<int>(longOnes), static_cast<int>(std::ranges::count_if(templates, pastTheList)));
		}
	}
	// the mouse messages the samples come from: a still mouse still sends one every 40 ms while the window is active
	{
		namespace messages = gestures::mouse_messages;
		const auto& clock = gestures::sampling::MessageClock();
		ImGui::Text("Mouse messages: %.0f ms since the last, the input thread %s %.0f ms, %.0f ms toward a sample",
		            static_cast<double>(clock.sinceMessage),
		            clock.thread == messages::Thread::Waiting ? "waiting for a move" : "asleep (window inactive)",
		            static_cast<double>(clock.threadElapsed), static_cast<double>(clock.handlerMs));
	}
	if (Locator::gestureSystem::has_value())
	{
		ImGui::Text("Leash picker: %s", Locator::gestureSystem::value().IsLeashPickerOpen() ? "open" : "closed");
	}
	// the creature's fight, which keeps the selections and the leash from being looked for
	if (const auto fight = gestures::CurrentFight(); fight.fighting)
	{
		ImGui::Text("Your creature fights: the fight controls %s%s", fight.controlsUp ? "up" : "down",
		            fight.controlsUp && fight.moveHeld ? ", the move button wipes the path" : "");
	}
	// the hand's readiness for an object, which a miracle's seed needs to go into the hand
	if (Locator::handSystem::has_value())
	{
		const auto& hand = Locator::handSystem::value();
		ImGui::Text("Hand ready for an object: %s (a creature locked for the hand: %s)",
		            hand.IsHandReadyForObject() ? "yes" : "no", hand.IsCreatureLockedForHand() ? "yes" : "no");
	}
	// why the interface's pass wipes the path instead of looking for gestures, if it does
	if (const auto pass = gestures::CurrentPassState(); !gestures::PowerUpPassGoesOn(pass))
	{
		ImGui::Text("Path wiped every pass:%s%s%s%s", pass.insideCitadel ? " inside the temple" : "",
		            pass.lockedSelect ? " locked select" : "", pass.interfaceActive ? "" : " interface inactive",
		            pass.boxHasMouse ? " a box has the mouse" : "");
	}
	// a camera shake keeps the path while the camera moves
	if (Locator::scriptState::has_value() && !camera_shake::Checkers().empty())
	{
		ImGui::Text("The camera shakes: a moving camera keeps the path");
	}
	if (state.circlePending)
	{
		ImGui::Text("Circle (%s) at %.1f, %.1f, %.1f, size %.1f, held for %.1f s", GestureName(state.circleGesture).data(),
		            static_cast<double>(state.circlePosition.x), static_cast<double>(state.circlePosition.y),
		            static_cast<double>(state.circlePosition.z), static_cast<double>(state.circleSize),
		            static_cast<double>(state.circleTimer));
	}
	if (state.selection.open)
	{
		ImGui::Text("Miracle selection open: %s, stage %u, %.1f s", GestureName(state.selection.category).data(),
		            state.selection.stage, static_cast<double>(state.selection.timer));
	}
	if (state.cooldown > 0.0f)
	{
		ImGui::Text("Resting for %.2f s after a gesture", static_cast<double>(state.cooldown));
	}

	ImGui::SeparatorText("Waiting for");
	bool waiting = false;
	for (size_t gesture = 0; gesture < state.lookingFor.size(); ++gesture)
	{
		const auto purpose = LookingForName(state.lookingFor.at(gesture).type);
		if (!purpose.empty())
		{
			waiting = true;
			ImGui::BulletText("%s: %s", GestureName(static_cast<gestures::Gesture>(gesture)).data(), purpose.c_str());
		}
	}
	if (!waiting)
	{
		ImGui::TextDisabled("nothing");
	}

	// the gesture icons at the bottom right of the screen, from the table above, and their fade
	ImGui::SeparatorText("Gesture icons");
	const auto& icons = gestures::IconState();
	ImGui::Text("Alpha %.2f%s", static_cast<double>(icons.alpha), icons.helpTextShown ? ", a help text is on the screen" : "");
	if (icons.list.Icons().empty())
	{
		ImGui::TextDisabled("none");
	}
	// what each shows above its symbol
	const auto modelName = [](gestures::IconModelKind kind) {
		switch (kind)
		{
		case gestures::IconModelKind::Leash:
			return "the leash";
		case gestures::IconModelKind::SeedGraphic:
			return "the seed's graphic";
		case gestures::IconModelKind::Bubble:
			return "the bubble";
		case gestures::IconModelKind::None:
		default:
			return "no model";
		}
	};
	for (const auto& icon : icons.list.Icons())
	{
		ImGui::BulletText("%s: %s, seed %d, leash %d, power-up %d, sparkle at %.2f, %s", GestureName(icon.gesture).data(),
		                  LookingForName(icon.order).c_str(), icon.seed, icon.leash, icon.powerUp,
		                  static_cast<double>(icon.phase), modelName(gestures::IconModelKindOf(icon)));
	}
	// the seeds' graphics inside the icons, one for each magic type, kept until the map is cleared
	const auto& seeds = gestures::IconSeeds();
	if (std::ranges::any_of(seeds, [](const auto& graphic) { return graphic.has_value(); }))
	{
		ImGui::TextUnformatted("Seed graphics inside the icons:");
		for (size_t magic = 0; magic < seeds.size(); ++magic)
		{
			if (const auto& graphic = seeds.at(magic); graphic.has_value())
			{
				ImGui::BulletText("magic %zu: seed %d, power-up %d, holder effect %s, turned %.2f, pulse at %.2f", magic,
				                  static_cast<int>(graphic->seed), graphic->powerUp,
				                  graphic->effect != 0 ? std::to_string(graphic->effect).c_str() : "none",
				                  static_cast<double>(graphic->spin), static_cast<double>(graphic->pulsePhase));
			}
		}
	}

	ImGui::SeparatorText("Matches now");
	const auto matches = MatchesNow(templates, keyPoints, ratio);
	if (matches.empty())
	{
		ImGui::TextDisabled("nothing");
	}
	for (const auto& match : matches)
	{
		ImGui::BulletText("%s (template %d)%s", GestureName(match.gesture).data(), static_cast<int>(match.templateIndex),
		                  match.mirrored ? " mirrored" : "");
	}

	ImGui::SeparatorText("Last recognised");
	if (state.result.gesture != gestures::k_None)
	{
		ImGui::Text("%s by template %d%s, key points %d to %d", GestureName(state.result.gesture).data(),
		            static_cast<int>(state.result.templateIndex), state.result.reversed ? " mirrored" : "",
		            static_cast<int>(state.result.start), static_cast<int>(state.result.end));
		const auto& packet = state.gesture;
		ImGui::Text("at %.1f, %.1f, %.1f, size %.1f", static_cast<double>(packet.position.x),
		            static_cast<double>(packet.position.y), static_cast<double>(packet.position.z),
		            static_cast<double>(packet.size));
	}
	else
	{
		ImGui::TextDisabled("nothing yet");
	}
}

void Gestures::DrawTools() noexcept
{
	// Draw a gesture through the recogniser, from its first template, as the mouse would
	ImGui::SetNextItemWidth(200.0f);
	if (ImGui::BeginCombo("##gesture", GestureName(static_cast<gestures::Gesture>(_gesture)).data()))
	{
		for (int i = 1; i < static_cast<int>(gestures::k_GestureCount); ++i)
		{
			if (ImGui::Selectable(GestureName(static_cast<gestures::Gesture>(i)).data(), i == _gesture))
			{
				_gesture = i;
			}
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	const auto* gestureTemplate = FirstTemplate(gestures::Templates(), static_cast<gestures::Gesture>(_gesture));
	const bool playing = gestures::sampling::PlayingStroke();
	ImGui::BeginDisabled(gestureTemplate == nullptr || playing);
	if (ImGui::Button("Draw it") && gestureTemplate != nullptr)
	{
		const auto points =
		    TemplateStroke(*gestureTemplate, k_DrawnSize, WindowSize() * 0.5f, gestures::sampling::ScreenRatio());
		if (points.size() >= 2)
		{
			gestures::sampling::PlayStroke(MousePositions(points));
		}
	}
	ImGui::EndDisabled();
	if (playing)
	{
		ImGui::SameLine();
		ImGui::TextDisabled("drawing...");
	}
	if (ImGui::Button("Forget the path"))
	{
		gestures::ClearBuffer();
	}
}
