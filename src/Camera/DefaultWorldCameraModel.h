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

#include <functional>

#include <glm/vec2.hpp>

#include "CameraModel.h"
#include "CameraPan.h"
#include "Common/Zoomer.h"
#include "FightWatch.h"

class TestDefaultCameraModel;
class TestDefaultCameraModel_single_line_Test;

namespace openblack
{
class LandIslandInterface;

class DefaultWorldCameraModel final: public CameraModel, public camera::FightWatch
{
	enum class Mode : std::uint8_t
	{
		Cartesian,
		Polar,
		ArcBall,
		DraggingLandscape,
		FlyingToPoint,
	};

public:
	/// How big a fighter is across, for watching a fight: its radius
	using FighterRadius = std::function<float(entt::entity)>;

	/// The fighters' radii are the objects' own
	DefaultWorldCameraModel();
	explicit DefaultWorldCameraModel(FighterRadius fighterRadius);
	~DefaultWorldCameraModel() final;

	std::optional<CameraInterpolationUpdateInfo> Update(std::chrono::microseconds dt, const Camera& camera) final;
	void HandleActions(std::chrono::microseconds dt) final;
	void SetFlight(glm::vec3 origin, glm::vec3 focus) final;
	/// The camera's woosh G_Woosh_01..04 (the flights to a place, the double click's and the watched fight's)
	static void PlayWoosh();
	[[nodiscard]] glm::vec3 GetTargetOrigin() const final;
	[[nodiscard]] glm::vec3 GetTargetFocus() const final;
	[[nodiscard]] std::chrono::seconds GetIdleTime() const final;
	[[nodiscard]] HandCues GetHandCues() const final;
	[[nodiscard]] camera::FightWatch* GetFightWatch() final { return this; }
	[[nodiscard]] const camera::FightWatch* GetFightWatch() const final { return this; }

	// Watching a creature fight
	void StartFight(entt::entity fighterA, entt::entity fighterB, glm::vec3 arenaCentre, float arenaRadius) final;
	void EndFightNow() final;
	void EndFight() final;
	[[nodiscard]] bool IsWatchingFight() const final { return _fight.watching; }
	[[nodiscard]] bool WantToQuitFight(glm::vec3 arenaCentre, float arenaRadius) const final;
	/// Where a drag of the land takes the camera, stopped 3 short of the land in its way from the camera at the grip,
	/// or of the sea where that line meets it within 7500 across from the drawn camera
	[[nodiscard]] static camera_pan::CameraPlace StopPanShortOfLand(const LandIslandInterface& island,
	                                                                const camera_pan::CameraPlace& place,
	                                                                glm::vec3 originAtGrip, glm::vec3 drawnCamera);

private:
	void UpdateCameraInterpolationValues(const Camera& camera);
	void UpdateRaycastHitPoints(const Camera& camera);
	void UpdateFocusDistance();
	/// Follows a drag of the land while it is held: what it turns into, from where it was pressed and how the mouse
	/// moves
	void HandleDrag(bool held);
	/// What the drag of the land does once it is decided: pans, turns the camera round the edge with the cursor held on
	/// a ring, or tilts it. Gives the mode the camera takes for it; until it is decided, the camera stays put
	[[nodiscard]] Mode FollowDrag();
	/// The height the mouse controls measure by, the cinema bars' picture's while they are in
	[[nodiscard]] static int ViewHeight(glm::ivec2 screenSize);
	/// The game runs in a window, which brings the hints of the very bottom and the top nearer the middle
	[[nodiscard]] static bool Windowed();

	void UpdateMode(const Camera& camera, glm::vec3 eulerAngles, float zoomDelta, glm::uvec2 mouseCurrent);
	void UpdateModeCartesian();
	void UpdateModePolar(glm::vec3 eulerAngles, bool recalculatePoint);
	void UpdateModeArcBall(glm::vec3 eulerAngles, glm::u16vec2 mouseCurrent, float xFov);
	/// Drags the land gripped, or gives the drag up when the land was gripped too far ahead
	/// @return The drag was given up this frame
	bool UpdateModeDragging(const Camera& camera, glm::u16vec2 mouseCurrent);
	/// The double click's flight to the hand's point. After a drag given up it goes to 1000 from the point, keeping the
	/// camera's heading, with the pitch held between pi / 8 and 10 pi / 21.
	void UpdateModeFlying(glm::vec3 eulerAngles, bool afterGivenUpDrag = false);
	/// The camera help's report of the double click's flight, also made by the flight after a drag given up
	static void ReportFlightToHand();
	/// Where the clear view goes, from the view the camera has as Ctrl and Shift are pressed
	void PlanClearView();

	/// A flight from the camera's current origin to a new place, through the middle point CharterFlight makes with the
	/// rise, and the woosh when the new place is farther than 100 x 1.5
	void FlyTo(const LandIslandInterface& land, glm::vec3 origin, glm::vec3 focus, float rise);

	/// The features the scripts allow, less the double click's flight while a fight is watched
	[[nodiscard]] uint32_t CameraFeatures() const;
	/// A watched fight's time running out after it is over, and its arena going
	void UpdateFightWatch(std::chrono::microseconds dt);
	/// Whether the land is dragged away from the arena long enough after the last flight to stop watching
	[[nodiscard]] bool DraggedAwayFromFight() const;
	/// Turns the camera's watch round the fighters this frame, after the player's turn and tilt; false, and nothing
	/// moved, without both fighters
	bool FollowFight(float zoomDelta, float seconds);

	/// Updates the model's focus point parameters after a change in position or focus point of view
	void UpdateFocusPointInteractionParameters(glm::vec3 origin, glm::vec3 focus, glm::vec3 eulerAngles, const Camera& camera);
	/// Modifies the given Euler angles based on the rotate Around and keyboard Move Deltas for rotation and zoom.
	/// @param eulerAngles A reference representing Euler angles (yaw, pitch, roll) to be adjusted. Roll is always 0.
	void TiltZoom(glm::vec3& eulerAngles, float scalingFactor, float zoomDelta);
	/// How far a unit of zoom input moves the camera, growing with the camera's height above its focus
	[[nodiscard]] float GetZoomScale() const;
	/// Computes the harmonic mean of the distances from a point of origin to a set of points determined by raycasting in screen
	/// space.
	///
	/// The function casts 16 rays from the center of the screen to vertically distributed points on the screen.
	/// The harmonic mean of these distances is then calculated by averaging their reciprocals and taking the reciprocal of that
	/// average.
	///
	/// @return The harmonic mean of the distances from the origin to each hit point.
	[[nodiscard]] float GetVerticalLineInverseDistanceWeighingRayCast(const Camera& camera) const;

	void ComputeDistanceFromBoundY();
	bool ConstrainCamera(std::chrono::microseconds dt, float mouseMovementDistance, glm::vec3 eulerAngles,
	                     const Camera& camera);
	/// Corrects altitude of the camera
	/// @return If a modification to the camera position was applied.
	bool ConstrainAltitude();
	/// Corrects distance of the camera from the island
	/// @return If a modification to the camera position was applied.
	bool ConstrainDisc();

	[[nodiscard]] glm::vec3 GetTargetForwardVector() const;
	[[nodiscard]] glm::vec3 GetTargetForwardUnitVector() const;
	[[nodiscard]] glm::vec3 ProjectPointOnForwardVector(float distanceFromOrigin) const;

	[[nodiscard]] std::optional<CameraInterpolationUpdateInfo> ComputeUpdateReturnInfo(bool originHasBeenAdjusted,
	                                                                                   std::chrono::microseconds t);

	Mode _mode = Mode::Cartesian;
	Mode _modePrev = _mode;

	// Values from camera state where the camera has interpolated to.
	glm::vec3 _currentOrigin = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::vec3 _currentFocus = glm::vec3(0.0f, 0.0f, 0.0f);

	// Values from target camera state which the camera may interpolate to. Not the current camera state.
	glm::vec3 _targetOrigin = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::vec3 _targetFocus = glm::vec3(0.0f, 0.0f, 0.0f);
	float _arcBallRadius = 0.0f;

	std::optional<glm::vec3> _screenSpaceMouseRaycastHit;
	std::optional<glm::vec3> _screenSpaceMouseRaycastHitAtClick;
	std::optional<glm::vec3> _screenSpaceCenterRaycastHit;

	// State of input Action
	glm::vec3 _rotateAroundDelta = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::vec2 _keyBoardMoveDelta = glm::vec2(0.0f, 0.0f);
	std::optional<glm::vec3> _handPosition;

	float _focusDistance = 0.0f;
	float _distanceFromBoundY = 0.0f;

	// Estimate of camera to island geometry
	float _averageIslandDistance = 0.0f;

	// Updated at the start of a click+drag or keyboard input
	// Only useful for interaction.
	float _originFocusDistanceAtInteractionStart = 0.0f;
	glm::vec3 _originToHandPlaneNormal = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::vec3 _originAtClick = glm::vec3(0.0f, 0.0f, 0.0f);
	float _alignmentAtInteractionStart = 0.0f;
	glm::vec3 _focusAtClick = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::u16vec2 _mouseAtClick = glm::u16vec2(0.0f, 0.0f);
	std::chrono::microseconds _elapsedTime = std::chrono::microseconds::zero();
	std::optional<FlightPath> _flightPath;

	/// What the camera lets the player do, as the scripts allow it, read where the controls and the update use it
	uint32_t _features {camera_drag::k_DefaultFeatures};
	/// The camera hints where the cursor is, with nothing dragged
	uint32_t _tricons {camera_drag::tricon::k_Idle};
	/// A drag of the land, what it turns into, and where the cursor is held dragging round the edge
	bool _dragging {false};
	camera_drag::DragClassifier _drag;
	glm::ivec2 _ringCursor {0, 0};
	/// The turn (x) and the tilt (y) inputs a drag round the edge or up and down gave this frame, which the camera help
	/// does not count as the player's turning and tilting
	glm::vec2 _dragTurnAndTilt {0.0f, 0.0f};
	/// Where the land was gripped as the drag was pressed: the camera then, the cursor, the plane the land is dragged
	/// across, whether there was land under the cursor and how far ahead it was
	struct LandGrip
	{
		glm::vec3 origin;
		glm::vec3 focus;
		glm::u16vec2 cursor;
		camera_pan::GripPlane plane;
		bool land;
		float depth;
	};
	std::optional<LandGrip> _landGrip;
	/// A drag gripping land too far ahead was given up: every control is dropped until all are let go
	bool _dragGivenUp {false};
	/// This frame moves the camera, or the grip, the middle button or both buttons are held with the mouse moved more
	/// than 2 pixels: only then does a pan count as a drag for the camera help
	bool _dragCountsForHelp {false};
	/// Both buttons turn the camera once the mouse has moved far enough across
	camera_drag::TwoButtonTurn _twoButtonTurn;
	/// How far the clear view of Ctrl and Shift held together has come, easing in and out over half a second
	Zoomer _clearView;
	/// The clear view's two views: the camera's as it began, and the close view of the hand's point, which needs the
	/// hand over the land; the distance, heading and pitch it is seen from
	struct ClearViewPlan
	{
		glm::vec3 fromOrigin {0.0f};
		glm::vec3 fromFocus {0.0f};
		std::optional<glm::vec3> point;
		float distance {0.0f};
		float heading {0.0f};
		float pitch {0.0f};
		std::optional<glm::vec3> toOrigin;
		glm::vec3 toFocus {0.0f};
	};
	ClearViewPlan _clearViewPlan;
	/// The camera tilts itself towards the self-tilting pitch this frame; the player's own tilt was dropped for it
	bool _autoTilting {false};
	/// Nothing is gripped and nothing is done with the camera this frame
	bool _idleMouse {true};
	/// Time spent handling the controls, for timing the start of a drag
	std::chrono::microseconds _controlsTime {std::chrono::microseconds::zero()};
	/// The land is gripped, this frame and the one before, and gripped with no other camera control this frame
	bool _gripping {false};
	bool _grippedBefore {false};
	bool _gripOnly {false};
	/// Seconds since the camera's last flight began, which a drag away from a watched fight waits for. A short double
	/// click flight starts it at 0.75 and the flight after a drag given up at 0.01
	float _flightSeconds {0.0f};

	/// A fight the camera watches: the fighters and their arena; how far out (in the second fighter's radii), turned and
	/// tilted the camera is; how long it lingers once the fight is over; how long it has been over an arena; and
	/// whether the fight is on, over and lingering, or ended. The focus and the turn follow the fighters, set at once on
	/// the first frame
	enum class FightStatus : uint8_t
	{
		On,
		Lingering,
		Ended,
	};
	struct FightArena
	{
		entt::entity fighterA;
		entt::entity fighterB;
		glm::vec3 centre;
		float radius;
	};
	struct FightWatchState
	{
		bool watching {false};
		std::optional<FightArena> arena;
		float yaw {0.0f};
		float pitch {0.0f};
		float distance {0.0f};
		int32_t lingerLeftMs {0};
		int32_t overArenaMs {0};
		FightStatus status {FightStatus::On};
		bool firstFrame {false};
		/// The seconds the camera takes to the orbit's place this frame; 0 when the orbit did not run
		float easeSeconds {0.0f};
		Zoomer turn;
		Zoomer3 focus;
	};
	FightWatchState _fight;
	FighterRadius _fighterRadius;

	// For unit testing
	friend TestDefaultCameraModel;
	friend TestDefaultCameraModel_single_line_Test;
};
} // namespace openblack
