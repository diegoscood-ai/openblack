/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// The resource loaders (LoaderTypes.h) with every type they make, for the code that reads what they load

#include <map>
#include <optional>
#include <queue>
#include <span>
#include <string>

#include <PackFile.h>

#include "3D/CameraPath.h"
#include "3D/CameraTracks.h"
#include "3D/L3DAnim.h"
#include "3D/L3DSubMesh.h"
#include "3D/Light.h"
#include "3D/PickMask.h"
#include "Audio/Device/Sound.h"
#include "Common/HelpText.h"
#include "Creature/CreatureMind.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureSkin.h"
#include "Graphics/Texture2D.h"
#include "Level.h"
#include "LoaderTypes.h"
