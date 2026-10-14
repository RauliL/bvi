/* io.c - file in/out and alloc subroutines for BVI
 *
 * 1996-02-28 V 1.0.0
 * 1999-01-20 V 1.1.0
 * 1999-04-27 V 1.1.1
 * 1999-07-02 V 1.2.0 beta
 * 1999-10-15 V 1.2.0 final
 * 2000-03-23 V 1.3.0 beta
 * 2000-08-17 V 1.3.0 final
 * 2004-01-04 V 1.3.2
 * 2010-06-02 V 1.3.4
 * 2014-05-03 V 1.4.0
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

#include "bvi.hpp"
#include "set.hpp"

#include <format>
#include <string>

#if 0
/* nowadays (2009) we should make sure that bvi is compiled with LFS support: */
/* defines _LARGEFILE_SOURCE _FILE_OFFSET_BITS=64 */
/* if compiled without LFS support, stat() open() lseek() will fail with */
/* EOVERFLOW(75)   Value too large for defined data type  */
/* see README for configure arguments to enable lfs support in 32-bit executables */
/* cygwin enables lfs by default */
#endif

#include <limits.h>
#	ifndef OFF_T_MAX
#  		define OFF_T_MAX    ULONG_LONG_MAX
#	endif

#ifdef HAVE_UNISTD_H
#	include <unistd.h>
#endif

#ifdef HAVE_FCNTL_H
#	include <fcntl.h>
#endif

int		filemode;
static	struct	stat	buf;
static	off_t	block_read;
const char	*terminal;


/*********** Save the patched file ********************/
int
save(const char *fname, const char *start, const char *end, int flags)
{
	int		fd;
	std::string	status;
	const char	*newstr;
	off_t	filesize;

	if (!fname) {
		emsg("No file|No current filename");
		return 0;
	}
	if (stat(fname, &buf) == -1) {
		newstr = "[New file] ";
	} else {
		if (S_ISDIR(buf.st_mode)) {
			msg(std::format("{}{}{} Is a directory", '"', fname, '"').c_str());
			return 0;
		} else if (S_ISCHR(buf.st_mode)) {
			msg(std::format("{}{}{} Character special file", '"', fname, '"').c_str());
			return 0;
		} else if (S_ISBLK(buf.st_mode)) {
			/*
			msg(... Block special file ...);
			return 0;
			*/
		}
		newstr = "";
	}

	if (filemode == PARTIAL) flags = O_RDWR;
	if ((fd = open(fname, flags, 0666)) < 0) {
        sysemsg(fname);
		return 0;
	}
	if (filemode == PARTIAL) {
		if (block_read) {
			filesize = block_read;
			status = std::format("{}{}{} range {}-{}", '"', fname, '"',
				block_begin, block_begin - 1 + filesize);
			if (lseek(fd, block_begin, SEEK_SET) < 0) {
				sysemsg(fname);
				return 0;
			}
		} else {
			msg("Null range");
			return 0;
        }
	} else {
		filesize = end - start + 1L;
		status = std::format("{}{}{} {}{} bytes", '"', fname, '"', newstr, filesize);
	}

	if (write(fd, start, filesize) != filesize) {
		sysemsg(fname);
		close(fd);
		return 0;
	}
	close(fd);
	edits = 0;
	msg(status.c_str());
	return 1;
}


/* loads a file, returns the filesize */
off_t
load(const char *fname)
{
	int		fd = -1;
	std::string	status;

	buf.st_size = 0L;
	if (fname != nullptr) {
		/*
		sprintf(string, "\"%s\"", fname);
		msg(string);
		refresh();
		*/
		if (stat(fname, &buf) == -1) {
		/* check for EOVERFLOW       75              */
		/* Value too large for defined data type     */
		/* means bvi is compiled without lfs support */
			if (errno == ENOENT) {
				filemode = NEW;
			} else {
				move(maxy, 0);
				endwin();
				std::perror(fname);
				std::exit(0);
			}
		/*
		} else if (S_ISDIR(buf.st_mode)) {
			filemode = DIRECTORY;
		*/
		} else if (S_ISCHR(buf.st_mode)) {
			filemode = CHARACTER_SPECIAL;
		} else if (S_ISBLK(buf.st_mode)) {
			filemode = BLOCK_SPECIAL;
			if (!block_flag) {
				block_flag = 1;
				block_begin = 0;
				block_size = 1024;
				block_end = block_begin + block_size - 1;
			}
			if ((fd = open(fname, O_RDONLY)) > 0) {
				B(P_RO) = true;
				params[P_RO].changed = true;
			} else {
				sysemsg(fname);
				filemode = ERROR;
			}
		} else if (S_ISREG(buf.st_mode) || S_ISDIR(buf.st_mode)) {
#if 0
           /* stat() call above will fail if file is too large */
           /* this size check will never fail                  */
           if ((unsigned long long)buf.st_size > (unsigned long long)OFF_T_MAX) {
				move(maxy, 0);
				endwin();
				std::printf("File too large\n");
				std::exit(0);
			}
#endif
			if ((fd = open(fname, O_RDONLY)) > 0) {
				if (S_ISREG(buf.st_mode)) {
					filemode = REGULAR;
				} else {
					filemode = DIRECTORY;
				}
				if (access(fname, W_OK)) {
					B(P_RO) = true;
					params[P_RO].changed = true;
				}
			} else {
	            sysemsg(fname);
				filemode = ERROR;
			}
		}
	} else {
		filemode = NEW;
	}
	if (mem != nullptr) std::free(mem);
	memsize = 1024;
	if (block_flag) {
		if (block_flag == BLOCK_BEGIN) {
			block_size = buf.st_size - block_begin;
		}
		memsize += block_size;
	} else if (filemode == REGULAR) {
		memsize += buf.st_size;
	}
	if ((mem = static_cast<char*>(std::malloc(memsize))) == nullptr) {
		move(maxy, 0);
		endwin();
		std::printf("Out of memory\n");
		std::exit(0);
	}
	clear_marks();

	if (block_flag && ((filemode == REGULAR) || (filemode == BLOCK_SPECIAL))) {
		if (lseek(fd, block_begin, SEEK_SET) < 0) {
			sysemsg(fname);
			filemode = ERROR;
		} else {
			if ((filesize = read(fd, mem, block_size)) == 0) {
				status = std::format("{}{}{} Empty file", '"', fname, '"');
				filemode = ERROR;
			} else {
				status = std::format("{}{}{} range {}-{}", '"', fname, '"',
					block_begin, block_begin + filesize - 1);
				filemode = PARTIAL;
				block_read = filesize;
				P(P_OF) = block_begin;
				params[P_OF].changed = true;
			}
			msg(status.c_str());
			refresh();
		}
	} else if ((filemode == REGULAR) || (filemode == DIRECTORY)) {
		filesize = buf.st_size;
		if (read(fd, mem, filesize) != filesize) {
            sysemsg(fname);
			filemode = ERROR;
		}
	} else {
		filesize = 0L;
	}
	if (fd > 0) close(fd);
	if (fname != nullptr) {
		switch (filemode) {
		case NEW:
			status = std::format("{}{}{} [New File]", '"', fname, '"');
			break;
		case REGULAR:
			status = std::format("{}{}{} {}{} bytes", '"', fname, '"',
				B(P_RO) ? "[Read only] " : "", filesize);
			break;
		case DIRECTORY:
			status = std::format("{}{}{} Directory", '"', fname, '"');
			break;
		case CHARACTER_SPECIAL:
			status = std::format("{}{}{} Character special file", '"', fname, '"');
			break;
		case BLOCK_SPECIAL:
			status = std::format("{}{}{} Block special file", '"', fname, '"');
			break;
		}
		if (filemode != ERROR) msg(status.c_str());
	}
	pagepos = mem;
	maxpos = mem + filesize;
	loc = HEX;
	x = AnzAdd; y = 0;
	repaint();
	return(filesize);
}


/* argument "dir" not used! 
 * Needed for DOS version only
 */
void
bvi_init(const char *dir)
{
	char    *initstr;

	terminal = std::getenv("TERM");
	shell = std::getenv("SHELL");
	if (shell == nullptr || *shell == '\0')
		shell = "/bin/sh";

	if ((initstr = std::getenv("BVIINIT")) != nullptr) {
		docmdline(initstr);
		return;
	}

	const char *home = std::getenv("HOME");
	std::string rcpath = home ? std::string(home) + "/.bvirc" : "/.bvirc";
	if (stat(rcpath.c_str(), &buf) == 0) {
		if (buf.st_uid == getuid()) read_rc(rcpath.c_str());
	}

	rcpath = ".bvirc";
	if (stat(rcpath.c_str(), &buf) == 0) {
		if (buf.st_uid == getuid())	read_rc(rcpath.c_str());
	}
}


int
enlarge(off_t add)
{
	char	*newmem;
	off_t	savecur, savepag, savemax, saveundo;

	savecur = curpos - mem;
	savepag = pagepos - mem;
	savemax = maxpos - mem;
	saveundo = undo_start - mem;

	if (mem == nullptr) {
		newmem = static_cast<char*>(std::malloc(memsize + add));
	} else {
		newmem = static_cast<char*>(std::realloc(mem, memsize + add));
	}
	if (newmem == nullptr) {
		emsg("Out of memory");
		return 1;
	}

	mem = newmem;
	memsize += add;
	curpos = mem + savecur;
	pagepos = mem + savepag;
	maxpos = mem + savemax;
	undo_start = mem + saveundo;
	current = curpos + 1L;
	return 0;
}


void
do_shell()
{
	int ret;

	addch('\n');
	savetty();
	ret = std::system(shell);
	resetty();
}


off_t
alloc_buf(off_t n, char **buffer)
{
	if (*buffer == nullptr) {
		*buffer = static_cast<char*>(std::malloc(n));
	} else {
		*buffer = static_cast<char*>(std::realloc(*buffer, n));
	}
	if (*buffer == nullptr) {
		emsg("No buffer space available");
		return 0L;
	}
	return n;
}


int
addfile(const char *fname)
{
	int		fd;
	off_t	oldsize;

	if (stat(fname, &buf)) {
		sysemsg(fname);
		return 1;
	}
	if ((fd = open(fname, O_RDONLY)) == -1) {
		sysemsg(fname);
		return 1;
	}
	oldsize = filesize;
	if (enlarge(buf.st_size)) return 1;
	if (read(fd, mem + filesize, buf.st_size) == -1) {
		sysemsg(fname);
		return 1;
	}
	filesize += buf.st_size;
	maxpos = mem + filesize;
	close(fd);
	setpage(mem + oldsize);
	return 0;
}
