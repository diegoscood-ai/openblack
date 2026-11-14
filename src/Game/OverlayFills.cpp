/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "OverlayFills.h"

#include <cstdint>

#include <algorithm>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

#include "3D/AffineMatrix.h"
#include "3D/Billboard.h"
#include "3D/TempleInteriorInterface.h"
#include "3D/TempleScrollPictures.h"
#include "Audio/Services/ScriptAudioState.h"
#include "Camera/Camera.h"
#include "Camera/ScreenPoint.h"
#include "Camera/ScriptCamera.h"
#include "Common/HelpText.h"
#include "ECS/AbodeKnock.h"
#include "ECS/Archetypes/OneOffSpellSeedArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/FloatingNumber.h"
#include "ECS/Components/Transform.h"
#include "ECS/FloatingNumber.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AbodeKnockSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "GameClock.h"
#include "GameServices.h"
#include "Graphics/ArgbColour.h"
#include "Graphics/GestureIconEffect.h"
#include "Graphics/OverlayFrame.h"
#include "Graphics/RendererInterface.h"
#include "Gui/CinemaBars.h"
#include "Gui/VillagerLabels.h"
#include "Help/Bubble.h"
#include "Help/DialogueText.h"
#include "Help/HelpSystem.h"
#include "Help/HelpTextDisplay.h"
#include "Help/InputPromptIcon.h"
#include "Help/Karaoke.h"
#include "Help/SpeedIndicator.h"
#include "Input/GameCursor.h"
#include "Locator.h"
#include "Magic/Gestures/GestureIconModels.h"
#include "Magic/Gestures/GestureIconSeeds.h"
#include "Magic/Gestures/GestureIcons.h"
#include "Magic/Gestures/GestureShapes.h"
#include "Resources/ResourceManager.h"
#include "Resources/SharedAssets.h"
#include "Sound/GameMusic.h"
#include "Video/FallingSpellVideo.h"
#include "Video/VideoPlayer.h"

namespace openblack::game_overlay
{

/// The read-out's little people over the drawn buildings, as world triangles in the plane of the screen: the sprite's
/// cell 57 of misc0, its half size, no turn. One row per building, which the renderer draws right after the building
void FillKnockReadout(std::span<const entt::entity> drawn, const graphics::billboard::CameraFrame& frame,
                      std::vector<graphics::SpiritQuadVertex>& out, std::vector<graphics::KnockRow>& rows)
{
	out.clear();
	rows.clear();
	if (!Locator::abodeKnockSystem::has_value())
	{
		return;
	}
	const auto& knocks = Locator::abodeKnockSystem::value();
	const auto& registry = Locator::entitiesRegistry::value();
	for (const auto object : drawn)
	{
		if (!registry.AllOf<ecs::components::Abode>(object))
		{
			continue;
		}
		const auto first = static_cast<uint32_t>(out.size());
		for (const auto& person : knocks.PeopleOver(object))
		{
			const graphics::billboard::Sprite sprite {
			    .position = person.position,
			    .size = person.size,
			    .argb = person.colour,
			    .cell = ecs::abode_knock::k_PersonCell,
			};
			const auto quad = graphics::billboard::SpriteQuad(sprite, frame);
			if (!quad.has_value())
			{
				continue;
			}
			const uint32_t abgr = argb_colour::ToAbgr(person.colour);
			for (const int k : graphics::billboard::k_SpriteTriangles)
			{
				const auto& p = quad->corners.at(static_cast<size_t>(k));
				const auto& uv = quad->uv.at(static_cast<size_t>(k));
				out.push_back({p.x, p.y, p.z, uv.x, uv.y, abgr});
			}
		}
		if (const auto count = static_cast<uint32_t>(out.size()) - first; count != 0)
		{
			rows.push_back({.building = object, .first = first, .count = count});
		}
	}
}

/// The gesture icons (Magic/Gestures/GestureIcons.h), drawn with the world's particles: none inside the temple. Their
/// fade reads whether a help text was drawn on the screen the frame before, as the help text is drawn after them; this
/// frame's is kept for the next. Each icon's symbol and sparkles are sprites at their pixels, a fixed depth along the
/// view; its model (Magic/Gestures/GestureIconModels.h: a leash, the bubble, or a seed's graphic of
/// Magic/Gestures/GestureIconSeeds.h, whose holder effect steps here) is placed in the models' camera's space and moved
/// on the screen to the box above it
void FillGestureIcons(graphics::OverlayFrame& out)
{
	namespace gestures = magic::gestures;
	out.gestureIcons.clear();
	if (!Locator::magicSystem::has_value())
	{
		return;
	}
	auto& icons = gestures::IconState();
	const bool shownBefore = icons.helpTextShown;
	const auto* dialogue = TheDialogue();
	icons.helpTextShown = out.helpText.active && dialogue != nullptr &&
	                      dialogue->GetDisplay().IsTextDrawn(dialogue->GetTextDraw()) && dialogue->GetDisplay().HasNewestText();
	if (game_clock::IsInsideCitadel() || !Locator::camera::has_value() || out.width == 0 || out.height == 0)
	{
		return;
	}
	const auto draws =
	    gestures::StepIcons(icons.list.Icons(), icons.alpha, help::Get() != nullptr, shownBefore, game_clock::FrameGameMs(),
	                        [](gestures::Gesture g) -> const std::vector<glm::vec3>& { return gestures::ShapeOf(g).points; });
	const auto& camera = Locator::camera::value();
	const auto plane = screen_point::NearPlaneOf(glm::ivec2(out.width, out.height), camera.GetHorizontalFieldOfView(),
	                                             camera.GetPixelNearClip());
	const auto worldToCamera = affine::WorldToCamera(camera.GetOrigin(), camera.GetFocus());
	const auto eye = camera.GetEye();
	const auto at = [&](glm::ivec2 pixel) {
		return screen_point::PixelPoint(worldToCamera, eye, plane, pixel, gestures::k_IconDepth);
	};
	const auto screen = glm::ivec2(out.width, out.height);
	constexpr auto k_LeashMesh = resources::shared_assets::k_LeashCollarMesh.value();
	const auto bubbleMesh = resources::HashIdentifier(ecs::archetypes::OneOffSpellSeedArchetype::k_MeshName);
	const float bubbleHalfDiagonal = ecs::object::MeshHalfDiagonal(bubbleMesh);
	for (const auto& draw : draws)
	{
		graphics::GestureIconOverlay icon {.page = draw.page};
		const auto& listed = icons.list.Icons()[draw.icon];
		icon.depthBox = gestures::IconModelDepthBox(listed.box);
		icon.screenOffset = gestures::IconModelScreenOffset(listed.box, screen);
		// the model first, as the icon's draw does: a leash's spin turns only while its icon is drawn
		const auto model = gestures::StepIconModel(listed, gestures::IconLeashSpins(), icons.alpha, game_clock::FrameGameMs(),
		                                           bubbleHalfDiagonal);
		if (model.kind == gestures::IconModelKind::Leash || model.kind == gestures::IconModelKind::Bubble)
		{
			icon.models.push_back({
			    .mesh = model.kind == gestures::IconModelKind::Leash ? k_LeashMesh : bubbleMesh,
			    .matrix = model.matrix,
			    .uv = {0.0f, model.band},
			    .alpha = model.alpha,
			});
		}
		else if (model.kind == gestures::IconModelKind::SeedGraphic)
		{
			// the seed's graphic: its mesh (lit, its colour set), its holder effect drawn now, its bands
			if (auto seed = gestures::DrawIconSeed(listed, model.alpha, game_clock::FrameGameMs()); seed.has_value())
			{
				for (const auto& mesh : seed->draw.meshes)
				{
					if (!mesh.inReach)
					{
						continue;
					}
					icon.models.push_back({
					    .mesh = seed->mesh,
					    .matrix = mesh.matrix,
					    .uv = mesh.uv,
					    .alpha = static_cast<uint8_t>(mesh.colour >> 24u),
					    .colour = mesh.colour & 0xFFFFFFu,
					    .specular = mesh.specular,
					    .lit = true,
					    .landLight = mesh.landLight,
					    .fading = mesh.fading,
					});
				}
				if (!seed->sprites.empty())
				{
					icon.effect = std::make_shared<const graphics::GestureIconEffect>(
					    graphics::GestureIconEffect {.sprites = std::move(seed->sprites)});
				}
				for (const auto& band : seed->draw.bands)
				{
					if (!band.inReach)
					{
						continue;
					}
					// the band's own material: additive, two-sided, no Z write; its colour set, unlit
					icon.bands.push_back({
					    .mesh = resources::shared_assets::k_PowerUpBandMesh.value(),
					    .matrix = band.matrix,
					    .alpha = static_cast<uint8_t>(band.colour >> 24u),
					    .colour = band.colour & 0xFFFFFFu,
					    .specular = gestures::k_IconSeedBandSpecular,
					});
				}
			}
		}
		icon.symbol = {.position = at(draw.symbolPixel),
		               .size = gestures::k_IconSymbolHalfSize,
		               .argb = draw.symbolArgb,
		               .cell = draw.cell,
		               .cellsPerRow = gestures::k_IconSymbolCellsPerRow};
		for (int i = 0; i < draw.sparkles; ++i)
		{
			const auto k = static_cast<size_t>(i);
			icon.sparkles.push_back({.position = at(draw.sparklePixels.at(k)),
			                         .size = gestures::k_IconSparkleHalfSize,
			                         .argb = draw.sparkleArgb.at(k),
			                         .cell = gestures::k_IconSparkleCell});
		}
		out.gestureIcons.push_back(std::move(icon));
	}
}

/// The floating numbers drawn this frame (ecs/FloatingNumber.h): those still showing whose point falls in a pixel of
/// the screen, the furthest from the camera first, as the transparent queue draws them. None while the help system
/// has the wide screen on, nor inside the temple
void FillFloatingNumbers(int width, int height, std::vector<graphics::FloatingNumberOverlay>& out)
{
	out.clear();
	const auto* helpSystem = help::Get();
	const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	if (width == 0 || height == 0 || !Locator::camera::has_value() || inTemple ||
	    (helpSystem != nullptr && helpSystem->GetWideScreen() != 0))
	{
		return;
	}
	const auto& camera = Locator::camera::value();
	const auto eye = camera.GetOrigin();
	std::vector<std::pair<float, graphics::FloatingNumberOverlay>> placed;
	Locator::entitiesRegistry::value().Each<const ecs::components::FloatingNumber>(
	    [&](entt::entity /*unused*/, const ecs::components::FloatingNumber& number) {
		    const auto alpha = ecs::floating_number::Alpha(number.life);
		    glm::vec3 screen {0.0f};
		    const bool projected =
		        alpha.has_value() && camera.ProjectWorldToScreen(number.position, glm::vec4(0.0f, 0.0f, width, height), screen);
		    if (auto log = spdlog::get("game"); log != nullptr)
		    {
			    log->debug("Floating number \"{}\" at ({:.2f}, {:.2f}, {:.2f}) life {:.2f} alpha {} colour {:#010x}: {}{}",
			               number.text, number.position.x, number.position.y, number.position.z, number.life,
			               alpha.has_value() ? static_cast<int>(*alpha) : -1, number.colour,
			               projected ? "screen " : "not projected",
			               projected ? fmt::format("({:.1f}, {:.1f}, {:.3f})", screen.x, screen.y, screen.z) : std::string());
		    }
		    if (!projected)
		    {
			    return;
		    }
		    const auto pixel = ecs::floating_number::ScreenPixel(screen, {width, height});
		    if (!pixel.has_value())
		    {
			    return;
		    }
		    const glm::vec3 d = number.position - eye;
		    placed.push_back({glm::dot(d, d),
		                      {.screen = *pixel,
		                       .text = std::u16string(number.text.begin(), number.text.end()),
		                       .colour = number.colour,
		                       .alpha = *alpha}});
	    });
	std::ranges::stable_sort(placed, std::ranges::greater {}, &std::pair<float, graphics::FloatingNumberOverlay>::first);
	out.reserve(placed.size());
	for (auto& each : placed)
	{
		out.push_back(std::move(each.second));
	}
}

/// The villagers' names the land's draw placed this frame (Gui/VillagerLabels.h), laid out for the draw: each a box,
/// a pointer and its lines, drawn in the world at the depth the help system's bubbles take, in front of the point and
/// pulled towards the camera as it looks down
void FillVillagerNames(std::span<const gui::villager_labels::Label> labels, std::vector<graphics::VillagerNameOverlay>& out)
{
	out.clear();
	if (labels.empty() || !Locator::camera::has_value())
	{
		return;
	}
	const auto& camera = Locator::camera::value();
	float heading = 0.0f;
	float pitch = 0.0f;
	script_camera::HeadingAndPitchFromPoints(camera.GetOrigin(), camera.GetFocus(), heading, pitch);
	const float nearClip = camera.GetPixelNearClip();
	out.reserve(labels.size());
	for (const auto& label : labels)
	{
		graphics::VillagerNameOverlay name {.keyPoint = label.point, .depth = help::DrawDepth(label.depth, pitch, nearClip)};
		// the box: black, its alpha the box's at its top and bottom, its corners rounded by a quarter of its height
		const auto shape =
		    help::BubbleShape(label.left, label.top, label.right, label.bottom, (label.bottom - label.top) * 0.25f, 0.0f, false,
		                      0xFF000000u, false, label.boxAlpha, label.boxAlpha);
		name.box.reserve(shape.size());
		for (const auto& v : shape)
		{
			const uint32_t abgr = (v.argb & 0xFF00FF00u) | ((v.argb >> 16) & 0xFFu) | ((v.argb & 0xFFu) << 16);
			name.box.push_back({v.x, v.y, name.depth, v.u, v.v, abgr});
		}
		const auto pointer = gui::villager_labels::PointerShape(label);
		for (size_t i = 0; i < pointer.size(); ++i)
		{
			const auto& piece = pointer.at(i);
			name.pointer.at(i) = {static_cast<float>(piece.x0),
			                      static_cast<float>(piece.y0),
			                      static_cast<float>(piece.x1),
			                      static_cast<float>(piece.y1),
			                      piece.uv0.x,
			                      piece.uv0.y,
			                      piece.uv1.x,
			                      piece.uv1.y};
		}
		name.pointerAlpha = label.boxAlpha;
		// the lines, one under another from the box's top, set in from its left and top by the inset
		for (size_t line = 0; line < label.lines.size(); ++line)
		{
			name.lines.push_back(
			    {label.lines[line], label.left + label.inset,
			     ((static_cast<float>(static_cast<int32_t>(line)) * label.lineHeight) + label.inset) + label.top});
		}
		name.lineHeight = label.lineHeight;
		name.wordsAbgr = (static_cast<uint32_t>(label.wordsAlpha) << 24) | (static_cast<uint32_t>(label.colour.b) << 16) |
		                 (static_cast<uint32_t>(label.colour.g) << 8) | label.colour.r;
		out.push_back(std::move(name));
	}
}

/// The PreDraw for the overlays (Graphics/OverlayFrame.h): what Renderer::DrawFinishFrameOverlays and
/// DrawHelpText draw, read from the game and laid out once a frame before DrawScene, at the Main view's resolution.
/// boxActive: a box with a background panel takes the input (GameInterface::IsBoxActive)
void FillOverlayFrame(const ecs::systems::CinematicDirectorSystemInterface& director, bool boxActive,
                      graphics::OverlayFrame& out)
{
	const auto& renderer = Locator::rendererInterface::value();
	const auto resolution = renderer.GetResolution();
	out.width = resolution.x;
	out.height = resolution.y;
	// drawn at the end of the frame: the script fade's colour and the wide screen bars
	out.fade.colour = director.GetFadeColour();
	out.fade.barPixels = gui::CinemaBars::BarHeight(out.width, out.height, director.GetWideScreenFraction());

	// the help text's frame: not drawn while a full screen film is open, from its start to the end of its fade (the
	// input prompts below still are)
	auto& helpText = out.helpText;
	auto& helpTexts = TheHelpTextSystem();
	helpText.active = helpTexts.IsStarted() && out.width != 0 && out.height != 0 && !video::IsPlaying();
	helpText.text = {};
	helpText.songBox.reset();
	if (helpText.active)
	{
		const help::WidthFn widthOf = [&renderer](help::TextFont font, std::u16string_view text, float size) {
			return renderer.MeasureText(font, text, size);
		};
		// the song's karaoke: the language follows it, one of its verses is the script's music, and the verse's beat
		{
			const auto lock = audio::game_music::Lock();
			const auto* music = audio::game_music::Get();
			helpTexts.SetSong({.bouncingBall = help::karaoke::UsesBouncingBall(helptext::Language()),
			                   .versePlaying = music != nullptr && help::karaoke::IsVerseMusic(music->GetScriptType()),
			                   .beat = audio::GetScriptAudioState().musicBeat.load()});
		}
		if (const auto* frame = helpTexts.Layout(glm::ivec2(out.width, out.height), out.fade.barPixels, widthOf);
		    frame != nullptr)
		{
			helpText.text = *frame;
			// the newest text's $M codes ask for their control icons as it is drawn (help::control_icon)
			if (auto* dialogue = TheDialogue(); dialogue != nullptr && !frame->controlIcons.empty())
			{
				dialogue->ShowControlIcons(frame->controlIcons, frame->box, out.width);
			}
			// the box glides to the sung word once a frame the texts are drawn, by the frame's clamped ms
			helpText.songBox = helpTexts.FollowSong(helpText.text, game_clock::ClampedFrameMs(game_clock::IsInsideCitadel()));
		}
	}
	// the game speed's words (paused, slow or fast) near the top of the screen: the help text draws them right after the
	// dialogue texts, so not while a film is open either
	out.speedIndicator = {};
	if (helpText.active)
	{
		const auto state = help::StateOf(game_clock::IsPaused(), game_clock::Speed());
		const help::SpeedIndicatorGates gates {
		    .gameRunning = true, // a land is up whenever the overlays are filled
		    .sequenceMode = game_clock::SequenceMode() != game_clock::k_SequenceModeNone,
		    // task 0 is no task: it may set the speed only while no script has it
		    .scriptOwnsSpeed = !TheScriptControlSystem().MaySetGameSpeed(0),
		    .editorMode = false,                // openblack has no such mode
		    .videoPlaying = video::IsPlaying(), // false here: the help text draws nothing while a film is open
		    .setupBoxFlag = boxActive,          // the menu open or the SkipBox shown
		};
		const auto frame = help::Get()->GetSpeedIndicator().Update(state, game_clock::FrameRealMs(), gates);
		if (frame.has_value() && Locator::camera::has_value())
		{
			const auto& camera = Locator::camera::value();
			const auto& text = helptext::Get(help::SpeedText(frame->shown));
			// the text up to its first null, its spaces counted
			const std::u16string_view letters(text.c_str());
			const auto frames =
			    help::PlaceLetters(graphics::billboard::CameraFrame::From(camera),
			                       screen_point::LensOf(out.width, out.height, camera.GetHorizontalFieldOfView()), out.width,
			                       out.height, letters.size(), frame->depth, frame->phase);
			out.speedIndicator.alpha = static_cast<uint8_t>(static_cast<int32_t>(frame->t * 255.0f));
			for (size_t i = 0; i < letters.size(); ++i)
			{
				const auto letter = letters.substr(i, 1);
				const float width = renderer.MeasureText(help::TextFont::J0, letter, help::k_SpeedLetterSize);
				out.speedIndicator.letters.push_back({letter.front(), frames[i], width * -0.5f});
			}
		}
	}
	// the key / mouse icons' draw half (Renderer::DrawInputPrompts): the input prompt icons as the two updates left them,
	// newest first; the hand's point on the screen for DrawKeyOrMouse (the hand's position projected, unclipped; (inferred)
	// its ints are truncated); the tick count of the icons' update. Under an active box no icon is drawn, though their fades go
	// on
	help::input_prompt::Snapshot(out.inputPrompts.icons, boxActive);
	out.inputPrompts.tickCount = game_clock::TickCount();
	out.inputPrompts.hand.reset();
	if (Locator::handSystem::has_value() && Locator::camera::has_value() && out.width != 0 && out.height != 0)
	{
		const auto hands = Locator::handSystem::value().GetPlayerHands();
		const auto& registry = Locator::entitiesRegistry::value();
		const auto position = !hands.empty() && registry.Valid(hands[0])
		                          ? std::optional<glm::vec3>(registry.Get<ecs::components::Transform>(hands[0]).position)
		                          : std::nullopt;
		// the sprites' projection, without clipping: a hand off the screen still has its point, which the box is then
		// kept on the screen from (none only at or behind the eye)
		const auto& camera = Locator::camera::value();
		const auto frame = graphics::billboard::CameraFrame::From(camera);
		const auto lens = screen_point::LensOf(out.width, out.height, camera.GetHorizontalFieldOfView());
		out.inputPrompts.hand = help::input_prompt::HandAnchor(position, [&frame, &lens](const glm::vec3& point) {
			const auto projected = screen_point::Project(frame, lens, point);
			return projected.has_value() ? std::optional<glm::vec3>(glm::vec3(projected->pixel, projected->depth))
			                             : std::nullopt;
		});
	}

	// the control pictures over the temple's scroll seen close up: each line's points projected by the frame's camera as
	// the game projects them (none nearer than the near clip; on the screen within its pixels), in the temple's place
	out.templeScrollIcons.clear();
	if (Locator::temple::has_value() && Locator::temple::value().Active() && Locator::camera::has_value() && out.width != 0 &&
	    out.height != 0)
	{
		const auto& temple = Locator::temple::value();
		const auto& camera = Locator::camera::value();
		const auto viewProjection = camera.GetViewProjectionMatrix(Camera::Projection::Normal);
		const float nearClip = camera.GetPixelNearClip();
		const auto width = static_cast<float>(out.width);
		const auto height = static_cast<float>(out.height);
		const glm::vec3 templePlace = temple.GetPosition();
		const temple_scroll_pictures::Project project =
		    [&](const glm::vec3& point) -> std::optional<temple_scroll_pictures::ScreenPoint> {
			const auto clip = viewProjection * glm::vec4(point + templePlace, 1.0f);
			if (clip.w < nearClip)
			{
				return std::nullopt;
			}
			const float inverse = 1.0f / clip.w;
			const float x = ((clip.x * inverse) + 1.0f) * (width * 0.5f);
			const float y = (1.0f - (clip.y * inverse)) * (height * 0.5f);
			return temple_scroll_pictures::ScreenPoint {
			    .x = x, .y = y, .onScreen = x >= 0.0f && y >= 0.0f && x < width && y < height};
		};
		for (const auto& line : temple.GetScrollPictures())
		{
			const auto place =
			    temple_scroll_pictures::ControlPicture(line.frame, line.across, line.down, line.lineX, line.lineY, project);
			int32_t animType = 0;
			int32_t clickType = 0;
			int32_t row = 0;
			std::u16string keyName;
			if (place.has_value() && help::input_prompt::ResolveAction(line.control, animType, clickType, row, keyName))
			{
				out.templeScrollIcons.push_back(
				    {animType, clickType, row, std::move(keyName), place->x, place->y, place->size, 0xFF});
			}
		}
	}

	// the did-you-know bubble, which the help system's 3D draw puts among the world's blended objects, sorted by the sign's
	// point, so that it is drawn before (under) the dialogue texts
	// (note) the bubble's life (Age) and scroll (ClampScroll) change here, in the fill, as the original changes them in
	// the draw; FillOverlayFrame runs once a frame on the logic side, so the Renderer still only reads the copy
	out.didYouKnow = {};
	auto& tipBubble = TheTipBubbleSystem();
	if (tipBubble.IsUp() && Locator::camera::has_value() && out.width != 0 && out.height != 0)
	{
		auto& bubble = tipBubble.GetBubble();
		const auto& camera = Locator::camera::value();
		if (const auto anchor = tipBubble.GetAnchor(); anchor.has_value() && tipBubble.GetDisplayTime() > 0.0f)
		{
			const glm::vec3 point = *anchor;
			const glm::vec3 origin = camera.GetOrigin();
			const glm::vec3 forward = glm::normalize(camera.GetFocus() - origin);
			const float depth = glm::dot(point - origin, forward);
			// the sign's point on the screen wherever it falls, past the screen's edges too; given up only nearer than
			// the near clip
			const glm::vec3 projected = glm::project(point, camera.GetViewMatrix(Camera::Interpolation::Current),
			                                         camera.GetProjectionMatrix(Camera::Projection::Normal),
			                                         glm::vec4(0.0f, 0.0f, out.width, out.height));
			if (const auto screen =
			        help::AnchorOnScreen(projected, depth, camera.GetPixelNearClip(), static_cast<float>(out.height)))
			{
				const auto tail = help::TailFor(screen->x, out.width);
				if (const auto layout = help::LayoutBubble(bubble.Sizes(), depth, *screen, tail.offset, tail.place))
				{
					// in the world at its draw depth, in front of the sign and pulled toward the camera as it looks down
					float heading = 0.0f;
					float pitch = 0.0f;
					script_camera::HeadingAndPitchFromPoints(origin, camera.GetFocus(), heading, pitch);
					const float drawDepth = help::DrawDepth(layout->depth, pitch, camera.GetPixelNearClip());
					const auto depths = help::DepthsOf(drawDepth);
					out.didYouKnow.keyPoint = point;
					out.didYouKnow.depth = drawDepth;
					// the dark colour at 255 x alpha
					constexpr uint32_t k_Colour = 0xFF3BBDED;
					const uint32_t dark = ((k_Colour >> 1) & 0x7F7F7Fu) | 0xFF000000u;
					const float alpha = std::min(bubble.Life(), 1.0f) * layout->fade;
					const auto a = static_cast<uint32_t>(std::clamp(static_cast<int>(alpha * 255.0f), 0, 255));
					// the scroll clamped with the last draw's text height, before it is taken in pixels for this
					// frame's draw (scroll x text size); the drag changes the scroll only after that
					const float size = layout->textSize;
					const auto marks = bubble.ClampScroll(layout->height, layout->margin, size);
					const float scrollPixels = bubble.Scroll() * size;
					// the words (help::LayOutBubbleWords): the tip, a blank line and the title, each part wrapped
					// again only when the part, the width or the size changed
					const auto parts = help::BubbleParts(helptext::Get(tipBubble.GetText()), helptext::Get(0x1016));
					const auto words = help::LayOutBubbleWords(
					    *layout, scrollPixels, alpha, dark, parts,
					    [&bubble, &renderer](size_t part, std::u16string_view text, float maxWidth,
					                         float lineSize) -> const std::vector<std::u16string>& {
						    return bubble.PartWrap(part).Wrap(text, maxWidth, lineSize,
						                                      [&renderer, lineSize](std::u16string_view t) {
							                                      return renderer.MeasureText(help::TextFont::J0, t, lineSize);
						                                      });
					    },
					    [&renderer](std::u16string_view text, float lineSize) {
						    return renderer.MeasureText(help::TextFont::J0, text, lineSize);
					    },
					    [](int action) -> std::optional<help::BubbleKeyBinding> {
						    help::BubbleKeyBinding binding;
						    if (!help::input_prompt::ResolveAction(action, binding.animType, binding.clickType, binding.row,
						                                           binding.keyName))
						    {
							    return std::nullopt;
						    }
						    return binding;
					    });
					for (const auto& line : words.lines)
					{
						out.didYouKnow.lines.push_back({line.run, line.alphaBottom, line.shadow ? depths.shadow : depths.text});
					}
					for (const auto& icon : words.keyIcons)
					{
						out.didYouKnow.keyIcons.push_back({icon.binding.animType, icon.binding.clickType, icon.binding.row,
						                                   icon.binding.keyName, icon.x, icon.y, icon.size, icon.alpha});
					}
					for (const auto& gesture : words.gestures)
					{
						auto& quads = out.didYouKnow.gestures.at(gesture.page);
						const float x = gesture.x;
						const float qy = gesture.y;
						const float w = gesture.width;
						const float h = gesture.height;
						const float uu = gesture.u;
						const float z = gesture.shadow ? depths.shadow : depths.text;
						const graphics::SpiritQuadVertex p0 {x, qy, z, uu, gesture.vTop, gesture.abgr};
						const graphics::SpiritQuadVertex p1 {x + w, qy, z, uu + 0.25f, gesture.vTop, gesture.abgr};
						const graphics::SpiritQuadVertex p2 {x + w, qy + h, z, uu + 0.25f, gesture.vBottom, gesture.abgr};
						const graphics::SpiritQuadVertex p3 {x, qy + h, z, uu, gesture.vBottom, gesture.abgr};
						quads.insert(quads.end(), {p0, p1, p2, p0, p2, p3});
					}
					// the text height (0, then each part's and icon's height added) is written by the z object's draw
					// only: none is queued while alpha x the fade is 0 or less, and the height keeps the last value
					if (alpha > 0.0f)
					{
						bubble.SetTextHeight(words.textHeight);
					}
					// the scroll arrows, while the tick count % 750 < 400, with h the box's height and s = 0.2 h, k = s
					// / 9: the below mark's arrow at the top right, the above mark's one at the bottom right, squares
					// of 0.5 s + 2 k in atmos.raw; colour {0, 0xFF, 0xFF, 0xFF}, alpha = the dark colour's alpha / 2
					if (game_clock::TickCount() % 750u < 400u)
					{
						const float boxTop = layout->topLeft.y;
						const float boxBottom = layout->topLeft.y + layout->height;
						const float boxRight = layout->topLeft.x + layout->width;
						const float s5 = 0.2f * layout->height;
						const float k = s5 / 9.0f;
						const float xr = boxRight - 0.8f * s5;
						// the alpha of the z object's dark colour, stored at 0xFF and never faded: 0xFF >> 1
						out.didYouKnow.arrowAlpha = 0x7F;
						const auto ftolf = [](float v) { return static_cast<float>(static_cast<int>(v)); };
						if (marks.below)
						{
							out.didYouKnow.arrows.push_back({ftolf(xr + 0.25f * s5 - k), ftolf(boxTop + 0.5f * s5 - k),
							                                 ftolf(xr + k + 0.75f * s5), ftolf(boxTop + s5 + k), 0.87890625f,
							                                 0.12890625f, 0.99609375f, 0.24609375f});
						}
						if (marks.above)
						{
							out.didYouKnow.arrows.push_back({ftolf(xr + 0.25f * s5 - k), ftolf(boxBottom - s5 - k),
							                                 ftolf(xr + k + 0.75f * s5), ftolf(boxBottom - 0.5f * s5 + k),
							                                 0.75390625f, 0.00390625f, 0.87109375f, 0.12109375f});
						}
					}
					const auto shape = help::BubbleShape(layout->topLeft.x, layout->topLeft.y,
					                                     layout->topLeft.x + layout->width, layout->topLeft.y + layout->height,
					                                     layout->margin, layout->topLeft.x + layout->width * layout->tailPlace,
					                                     layout->tailRight, (dark & 0x00FFFFFFu) | (a << 24), true, 0, 0xFF);
					// queued only while its alpha times the distance fade is above 0
					out.didYouKnow.active = alpha > 0.0f && !shape.empty();
					for (const auto& v : shape)
					{
						const uint32_t abgr = (v.argb & 0xFF00FF00u) | ((v.argb >> 16) & 0xFFu) | ((v.argb & 0xFFu) << 16);
						out.didYouKnow.shape.push_back({v.x, v.y, depths.box, v.u, v.v, abgr});
					}
					// the hand's drag of the bubble and its hover
					{
						const auto mouse = input::GameCursor();
						const auto id = entt::to_integral(tipBubble.GetSign());
						auto* hand = Locator::handSystem::has_value() ? &Locator::handSystem::value() : nullptr;
						bubble.Drag(hand != nullptr && hand->DraggedScreenObject() == id, mouse.y, size);
						const float x0 = layout->topLeft.x;
						const float y0 = layout->topLeft.y;
						const auto mx = static_cast<float>(mouse.x);
						const auto my = static_cast<float>(mouse.y);
						if (mx > x0 && mx < x0 + layout->width && my > y0 && my < y0 + layout->height)
						{
							if (hand != nullptr) // (openblack guard) the original always has its interface
							{
								static_cast<void>(hand->OfferScreenObject(id, layout->depth));
							}
							tipBubble.Hover(); // life back to 3.0
						}
					}
				}
			}
		}
		tipBubble.UpdateFrame(static_cast<float>(game_clock::FrameGameMs())); // the frame's game ms x 0.0002
	}

	// the advisor spirits (Locator::advisorSystem): the spirits and their trails. Not while the film covers the screen
	// or the falling spell hides the world: DrawScene draws no spirit then, and the halo clock stays (as when the
	// Renderer asked the runtime itself)
	out.spirits.clear();
	out.spiritTrails.clear();
	if (auto& advisors = TheAdvisorSystem();
	    advisors.IsLoaded() && !video::Get().CoversScreen() && !video::GetFallingSpell().HidesWorld())
	{
		advisors.FillOverlay(out);
	}
}

} // namespace openblack::game_overlay
