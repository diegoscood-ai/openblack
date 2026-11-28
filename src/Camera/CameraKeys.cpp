/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CameraKeys.h"

namespace openblack::camera_keys
{

Steps Step(const Held& held, float seconds)
{
	Steps steps;
	// Each step is the seconds times 400 or times -400, as the game multiplies them, and is added or taken away
	const float step = seconds * k_StepPerSecond;
	const float negativeStep = seconds * -k_StepPerSecond;
	// The turn and tilt keys come first
	if (held.turnRight)
	{
		steps.turn -= negativeStep;
	}
	if (held.turnLeft)
	{
		steps.turn -= step;
	}
	if (held.tiltDown)
	{
		steps.tilt -= step;
	}
	if (held.tiltUp)
	{
		steps.tilt += step;
	}
	// Then the movement keys, which the modifiers make tilt, zoom or turn
	if (held.forwards)
	{
		(held.turnModifier ? steps.tilt : held.zoom ? steps.zoom : steps.forwards) -= step;
	}
	if (held.backwards)
	{
		(held.turnModifier ? steps.tilt : held.zoom ? steps.zoom : steps.forwards) += step;
	}
	if (held.left)
	{
		if (held.turnModifier || held.zoom)
		{
			steps.turn -= negativeStep;
		}
		else
		{
			steps.strafe -= step;
		}
	}
	if (held.right)
	{
		if (held.turnModifier || held.zoom)
		{
			steps.turn -= step;
		}
		else
		{
			steps.strafe += step;
		}
	}
	return steps;
}

} // namespace openblack::camera_keys
