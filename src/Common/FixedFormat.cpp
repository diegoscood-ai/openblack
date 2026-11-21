/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FixedFormat.h"

#include <cstdint>

#include <algorithm>
#include <array>
#include <bit>
#include <optional>

namespace openblack::fixed_format
{

namespace
{

// The original's C library writes a number in fixed point in two steps. It first finds the number's 17 significant
// digits with its own 80-bit arithmetic, rounding on the 18th digit half up, and then rounds those digits, with zeros
// after them, half up again at the precision asked for. Its arithmetic is close to exact but not always: an exact half
// in the 18th digit can go either way. So the first step is done here as it does it, word for word.

// A number as that arithmetic holds it: six 16-bit words, the lowest first. Words 0 to 4 are an 80-bit mantissa with
// its top bit explicit, word 5 is the sign bit and a 15-bit exponent biased by 0x3FFF
using Extended = std::array<uint16_t, 6>;

// 96 bits as three 32-bit limbs, the lowest first
using Bits96 = std::array<uint32_t, 3>;

constexpr uint16_t k_Sign = 0x8000;
constexpr uint16_t k_ExponentMask = 0x7FFF;
constexpr Extended k_Infinity {0, 0, 0, 0, 0x8000, 0x7FFF};
constexpr size_t k_SignificantDigits = 17;

// The library's powers of ten: entry 7g + k - 1 is 10^(k * 8^g), for k from 1 to 7. Three groups reach every double.
constexpr std::array<Extended, 21> k_PowersOfTen {{
    {0x0000, 0x0000, 0x0000, 0x0000, 0xA000, 0x4002}, {0x0000, 0x0000, 0x0000, 0x0000, 0xC800, 0x4005},
    {0x0000, 0x0000, 0x0000, 0x0000, 0xFA00, 0x4008}, {0x0000, 0x0000, 0x0000, 0x0000, 0x9C40, 0x400C},
    {0x0000, 0x0000, 0x0000, 0x0000, 0xC350, 0x400F}, {0x0000, 0x0000, 0x0000, 0x0000, 0xF424, 0x4012},
    {0x0000, 0x0000, 0x0000, 0x8000, 0x9896, 0x4016}, {0x0000, 0x0000, 0x0000, 0x2000, 0xBEBC, 0x4019},
    {0x0000, 0x0000, 0x0400, 0xC9BF, 0x8E1B, 0x4034}, {0x0000, 0xA100, 0xCCED, 0x1BCE, 0xD3C2, 0x404E},
    {0xF020, 0xB59E, 0x2B70, 0xADA8, 0x9DC5, 0x4069}, {0x5DD0, 0x25FD, 0x1AE5, 0x4F8E, 0xEB19, 0x4083},
    {0x9671, 0x95D7, 0x0E43, 0x8D05, 0xAF29, 0x409E}, {0xBFF9, 0x44A0, 0x81ED, 0x8F12, 0x8281, 0x40B9},
    {0x3CBF, 0xA6D5, 0xFFCF, 0x1F49, 0xC278, 0x40D3}, {0xC66F, 0x8CE0, 0x80E9, 0x47C9, 0x93BA, 0x41A8},
    {0x85BC, 0x556B, 0x3927, 0xF78D, 0xE070, 0x427C}, {0xDDBC, 0xDE8E, 0x9DF9, 0xEBFB, 0xAA7E, 0x4351},
    {0xE6A1, 0xE376, 0xF2CC, 0x2F29, 0x8184, 0x4426}, {0x1028, 0xAA17, 0xAEF8, 0xE310, 0xC4C5, 0x44FA},
    {0xA7EB, 0xF3D4, 0xEBF7, 0x4AE1, 0x957A, 0x45CF},
}};

// The same for the powers of a tenth
constexpr std::array<Extended, 21> k_PowersOfATenth {{
    {0xCCCD, 0xCCCD, 0xCCCC, 0xCCCC, 0xCCCC, 0x3FFB}, {0x3D71, 0xD70A, 0x70A3, 0x0A3D, 0xA3D7, 0x3FF8},
    {0x645A, 0xDF3B, 0x8D4F, 0x6E97, 0x8312, 0x3FF5}, {0xD3C3, 0x652C, 0xE219, 0x1758, 0xD1B7, 0x3FF1},
    {0x0FD0, 0x8423, 0x1B47, 0xAC47, 0xA7C5, 0x3FEE}, {0xA640, 0x69B6, 0xAF6C, 0xBD05, 0x8637, 0x3FEB},
    {0x3D33, 0x42BC, 0xE57A, 0x94D5, 0xD6BF, 0x3FE7}, {0xFDC2, 0xCEFD, 0x8461, 0x7711, 0xABCC, 0x3FE4},
    {0x4C2F, 0xE15B, 0xC44D, 0x94BE, 0xE695, 0x3FC9}, {0xC492, 0x3B53, 0x4475, 0x14CD, 0x9ABE, 0x3FAF},
    {0x67DE, 0x94BA, 0x4539, 0x1EAD, 0xCFB1, 0x3F94}, {0x2324, 0xE2C6, 0xBABC, 0x313B, 0x8B61, 0x3F7A},
    {0x5561, 0xC159, 0xB17E, 0x7C53, 0xBB12, 0x3F5F}, {0xEED7, 0x8D2F, 0xBE06, 0x8592, 0xFB15, 0x3F44},
    {0x3F24, 0xE9A5, 0xA539, 0xEA27, 0xA87F, 0x3F2A}, {0xAC7D, 0xE4A1, 0x64BC, 0x467C, 0xDDD0, 0x3E55},
    {0x7B63, 0xCC06, 0x5423, 0x8377, 0x91FF, 0x3D81}, {0xFA91, 0x193A, 0x637A, 0x4325, 0xC031, 0x3CAC},
    {0x8921, 0x38D1, 0x4782, 0xB897, 0xFD00, 0x3BD7}, {0x88DC, 0x0858, 0xB11B, 0xE3E8, 0xA686, 0x3B03},
    {0x84C6, 0x4245, 0xB607, 0x7599, 0xDB37, 0x3A2E},
}};

// A tenth rounded down, which brings a number of 1 or more under 1
constexpr Extended k_Tenth {0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC, 0xCCCC, 0x3FFB};

[[nodiscard]] constexpr Bits96 ToBits(const std::array<uint16_t, 6>& words)
{
	return {words[0] | (static_cast<uint32_t>(words[1]) << 16), words[2] | (static_cast<uint32_t>(words[3]) << 16),
	        words[4] | (static_cast<uint32_t>(words[5]) << 16)};
}

[[nodiscard]] constexpr std::array<uint16_t, 6> ToWords(const Bits96& bits)
{
	return {static_cast<uint16_t>(bits[0]),       static_cast<uint16_t>(bits[0] >> 16), static_cast<uint16_t>(bits[1]),
	        static_cast<uint16_t>(bits[1] >> 16), static_cast<uint16_t>(bits[2]),       static_cast<uint16_t>(bits[2] >> 16)};
}

constexpr void ShiftLeft(Bits96& bits)
{
	bits[2] = (bits[2] << 1) | (bits[1] >> 31);
	bits[1] = (bits[1] << 1) | (bits[0] >> 31);
	bits[0] <<= 1;
}

constexpr void ShiftRight(Bits96& bits)
{
	bits[0] = (bits[0] >> 1) | (bits[1] << 31);
	bits[1] = (bits[1] >> 1) | (bits[2] << 31);
	bits[2] >>= 1;
}

// Times ten, keeping the low 96 bits
constexpr void TimesTen(Bits96& bits)
{
	uint64_t carry = 0;
	for (auto& limb : bits)
	{
		const uint64_t product = static_cast<uint64_t>(limb) * 10 + carry;
		limb = static_cast<uint32_t>(product);
		carry = product >> 32;
	}
}

[[nodiscard]] constexpr Extended Infinity(uint16_t sign)
{
	Extended infinity = k_Infinity;
	infinity[5] |= sign;
	return infinity;
}

[[nodiscard]] constexpr bool IsZeroMantissa(const Extended& e)
{
	return e[4] == 0 && e[3] == 0 && e[2] == 0 && e[1] == 0 && e[0] == 0;
}

// The library's product of two numbers: the partial products below the top 96 bits are left out, and the result is
// rounded to 80 bits from the 16 bits under them
[[nodiscard]] constexpr Extended Multiply(const Extended& x, const Extended& y)
{
	const auto sign = static_cast<uint16_t>((x[5] ^ y[5]) & k_Sign);
	const auto exponentX = static_cast<uint16_t>(x[5] & k_ExponentMask);
	const auto exponentY = static_cast<uint16_t>(y[5] & k_ExponentMask);
	uint32_t exponentSum = static_cast<uint32_t>(exponentX) + exponentY;
	if (exponentX >= k_ExponentMask || exponentY >= k_ExponentMask || static_cast<uint16_t>(exponentSum) > 0xBFFD)
	{
		return Infinity(sign);
	}
	if (static_cast<uint16_t>(exponentSum) <= 0x3FBF)
	{
		return {};
	}
	if (exponentX == 0)
	{
		++exponentSum;
		if (IsZeroMantissa(x))
		{
			return {};
		}
	}
	if (exponentY == 0)
	{
		++exponentSum;
		if (IsZeroMantissa(y))
		{
			return {};
		}
	}

	// A carry out of a 32-bit sum goes up by one word only, as in the library; the seventh word is never read
	std::array<uint16_t, 7> product {};
	for (size_t row = 0; row < 5; ++row)
	{
		for (size_t k = 0; k < 5 - row; ++k)
		{
			const uint64_t sum =
			    (product[row] | (static_cast<uint64_t>(product[row + 1]) << 16)) + static_cast<uint64_t>(x[row + k]) * y[4 - k];
			product[row] = static_cast<uint16_t>(sum);
			product[row + 1] = static_cast<uint16_t>(sum >> 16);
			if ((sum >> 32) != 0)
			{
				++product[row + 2];
			}
		}
	}
	Bits96 bits = ToBits({product[0], product[1], product[2], product[3], product[4], product[5]});

	auto exponent = static_cast<uint16_t>(exponentSum + 0xC002);
	if (static_cast<int16_t>(exponent) > 0)
	{
		while ((bits[2] & 0x80000000u) == 0)
		{
			ShiftLeft(bits);
			--exponent;
			if (static_cast<int16_t>(exponent) <= 0)
			{
				break;
			}
		}
	}
	if (static_cast<int16_t>(exponent) <= 0)
	{
		--exponent;
		if (static_cast<int16_t>(exponent) < 0)
		{
			bool sticky = false;
			for (int n = -static_cast<int16_t>(exponent); n > 0; --n)
			{
				sticky = sticky || (bits[0] & 1u) != 0;
				ShiftRight(bits);
			}
			exponent = 0;
			if (sticky)
			{
				bits[0] |= 1u;
			}
		}
	}

	auto words = ToWords(bits);
	if (words[0] > 0x8000 || (bits[0] & 0x1FFFF) == 0x18000)
	{
		bool carry = true;
		for (size_t i = 1; i < words.size() && carry; ++i)
		{
			++words[i];
			carry = words[i] == 0;
		}
		if (carry)
		{
			words[5] = 0x8000;
			++exponent;
		}
	}
	if (exponent >= k_ExponentMask)
	{
		return Infinity(sign);
	}
	return {words[1], words[2], words[3], words[4], words[5], static_cast<uint16_t>(exponent | sign)};
}

// Times 10^power, one table entry for each octal digit of the power
[[nodiscard]] constexpr Extended TimesPowerOfTen(Extended x, int power)
{
	const auto& table = power < 0 ? k_PowersOfATenth : k_PowersOfTen;
	auto remaining = static_cast<unsigned>(power < 0 ? -power : power);
	for (size_t group = 0; remaining != 0; ++group, remaining >>= 3)
	{
		const size_t k = remaining & 7u;
		if (k == 0)
		{
			continue;
		}
		Extended entry = table.at(7 * group + k - 1);
		// An entry that was rounded up is used one lower in its low 32 bits
		if (entry[0] >= 0x8000)
		{
			const auto low = (entry[1] | (static_cast<uint32_t>(entry[2]) << 16)) - 1;
			entry[1] = static_cast<uint16_t>(low);
			entry[2] = static_cast<uint16_t>(low >> 16);
		}
		x = Multiply(x, entry);
	}
	return x;
}

// The first step: the sign, the position of the decimal point and up to 17 significant digits with no trailing zeros,
// so that the number is 0.digits times 10^point. An infinity or a NaN gives the library's words for it, which the
// second step then treats as digits.
struct Significant
{
	bool negative;
	int point;
	std::string digits;
};

[[nodiscard]] Significant SignificantDigits(double value)
{
	const auto bits = std::bit_cast<uint64_t>(value);
	const bool negative = (bits >> 63) != 0;
	const auto biased = static_cast<uint32_t>((bits >> 52) & 0x7FF);
	uint64_t fraction = bits & ((uint64_t {1} << 52) - 1);
	// A zero, negative or not, is written with no sign
	if (biased == 0 && fraction == 0)
	{
		return {.negative = false, .point = 0, .digits = "0"};
	}
	if (biased == 0x7FF)
	{
		// The number goes through the x87 first, which makes a signalling NaN quiet
		if (fraction != 0)
		{
			fraction |= uint64_t {1} << 51;
		}
		const uint64_t mantissa = (fraction << 11) | (uint64_t {1} << 63);
		if (mantissa == uint64_t {1} << 63)
		{
			return {.negative = negative, .point = 1, .digits = "1#INF"};
		}
		if (negative && mantissa == uint64_t {3} << 62)
		{
			return {.negative = negative, .point = 1, .digits = "1#IND"};
		}
		return {.negative = negative, .point = 1, .digits = "1#QNAN"};
	}

	uint32_t exponent = biased + 0x3C00;
	uint64_t mantissa = (fraction << 11) | (uint64_t {1} << 63);
	if (biased == 0)
	{
		exponent = 0x3C01;
		mantissa = fraction << 11;
		while ((mantissa >> 63) == 0)
		{
			mantissa <<= 1;
			--exponent;
		}
	}
	const auto high = static_cast<uint32_t>(mantissa >> 32);
	const auto low = static_cast<uint32_t>(mantissa);

	// The decimal exponent, estimated from the binary one and the top byte of the mantissa
	const auto estimate = static_cast<int32_t>(((exponent >> 8) + (high >> 24) * 2) * 0x4D + exponent * 0x4D10) - 0x134312F4;
	int point = static_cast<int16_t>(estimate >> 16);

	Extended scaled {0,
	                 static_cast<uint16_t>(low),
	                 static_cast<uint16_t>(low >> 16),
	                 static_cast<uint16_t>(high),
	                 static_cast<uint16_t>(high >> 16),
	                 static_cast<uint16_t>(exponent)};
	scaled = TimesPowerOfTen(scaled, -point);
	if (scaled[5] >= 0x3FFF)
	{
		++point;
		scaled = Multiply(scaled, k_Tenth);
	}

	// The scaled number, under 1, as a fraction with its point 88 bits up
	const int shift = scaled[5] - 0x3FFE;
	scaled[5] = 0;
	Bits96 fixed = ToBits(scaled);
	for (int i = 0; i < 8; ++i)
	{
		ShiftLeft(fixed);
	}
	if (shift < 0)
	{
		for (int i = (-shift) & 0xFF; i > 0; --i)
		{
			ShiftRight(fixed);
		}
	}

	std::string digits(k_SignificantDigits + 1, '0');
	for (auto& digit : digits)
	{
		TimesTen(fixed);
		digit = static_cast<char>('0' + (fixed[2] >> 24));
		fixed[2] &= 0x00FFFFFF;
	}
	const char next = digits.back();
	digits.pop_back();
	if (next >= '5')
	{
		auto last = digits.find_last_not_of('9');
		if (last == std::string::npos)
		{
			last = 0;
			digits[0] = '0';
			++point;
		}
		++digits[last];
		digits.resize(last + 1);
	}
	else
	{
		const auto last = digits.find_last_not_of('0');
		if (last == std::string::npos)
		{
			return {.negative = false, .point = 0, .digits = "0"};
		}
		digits.resize(last + 1);
	}
	return {.negative = negative, .point = point, .digits = std::move(digits)};
}

} // namespace

std::string Fixed(double value, int width, int precision)
{
	precision = std::clamp(precision, 0, 9);
	auto [negative, point, significant] = SignificantDigits(value);

	// The second step: the digits up to the precision, zeros after the significant ones, rounded half up on the next
	const int count = point + precision;
	std::string digits = "0";
	for (int i = 0; i < count; ++i)
	{
		const auto at = static_cast<size_t>(i);
		digits.push_back(at < significant.size() ? significant[at] : '0');
	}
	if (count >= 0 && static_cast<size_t>(count) < significant.size() && significant[static_cast<size_t>(count)] >= '5')
	{
		const auto last = digits.find_last_not_of('9');
		std::fill(digits.begin() + static_cast<std::ptrdiff_t>(last) + 1, digits.end(), '0');
		++digits[last];
	}
	if (digits.front() == '1')
	{
		++point;
	}
	else
	{
		digits.erase(0, 1);
	}

	auto at = static_cast<size_t>(std::max(point, 0));
	if (point <= 0)
	{
		digits.insert(0, 1, '0');
		at = 1;
	}
	if (precision > 0)
	{
		digits.insert(at, 1, '.');
		if (point < 0)
		{
			digits.insert(at + 1, static_cast<size_t>(std::min(precision, -point)), '0');
		}
	}
	if (negative)
	{
		digits.insert(0, 1, '-');
	}
	if (static_cast<int>(digits.size()) < width)
	{
		digits.insert(0, static_cast<size_t>(width) - digits.size(), ' ');
	}
	return digits;
}

std::u16string WithNumber(std::u16string_view text, double value)
{
	std::u16string written;
	written.reserve(text.size() + 8);
	for (size_t i = 0; i < text.size(); ++i)
	{
		if (text[i] != u'%' || i + 1 >= text.size())
		{
			written.push_back(text[i]);
			continue;
		}
		if (text[i + 1] == u'%')
		{
			written.push_back(u'%');
			++i;
			continue;
		}
		// %[width][.digits]f
		size_t at = i + 1;
		int width = 0;
		while (at < text.size() && text[at] >= u'0' && text[at] <= u'9')
		{
			width = width * 10 + (text[at] - u'0');
			++at;
		}
		std::optional<int> precision;
		if (at < text.size() && text[at] == u'.')
		{
			++at;
			precision = 0;
			while (at < text.size() && text[at] >= u'0' && text[at] <= u'9')
			{
				*precision = *precision * 10 + (text[at] - u'0');
				++at;
			}
		}
		if (at >= text.size() || text[at] != u'f')
		{
			written.push_back(text[i]);
			continue;
		}
		for (const char c : Fixed(value, width, precision.value_or(6)))
		{
			written.push_back(static_cast<char16_t>(c));
		}
		i = at;
	}
	return written;
}

} // namespace openblack::fixed_format
