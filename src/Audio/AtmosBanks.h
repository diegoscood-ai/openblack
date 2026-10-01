/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::audio::atmos_banks
{

/// The ambient banks: GAudio's atmos part (InitAtmos 0x428EF0, fn_00429100, ProcessAtmosBanks 0x428FE0) and the
/// LHaudiodllR mixer of the banks (bank registration fn_10001610, LHAtmosProcess 0x100018B0). Each of the 14 ATMOS_TYPE
/// banks of Audio\SFX\Atmos (the sound groups "ocean.sad"...) has a volume 0..127 that follows GSoundMap's volume of its
/// type; its loops (.sad +0x27C = 0) play 2D while that is not 0, fading in by 5 a turn, and one loose sample (+0x27C =
/// f > 0, next time = counter + 4f + U[0, 12f] turns) at most starts a turn, from the head of one queue for all banks,
/// at a random point beside the listener. Research: dev\tmp_dis\agua\audio.md §3.

/// The atmos of GAudio::ProcessAudioGameTurn 0x427080 (audio::ProcessTurn calls them in its order, once per game turn
/// after GSoundMap::Update, when the game is not paused, past turn 5 and with the audio active): UpdateBanks is
/// fn_00429100 (the targets = GSoundMap's volumes, all 0 inside the citadel: the test 0x4282F0, misnamed
/// HelpSystem::GetWideScreenControl, is g_game+0x205A28 == 1) and ProcessAtmosBanks (step 0.02 / 0.04 towards them, group
/// by the alignment, LHAtmosSetBankVolume(current * 127)); then fn_004270D0 (the channels and the listener); then Mix,
/// LHAtmosProcess(1) unless a video plays. The first UpdateBanks registers the banks (InitAtmos).
void UpdateBanks();
void Mix();

/// GAudio+0x190 (-1 evil .. 1 good), which ProcessAtmosBanks compares with -0.6 to put every bank in group 1 or 2
[[nodiscard]] float Alignment();

/// GAudio::AtmosProcess(0) 0x4286C0 -> LHAtmosProcess(0) (0x10001EBF): the loops stop (their AtmosInfo cleared) and
/// every channel with an AtmosInfo (the loops' 1, the loose samples' entries) is stopped; the other samples play on.
/// EndTurn calls it instead of ProcessAudioGameTurn while the game is paused (g_game+0x14 & 4) or in the first 5 turns.
void Silence();

/// A new map (GGame::ClearMap -> GAudio::Reset 0x426CA0): LHAtmosProcess(0), the atmos channels stopped; the banks keep
/// their volumes, as in the original
void Clear();

} // namespace openblack::audio::atmos_banks
