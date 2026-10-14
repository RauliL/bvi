/* In-memory matcher for binary-safe regexp bytecode. */

#include "regex.hpp"

#include <cctype>

namespace {

[[nodiscard]] int
fold_haystack_byte(char c, ReMode mode, bool fold_case) noexcept
{
	auto uc = static_cast<unsigned char>(c);
	if (fold_case && mode == ReMode::Ascii) {
		return std::toupper(uc);
	}
	return uc;
}

} // namespace


char *
re_match_at(char *start, char *limit, const char *scan, ReMode mode, bool fold_case)
{
	while (*scan != END) {
		switch (static_cast<ReOp>(static_cast<unsigned char>(*scan++))) {
		case ReOp::One: {
			int count = static_cast<unsigned char>(*scan++);
			int test = fold_haystack_byte(*start, mode, fold_case);
			if (count == 1) {
				if (test != static_cast<unsigned char>(*scan)) {
					return nullptr;
				}
				++scan;
			} else if (count > 1) {
				if (class_rejects(test, scan, count)) {
					return nullptr;
				}
				scan += count;
			}
			++start;
			break;
		}
		case ReOp::Star: {
			int count = static_cast<unsigned char>(*scan++);
			int test = fold_haystack_byte(*start, mode, fold_case);
			if (count == 1) {
				auto lit = static_cast<unsigned char>(*scan);
				while (test == lit) {
					++start;
					test = fold_haystack_byte(*start, mode, fold_case);
				}
				++scan;
			} else if (count > 1) {
				if (*scan == '^') {
					while (start < limit) {
						if (re_match_at(start, limit, scan + count, mode, fold_case)) {
							return start;
						}
						if (class_rejects(test, scan, count)) {
							return nullptr;
						}
						++start;
						test = fold_haystack_byte(*start, mode, fold_case);
					}
				} else {
					while (!class_rejects(test, scan, count)) {
						++start;
						test = fold_haystack_byte(*start, mode, fold_case);
					}
					scan += count;
				}
			} else {
				/* ".*" */
				while (start < limit) {
					if (re_match_at(start, limit, scan, mode, fold_case)) {
						return start;
					}
					++start;
				}
			}
			break;
		}
		case ReOp::End:
			break;
		}
	}
	return start;
}
