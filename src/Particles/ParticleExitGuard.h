/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

/// Whether the program is exiting, for the particle data that would otherwise reach other services as it goes (the
/// tornado's and the vortex's carriers, the storm's clouds; port bookkeeping, not in the original). The effects
/// (Locator::particleSystem) may outlive the registry and the storms' list: a normal exit clears them first
/// (ShutDownServices), but an exit that skips that, or a test that resets its services without clearing them, destroys
/// the effects with the locator's own statics, and while a locator slot is being reset it is already empty, so those
/// destructors could not reach the services they use.
///
/// It stays a static on purpose (docs/wiki/project/refactor-conventions.md#what-stays-on-purpose): it must outlive
/// every service, and it is set from the destructor of a function-scope static made on first use, after every
/// namespace-scope static, so it is destroyed before them. No service can hold it, as it is read after they go
namespace openblack::particles::exit_guard
{
/// Called as such data is made: the first call makes the guard
void Watch();
/// True once the program's statics are being destroyed
[[nodiscard]] bool Exiting();
} // namespace openblack::particles::exit_guard
