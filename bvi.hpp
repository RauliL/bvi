/*  BVI.HPP
 *
 * 1996-02-28  V 1.0.0
 * 1999-01-21  V 1.1.0
 * 1999-03-17  V 1.1.1
 * 1999-07-01  V 1.2.0 beta
 * 1999-08-21  V 1.2.0 final
 * 2000-05-10  V 1.3.0 alpha
 * 2000-10-24  V 1.3.0 final
 * 2001-10-29  V 1.3.1
 * 2003-07-04  V 1.3.2
 * 2010-08-04  V 1.3.4
 * 2014-10-01  V 1.4.0
 *
 * NOTE: Edit this file with tabstop=4 !
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

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <cctype>
#include <csignal>
#include <sys/stat.h>
#include <csetjmp>
#include <string>
#include <string_view>

#define JOEHTG

#include "patchlevel.h"
#include "config.h"
#if defined(HAVE_NCURSES_H)
#   include <ncurses.h>
#else
#   include <curses.h>
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

/* undo modes */
#define U_EDIT		1		/* undo o r R */
#define U_TRUNC		2		/* undo D */
#define U_INSERT	4		/* undo i */
#define U_DELETE	8		/* undo x */
#define U_BACK		16		/* undo X */
#define U_APPEND	32		/* undo P A */
#define U_TILDE		64		/* ~ */

#define S_GLOBAL	0x100

/* logic modes */
#define LSHIFT  1
#define RSHIFT  2
#define LROTATE 3
#define RROTATE 4
#define AND 5
#define OR  6
#define XOR 7
#define NEG 8
#define NOT 9

#define HEX			0
#define ASCII		1
#define FORWARD		0
#define BACKWARD	1
#define CR			'\r'
#define NL			'\n'
#define BS			8
#define	ESC			27
#define SEARCH		0
#define BVICTRL(n)	(n&0x1f)
#define	ASCII_DEL	0x7f

#define CMDLNG(a,b)     (len <= a && len >= b)


using PTR = char *;
#define DELIM	'/'

#define MAXCMD	255
#define BUFFER	1024

#define SKIP_WHITE  while(*cmd!='\0'&&std::isspace(static_cast<unsigned char>(*cmd)))cmd++;

enum block_datum {
    BLOCK_BEGIN = 1,
    BLOCK_END = 2,
    BLOCK_LEN = 4
};

#ifdef DEBUG
	extern FILE *debug_fp;
#endif

extern	const char	*version;
extern	char	addr_form[];
extern	char	search_pat[];
extern	bool	hl_spat;
extern	char    pattern[];
extern	std::string	rep_buf;
extern	int		maxx, maxy, x, y;
extern	int		filemode, loc;
extern	int		edits;
extern	int		AnzAdd;
extern	int		Anzahl, Anzahl3;
extern	int		addr_flag;
extern	bool	ignore_case, magic;
extern	int		screen, status, statsize;
extern	PTR		mem;
extern	PTR		maxpos;
extern	PTR		pagepos;
extern	PTR		undo_start;
extern	PTR		current_start;
extern  PTR		curpos;
extern	PTR		current;
extern	PTR		start_addr;
extern	PTR		end_addr;
extern	char	*name;
extern	std::string	cmdstr;
extern	off_t	filesize, memsize;
extern	PTR		markbuf[];
extern  PTR		last_motion;
extern	off_t	undo_count;
extern	off_t	yanked;
extern	off_t	undosize;
extern	const char	*copyright, *notfound;
extern	const char	*terminal;
extern	char	*undo_buf;
extern	char	*yank_buf;
extern	int		repl_count;
extern	const char	*shell;
extern	char	*poi;
extern	int		smode;
extern	bool	again;
extern  int     block_flag;
extern  off_t   block_begin, block_end, block_size;

#ifndef S_ISDIR				/* POSIX 1003.1 file type tests. */
#define	S_ISDIR(m)	((m & 0170000) == 0040000)	/* directory */
#define	S_ISCHR(m)	((m & 0170000) == 0020000)	/* char special */
#define	S_ISBLK(m)	((m & 0170000) == 0060000)	/* block special */
#define	S_ISREG(m)	((m & 0170000) == 0100000)	/* regular file */
#define	S_ISFIFO(m)	((m & 0170000) == 0010000)	/* fifo */
#endif

int		addfile(const char *fname);
off_t	alloc_buf(off_t n, char **buffer);
off_t	yd_addr();
off_t	range(int ch);
off_t	calc_size(const char *arg);
void	do_mark(int mark, PTR addr);
void	movebyte();
void	do_back(off_t n, PTR start);
void	do_ins_chg(PTR start, const char *arg, int mode);
void	jmpproc(int sig);
void	printline(PTR mempos, int scpos);
int		chk_comm(int flag);
void	docmdline(const char *cmdline);
void	do_over(PTR loc, off_t n, const char *buf);
void	do_put(PTR loc, off_t n, const char *buf);
bool	doecmd(const char *arg, bool force);
void	do_exit();
void	do_shell();
void	do_undo();
void	do_tilde(off_t count);
void	trunc_cur();
int		do_append(off_t count, const char *buf);
int		do_logic(int mode, const char *str);
int		do_delete(off_t n, PTR start);
int		doset(const char *arg);
int		do_substitution(int delim, const char *line, PTR startpos, PTR endpos);
int		hexchar();
int		outmsg(std::string_view s);
PTR		searching(int ch, const char *line, PTR startpos, PTR endpos, int flag);
PTR		wordsearch(PTR start, char mode);
PTR		backsearch(PTR start, char mode);
PTR		bregexec(PTR start, const char *scan);
PTR		fsearch(PTR start, PTR end, const char *smem);
PTR		fsearch_end(PTR start, PTR end, const char *smem, PTR *s_end);
PTR		rsearch(PTR start, PTR end, const char *smem);
PTR		end_word(PTR start);
PTR		calc_addr(char **pointer, PTR def_addr);
PTR		do_ft(int ch, bool flag);
const char	*patcpy(char *s1, const char *s2, char delim);
void	setpage(PTR addr);
void	msg(std::string_view s);
void	emsg(std::string_view s);
void	smsg(std::string_view s);
void	usage();
void	bvi_init(const char *dir);
void	statpos();
void	setcur();
void	showparms(bool all);
void	toggle();
void	scrolldown(int lns);
void	scrollup(int lns);
void	fileinfo(const char *fname);
void	clearstr();
void	clear_marks();
void	new_screen();
void	repaint();
void	quit();
void	sysemsg(std::string_view s);
void	do_z(int mode);
void	stuffin(std::string_view s);
off_t	edit(int mode);
off_t	load(const char *fname);
int     ascii_comp(char *smem, const char *pattern);
int     hex_comp(char *smem, const char *pattern);
int		cur_forw(bool check);
int		cur_back();
int		lineout();
int		save(const char *fname, const char *start, const char *end, int flags);
int		read_rc(const char *fn);
int		getcmdstr(std::string& p, int x);
int		enlarge(off_t add);
int		vgetc();
int		xpos();
bool	wait_return(bool flag);
