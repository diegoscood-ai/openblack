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

#include <span>
#include <string>
#include <string_view>
#include <vector>

/// The game's VERSION launch switch: as the game starts, it writes mpver.txt in the working folder, holding the
/// program's checksum, the number online games compare to tell their versions apart, and carries on. Pure: the
/// switch's reading of the command line, the checksum and the file's text.
namespace openblack::version_file
{

/// The switch's name, as the command line gives it after a "-" or a "/"
inline constexpr std::string_view k_Switch = "VERSION";
/// The file written, in the working folder
inline constexpr std::string_view k_FileName = "mpver.txt";
/// The checksum when the program's own file could not be read whole
inline constexpr uint32_t k_ChecksumError = 0xFFFFFFFF;

/// Whether the command line, from where a switch's name would start, names the switch: its letters in any case, and
/// anything may follow them
[[nodiscard]] bool NamesSwitch(std::string_view from, std::string_view name);

/// Whether the command line gives the switch, as the game reads its switches: each "-" or "/" anywhere on it, a path
/// included, starts a switch, and the switch is named right after it
[[nodiscard]] bool HasSwitch(std::string_view commandLine, std::string_view name);

/// The command line as the game reads it: the program and its arguments one after the other, a space between each
[[nodiscard]] std::string CommandLine(std::span<const std::string_view> arguments);

/// The arguments left for openblack's own options: those that are a "-" or a "/" and the switch's name are taken out
[[nodiscard]] std::vector<std::string_view> WithoutSwitch(std::span<const std::string_view> arguments, std::string_view name);

/// The program's checksum: the sum of every byte of its own file, kept to 32 bits
[[nodiscard]] uint32_t Checksum(std::span<const uint8_t> program);

/// The file's text: the checksum as a whole number without a sign, and nothing after it
[[nodiscard]] std::string Text(uint32_t checksum);

} // namespace openblack::version_file
