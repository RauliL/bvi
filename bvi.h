/*  BVI.H
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <signal.h>
#include <sys/stat.h>
#include <setjmp.h>

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

#ifndef NULL
#	define NULL		((void *)0)
#endif

#ifndef TRUE
#	define TRUE		1
#	define FALSE	0
#endif

#define PTR		char *
#define DELIM	'/'

#define MAXCMD	255
#define BUFFER	1024

#define SKIP_WHITE  while(*cmd!='\0'&&isspace(*cmd))cmd++;

typedef enum _block_datum {
    BLOCK_BEGIN = 1,
    BLOCK_END = 2,
    BLOCK_LEN = 4
} block_datum;

#ifdef DEBUG
	extern FILE *debug_fp;
#endif

extern	char	*version;
extern	char	addr_form[];
extern	char	search_pat[];
extern	long	hl_spat;
extern	char    pattern[];
extern	char    rep_buf[];
extern	int		maxx, maxy, x, y;
extern	int		filemode, loc;
extern	int		edits, new;
extern	int		AnzAdd;
extern	int		Anzahl, Anzahl3;
extern	int		addr_flag;
extern	int		ignore_case, magic;
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
extern	char	*name, cmdstr[];
extern	off_t	filesize, memsize;
extern	PTR		markbuf[];
extern  PTR		last_motion;
extern	off_t	undo_count;
extern	off_t	yanked;
extern	off_t	undosize;
extern	char	*copyright, *notfound;
extern	char	*terminal;
extern	char	*undo_buf;
extern	char	*yank_buf;
extern	int		repl_count;
extern	char	string[];
extern	char	*shell;
extern	char	*poi;
extern	int		smode;
extern	int		again;
extern  int     block_flag;
extern  off_t   block_begin, block_end, block_size;

#ifndef S_ISDIR				/* POSIX 1003.1 file type tests. */
#define	S_ISDIR(m)	((m & 0170000) == 0040000)	/* directory */
#define	S_ISCHR(m)	((m & 0170000) == 0020000)	/* char special */
#define	S_ISBLK(m)	((m & 0170000) == 0060000)	/* block special */
#define	S_ISREG(m)	((m & 0170000) == 0100000)	/* regular file */
#define	S_ISFIFO(m)	((m & 0170000) == 0010000)	/* fifo */
#endif

	int		addfile();
	off_t	alloc_buf(), yd_addr();
	off_t	range();
	off_t	calc_size();
	void	do_mark(), badcmd(), movebyte();
	void	do_back(), do_ins_chg();
	void	jmpproc(), printline();
	int		chk_comm();
	void	docmdline(), do_over(), do_put();
	int		doecmd();
	void	do_dot(), do_exit(), do_shell(), do_undo();
	void	do_tilde(), trunc_cur();
	int		do_append(), do_logic();
	int		do_delete();
	int		doset();
	int		do_substitution();
	int		hexchar();
	int		outmsg();
	PTR		searching();
	PTR		wordsearch();
	PTR		backsearch();
	/*	int		bregexec();	*/
	PTR		bregexec();
	PTR		fsearch();
	PTR		fsearch_end();
	PTR		rsearch();
	PTR		end_word();
	PTR		calc_addr();
	PTR		do_ft();
	char	*patcpy();
	void	setpage(), msg(), emsg(), smsg();
	void	usage(), bvi_init(), statpos(), setcur();
	void	showparms(), toggle(), scrolldown(), scrollup();
	void	fileinfo();
	void	clearstr(), clear_marks(), new_screen(), repaint();
	void	quit(), sysemsg(), do_z(), stuffin();
	off_t	edit(), load();
	int     ascii_comp(), hex_comp();
	int		cur_forw(), cur_back();
	int		lineout(), save(), at_least(), read_rc();
	int		getcmdstr(), enlarge();
	int		vgetc(), xpos();
	int		wait_return();
