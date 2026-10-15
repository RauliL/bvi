/* recomp - regular expression compiler (binary-safe)
 *
 * You cannot use a common regexp subroutine, because \0 is a regular
 * character in a binary string!
 */

#include "regex.hpp"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>

/* Provided by bvi or bmore. */
void emsg(std::string_view s);
extern const char *emptyclass;

int smode = static_cast<int>(ReMode::Hex);
bool again = false;
bool magic = true;
bool ignore_case = false;

namespace {

[[nodiscard]] unsigned char
fold_byte(unsigned char c) noexcept
{
	return ignore_case ? static_cast<unsigned char>(std::toupper(c)) : c;
}

[[nodiscard]] bool
atom_starred(const char *p) noexcept
{
	if (magic) {
		return *(p + 1) == '*';
	}
	return *(p + 1) == '\\' && *(p + 2) == '*';
}

[[nodiscard]] bool
bracket_starred(const char *end) noexcept
{
	if (magic) {
		return *(end + 1) == '*';
	}
	return *(end + 1) == '\\' && *(end + 2) == '*';
}

void
skip_star(char *&p) noexcept
{
	if (magic) {
		if (*p == '*') {
			++p;
		}
	} else if (*p == '\\' && *(p + 1) == '*') {
		p += 2;
	}
}

} // namespace


int
hexchar(char *&cursor)
{
	if (!std::isxdigit(static_cast<unsigned char>(*cursor))) {
		emsg("Bad hex character@in expression");
		return -1;
	}

	char tmpbuf[3];
	tmpbuf[0] = *cursor++;
	tmpbuf[1] = '\0';
	if (std::isxdigit(static_cast<unsigned char>(*cursor))) {
		tmpbuf[1] = *cursor++;
	}
	tmpbuf[2] = '\0';

	int nr = 0;
	std::sscanf(tmpbuf, "%2x", &nr);
	while (*cursor == ' ' || *cursor == '\t') {
		++cursor;
	}
	return nr;
}


int
ascii_comp(char *smem, std::string_view pattern)
{
	std::string pat_buf{pattern};
	char *comp = smem;
	char *p = pat_buf.data();

	while (*p != END) {
		bool bracket = false;
		if (magic) {
			bracket = (*p == '[');
		} else if (*p == '\\' && *(p + 1) == '[') {
			bracket = true;
			++p;
		}

		if (bracket) {
			char *end = std::strchr(p, ']');
			if (end == nullptr) {
				emsg("Missing ]");
				return 1;
			}
			++p;
			if (*p == ']' || (*p == '^' && *(p + 1) == ']')) {
				emsg(emptyclass);
				return 1;
			}

			*comp++ = bracket_starred(end) ? STAR : ONE;
			char *counter = comp++;
			int count = 0;

			if (*p != '^') {
				*comp++ = '\0';
				++count;
			}

			while (end > p) {
				if (*p == '-') {
					unsigned char cc = fold_byte(static_cast<unsigned char>(*(p - 1)));
					unsigned char cc1 = fold_byte(static_cast<unsigned char>(*(p + 1)));
					while (cc <= cc1) {
						*comp++ = static_cast<char>(cc++);
						++count;
					}
					p += 2;
				} else {
					++count;
					*comp++ = static_cast<char>(fold_byte(static_cast<unsigned char>(*p++)));
				}
			}
			++p;
			*counter = static_cast<char>(count);
		} else {
			bool dot = false;
			if (magic) {
				dot = (*p == '.');
			} else if (*p == '\\' && *(p + 1) == '.') {
				dot = true;
				++p;
			}

			if (*p == '\\') {
				switch (*(p + 1)) {
				case 'n': *++p = '\n'; break;
				case 'r': *++p = '\r'; break;
				case 't': *++p = '\t'; break;
				case '0': *++p = '\0'; break;
				default: ++p; break;
				}
			}

			*comp++ = atom_starred(p) ? STAR : ONE;

			if (dot) {
				*comp++ = 0;
				++p;
			} else {
				*comp++ = 1;
				*comp++ = static_cast<char>(fold_byte(static_cast<unsigned char>(*p++)));
			}
		}
		skip_star(p);
	}

	*comp = END;
	smode = static_cast<int>(ReMode::Ascii);
	return 0;
}


int
hex_comp(char *smem, std::string_view pattern)
{
	std::string pat_buf{pattern};
	char *comp = smem;
	char *p = pat_buf.data();

	while (*p != END) {
		while (*p == ' ' || *p == '\t') {
			++p;
		}
		if (*p == '[') {
			char *end = std::strchr(p, ']');
			if (end == nullptr) {
				emsg("Missing ]");
				return 1;
			}
			++p;
			while (*p == ' ' || *p == '\t') {
				++p;
			}

			if (*p == ']' || (*p == '^' && *(p + 1) == ']')) {
				emsg(emptyclass);
				return 1;
			}

			*comp++ = (*(end + 1) == '*') ? STAR : ONE;
			char *counter = comp++;
			int count = 1;

			if (*p == '^') {
				*comp++ = *p++;
			} else {
				*comp++ = '\0';
			}

			while (end > p) {
				if (*p == ' ' || *p == '\t') {
					++p;
				} else if (*p == '-') {
					int cc = static_cast<unsigned char>(*(comp - 1));
					++p;
					int ccm = hexchar(p);
					if (ccm < 0) {
						return 1;
					}
					while (cc <= ccm) {
						*comp++ = static_cast<char>(cc++);
						++count;
					}
					/* Preserve historical cursor advance after a hex range. */
					++p;
					++p;
				} else {
					int nr = hexchar(p);
					if (nr < 0) {
						return 1;
					}
					++count;
					*comp++ = static_cast<char>(nr);
				}
			}
			++p;
			*counter = static_cast<char>(count);
			if (*p == '*') {
				++p;
			}
		} else if (*p == '"') {
			++p;
			char *end = std::strchr(p, '"');
			if (end == nullptr) {
				end = p + std::strlen(p);
			}
			while (end > p) {
				*comp++ = 1;
				*comp++ = 1;
				*comp++ = static_cast<char>(fold_byte(static_cast<unsigned char>(*p++)));
			}
			++p;
		} else if (*p == '.') {
			if (*(p + 1) == '*') {
				*comp++ = STAR;
				++p;
			} else {
				*comp++ = ONE;
			}
			*comp++ = 0;
			++p;
		} else {
			int nr = hexchar(p);
			if (nr < 0) {
				return 1;
			}
			if (*p == '*') {
				++p;
				*comp++ = STAR;
			} else {
				*comp++ = ONE;
			}
			*comp++ = 1;
			*comp++ = static_cast<char>(nr);
		}
	}

	*comp = END;
	smode = static_cast<int>(ReMode::Hex);
	return 0;
}
