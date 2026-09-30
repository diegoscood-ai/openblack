/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// CHL natives of the influence (ECS/Influence), called from CHLApi.cpp. They pop and push the VM stack themselves.

namespace openblack::magic::script
{
/// 060 INFLUENCE_OBJECT, GScript::InfluenceObject 0x6F9AA0
void InfluenceObject();
/// 061 INFLUENCE_POSITION, GScript::InfluencePosition 0x6F9B60
void InfluencePosition();
/// 062 GET_INFLUENCE, GScript::GetInfluence 0x6F9C60
void GetInfluence();
} // namespace openblack::magic::script
