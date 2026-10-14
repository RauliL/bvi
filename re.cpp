/* re - regular expression search for bvi
 *
 * NOTE: Edit this file with tabstop=4 !
 *
 * 1996-01-06 created;
 * 1999-01-19 V 1.1.0
 * 1999-03-17 V 1.1.1
 * 1999-09-10 V 1.2.0
 * 2000-04-25 V 1.3.0 beta
 * 2000-09-29 V 1.3.0 final
 * 2010-06-02 V 1.3.4
 * 2013-08-24 V 1.4.0
 *
 * Copyright 1996-2013 by Gerhard Buergmann
 * gerhard@puon.at
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * See file COPYING for information on distribution conditions.
 */

/* You cannot use a common regexp subroutine, because \0 is a regular
 * character in a binary string !
 */

#include	"bvi.hpp"
#include	"set.hpp"

#include <csignal>
#include <format>
#include <string>

namespace {

struct SigIntGuard {
	explicit SigIntGuard(void (*handler)(int)) noexcept
	{
		std::signal(SIGINT, handler);
	}
	~SigIntGuard() { std::signal(SIGINT, SIG_IGN); }
	SigIntGuard(const SigIntGuard &) = delete;
	SigIntGuard &operator=(const SigIntGuard &) = delete;
};

} // namespace

char	pattern[MAXCMD + 1];
char	search_pat[BUFFER];	/* / or ? command */
const char	*notfound = "Fail|Pattern not found";
const char	*noprev = "No previous expression";
const char	*emptyclass = "Bad character class|Empty byte class '[]' or '[^]' cannot match";


PTR
bregexec(PTR start, const char *scan)
{
	return re_match_at(start, maxpos, scan, static_cast<ReMode>(smode), B(P_IC));
}


PTR
end_word(PTR start)
{
	PTR	pos;

	pos = start;
	if (!std::isprint(*pos & 0xff)) return start;
	while (std::isprint(*pos & 0xff)) if (pos++ > maxpos) return start;
	return --pos;
}


/* wordsearch serves the 'W' and 'w' - command
 */
PTR
wordsearch(PTR start, char mode)
{
	PTR		found;
	PTR		pos;
	int		ccount;

	pos = start + 1;
	do {
		while (std::isprint(*pos & 0xff)) if (pos++ > maxpos) return start;
		while (!std::isprint(*pos & 0xff)) if (pos++ > maxpos) return start;
		found = pos;
		ccount = 0;
		while (std::isprint(*pos & 0xff)) {
			if (pos++ > maxpos) return start;
			ccount++; }
		if (ccount < P(P_WL)) continue;

		if (mode == 'W') {
			if (*pos == '\0' || *pos == '\n') return found;
		} else {
			return found;
		}
	} while (pos < maxpos);
	return	start;
}


/* backsearch serves the 'b' and 'B' command
 */
PTR
backsearch(PTR start, char mode)
{
	PTR		pos;
	int		ccount;

	pos = start - 1;
	do {
		if (mode == 'B') {
			while (*pos != '\0' && *pos != '\n') if (pos-- < mem) return start;
		} else {
			while (!std::isprint(*pos & 0xff))		if (pos-- < mem) return start;
		}
		pos--;
		ccount = 0;
		while (std::isprint(*pos & 0xff)) {
			if (pos-- < mem) return start;
			ccount++; }
		if (ccount >= P(P_WL)) 	return (pos + 1);

	} while (pos > mem);
	return	start;
}


/* used by :s
 */
int
do_substitution(int delim, const char *line, PTR startpos, PTR endpos)
{
	int		n;
	char	*found;
	const char	*cmd = nullptr;
	int		repl_count = 0;
	bool		global = false;
	bool		conf = false;
	static	int		direct;
	static	int		pat_len = -1;
	static	int		ch;
	static	char	find_pat[BUFFER];
	static	char	repl_pat[MAXCMD];

	ignore_case = B(P_IC);
	magic = B(P_MA);
	switch (delim) {
	case '/':
	case '?':
		ch = delim;
		direct = (ch == '/' ? FORWARD : BACKWARD);
		cmd = patcpy(pattern, line, ch);
		if (pattern[0] == '\0') break;
		if (ascii_comp(find_pat, pattern)) return 0;
		cmd = patcpy(pattern, cmd, ch);	/* Replace Pattern */
		{
			const char *p = pattern;
			pat_len = 0;
			while (*p) {
				if (*p == '\\') {
					switch (*(++p)) {
					case '0':	repl_pat[pat_len++] = '\0'; break;
					case 'n':	repl_pat[pat_len++] = '\n'; break;
					case 'r':	repl_pat[pat_len++] = '\r'; break;
					case 't':	repl_pat[pat_len++] = '\t'; break;
					case '\\':	repl_pat[pat_len++] = '\\'; break;
					default:	emsg(std::format("No such escape sequence \\{}", *p));
						return 0;
					}
				} else {
					repl_pat[pat_len++] = *p;
				}
				++p;
			}
		}
		break;
	case '#':
	case '\\':
		ch = delim;
		direct = (ch == '\\' ? FORWARD : BACKWARD);
		cmd = patcpy(pattern, line, ch);
		if (hex_comp(find_pat, pattern)) return 0;
		cmd = patcpy(pattern, cmd, ch);	/* Replace Pattern */
		{
			char *p = pattern;
			pat_len = 0;
			while (*p) {
				if (*p == ' ' || *p == '\t') {
					++p;
				} else {
					if ((n = hexchar(p)) < 0) {
						emsg("Badly formed replacement pattern");
						return 0;
					}
					repl_pat[pat_len] = static_cast<char>(n);
					++pat_len;
				}
			}
		}
		break;
	case '\0':
	case 'g':
	case 'c':
		break;
	default:
		emsg("Extra chars|Extra characters at end of command");
		return -1;
	}
	if (pat_len == -1) {
		emsg("No previous substitute re|No previous substitute to repeat");
		return -1;
	}
	if (delim != '\0') {
		if (std::strchr(cmd, 'g')) global = true;
		if (std::strchr(cmd, 'c')) conf = true;
	}
	if ((std::strchr("\\#", ch) && loc == ASCII)
		|| (std::strchr("/?", ch) && loc == HEX)) {
				toggle();
	}
	startpos--;
	move(maxy, 0);
	refresh();

	if (global) {
		if ((undo_count = alloc_buf(endpos - startpos, &undo_buf))) {
			std::memcpy(undo_buf, startpos + 1, undo_count);
		}
		undo_start = startpos + 1;
		edits = U_EDIT;
	}

AGAIN:
	if (direct == FORWARD) {
		found = fsearch(startpos + 1, endpos, find_pat);
	} else {
		found = rsearch(startpos - 1, mem, find_pat);
	}
	if (!found) {
		if (!repl_count) {
			if (B(P_WS)) {
				emsg(notfound);
			} else {
				if (B(P_TE))
				  emsg(std::format("No match to {}", direct == FORWARD ? "BOTTOM" : "TOP"));
				else
				  emsg(std::format("Address search hit {} without matching pattern",
						direct == FORWARD ? "BOTTOM" : "TOP"));
			}
		}
		return repl_count;
	} else {
		setpage(found);
		if (conf) {
			repaint();
			msg("Replace?");
			move(y, x);
			if (vgetc() != 'y') goto SKIP;
		}
		repl_count++;
		current_start = pagepos + y * Anzahl + xpos();
		if (!global) {
			if ((undo_count = alloc_buf(pat_len, &undo_buf))) {
				std::memcpy(undo_buf, current_start, undo_count);
			}
			undo_start = current_start;
			edits = U_EDIT;
		}
		std::memcpy(current_start, repl_pat, pat_len);
SKIP:
		if (global)  {
			startpos = found + pat_len - 1;
			goto AGAIN;
		}
	}
	return repl_count;
}


/*
 * Used by /, ?, \, #, n or N command
 * ch		is this command
 * line		are the characters after
 *
 * return	address found
 */
PTR
searching(int ch, const char *line, PTR startpos, PTR endpos, int flag)
{
	const char	*cmd = nullptr;
	PTR		found;
	int		sdir;
	static	char	m[2];
	static	int		direct;

	if (line[0] == '\0' && again == 0) {
		emsg(noprev);
		return 0L;
	}

	ignore_case = (B(P_IC));
	magic = B(P_MA);
	start_addr--;
	if ((std::strchr("\\#", ch) && loc == ASCII)
		|| (std::strchr("/?", ch) && loc == HEX)) {
				toggle();
	}
	if (!std::strchr("Nn", ch)) {
		m[0] = ch;
		m[1] = '\0';
		switch (ch) {
		case '/':
		case '?':
			direct = (ch == '/' ? FORWARD : BACKWARD);
			cmd = patcpy(pattern, line, ch);
			if (pattern[0] != '\0') {
				if (ascii_comp(search_pat, pattern)) return 0L;
				again = true;
			}
			break;
		case '#':
		case '\\':
			direct = (ch == '\\' ? FORWARD : BACKWARD);
			cmd = patcpy(pattern, line, ch);
			if (pattern[0] != '\0') {
				if (hex_comp(search_pat, pattern)) break;
				again = true;
			}
			break;
		}
		if (!again) return 0L;
	} else {
		cmd = "";
		msg(m);
	}
	move(maxy, 0);
	refresh();
	sdir = (ch == 'N') ? !direct : direct;

	if (sdir == FORWARD) {
		found = fsearch(startpos + 1, endpos, search_pat);
		if (flag & S_GLOBAL) return(found);
		if (!found)
			if (flag & 1) {
				msg("Search wrapped BOTTOM|Search wrapped around BOTTOM of buffer");
				found = fsearch(mem, startpos, search_pat);
			}
	} else {
		found = rsearch(startpos - 1, mem, search_pat);
		if (flag & S_GLOBAL) return(found);
		if (!found)
			if (flag & 1) {
				msg("Search wrapped TOP|Search wrapped around TOP of buffer");
				found = rsearch(endpos, startpos, search_pat);
			}
	}
	if (!found) {
		if (flag & 1) {
			emsg(notfound);
		} else {
			if (B(P_TE)) {
			  emsg(std::format("No match to {}", sdir == FORWARD ? "BOTTOM" : "TOP"));
			} else {
			  emsg(std::format("Address search hit {} without matching pattern",
					sdir == FORWARD ? "BOTTOM" : "TOP"));
			}
		}
	} else {
		setpage(found);
		if (cmd) {
			switch(*cmd) {
			case 'z':	do_z(*++cmd);
						break;
			case 's':	do_substitution(ch, cmd + 2, found, endpos);
						repaint();
						break;
			case ';':	searching(*(cmd + 1), cmd + 2, found, maxpos - 1, flag);
			case '\0':	break;
			default:  	beep();
			}
		}
	}
	return found;
}


/* Copies a string from s2 to s1, up to delim or 0
 * returns pointer to next character
 */
const char *
patcpy(char *s1, const char *s2, char delim)
{
	while (*s2 != '\0' && *s2 != delim) {
		if (*s2 == '\\' && *(s2 + 1) == delim) s2++;
		*s1++ = *s2++;
	}
	*s1 = '\0';
	if (*s2 == delim) s2++;
	return s2;
}


PTR
fsearch_end(PTR start, PTR end, const char *smem, PTR *s_end)
{
	SigIntGuard interrupt{jmpproc};
	for (PTR spos = start; spos <= end; ++spos) {
		if ((*s_end = bregexec(spos, smem))) {
			++(*s_end);
			return spos;
		}
	}
	return nullptr;
}


PTR
fsearch(PTR start, PTR end, const char *smem)
{
	PTR s_end;
	return fsearch_end(start, end, smem, &s_end);
}


PTR
rsearch(PTR start, PTR end, const char *smem)
{
	SigIntGuard interrupt{jmpproc};
	for (PTR spos = start; spos >= end; --spos) {
		if (bregexec(spos, smem)) {
			return spos;
		}
	}
	return nullptr;
}


/* Calculates an address of a colon command
 * returns nullptr on error or default_address, if nothing found
 */
PTR
calc_addr(char **pointer, PTR def_addr)
{
	PTR		addr;
	int		ch, mark;
	char	*cmd;

	cmd = *pointer;
	addr = def_addr;
	SKIP_WHITE
	if (*cmd >= '1' && *cmd <= '9')  {
		addr = mem + std::strtoll(cmd, &cmd, 10) - P(P_OF);
	} else {
		ch = *cmd;
		switch (ch) {
		case '.':			/* Current position */
			addr = current;
			cmd++;
			break;
		case '^':
			addr = mem;
			cmd++;
			break;
		case '$':
			addr = maxpos - 1;
			cmd++;
			break;
		case '\'':			/* Mark */
			mark = (*++cmd);
			if (mark == '\'') {
				addr = last_motion;
				cmd++;
				break;
			} else if (mark < 'a' || mark > 'z') {
				emsg("Marks are ' and a-z");
				return nullptr;
			}
			if (markbuf[mark - 'a'] == nullptr) {
				emsg("Mark not defined");
				return nullptr;
			}
			addr = markbuf[mark - 'a'];
			cmd++;
			break;
		case '\\':
		case '/':
			cmd = const_cast<char *>(patcpy(pattern, cmd + 1, ch));
			if (pattern[0] == '\0' && again == 0) {
				emsg(noprev);
				return nullptr;
			}
			if (pattern[0] != '\0') {
				again = true;
				if (ch == '/') {
					if (ascii_comp(search_pat, pattern)) return nullptr;
				} else {
					if (hex_comp(search_pat, pattern)) return nullptr;
				}
			}
			addr = fsearch(mem, maxpos - 1, search_pat);
			break;
		case '#':
		case '?':
			cmd = const_cast<char *>(patcpy(pattern, cmd + 1, ch));
			if (pattern[0] == '\0' && again == 0) {
				emsg(noprev);
				return nullptr;
			}
			if (pattern[0] != '\0') {
				again = true;
				if (ch == '?') {
					if (ascii_comp(search_pat, pattern)) return nullptr;
				} else {
					if (hex_comp(search_pat, pattern)) return nullptr;
				}
			}
			addr = rsearch(maxpos - 1, mem, search_pat);
			break;
		case '0':
			addr = mem + std::strtol(cmd, &cmd, 16) - P(P_OF);
			break;
		}
	}
	SKIP_WHITE
	while (*cmd == '+' || *cmd == '-') {
		if (*cmd == '+') {
			cmd++;
			SKIP_WHITE
			addr += std::strtoll(cmd, &cmd, 0);
		} else {
			cmd++;
			SKIP_WHITE
			addr -= std::strtoll(cmd, &cmd, 0);
		}
		SKIP_WHITE
	}
	if (*pointer != cmd) {
		*pointer = cmd;
		addr_flag++;
	}
	return addr;
}
