/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// The CHL fire natives, called from CHLApi.cpp. They pop and push the VM stack themselves (the handlers' order).

namespace openblack::magic::script
{
/// 170 IS_ON_FIRE, GScript::IsOnFire 0x6FB4C0: the thing's IsOnFire (vt 0x298)
void IsOnFire();
/// 171 IS_FIRE_NEAR, GScript::IsFireNear 0x6F7910: FindNearForScript with the predicate 0x6F7100 (on fire and within the
/// radius of its position, a worship site's totem)
void IsFireNear();
/// 174 SET_TEMPERATURE, GScript::SetTemperature 0x6FB840 (Object::SetTemperature 0x639A60, no source)
void SetTemperature();
/// 175 SET_ON_FIRE, GScript::SetOnFire 0x6FB780: SetOnFire(speed), or back to the ambient temperature
void SetOnFire();
/// 321 SET_HURT_BY_FIRE, GScript::SetHurtByFire 0x6FDF40: Object +0x0A bit 2 = !enable
void SetHurtByFire();
/// 426 SET_SET_ON_FIRE, GScript::SetSetOnFire 0x6FDEE0: Object +0x0A bit 3 = !enable
void SetSetOnFire();
} // namespace openblack::magic::script
