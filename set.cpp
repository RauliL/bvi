/* SET.C - performing :set - command
 *
 * NOTE: Edit this file with tabstop=4 !
 *
 * 1996-02-29 created;
 * 1998-03-14 V 1.0.1
 * 1999-01-14 V 1.1.0
 * 1999-03-17 V 1.1.1
 * 1999-07-02 V 1.2.0 beta
 * 1999-08-14 V 1.2.0 final
 * 2000-07-15 V 1.3.0 final
 * 2001-10-10 V 1.3.1 
 * 2003-07-03 V 1.3.2
 * 2010-06-02 V 1.2.4
 * 2014-09-30 V 1.4.0
 *
 * Copyright 1996-2014 by Gerhard Buergmann 
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

#include "bvi.hpp"
#include "set.hpp"

#include <format>
#include <string>

static	bool		from_file = false;
static	FILE	*ffp;
static	char	fbuf[MAXCMD+1];

std::vector<param> params = {
	{ "autowrite",	"aw",		false, false },
	{ "columns",	"cm",		off_t{16}, false },
	{ "errorbells",	"eb",		false, false },
	{ "ignorecase",	"ic",		false, false },
	{ "magic",		"ma",		true, false },
	{ "memmove",	"mm",		false, false },
	{ "offset",		"of",		off_t{0}, false },
	{ "readonly",	"ro",		false, false },
	{ "scroll",		"scroll",	off_t{12}, false },
	{ "showmode",	"mo",		true, false },
	{ "term",		"term",		std::string{}, false },
	{ "terse",		"terse",	false, false },
	{ "unixstyle",	"us",		false, false },
	{ "window",		"window",	off_t{25}, false },
	{ "wordlength",	"wl",		off_t{4}, false },
	{ "wrapscan",	"ws",		true, false },
	{ "highlight",	"hl",		true, false },
};

int
doset(const char *arg)		/* parameter string */
{
	int		i;
	const char	*s;
	bool		did_window = false;
	bool		state = true;		/* new state of boolean parms. */

	if (arg == nullptr) {
		showparms(false);
		return 0;
	}
	if (!strcmp(arg, "all")) {
		showparms(true);
		return 0;	}
	if (!strncmp(arg, "no", 2)) {
		state = false;
		arg += 2; }

	for (i = 0; i < static_cast<int>(params.size()); i++) {
		s = params[i].fullname;
		if (strncmp(arg, s, strlen(s)) == 0)	/* matched full name */
			break;
		s = params[i].shortname;
		if (strncmp(arg, s, strlen(s)) == 0)	/* matched short name */
			break;
	}

	if (i < static_cast<int>(params.size())) {	/* found a match */
		if (arg[strlen(s)] == '?') {
			std::string buf;
			if (std::holds_alternative<bool>(params[i].value))
				buf = std::format("    {}{}",
					(std::get<bool>(params[i].value) ? "  " : "no"),
					params[i].fullname);
			else if (std::holds_alternative<std::string>(params[i].value))
				buf = std::format("      {}={}", params[i].fullname,
					std::get<std::string>(params[i].value));
			else
				buf = std::format("      {}={}", params[i].fullname,
					std::get<off_t>(params[i].value));
			msg(buf.c_str());
			return 0;
		}
		if (!strcmp(params[i].fullname, "term")) {
			emsg("Can't change type of terminal from within bvi");
			return 1;
		}
		if (std::holds_alternative<off_t>(params[i].value)) {
			if ((i == P_LI) || (i == P_OF)) did_window = true;
			if (arg[strlen(s)] != '=' || state == false) {
				emsg(std::format("Option {} is not a toggle",
						params[i].fullname).c_str());
				return 1;
			} else {
				char *end = nullptr;
				s = arg + strlen(s) + 1;
				std::get<off_t>(params[i].value) = strtoll(s, &end, 0);
				s = end;
				params[i].changed = true;
				if (i == P_CM) {
					if (((COLS - AnzAdd - 1) / 4) >= P(P_CM)) {
						Anzahl = P(P_CM);
					} else {
						Anzahl = P(P_CM) = ((COLS - AnzAdd - 1) / 4);
					}
				    maxx = Anzahl * 4 + AnzAdd + 1;
				    Anzahl3 = Anzahl * 3;
				    status = Anzahl3 + Anzahl - 17;
				    screen = Anzahl * (maxy - 1);
					did_window = true;
					stuffin("H");	/* set cursor at HOME */
				}
			}
		} else if (std::holds_alternative<bool>(params[i].value)) {
			if (arg[strlen(s)] == '=') {
				emsg("Invalid set of boolean parameter");
				return 1;
			} else {
				std::get<bool>(params[i].value) = state;
				params[i].changed = true;
				if (i == P_HL && state == false) {
					hl_spat = false;
					repaint();
				}
			}
		} else {
			emsg("Can't change type of terminal from within bvi");
			return 1;
		}
	} else  {
		emsg("No such option@- `set all' gives all option values");
		return 1;
	}

	if (did_window) {
		maxy = P(P_LI) - 1;
		new_screen();
	}

	return 0;
}


/* show ALL parameters */
void
showparms(bool all)
{
	int		n;

	n = 2;
	msg("Parameters:\n");
	for (const auto& p : params) {
		if (!all && !p.changed)
			continue;
		std::string buf;
		if (std::holds_alternative<bool>(p.value))
			buf = std::format("    {}{}\n",
				(std::get<bool>(p.value) ? "  " : "no"), p.fullname);
		else if (std::holds_alternative<std::string>(p.value))
			buf = std::format("      {}={}\n", p.fullname,
				std::get<std::string>(p.value));
		else
			buf = std::format("      {}={}\n", p.fullname,
				std::get<off_t>(p.value));

		msg(buf.c_str());
		n++;
		if (n == P(P_LI)) {
			if (wait_return(false)) return;
			n = 1;
		}
	}
	wait_return(true);
}


/* reads the init file (.bvirc) */
int
read_rc(const char *fn)
{
	int i;

	if ((ffp = fopen(fn, "r")) == nullptr) return -1;
	from_file = true;
	while (fgets(fbuf, MAXCMD, ffp) != nullptr) {
		i = strlen(fbuf) - 1;
		while ((i >= 0) && (fbuf[i] == NL || fbuf[i] == CR)) fbuf[i--] = '\0';
		docmdline(fbuf);
	}
	fclose(ffp);
	from_file = false;
	return 0;
}


int
do_logic(int mode, const char *str)
{
	int		a, b;
	int		value;
	size_t	n;
	const char	*err_str ="Invalid value@for bit manipulation";

	if (mode==LSHIFT || mode==RSHIFT || mode==LROTATE || mode==RROTATE) {
		value = atoi(str);
		if (value < 1 || value > 8) {
			emsg(err_str);
			return 1;
		}
	} else {
		if (strlen(str) == 8) {
			value = strtol(str, nullptr, 2);
			for (n = 0; n < 8; n++) {
				if (str[n] != '0' && str[n] != '1') {
					value = -1;
					break;
				}
			}
		} else if (str[0] == 'b' || str[0] == 'B') {
			value = strtol(str + 1, nullptr, 2);
		} else if (str[0] == '0') {
			value = strtol(str, nullptr, 0);
			for (n = 0; n < strlen(str); n++) {
				if (!isxdigit(str[n])) {
					value = -1;
					break;
				}
			}
		} else {
			value = atoi(str);
		}
		if (value < 0 || value > 255) {
			emsg(err_str);
			return 1;
		}
	}
	if ((undo_count = alloc_buf((off_t)(end_addr - start_addr + 1), &undo_buf))) {
		memcpy(undo_buf, start_addr, undo_count);
	}
	undo_start = start_addr;
	edits = U_EDIT;
	while(start_addr <= end_addr) {
		a = *start_addr;
		a &= 0xff;
		switch(mode) {
		case LSHIFT:	a <<= value;
						break;
		case RSHIFT:	a >>= value;
						break;
		case LROTATE:	a <<= value;
						b = a >> 8;
						a |= b;
						break;
		case RROTATE:	
						b = a << 8;
						a |= b;
						a >>= value; 
						/*
						b = a << (8 - value);
						a >>= value; 
						a |= b;
						*/
						break;
		case AND:		a &= value;
						break;
		case OR:		a |= value;
						break;
		case XOR:		
		case NOT:		a ^= value;
						break;
		case NEG:		a ^= value;
						a++;		/* Is this true */
						break;
		}
		*start_addr++ = (char)(a & 0xff);
	}
	repaint();
	return(0);
}


int
getcmdstr(std::string& p, int x)
{
	int		c;
	int		n;

	if (from_file) {
		char	tmp[MAXCMD+1];
		if (fgets(tmp, sizeof(tmp), ffp) != nullptr) {
			size_t i = strlen(tmp);
			while (i > 0 && (tmp[i - 1] == NL || tmp[i - 1] == CR))
				tmp[--i] = '\0';
			p = tmp;
			return 0;
		} else {
			return 1;
		}
	}

	signal(SIGINT, jmpproc);
	p.clear();
	move(maxy, x);
    do {
        switch (c = vgetc()) {
		case BVICTRL('H'):
		case ASCII_DEL:
		case KEY_BACKSPACE:
		case KEY_LEFT:
            if (!p.empty()) {
                p.pop_back();
				move(maxy, x);
				n = x;
                for (char ch : p)  {
                    addch(ch);
					n++;
				}
				addch(' ');
				move(maxy, n);
            } else {
                msg("");
				signal(SIGINT, SIG_IGN);
                return 1;
            }
            break;
        case ESC:           /* abandon command */
			p.clear();
            msg("");
			signal(SIGINT, SIG_IGN);
            return  1;
#if NL != KEY_ENTER
        case NL:
#endif
#if CR != KEY_ENTER
        case CR:
#endif
		case KEY_RIGHT:
		case KEY_UP:
		case KEY_DOWN:
		case KEY_ENTER:
            break;
        default:        /* a normal character */
            addch(c);
            p.push_back(static_cast<char>(c));
            break;
        }
		refresh();
    } while (c != NL && c != CR && c != KEY_ENTER);

	signal(SIGINT, SIG_IGN);
	return 0;
}
