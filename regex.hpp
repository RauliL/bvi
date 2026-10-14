/* Binary-safe regular expression bytecode for bvi/bmore.
 *
 * A common C-string regexp cannot be used: NUL is a regular character
 * in a binary buffer.  Compiled programs are terminated by ReOp::End;
 * literal NUL appears only inside One/Star units.
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <string_view>

enum class ReOp : std::uint8_t {
	End = 0,
	One = 1,
	Star = 2,
};

enum class ReMode : int {
	Hex = 0,
	Ascii = 1,
};

/* Legacy aliases used by matchers and compilers. */
inline constexpr auto END = static_cast<char>(ReOp::End);
inline constexpr auto ONE = static_cast<char>(ReOp::One);
inline constexpr auto STAR = static_cast<char>(ReOp::Star);

/* True if `byte` fails the character-class at `scan` (length `count`). */
[[nodiscard]] inline bool
class_rejects(int byte, const char *scan, int count) noexcept
{
	if (*scan++ == '^') {
		return std::memchr(scan, byte, --count) != nullptr;
	}
	return std::memchr(scan, byte, --count) == nullptr;
}

/* Match compiled program `scan` at `start`, not reading past `limit`.
 * On success returns a pointer one past the last matched byte; else nullptr.
 * When `fold_case` and mode is Ascii, compare via toupper. */
[[nodiscard]] char *re_match_at(char *start, char *limit, const char *scan,
                                ReMode mode, bool fold_case);

[[nodiscard]] int ascii_comp(char *out, std::string_view pattern);
[[nodiscard]] int hex_comp(char *out, std::string_view pattern);
[[nodiscard]] int hexchar(char *&cursor);

extern int smode;
extern bool again;
extern bool magic;
extern bool ignore_case;
