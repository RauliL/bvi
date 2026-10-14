/* BMORE - binary more
 *
 * 1990-01-31  V 1.0.0
 * 1990-09-04  V 1.1.0
 * 2000-05-31  V 1.3.0 beta
 * 2000-10-18  V 1.3.0 final
 * 2002-01-16  V 1.3.1
 * 2004-01-09  V 1.3.2
 * 2013-08-23  V 1.4.0
 *
 * NOTE: Edit this file with tabstop=4 !
 *
 * Copyright 1990-2013 by Gerhard Buergmann
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


#include <sys/types.h>
#include <clocale>

#include "bmore.hpp"

#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <system_error>

namespace fs = std::filesystem;

const char	*copyright  = "Copyright (C) 1990-2013 by Gerhard Buergmann";

int		maxx, maxy;
int		mymaxx = 0, mymaxy = 0;
fs::path	name;
char	sstring[MAXCMD] = "";	/* string for search */
char	estring[MAXCMD] = "";	/* string for shell escape */
char	string[MAXCMD];
FILE	*curr_file = nullptr, *help_file;
int		AnzAdd;
long	precount = -1;	/* number preceding command */

char	**files;		/* list of input files */
int		numfiles;		/* number of input files */
int		file_nr = 0;	/* number of current input file */

std::string	numarr;		/* string for collecting number */
char	addr_form[15];

bool		ascii_flag = false;
bool		dup_print_flag = false;
bool		c_flag = false, d_flag = false;
int		exval = 0;
int		init_search = 0;
char	buffer1[MAXCMD], buffer2[MAXCMD];
int		out_len;
int		corr = 0, to_print;
bool		do_header = false;
off_t	init_byte = 0;
off_t	last_search = 0;
off_t	screen_home, filesize;
off_t	bytepos, oldpos;
int		prompt = 1;

static	char	progname[10];
static	char	cmdbuf[MAXCMD];
static	int		cnt = 0;
static	int		icnt = 0;
static	int		smode;

/* char	search_pat[BUFFER];	*/  /* / or ? command */
char	bmore_search_pat[BUFFER];   /* / or ? command */
const char	*emptyclass = "Empty byte class '[]' or '[^]'";

static const char* help =
"Most commands optionally preceded by integer argument k.  Defaults in brackets.\n"
"Star (*) indicates argument becomes new default.\n"
"-------------------------------------------------------------------------------\n"
"<space>                 Display next k lines of bytes [current screen size]\n"
"z                       Display next k lines of bytes [current screen size]*\n"
"<return>                Display next k lines of bytes [1]*\n"
"d or ctrl-D             Scroll k lines [current scroll size, initially 11]*\n"
"q or Q or <interrupt>   Exit from bmore\n"
"s                       Skip forward k lines of bytes [1]\n"
"f                       Skip forward k screenfuls of bytes [1]\n"
"b or ctrl-B             Skip backwards k screenfuls of bytes [1]\n"
"'                       Go to place where previous search started\n"
"=                       Display current byte number\n"
"/<regular expression>   Search for kth occurrence of ascii regular expr [1]\n"
"\\<regular expression>   Search for kth occurrence of binary regular expr [1]\n"
"n                       Search for kth occurrence of last r.e [1]\n"
"!<cmd> or :!<cmd>       Execute <cmd> in a subshell\n"
"v                       Start up bvi at current position\n"
"w                       Start up bvi reading only a screenful of bytes [screen]\n"
"ctrl-L                  Redraw screen\n"
":n                      Go to kth next file [1]\n"
":p                      Go to kth previous file [1]\n"
":f                      Display current file name and byte position\n"
".                       Repeat previous command\n"
"-------------------------------------------------------------------------------\n";

static void display_help();


/* -a   ASCII mode
 * -d   beginners mode
 * -c	clear before displaying
 * -i   ignore case
 * -n	number of lines/screen
 * -w   width of screen
 */
void
usage()
{
	std::fprintf(stderr, "Usage: %s [-acdi] [-lines] [+linenum | +/pattern] name1 name2 ...\n", progname);
	std::exit(1);
}


int
main(int argc, char *argv[])
{
	int		ch;
	int		colon = 0, last_ch = 0;
	long	last_pre = 0;
	int		repeat;
	long	count;
	int		i, n = 1;
	int		d_line, r_line, z_line;
	char	*poi;


	std::setlocale(LC_ALL, "");

	poi = std::strrchr(argv[0], DELIM);

	if (poi) std::strncpy(progname, ++poi, 9);
	else std::strncpy(progname, argv[0], 9);
	std::strtok(progname, ".");

	while (n < argc) {
		switch (argv[n][0]) {
		case '-':
			if (argv[n][1] >= '0' && argv[n][1] <= '9') {
				std::sscanf(&argv[n][1], "%dx%d", &mymaxy, &mymaxx);
			} else if (argv[n][1] == 'n') {
				if (argv[n+1] == nullptr || argv[n+1][0] == '-') {
					usage();
				} else {
					std::sscanf(&argv[++n][0], "%d", &mymaxy);
				}
			} else if (argv[n][1] == 'w') {
				if (argv[n+1] == nullptr || argv[n+1][0] == '-') {
					usage();
				} else {
					std::sscanf(&argv[++n][0], "%d", &mymaxx);
				}
			} else {
				i = 1;
				while (argv[n][i] != '\0') {
					switch (argv[n][i]) {
					case 'a':	ascii_flag = true;
								break;
					case 'c':	c_flag = true;
								break;
					case 'd':	d_flag = true;
								break;
					case 'i':	ignore_case = true;
								break;
					default:
								usage();
					}
					i++;
				}
			}
			n++;
			break;
		case '+':			/* +cmd */
			if (argv[n][1] == '/' || argv[n][1] == '\\') {
				init_search = argv[n][1];
				std::strcpy(sstring, &argv[n][2]);
			} else {
				init_byte = std::strtoll(argv[n] + 1, nullptr, 0);
			}
			n++;
			break;
		default:			/* must be a file name */
			name = argv[n];
			files = &(argv[n]);
			numfiles = argc - n;
			n = argc;
			break;
		}
	}
	initterm();
	set_tty();
	maxy -= 2;
	if (mymaxy) {
		maxy = mymaxy;
	}
	z_line = maxy;
	d_line = maxy / 2;
	r_line = 1;

	if (numfiles == 0) {
		curr_file = stdin;
		if (isatty(fileno(stdin)) != 0) {
			reset_tty();
			usage();
		}
	} else {
		file_nr = 1;
		while (open_file(name)) {
			do_next(1);
		}
		if (exval) {
		} else {
			fseeko(curr_file, init_byte, SEEK_SET);
			bytepos += init_byte;
		}
	}
	screen_home = bytepos;

	AnzAdd = 10;
	std::strcpy(addr_form,  "%08lX  ");

	if (ascii_flag)
		out_len = ((maxx - AnzAdd - 1) / 4) * 4;
	else
		out_len = ((maxx - AnzAdd - 1) / 16) * 4;
	if (mymaxx) {
		out_len = mymaxx;
	}

	if (init_search)
		bmsearch(init_search);

	if (no_tty) {
		int	fileloop;

		for (fileloop = 0; fileloop < numfiles; fileloop++) {
			while(!printout(1));
			do_next(1);
			open_file(name);
		}
		if (curr_file) std::fclose(curr_file);
		reset_tty();
		std::exit(exval);
	}
	if (!exval) {
		if (printout(maxy)) {
			do_next(1);
		}
	}
	std::signal(SIGINT, sig);
	std::signal(SIGQUIT, sig);
	/* main loop */
	do {
		to_print = 0;
		dup_print_flag = false;
		if (prompt) {
			if (prompt == 2) {
				while (open_file(name)) {
					do_next(1);
				}
			}
			highlight();
			std::printf("--More--");
			if (prompt == 2) {
				std::printf("(Next file: %s)", name.c_str());
			} else if (!no_intty && filesize) {
				std::printf("(%d%%)", (int)((bytepos * 100) / filesize));
			}

			if (d_flag) std::printf("[Press space to continue, 'q' to quit]");
			normal();
			std::fflush(stdout);
		}
		ch = vgetc();
		/*
		if (prompt == 2) {
			open_file(name);
		}
		*/
		prompt = 1;
		std::printf("\r");
		numarr.clear();
		while (ch >= '0' && ch <= '9') {
			numarr.push_back(static_cast<char>(ch));
			ch = vgetc();
		}
		if (!numarr.empty()) precount = std::strtol(numarr.c_str(), nullptr, 10);
			else precount = -1;
		numarr.clear();

		if (ch == '.') {
			precount = last_pre;
			ch = last_ch;
			repeat = 1;
		} else {
			last_pre = precount;
			last_ch = ch;
			if (ch == ':') colon = vgetc();
			repeat = 0;
		}

		switch (ch) {
		case ' ':	/*  Display next k lines of text [current screen size] */
					dup_print_flag = true;
					if (precount > 0) to_print = precount;
						else to_print = maxy;
					break;
		case 'z':	/* Display next k lines of bytes [current screen size]* */
					dup_print_flag = true;
					if (precount > 0) z_line = precount;
					to_print = z_line;
					break;
		case '\r':
		case '\n':	/* Display next k lines of text [current screen size]* */
					dup_print_flag = true;
					if (precount > 0) r_line = precount;
					to_print = r_line;
					break;
		case 'q':
		case 'Q':
					cleartoeol();
					std::fclose(curr_file);
					reset_tty();
					std::exit(exval);
		case ':' :
					switch (colon) {
					case 'f':
						prompt = 0;
						if (!no_intty)
							std::printf("\"%s\" line %lu", name.c_str(),
								(unsigned long)(bytepos - out_len));
						else
							std::printf("[Not a file] line %lu",
								(unsigned long)(bytepos - out_len));
						std::fflush(stdout);
						break;
					case 'n':
						if (precount < 1) precount = 1;
						do_next(precount);
						std::printf("\r");
						cleartoeol();
						std::printf("\n...Skipping to file %s\r\n\r\n", name.c_str());
						prompt = 2;
						break;
					case 'p':
						if (precount < 1) precount = 1;
						do_next(-precount);
						std::printf("\r");
						cleartoeol();
						std::printf("\n...Skipping back to file %s\r\n\r\n", name.c_str());
						prompt = 2;
						break;
					case 'q':
						cleartoeol();
						std::fclose(curr_file);
						reset_tty();
						std::exit(exval);
						break;
					case '!':
						if (!no_intty) {
							cleartoeol();
							if (rdline(colon, estring)) break;
							doshell(estring);
							std::printf("------------------------\r\n");
							break;
						}
					default:
						bmbeep();
					}
					break;
		case '!':
			if (!no_intty) {
				cleartoeol();
				if (rdline(ch, estring)) break;
				doshell(estring);
				std::printf("------------------------\r\n");
				break;
			}
		case 'd':	/* Scroll k lines [current scroll size, initially 11]* */
		case BVICTRL('D'):
					if (precount > 0) d_line = precount;
					to_print = d_line;
					break;
		case BVICTRL('L'):   	/*** REDRAW SCREEN ***/
					if (no_intty) {
						bmbeep();
					} else {
						clearscreen();
						to_print = maxy + 1;
						fseeko(curr_file, screen_home, SEEK_SET);
						bytepos = screen_home;
					}
					break;
		case 'b':		/* Skip backwards k screenfuls of text [1] */
		case BVICTRL('B'):
					if (no_intty) {
						bmbeep();
					} else {
						if (precount < 1) precount = 1;
						std::printf("...back %ld page", precount);
						if (precount > 1) {
							std::printf("s\r\n");
						} else {
							std::printf("\r\n");
						}
						screen_home -= (maxy + 1) * out_len;
						if (screen_home < 0) screen_home = 0;
						fseeko(curr_file, screen_home, SEEK_SET);
						bytepos = screen_home;
						to_print = maxy + 1;
					}
					break;
		case 'f':		/* Skip forward k screenfuls of bytes [1] */
		case 's':		/* Skip forward k lines of bytes [1] */
					if (precount < 1) precount = 1;
					if (ch == 'f') {
						count = maxy * precount;
					} else {
						count = precount;
					}
					std::putchar('\r');
					cleartoeol();
					std::printf("\n...skipping %ld line", count);
					if (count > 1) {
						std::printf("s\r\n\r\n");
					} else {
						std::printf("\r\n\r\n");
					}
					screen_home += (count + maxy) * out_len;
					fseeko(curr_file, screen_home, SEEK_SET);
					bytepos = screen_home;
					to_print = maxy;
					break;
		case '\\':
					if (ascii_flag) {
						bmbeep();
						break;
					}
		case '/':	/**** Search String ****/
					if (!repeat) {
						cleartoeol();
						if (rdline(ch, sstring)) break;
					}
		case 'n': 		/**** Search Next ****/
		case 'N':
					bmsearch(ch);
					/*
					to_print--;
					*/
					break;
		case '\'':
					if (no_intty) {
						bmbeep();
					} else {
						bytepos = last_search;
						fseeko(curr_file, bytepos, SEEK_SET);
						screen_home = bytepos;
						to_print = maxy;
						std::printf("\r");
						cleartoeol();
						std::printf("\n\r\n***Back***\r\n\r\n");
					}
					break;
		case '=':
					prompt = 0;
					cleartoeol();
					std::printf("%lX hex  %lu dec", (unsigned long)bytepos,
						(unsigned long)bytepos);
					std::fflush(stdout);
					break;
		case '?':
		case 'h':
                    display_help();
					break;
		case 'w':
		case 'v':
					if (!no_intty) {
						cleartoeol();
						std::string cmd;
						if (ch == 'v') {
							cmd = std::format("bvi +{} {}",
								(unsigned long)(screen_home +
								(maxy + 1) / 2 * out_len), name.string());
						} else {
							if (precount < 1) precount = bytepos - screen_home;
							cmd = std::format("bvi -b {} -s {} {}",
								(unsigned long)screen_home,
								(unsigned long)precount, name.string());
						}
						doshell(cmd.c_str());
						to_print = maxy + 1;
						break;
					}
		default :
					if (d_flag) {
						emsg("[Press 'h' for instructions.]");
					} else {
						bmbeep();
					}
					break;
		}
		if (to_print) {
			if (printout(to_print)) {
				do_next(1);
			}
		}
	} while (1);
}


int
rdline(int ch, char *sstring)
{
	int		i = 0;
	int		ch1 = 0;
	char	bstring[MAXCMD];

	if (ch == '!') {
		std::strcpy(bstring, sstring);
		sstring[0] = '\0';
	}
	std::putchar(ch);
	std::fflush(stdout);

	while (i < MAXCMD) {
		ch1 = vgetc();
		if (ch1 == '\n' || ch1 == '\r' || ch1 == ESC) {
			break;
		} else if (ch1 == 8) {
			if (i) {
				sstring[--i] = '\0';
				std::printf("\r%c%s", ch, sstring);
				cleartoeol();
			} else {
				ch1 = ESC;
				break;
			}
		} else if (ch1 == '!' && i == 0) {
			if (bstring[0] == '\0') {
				emsg("No previous command");
				return 1;
			}
			std::putchar(ch1);
			std::printf("\r%c%s", ch, bstring);
			std::strcat(sstring, bstring);
			i = std::strlen(sstring);
		} else {
			std::putchar(ch1);
			sstring[i++] = ch1;
		}
		std::fflush(stdout);
	}
	if (ch1 == ESC) {
		std::putchar('\r');
		cleartoeol();
		return 1;
	}
	if (i) sstring[i] = '\0';
	return 0;
}


void
do_next(int n)
{
	if (numfiles) {
		if (n == 1 && file_nr == numfiles) {
			if (curr_file) std::fclose(curr_file);
			reset_tty();
			std::exit(exval);
		}
		if ((file_nr + n) > numfiles)
			file_nr = numfiles;
		else if ((file_nr + n) < 1)
			file_nr = 1;
		else
			file_nr += n;
		prompt = 2;
		name = *(files + file_nr - 1);
	} else {
		if (curr_file) std::fclose(curr_file);
		reset_tty();
		std::exit(exval);
	}
}


int
open_file(const fs::path& name)
{
	std::error_code ec;
	const auto sz = fs::file_size(name, ec);
	if (!ec) {
		filesize = static_cast<off_t>(sz);
	}
	if (curr_file) std::fclose(curr_file);
	if (numfiles > 1) do_header = true;
	if ((curr_file = std::fopen(name.c_str(), "rb")) == nullptr) {
		std::perror(name.c_str());
		exval = 1;
		return 1;
	}
	bytepos = screen_home = 0;
	return 0;
}


void
putline(const char *buf, int num)
{
	int			print_pos;
	unsigned	char	ch;

	std::printf(addr_form, (unsigned long)bytepos);
	for (print_pos = 0; print_pos < num; print_pos++) {
		ch = buf[print_pos];
		if (!ascii_flag) {
		    std::printf("%02X ", ch);
		}
		++bytepos;
		if ((ch > 31) && (ch < 127))
			*(string + print_pos) = ch;
		else
			*(string + print_pos) = '.';
	}
	for (; print_pos < out_len; print_pos++) {
		if (!ascii_flag) {
		    std::printf("   ");
		}
		++bytepos;
		*(string + print_pos) = ' ';
	}
	*(string + num) = '\0';
	if (no_tty) std::printf("%s\n", string);
	else std::printf("%s\r\n", string);
}


int
printout(int lns)
{
	int			c, num;
	int			doub = 0;
	static		bool		flag = false;

	if (c_flag) {
		clearscreen();
	}
	if (do_header) {
		if (no_tty) {
			std::printf("::::::::::::::\n%s\n::::::::::::::\n", name.c_str());
		} else {
			std::printf("\r");
			cleartoeol();
			std::printf("::::::::::::::\r\n%s\r\n::::::::::::::\r\n", name.c_str());
		}
		do_header = false;
		corr = 2;
	}
	if (corr && (lns > maxy - 2)) lns -= corr;
	corr = 0;
	do {
		for (num = 0; num < out_len; num++) {
			if ((c = nextchar()) == -1) break;
			buffer1[num] = c;
		}
		if (!num) return 1;
		if (std::memcmp(buffer1, buffer2, num) || !bytepos || !dup_print_flag) {
			std::memcpy(buffer2, buffer1, num);
			putline(buffer2, num);
			if (!no_tty) flag = true;
			lns--;
		} else {
			if (flag) {
				cleartoeol();
				std::printf("*\r\n");
				lns--;
			} else {
				doub++;
			}
			flag = false;
			bytepos += num;
		}
		if (lns == 0) {
			screen_home = bytepos - ((maxy + 1 + doub) * out_len);
			if (screen_home < 0) screen_home = 0;
			return 0;
		}
		dup_print_flag = true;
	} while(num);
	return 1;
}


int
nextchar()
{
	if (cnt == 0) return std::fgetc(curr_file);
	cnt--;
	return cmdbuf[icnt++] & 0xff;
}


void
pushback(int n, const char *where)
{
	if (cnt) std::memmove(cmdbuf + n, cmdbuf, n);
	std::memcpy(cmdbuf, where, n);
	icnt = 0;
	cnt += n;
}




/* Return:
 * -1	EOF
 *  0	not found at current position
 *  1   found
 */
int
bmregexec(const char *scan)
{
	char	*act;
	int		count, test;
	int		l;
	char	act_pat[MAXCMD];  /* found pattern */

	act = act_pat;
	l = 0;
	while (*scan != 0) {
		if ((test = nextchar()) == -1) return -1;
		*act++ = test;
		if (++l == MAXCMD) {
			pushback(l, act_pat);
			return 0;
		}
		if (ignore_case && smode == ASCII)	test = std::toupper(test);
		switch (*scan++) {
		case ONE:	/* exactly one character */
				count = *scan++;
				if (count == 1) {
					if (test != *scan) {
						bytepos++;
						if (l > 1) pushback(--l, act_pat + 1);
						return 0;
					}
					scan++;
				} else if (count > 1) {
					if (sbracket(test, scan, count)) {
						bytepos++;
						if (l > 1) pushback(--l, act_pat + 1);
						return 0;
					}
					scan += count;
				}
				break;
		case STAR:  /* zero or more characters */
				count = *scan++;
				if (count == 1) {	/* only one character, 0 - n times */
					while (test == *scan) {
						if ((test = nextchar()) == -1) return -2;
						*act++ = test;
						if (++l == MAXCMD) {
							pushback(l, act_pat);
							return 0;
						}
						if (ignore_case && smode == ASCII)
								test = std::toupper(test);
					}
					pushback(1, --act);
					l--;
					scan++;
				} else if (count > 1) {	/* characters in bracket */
					if (*scan == '^') {
						do {
/* If we found something matching the next part of the expression, we
 * abandon the search for not-matching characters. */
							if (bmregexec(scan + count)) {
								*act++ = test;	/* May be wrong case !! */
								l++;
								scan += count;
								bytepos--;
								break;
							}
							if (sbracket(test, scan, count)) {
								bytepos++;
								if (l > 1) pushback(--l, act_pat + 1);
								return 0;
							} else {
								if ((test = nextchar()) == -1) return -3;
								*act++ = test;
								if (++l == MAXCMD) {
									pushback(l, act_pat);
									return 0;
								}
								if (ignore_case && smode == ASCII)
										test = std::toupper(test);
							}
						} while(1);
					} else {
						while(!sbracket(test, scan, count)) {
							if ((test = nextchar()) == -1) return -4;
							*act++ = test;
							if (++l == MAXCMD) {
								pushback(l, act_pat);
								return 0;
							}
							if (ignore_case && smode == ASCII)
									test = std::toupper(test);
						}
						scan += count;
						pushback(1, --act);
						l--;
					}
				} else {	 /* ".*"  */
					do {
						if ((test = nextchar()) == -1) return -5;
						*act++ = test;
						if (++l == MAXCMD) {
							pushback(l, act_pat);
							return 0;
						}
						pushback(1, act - 1);
						bytepos--;
					} while (bmregexec(scan) == 0);
					bytepos++;
					act--;
					l--;
				}
				break;
		}
	}
	pushback(l, act_pat);
	return 1;	/* found */
}


int
sbracket(int start, const char *scan, int count)
{
	if (*scan++ == '^') {
		if (!std::memchr(scan, start, --count)) return 0;
	} else {
		if (std::memchr(scan, start, --count)) return 0;
	}
	return 1;
}


void
bmsearch(int ch)
{
	int	i;

	if (sstring[0] == '\0') {
		emsg("No previous regular expression");
		return;
	}
	if (ch == '/') {
		/* if (ascii_comp(search_pat, sstring)) return; */
		if (ascii_comp(bmore_search_pat, sstring)) return;
	}
	if (ch == '\\') {
		/* if (hex_comp(search_pat, sstring)) return; */
		if (hex_comp(bmore_search_pat, sstring)) return;
	}
	oldpos = bytepos;
	last_search = screen_home;
	if (precount < 1) precount = 1;
	while (precount--) {
		/* while ((i = bmregexec(search_pat)) == 0); */
		while ((i = bmregexec(bmore_search_pat)) == 0);
		if (i == 1) {
			screen_home = bytepos;
			to_print = maxy;
		} else {		/* i == -1 -> EOF */
			if (no_intty) {
				std::printf("\r\nPattern not found\r\n");
				do_next(1);
			} else {
/*
sprintf(string, "Pattern not found %d - %ul", i, (unsigned long)bytepos);
emsg(string);
*/
				emsg("Pattern not found");
				bytepos = oldpos;
				fseeko(curr_file, bytepos, SEEK_SET);
				break;
			}
		}
		if (precount) {
			nextchar();
			bytepos++;
		}
	}
	if (prompt) {
		std::printf("\r\n...skipping\r\n");
	}
}


void
emsg(std::string_view s)
{
	std::putchar('\r');
	cleartoeol();
	highlight();
	std::printf("%.*s", static_cast<int>(s.size()), s.data());
	normal();
	std::fflush(stdout);
	prompt = 0;
}


void
bmbeep() {
	std::putchar(7);
}

static void
display_help()
{
    const char* ptr;

    for (ptr = help; *ptr; ++ptr)
    {
        std::putchar(*ptr);
    }
    to_print = 0;
}
