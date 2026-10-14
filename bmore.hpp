/*  BMORE.HPP
 *
 * 1996-02-28  V 1.0.0
 * 1999-01-21  V 1.1.0
 * 1999-03-17  V 1.1.1
 * 1999-07-01  V 1.2.0 beta
 * 1999-08-21  V 1.2.0 final
 * 2000-05-31  V 1.3.0 beta
 * 2000-10-04  V 1.3.0 final
 * 2002-01-16  V 1.3.1
 * 2003-02-20  V 1.3.2
 * 2010-03-28  V 1.3.4
 *
 *  NOTE: Edit this file with tabstop=4 !
 *
 * Copyright 1996-2010 by Gerhard Buergmann
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

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <cctype>
#include <csignal>
#include <sys/stat.h>
#include <csetjmp>
#include <filesystem>
#include <string_view>

#include "version.hpp"
#include "config.hpp"

#include <unistd.h>
#include <term.h>

#if defined(HAVE_NCURSES_H)
#  include <ncurses.h>
#else
#  include <curses.h>
#endif

/* defines for filemode */
#define	ERROR				-1
#define REGULAR				0
#define NEW					1
#define DIRECTORY			2
#define CHARACTER_SPECIAL	3
#define BLOCK_SPECIAL		4
#define PARTIAL             5

/* regular expressions */
#define END     0
#define ONE     1
#define STAR    2

#define ASCII		1
#define FORWARD		0
#define BACKWARD	1
#define CR			'\r'
#define NL			'\n'
#define BS			8
#define	ESC			27
#define SEARCH		0
#define REPLACE		1
#define BVICTRL(n)		(n&0x1f)


using PTR = char *;
#define DELIM	'/'

#define MAXCMD	255
#define BUFFER	1024


#ifdef DEBUG
	extern FILE *debug_fp;
#endif

extern	const char	*version;
extern	int		maxx, maxy;
extern	bool	ignore_case, magic;
extern	bool	no_tty, no_intty;


void	initterm();
void	set_tty();
void	reset_tty();
void	cleartoeol();
void	clearscreen();
void	highlight();
void	normal();
void	bmbeep();
void	home();
void	sig(int sig);
void	doshell(const char *cmd);
void	emsg(std::string_view s);
void	do_next(int n);
void	bmsearch(int ch);
void	pushback(int n, const char *where);
int		open_file(const std::filesystem::path& name);
int		printout(int lns);
int		rdline(int ch, char *sstring);
int		nextchar();
int		vgetc();
int     sbracket(int start, const char *scan, int count);
int     bmregexec(const char *scan);
int		ascii_comp(char *smem, const char *pattern);
int		hex_comp(char *smem, const char *pattern);
void    putline(const char *buf, int num);
