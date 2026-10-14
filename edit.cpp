/* edit - the edit commands
 *
 * NOTE: Edit this file with tabstop=4 !
 *
 * 1999-04-19 V 1.1.2
 * 1999-07-02 V 1.2.0 beta
 * 1999-10-30 V 1.2.0 final
 * 2000-05-14 V 1.3.0 alpha
 * 2000-07-15 V 1.3.0 final
 * 2001-12-07 V 1.3.1
 * 2003-07-04 V 1.3.2
 * 2006-04-05 V 1.3.3 alpha - binary representation
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

#include    "bvi.hpp"
#include    "set.hpp"

#include <algorithm>
#include <format>
#include <string>

extern	long    precount;

char contrd[][4] = {"NUL", " ^A", " ^B", " ^C", " ^D", " ^E", " ^F", "BEL",
					" BS", "TAB", " NL", "HOM", "CLR", " CR", " ^N", " ^O",
					" ^P", " ^Q", " ^R", " ^S", " ^T", " ^U", " ^V", " ^W",
					" ^X", " ^Y", " ^Z", "ESC", " FS", " GS", " RS", " US",
					"DEL" };
char contru[][4] = {"NUL", "SOH", "STX", "ETX", "EOT", "ENQ", "ACK", "BEL",
					" BS", " HT", " NL", " VT", " NP", " CR", " SO", " SI",
					"DLE", "DC1", "DC2", "DC3", "DC4", "NAK", "SYN", "ETB",
					"CAN", " EM", "SUB", "ESC", " FS", " GS", " RS", " US",
					"DEL" };
char tmpbuf[10];
char linbuf[16384];
bool hl_spat = false;

static  char    getcbuff[BUFFER];
static  char    *getcnext = nullptr;


/* mode: ('A') append
 *       ('R') replace one or more different characters
 *       ('r') replace 1 character
 *       ('i') insert characters
 *       ('a') insert after cursor
 * a precount can be used
 *
 * for insert and append we misuse the undo buffer for the inserted
 * characters (for "." command)
 */
off_t
edit(int mode)
{
	int		ch, ch1;
	size_t	len;
	off_t	count = 0L;
	off_t	buffer = BUFFER;
	off_t	psize;

	if (!filesize && mode == 'i') {
		mode = 'A';
	}
	if (mode != 'A' && mode != 'a') {
		if (current - mem >= filesize) {
			beep();
			return 0L;
		}
	}
	if (precount < 1) precount = 1;
	len = rep_buf.size();
	if (mode == 'r' && current + precount > maxpos) {
		beep();
		rep_buf.resize(len);
		return 0L;
	}
	if (alloc_buf(buffer, &undo_buf) == 0L) {
		rep_buf.resize(len);
		return 0L;
	}
	switch(mode) {
	case 'A':	edits = U_APPEND;
				break;
	case 'R':	edits = U_EDIT;
				smsg("REPLACE MODE");
				break;
	case 'r':	edits = U_EDIT;
				smsg("REPLACE 1 CHAR");
				break;
	case 'a':
	case 'i':	edits = U_INSERT;
				smsg("INSERT MODE");
				break;
	}

	undo_start = current;

	while((ch = vgetc()) != ESC) {
		ch &= 0xff;
		rep_buf.push_back(static_cast<char>(ch));
		len = rep_buf.size();
		if (ch == '\t') {
			toggle();
			setcur();
			continue;
		}
		if (ch == KEY_BACKSPACE 
				|| ch == ASCII_DEL
				|| ch == BVICTRL('H')) {
			if (count > 0) {
				rep_buf.pop_back();
				len = rep_buf.size();
				count--;
				if (mode == 'A' || mode == 'a' || mode == 'i') {
					filesize--;
					maxpos--;
				}
				current--;
				cur_back();
				setcur();
			} else beep();
			continue;
		}
		if (loc == HEX) {
			if (std::isxdigit(ch)) {
				mvaddch(y, x + 1, ' ');
				mvaddch(y, x, ch);
				do {
					ch1 = vgetc() & 0xff;
					if (ch1 == ESC) {
						mvaddch(y, x, ' ');
						current--;
						cur_back();
						goto escape;
					}
					if (!std::isxdigit(ch1)) {
						beep();
						ch1 = -1;
					}
				} while (ch1 == -1);
				rep_buf.push_back(static_cast<char>(ch1));
				len = rep_buf.size();
				mvaddch(y, x + 1, ch1);
				std::sprintf(tmpbuf, "%c%c", ch, ch1);
				std::sscanf(tmpbuf, "%2x", &ch);
			} else {
				beep();
				rep_buf.pop_back();
				len = rep_buf.size();
				goto wrong;
			}
		} else {			/*** ASCII - Bereich ***/
			if (std::isprint(ch)) {
				mvaddch(y, x, ch);
			} else {
				beep();
				goto wrong;
			}
		}
		curpos = current++;
		if (mode == 'i' || mode == 'a') {
			std::memmove(current, curpos, maxpos - curpos);
		}
		if (mode == 'A' || mode == 'i' || mode == 'a') {
			maxpos++;
			filesize++;
			/* NEU
			undo_buf[count++] = ch;
			*/
			count++;
		} else {
			undo_buf[count++] = *curpos;
		}
		if (count == buffer) {
			buffer += BUFFER;
			if (alloc_buf(buffer, &undo_buf) == 0L) {
				rep_buf.resize(len);
				return count;
			}
		}

		*curpos = (char)ch;
		cur_forw(false);
		statpos();
		if (mode == 'i' || mode == 'a') {
			repaint();
		} else {
			lineout();
		}
		setcur();

		if (filesize > memsize - 2L) {
			if (enlarge(100L)) break;
		}

		if ((mode != 'A' && mode != 'a') && curpos == maxpos - 1) break;
		if (mode == 'r') { break; }
wrong:
		continue;
	}
	rep_buf.push_back(static_cast<char>(ESC));
	if (!count) goto escape;

	if (precount > 1) {
		switch (mode) {
		case 'i':
		case 'a':
		case 'A':
			psize = count * (precount - 1);
			if (filesize + psize > memsize - 2L) {
				if (enlarge(psize + 100L)) return count;
			}
			if (psize + count > buffer) {
				if (alloc_buf(psize + count, &undo_buf) == 0L)
					return count;
			}

			if (mode == 'i' || mode == 'a') {
				std::memmove(current + psize, current, maxpos - curpos);
			}

			/* NEU
			undo_pos = undo_buf + count - 1L;
			*/
			while (--precount) {
				/* NEU
				memcpy(undo_pos + 1L, undo_pos - count + 1L, count);
				undo_pos += count;
				*/
				std::memcpy(curpos + 1L, curpos - count + 1L, count);
				curpos += count;
			}
			filesize += psize;
			count += psize;
			maxpos += psize;
			undo_count += psize;
			current = curpos + 1L;
			setpage(current);
			repaint();
			break;
		case 'R':
			if (current + count * (precount - 1) > maxpos) break;
			psize = count;
			while (--precount) {
				std::memcpy(undo_buf + psize, curpos + 1L, count);
				psize += count;
				std::memcpy(curpos + 1L, curpos - count + 1L, count);
				curpos += count;
			}
			count = psize;
			setpage(++curpos);
			repaint();
			break;
		case 'r':
			while (--precount) {
				undo_buf[count++] = *(++curpos);
				*curpos = (char)ch;
				cur_forw(false);
				statpos();
				lineout();
			}
			break;
		}
	}
	cur_back();
escape:
	setcur();
	smsg("");
	return(count);
}


/* Do the f, F, t or T command
 * If flag == 1 save the character in rep_buf
 * else setpage()
 */
PTR
do_ft(int ch, bool flag)
{
	static	int	chi;
	static	int	chp = 1;
	int		dir;
	PTR		ptr;

	switch (ch) {
		/*
		case 1:		beep();
					return nullptr;					 no previous command
		case -1:	if (chp == 'f' || chp == 't') dir = BACKWARD;
						else dir = FORWARD;
					break;
		case 0:		if (chp == 'f' || chp == 't') dir = FORWARD;
						else dir = BACKWARD;
					break;
		*/
		case -1:	if (chp == 1) {
						beep();
						return nullptr;
					}
					if (chp == 'f' || chp == 't') dir = BACKWARD;
						else dir = FORWARD;
					break;
		case 0:		if (chp == 1) {
						beep();
						return nullptr;
					}
					if (chp == 'f' || chp == 't') dir = FORWARD;
						else dir = BACKWARD;
					break;
		default:	chp = ch;
					if (chp == 'f' || chp == 't') dir = FORWARD;
						else dir = BACKWARD;
					chi = vgetc();
					if (flag) {
						rep_buf.push_back(static_cast<char>(chi));
					}
	}
	ptr = current;
	do {
		if (dir == FORWARD) {
			do {
				ptr++;
				if (ptr > maxpos) break;
			} while (*ptr != chi);
			if (ptr > maxpos) break;
		} else {
			do {
				ptr--;
				if (ptr < mem) break;
			} while (*ptr != chi);
			if (ptr < mem) break;
		}
	} while (--precount > 0);
	/*
	if (*ptr == chi) {
	*/
	if (ptr >= mem && ptr <= maxpos) {
		if (loc == HEX) toggle();
		if (chp == 't') ptr--;
		if (chp == 'T') ptr++;
		if (!flag) {
			setpage(ptr);
		}
		return(ptr);
	}
	beep();
	return nullptr;
}


void
do_z(int mode)
{
	switch (mode) {
	case '.':	while (y != maxy / 2) {
					if (y > maxy / 2) {
						pagepos += Anzahl;
						y--;
					} else {
						if (pagepos == mem) break;
						pagepos -= Anzahl;
						y++;
					}
				}
				break;
	case '-':	while (y < maxy - 1) {
					if (pagepos == mem) break;
					pagepos -= Anzahl;
					y++;
				}
				break;
	case '\0':
	case '\n':
	case '\r':	while (y > 0) {
					y--;
					pagepos += Anzahl;
				}
				break;
	default :   beep();
				break;
	}
	repaint();
}



void
scrolldown(int lns)
{
	while (lns--) {
		if (maxpos >= (pagepos + Anzahl)) pagepos += Anzahl;
			else { beep(); lns = 0; }
		repaint();
		refresh();
	}
}


void
scrollup(int lns)
{
	while (lns--) {
		if (mem <= (PTR)(pagepos - Anzahl)) pagepos -= Anzahl;
			else { beep(); lns = 0; }
		repaint();
		refresh();
	}
}


int
xpos()
{
	if (loc == HEX) return((x - AnzAdd) / 3);
		else   return(x - AnzAdd - Anzahl3);
}


void
toggle()
{
	if (loc == HEX) {
		x = xpos() + AnzAdd + Anzahl3;
		loc = ASCII;
	}	else	{
		x = xpos() * 3 + AnzAdd;
		loc = HEX;
	}
}


void
setcur()
{
	move(y, x);
	refresh();
}


/************* display current position *************/
void
statpos()
{
	char bin_val [9];
	unsigned char  Char1;
	int i;
	off_t	bytepos;
	std::string str;

	if (!B(P_MO)) return;
	bytepos = current - mem;
	if (bytepos >= filesize) {
		// mvaddstr(maxy, status, "                           ");
		move(maxy, status);
		for (i = status; i < maxx; i++) addch(' ');
		return;
	}
	Char1 = *(mem + bytepos);

	if (std::isprint(Char1)) {
		str = std::format("'{}'", static_cast<char>(Char1));
	} else if (Char1 < 32) {
		str = B(P_US) ? contru[Char1] : contrd[Char1];
	} else if (Char1 == 127) {
		str = B(P_US) ? contru[32] : contrd[32];
	} else {
		str = "   ";
	}
	for (i = 0; i < 8; ++i) {
		if(Char1 & (1<<i)) {
			bin_val[7-i] = '1';
		} else {
			bin_val[7-i] = '0';
		}
	}
	bin_val[8] = '\0';

	std::string status_str = std::format("{:08X} {} \\{:03o} 0x{:02X} {:3d} {:3s}",
		(unsigned long long)(bytepos + P(P_OF)), bin_val, Char1, Char1, Char1, str);
	statsize = static_cast<int>(status_str.size());
	attrset(A_BOLD);
	status = maxx - 1 - statsize;
	mvaddstr(maxy, status, status_str.c_str());
	attrset(A_NORMAL);
}


void
printline(PTR mempos, int scpos)
{
	/* int		print_pos; */
	PTR			hl_start = 0;
	PTR			hl_end = 0;
	PTR			f_start = 0;
	PTR			f_end = 0;
	int			print_pos = 0, mv_pos = 0;
	unsigned char cur_ch;
	char		asc[MAXCMD+1];

	if (mempos > maxpos) {
		std::strcpy(linbuf, "~         ");
	} else {

		std::sprintf(linbuf, addr_form, (long long)(mempos - mem + P(P_OF)));
		*asc = '\0';
	}
	mvaddstr(scpos, mv_pos, linbuf);
	mv_pos = AnzAdd;
	*linbuf = '\0';
	if (hl_spat) {
	    f_start = (mempos - Anzahl) < mem ? mem : (mempos - Anzahl);
	    f_end = mempos + (2 * Anzahl);
	    hl_start = fsearch_end(f_start, f_end, search_pat, &hl_end);
	}

	for (print_pos = 0; print_pos < Anzahl; print_pos++) {
		if (hl_spat) {
		    while (hl_start != nullptr) {
		        if (hl_start < mempos) {
		            if (hl_end < mempos) {
		                f_start = hl_start + 1;
		                hl_start = fsearch_end(f_start, f_end, search_pat, &hl_end);
		            } else {
		                attrset(A_STANDOUT); /* start out highlighted */
		                break;
		            }
		        } else if (hl_start >= mempos) {
		            break;
		        }
		    }
		    if (hl_start != nullptr) {
		        if ((hl_start - mempos) == print_pos) {
		            mvaddstr(scpos, mv_pos, linbuf);
		            mv_pos = AnzAdd + (3 * print_pos);
		            *linbuf = '\0';
		            attrset(A_STANDOUT);
		        }
		        if ((hl_end - mempos) == print_pos) {
		            mvaddstr(scpos, mv_pos, linbuf);
		            *linbuf = '\0';
		            mv_pos = AnzAdd + (3 * print_pos);
		            f_start = hl_end;
		            hl_start = fsearch_end(f_start, f_end, search_pat, &hl_end);
		            if (f_start != hl_start) {
		                attrset(A_NORMAL);
		            }
		        }
		    }
		}
		if (mempos + print_pos >= maxpos) {
			std::sprintf(tmpbuf, "   ");
			cur_ch = ' ';
		} else {
			cur_ch = *(mempos + print_pos);
			std::sprintf(tmpbuf, "%02X ", cur_ch);
		}
		std::strcat(linbuf, tmpbuf);
		if (std::isprint(cur_ch))
			*(asc + print_pos) = cur_ch;
		else
			*(asc + print_pos) = '.';
	}
	mvaddstr(scpos, mv_pos, linbuf);
	mv_pos = AnzAdd + (3 * print_pos);
	attrset(A_NORMAL);
	*(asc + Anzahl) = '\0';
	mvaddstr(scpos, mv_pos, asc);
}


/* displays a line on screen
 * at pagepos + line y
 */
int
lineout()
{
	off_t	Adresse;

	Adresse = pagepos - mem + y * Anzahl;
	printline(mem + Adresse, y);
	move(y, x);
	return(0);
}


void
new_screen()
{
	screen = Anzahl * (maxy - 1);
	clear();
	repaint();
}


void
repaint()	/***** redraw screen *********************/
{
	int	save_y;

	save_y = y;
	for (y = 0; y < maxy; y++) lineout();
	y = save_y;
}



/******* display an arbitrary address on screen *******/
void
setpage(PTR addr)
{
	if ((addr >= pagepos) && ((addr - pagepos) < screen)) {
		y = (addr - pagepos) / Anzahl;
		if (loc == HEX)
			x = AnzAdd + ((addr - pagepos) - y * Anzahl) * 3;
		else
			x = AnzAdd + Anzahl3  + ((addr - pagepos) - y * Anzahl);
	} else {
		pagepos = (((addr - mem) / Anzahl) * Anzahl + mem)
					- (Anzahl * (maxy / 2));
		if (pagepos < mem) pagepos = mem;
		y = (addr - pagepos) / Anzahl;
		if (loc == HEX)
			x = AnzAdd + ((addr - pagepos) - y * Anzahl) * 3;
		else
			x = AnzAdd + Anzahl3 + ((addr - pagepos) - y * Anzahl);
		repaint();
	}
}


int
cur_forw(bool check)
{
	if (check) {
		if (current - mem >= filesize) {
			beep();
			return 1;
		}
	}
	if (loc == ASCII) {
		if (x < AnzAdd - 1 + Anzahl3 + Anzahl) {
			x++ ;
			return 0;
		} else x = AnzAdd + Anzahl3;
	} else {
		if (x < 5 + Anzahl3) {
			x += 3;
			return 0;
		} else x = AnzAdd;
	}
	statpos();
	lineout();
	if (y < maxy - 1) {
		y++;
		return 0;
	} else {
		if (pagepos < (PTR)(mem + filesize)) {
				pagepos += Anzahl;
				repaint();
				return 0;
		} else {
			beep();
			return 1;
		}
	}
}


int
cur_back()
{
	if (loc == ASCII) {
		if (x > AnzAdd + Anzahl3) {
			x-- ;
			return 0;
		} else {
			x = AnzAdd - 1 + Anzahl3 + Anzahl;
		}
	} else {
		if (x > AnzAdd + 2) {
			x -= 3;
			return 0;
		} else {
			if (current == mem) return 0;
			x = AnzAdd + Anzahl3 - 3;
		}
	}
	statpos();
	lineout();
	if (y > 0) {
		y--;
		return 0;
	} else {
		if (pagepos > mem) {
			pagepos -= Anzahl;
			repaint();
			return 0;
		} else {
			beep();
			return 1;
		}
	}
}


void
fileinfo(const std::filesystem::path& fname)
{
	off_t	bytepos;
	std::string	status;

	if (!fname.empty()) {
		status = std::format("{}{}{} ", '"', fname.string(), '"');
	} else {
		status = "No file ";
	}
	if (filemode != NEW && filemode != REGULAR)
		status += "[Not edited] ";
	if (B(P_RO)) status += "[Read only] ";
	if (edits) status += "[Modified] ";
	if (filesize) {
		bytepos = (pagepos + y * Anzahl + xpos()) - mem + 1L;
		status += std::format("byte {} of {} --{}%--",
			bytepos, filesize, bytepos * 100L / filesize);
	} else {
		status += " 0 bytes";
	}
	msg(status.c_str());
}


/********** END ************/
void
quit()
{
	move(maxy, 0);
	endwin();
	std::printf("\nbvi version %s %s\n", VERSION, copyright);
	std::exit(0);
}


int
vgetc()
{
    int nextc;

    if (getcnext != nullptr) {
        nextc = *getcnext++;
        if (*getcnext == '\0') {
            *getcbuff = '\0';
            getcnext = nullptr;
        }
        return(nextc);
    }
    return getch();
}


void
stuffin(std::string_view s)
{
    if (getcnext == nullptr) {
       if (s.size() >= BUFFER) s = s.substr(0, BUFFER - 1);
       std::memcpy(getcbuff, s.data(), s.size());
       getcbuff[s.size()] = '\0';
       getcnext = getcbuff;
    } else {
       size_t have = std::strlen(getcbuff);
       size_t room = (have < BUFFER - 1) ? (BUFFER - 1 - have) : 0;
       size_t n = std::min(room, s.size());
       std::memcpy(getcbuff + have, s.data(), n);
       getcbuff[have + n] = '\0';
    }
}


void
do_back(off_t n, PTR start)
{
	if (start - n < mem) {
		beep();
		return;
	}
	if ((undo_count = alloc_buf(n, &undo_buf)) == 0L)
		return;
    yanked = alloc_buf(n, &yank_buf);
	edits = U_BACK;
	undo_start = start - n;
	std::memcpy(undo_buf, start - undo_count, undo_count);
	std::memcpy(yank_buf, start - undo_count, undo_count);
	std::memmove(start - undo_count, start, maxpos - start);
	filesize -= undo_count;
	maxpos -= undo_count;
	setpage(start - undo_count);
	repaint();
}


int
do_delete(off_t n, PTR start)
{
	if (n + start > maxpos) {
		beep();
		return 1;
	}
	if ((undo_count = alloc_buf(n, &undo_buf)) == 0L)
		return 1;
	yanked = alloc_buf(n, &yank_buf);
	edits = U_DELETE;
	undo_start = start;
	std::memcpy(undo_buf, start, undo_count);
	std::memcpy(yank_buf, start, undo_count);
	std::memmove(start, start + undo_count, maxpos - (start + undo_count));
	filesize -= undo_count;
	maxpos -= undo_count;
	if (start == maxpos && start > mem) {
		start--;
		cur_back();
	}
	setpage(start);
	repaint();
	return 0;
}


/*
 * The :insert, :append and :change command
 */
void
do_ins_chg(PTR start, const char *arg, int mode)
{
	int		base;
	off_t	buffer = BUFFER;
	off_t	count = 0L;
	size_t	len;
	long	val;
	char	*tempbuf = nullptr;
	char	*poi, *epoi;

	if ((mode == U_EDIT) && (current - mem >= filesize)) {
		beep();
		return;
	}
	len = std::strlen(arg);
	if (!std::strncmp("ascii", arg, len) && CMDLNG(5, 1)) {
		base = 1;
	} else if (!std::strncmp("binary", arg, len) && CMDLNG(6, 1)) {
		base = 2;
	} else if (!std::strncmp("octal", arg, len) && CMDLNG(5, 1)) {
		base = 8;
	} else if (!std::strncmp("decimal", arg, len) && CMDLNG(7, 1)) {
		base = 10;
	} else if (!std::strncmp("hexadecimal", arg, len) && CMDLNG(11, 1)) {
		base = 16;
	} else {
		emsg("No such option");
		return;
	}
	addch('\n');
	if (getcmdstr(cmdstr, 0) == 1) {
		repaint();
		return;
	}
	if (alloc_buf(buffer, &tempbuf) == 0L) return;
	while(cmdstr != ".") {
		poi = cmdstr.data();
		if (base == 1) {			/* ASCII */
			while (*poi != '\0') {
				if (*poi == '\\') {
					 switch (*(++poi)) {
		                 case 'n': val = '\n'; break;
		                 case 'r': val = '\r'; break;
		                 case 't': val = '\t'; break;
		                 case '0': val = '\0'; break;
		                 case '\\': val = '\\'; break;
		                 default : val = '\\'; poi--;
	                 }
					 poi++;
				} else {
					val = *poi++;
				}
				*(tempbuf + count++) = val;
			}
		} else {
			while (!cmdstr.empty() && std::isspace(static_cast<unsigned char>(cmdstr.back())))
				cmdstr.pop_back();
			poi = cmdstr.data();
			while (*poi != '\0') {
				val = std::strtol(poi, &epoi, base);
				if (val > 255 || val < 0 || poi == epoi) {
					repaint();
					emsg("Invalid value");
					goto mfree;
				}
				poi = epoi;
				*(tempbuf + count++) = val;
			}
		}
		addch('\n');
		if (getcmdstr(cmdstr, 0) == 1) {
			repaint();
			goto mfree;
		}
	}
	if (count == 0) {
		repaint();
		goto mfree;
	}
	switch (mode) {
	case U_INSERT:
		do_put(start, count, tempbuf);
		break;
	case U_EDIT:
		do_over(start, count, tempbuf);
		break;
	case U_APPEND:
		if ((undo_count = alloc_buf(count, &undo_buf)) == 0L) {
			repaint();
		    goto mfree;
		}
		do_append((off_t)count, tempbuf);
		std::memcpy(undo_buf, tempbuf, count);
		repaint();
		break;
	}
mfree:
	std::free(tempbuf);
}


void
clear_marks()
{
	int n;

	for (n = 0; n < 26; markbuf[n++] = nullptr);
	undo_count = 0;
	last_motion = mem;
}


void
do_mark(int mark, PTR addr)
{
	if (mark < 'a' || mark > 'z' || current >= maxpos) 
		return;
	markbuf[mark - 'a'] = addr;
}


void
movebyte()
{
	emsg("Command disabled@- use ':set memmove' to enable ");
}
