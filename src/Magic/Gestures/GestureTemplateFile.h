/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <GestureFile.h>

#include "Magic/Gestures/GestureTemplates.h"

// Kept apart from GestureTemplates.h: this header brings in the gestures component's namespace, which would clash with
// the files that call the recogniser's namespace "gestures"
namespace openblack::magic::gestures
{
/// The recogniser's view of a template: every one of the 80 keypoints as stored, the low byte of the three byte-sized
/// fields, and the flags as "not zero"
[[nodiscard]] GestureData TemplateFromFile(const ::openblack::gestures::GestureTemplate& entry);
} // namespace openblack::magic::gestures
