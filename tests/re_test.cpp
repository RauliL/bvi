/* Standalone checks for the binary-safe regexp compiler and matcher. */

#include "regex.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>

const char *emptyclass = "Empty byte class";

void
emsg(std::string_view s)
{
	std::fprintf(stderr, "emsg: %.*s\n", static_cast<int>(s.size()), s.data());
}

namespace {

int failures = 0;

void
expect(bool cond, const char *msg)
{
	if (!cond) {
		std::fprintf(stderr, "FAIL: %s\n", msg);
		++failures;
	}
}

char *
find(char *hay, std::size_t len, const char *prog)
{
	char *end = hay + len;
	for (char *p = hay; p < end; ++p) {
		if (re_match_at(p, end, prog, static_cast<ReMode>(smode), ignore_case)) {
			return p;
		}
	}
	return nullptr;
}

} // namespace


int
main()
{
	char prog[1024];
	char buf[64];

	magic = true;
	ignore_case = false;

	/* Literal including embedded NUL via \\0 */
	expect(ascii_comp(prog, "a\\0b") == 0, "compile a\\0b");
	std::memcpy(buf, "xa\0by", 5);
	expect(find(buf, 5, prog) == buf + 1, "match a\\0b in binary");

	/* Dot and star */
	expect(ascii_comp(prog, "x.*z") == 0, "compile x.*z");
	std::memcpy(buf, "xabcz", 5);
	expect(find(buf, 5, prog) == buf, "match x.*z");

	/* Character class */
	expect(ascii_comp(prog, "[abc]") == 0, "compile [abc]");
	expect(find(buf, 5, prog) == buf + 1, "match [abc] at 'a'");

	/* Hex mode with NUL byte */
	expect(hex_comp(prog, "00 41") == 0, "compile hex 00 41");
	std::memcpy(buf, ".\0A.", 4);
	expect(find(buf, 4, prog) == buf + 1, "match NUL then 'A'");

	/* ignorecase folds ASCII literals at compile time */
	ignore_case = true;
	expect(ascii_comp(prog, "Ab") == 0, "compile Ab ignorecase");
	std::memcpy(buf, "xab", 3);
	expect(find(buf, 3, prog) == buf + 1, "match Ab ~ ab");

	if (failures != 0) {
		std::fprintf(stderr, "%d test(s) failed\n", failures);
		return 1;
	}
	std::puts("re_test: ok");
	return 0;
}
