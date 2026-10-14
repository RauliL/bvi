/* bm_unix.c - Unix specific subroutines for BMORE
 *
 * 2000-05-31 V 1.3.0 beta
 * 2000-10-12 V 1.3.0 final
 * 2002-02-10 V 1.3.1
 * 2003-07-04 V 1.3.2
 * 2010-06-02 V 1.3.4
 * 2013-08-22 V 1.4.0
 *
 * NOTE: Edit this file with tabstop=4 !
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


#include "bmore.hpp"
#include <termios.h>

#include <format>
#include <string>

#define TBUFSIZ 1024

#define stty(fd,argp)   tcsetattr(fd,TCSAFLUSH,argp)

struct termios ostate;


char	*rev_start, *rev_end;	/* enter and exit standout mode */
char	*Home;			/* go to home */
char	*clear_sc;		/* clear screen */
char	*erase_ln;		/* erase line */

extern	off_t	bytepos, screen_home;
extern	FILE	*curr_file;

bool    no_intty, no_tty;

int
putchr(int ch)
{return std::putchar(ch);}


void
initterm()
{
	char        buf[TBUFSIZ];
	static char clearbuf[TBUFSIZ];
	char        *term;
	char        *clearptr;

	struct  termios nstate;

	no_tty = tcgetattr(fileno(stdout), &ostate);
	if (!no_tty) {
		nstate = ostate;
		/*
		 * is this really necessary??
		 *
		nstate.c_lflag &= ~(ICANON|ECHO|ECHOE|ECHONL);
		 */
		nstate.c_lflag &= ~(ICANON|ECHO|ECHOE|ECHONL);
		nstate.c_cc[VMIN] = 1;
		nstate.c_cc[VTIME] = 0;
		tcsetattr(fileno(stdin), TCSADRAIN, &nstate);
	}

	if ((term = std::getenv("TERM")) == 0 || tgetent(buf, term) <= 0) {
		std::printf("Dumb terminal\n");
		maxx = 80;
		maxy = 24;
	} else {
		maxy = tgetnum("li");
		maxx = tgetnum("co");
	}
	clearptr = clearbuf;
	erase_ln = tgetstr("ce", &clearptr);
	clear_sc = tgetstr("cl", &clearptr);
	rev_start = tgetstr("so", &clearptr);
	rev_end = tgetstr("se", &clearptr);

	no_intty = tcgetattr(fileno(stdin), &ostate);
	tcgetattr(fileno(stderr), &ostate);

	if (!no_tty) {
		ostate.c_lflag &= ~(ICANON|ECHO);
	}
}


void
set_tty()
{
	if (no_tty) return;
	ostate.c_lflag &= ~(ICANON|ECHO);
	stty(fileno(stderr), &ostate);
}


void
reset_tty()
{
	if (no_tty) return;
	ostate.c_lflag |= ICANON|ECHO;
	stty(fileno(stderr), &ostate);
}


void
sig(int sig)
{
	reset_tty();
	std::printf("\r\n");
	std::exit(0);
}


/*
 * doshell() - run a command or an interactive shell
 */
void
doshell(const char *cmd)
{
	std::string shell;
	std::string	cline;

	std::printf("\n");

	if (const auto shell_env_var = std::getenv("SHELL")) {
		std::string::size_type pos;

		shell = shell_env_var;
		pos = shell.rfind(std::filesystem::path::preferred_separator);
		if (pos != std::string::npos) {
			shell = shell.substr(pos + 1);
		}
	} else {
		shell = "sh";
	}

	if (cmd[0] == '\0') {
		cline = std::format("{} -i", shell);
	} else {
		cline = std::format("{} -c \"{}\"", shell, cmd);
	}

	reset_tty();
	std::system(cline.c_str());
	set_tty();
	std::printf("\r");
	home();
	fseeko(curr_file, screen_home, SEEK_SET);
	bytepos = screen_home;
}


void
highlight()
{
	if (rev_start && rev_end)
		tputs(rev_start, 1, putchr);
}


void
normal()
{
	if (rev_start && rev_end)
		tputs(rev_end, 1, putchr);
}


void
clearscreen()
{
	tputs(clear_sc, 1, putchr);
}


void
home()
{
	tputs(Home, 1, putchr);
}


/* force clear to end of line */
void
cleartoeol()
{
	tputs(erase_ln, 1, putchr);
}


int
vgetc()
{
    char cha;
    extern int errno;

    errno = 0;
    if (read(2, &cha, 1) <= 0) {
        if (errno != EINTR)
            std::exit(2);
    }
    return (cha);
}
