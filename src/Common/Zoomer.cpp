/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Zoomer.h"

using namespace openblack;

void Zoomer::SetPosition(float position)
{
	value = destination = startValue = position;
	speed = destinationSpeed = startSpeed = 0.0f;
	time = duration = 0.0f;
	c2 = c3 = c4 = 0.0f;
}

void Zoomer::SetDestinationWithSpeedAndTime(float target, float targetSpeed, float seconds)
{
	if (seconds < 0.001f)
	{
		SetPosition(target);
		return;
	}
	// Start from the current value and speed; at t = T: value = target, speed = targetSpeed, acceleration = 0.
	// Solved in normalised time (the inverse of [[1/24,1/6,1/2],[1/6,1/2,1],[1/2,1,1]] scaled by T).
	startValue = value;
	startSpeed = speed;
	destination = target;
	destinationSpeed = targetSpeed;
	duration = seconds;
	time = 0.0f;
	const float r1 = target - startValue - startSpeed * seconds;
	const float r2 = (targetSpeed - startSpeed) * seconds;
	const float e = 72.0f * r1 - 48.0f * r2;
	const float d = -2.0f * r2 - 2.0f * e / 3.0f;
	const float c = 2.0f * r2 + e / 6.0f;
	c2 = c / (seconds * seconds);
	c3 = d / (seconds * seconds * seconds);
	c4 = e / (seconds * seconds * seconds * seconds);
}

void Zoomer::Update(float seconds)
{
	time += seconds;
	if (time >= duration)
	{
		value = destination;
		speed = destinationSpeed;
		time = duration;
		return;
	}
	const float t = time;
	speed = startSpeed + c2 * t + c3 * t * t / 2.0f + c4 * t * t * t / 6.0f;
	value = startValue + startSpeed * t + c2 * t * t / 2.0f + c3 * t * t * t / 6.0f + c4 * t * t * t * t / 24.0f;
}
