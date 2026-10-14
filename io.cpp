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

#include <filesystem>
#include <format>
#include <string>
#include <system_error>

#include <peelo/xdg.hpp>

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

namespace fs = std::filesystem;

int		filemode;
static	off_t	block_read;
const char	*terminal;

/* POSIX ownership check is not available through std::filesystem. */
static bool
owned_by_self(const fs::path& path)
{
	struct stat st{};
	return ::stat(path.c_str(), &st) == 0 && st.st_uid == getuid();
}

/* Directory "size" for the unusual directory-as-file load path. */
static off_t
posix_file_size(const fs::path& path)
{
	struct stat st{};
	if (::stat(path.c_str(), &st) != 0) return 0;
	return st.st_size;
}


/*********** Save the patched file ********************/
int
save(const fs::path& fname, const char *start, const char *end, int flags)
{
	int		fd;
	std::string	status;
	const char	*newstr;
	off_t	filesize;

	if (fname.empty()) {
		emsg("No file|No current filename");
		return 0;
	}

	std::error_code ec;
	const auto st = fs::status(fname, ec);
	if (!fs::exists(st)) {
		newstr = "[New file] ";
	} else {
		if (fs::is_directory(st)) {
			msg(std::format("{}{}{} Is a directory", '"', fname.string(), '"'));
			return 0;
		} else if (fs::is_character_file(st)) {
			msg(std::format("{}{}{} Character special file", '"', fname.string(), '"'));
			return 0;
		} else if (fs::is_block_file(st)) {
			/*
			msg(... Block special file ...);
			return 0;
			*/
		}
		newstr = "";
	}

	if (filemode == PARTIAL) flags = O_RDWR;
	if ((fd = open(fname.c_str(), flags, 0666)) < 0) {
        sysemsg(fname.string());
		return 0;
	}
	if (filemode == PARTIAL) {
		if (block_read) {
			filesize = block_read;
			status = std::format("{}{}{} range {}-{}", '"', fname.string(), '"',
				block_begin, block_begin - 1 + filesize);
			if (lseek(fd, block_begin, SEEK_SET) < 0) {
				sysemsg(fname.string());
				return 0;
			}
		} else {
			msg("Null range");
			return 0;
        }
	} else {
		filesize = end - start + 1L;
		status = std::format("{}{}{} {}{} bytes", '"', fname.string(), '"', newstr, filesize);
	}

	if (write(fd, start, filesize) != filesize) {
		sysemsg(fname.string());
		close(fd);
		return 0;
	}
	close(fd);
	edits = 0;
	msg(status);
	return 1;
}


/* loads a file, returns the filesize */
off_t
load(const fs::path& fname)
{
	int		fd = -1;
	std::string	status;
	off_t	fsize = 0;

	if (!fname.empty()) {
		std::error_code ec;
		const auto st = fs::status(fname, ec);
		if (!fs::exists(st)) {
		/* check for EOVERFLOW       75              */
		/* Value too large for defined data type     */
		/* means bvi is compiled without lfs support */
			if (ec == std::errc::no_such_file_or_directory || !ec) {
				filemode = NEW;
			} else {
				move(maxy, 0);
				endwin();
				std::perror(fname.c_str());
				std::exit(0);
			}
		} else if (fs::is_character_file(st)) {
			filemode = CHARACTER_SPECIAL;
		} else if (fs::is_block_file(st)) {
			filemode = BLOCK_SPECIAL;
			fsize = posix_file_size(fname);
			if (!block_flag) {
				block_flag = 1;
				block_begin = 0;
				block_size = 1024;
				block_end = block_begin + block_size - 1;
			}
			if ((fd = open(fname.c_str(), O_RDONLY)) > 0) {
				B(P_RO) = true;
				params[P_RO].changed = true;
			} else {
				sysemsg(fname.string());
				filemode = ERROR;
			}
		} else if (fs::is_regular_file(st) || fs::is_directory(st)) {
			if (fs::is_regular_file(st)) {
				fsize = static_cast<off_t>(fs::file_size(fname, ec));
				if (ec) {
					move(maxy, 0);
					endwin();
					std::perror(fname.c_str());
					std::exit(0);
				}
			} else {
				fsize = posix_file_size(fname);
			}
			if ((fd = open(fname.c_str(), O_RDONLY)) > 0) {
				if (fs::is_regular_file(st)) {
					filemode = REGULAR;
				} else {
					filemode = DIRECTORY;
				}
				/* access(2) considers credentials; filesystem perms do not. */
				if (access(fname.c_str(), W_OK)) {
					B(P_RO) = true;
					params[P_RO].changed = true;
				}
			} else {
	            sysemsg(fname.string());
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
			block_size = fsize - block_begin;
		}
		memsize += block_size;
	} else if (filemode == REGULAR) {
		memsize += fsize;
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
			sysemsg(fname.string());
			filemode = ERROR;
		} else {
			if ((filesize = read(fd, mem, block_size)) == 0) {
				status = std::format("{}{}{} Empty file", '"', fname.string(), '"');
				filemode = ERROR;
			} else {
				status = std::format("{}{}{} range {}-{}", '"', fname.string(), '"',
					block_begin, block_begin + filesize - 1);
				filemode = PARTIAL;
				block_read = filesize;
				P(P_OF) = block_begin;
				params[P_OF].changed = true;
			}
			msg(status);
			refresh();
		}
	} else if ((filemode == REGULAR) || (filemode == DIRECTORY)) {
		filesize = fsize;
		if (read(fd, mem, filesize) != filesize) {
            sysemsg(fname.string());
			filemode = ERROR;
		}
	} else {
		filesize = 0L;
	}
	if (fd > 0) close(fd);
	if (!fname.empty()) {
		switch (filemode) {
		case NEW:
			status = std::format("{}{}{} [New File]", '"', fname.string(), '"');
			break;
		case REGULAR:
			status = std::format("{}{}{} {}{} bytes", '"', fname.string(), '"',
				B(P_RO) ? "[Read only] " : "", filesize);
			break;
		case DIRECTORY:
			status = std::format("{}{}{} Directory", '"', fname.string(), '"');
			break;
		case CHARACTER_SPECIAL:
			status = std::format("{}{}{} Character special file", '"', fname.string(), '"');
			break;
		case BLOCK_SPECIAL:
			status = std::format("{}{}{} Block special file", '"', fname.string(), '"');
			break;
		}
		if (filemode != ERROR) msg(status);
	}
	pagepos = mem;
	maxpos = mem + filesize;
	loc = HEX;
	x = AnzAdd; y = 0;
	repaint();
	return(filesize);
}

void
bvi_init()
{
	const auto initstr = std::getenv("BVIINIT");

	terminal = std::getenv("TERM");
	shell = std::getenv("SHELL");

	if (!shell || *shell == '\0') {
		shell = "/bin/sh";
	}

	if (initstr) {
		docmdline(initstr);
		return;
	}

	if (const auto config_dir = peelo::xdg::config_dir()) {
		const auto rc = *config_dir / "bvi" / "init.bvi";

		if (fs::exists(rc) && owned_by_self(rc)) {
			read_rc(rc);
		}
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

	if (!mem) {
		newmem = static_cast<char*>(std::malloc(memsize + add));
	} else {
		newmem = static_cast<char*>(std::realloc(mem, memsize + add));
	}
	if (!newmem) {
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
addfile(const fs::path& fname)
{
	int		fd;
	off_t	oldsize;
	std::error_code ec;
	const auto st = fs::status(fname, ec);

	if (!fs::exists(st)) {
		sysemsg(fname.string());
		return 1;
	}
	off_t fsize = fs::is_regular_file(st)
		? static_cast<off_t>(fs::file_size(fname, ec))
		: posix_file_size(fname);
	if (ec && fs::is_regular_file(st)) {
		sysemsg(fname.string());
		return 1;
	}
	if ((fd = open(fname.c_str(), O_RDONLY)) == -1) {
		sysemsg(fname.string());
		return 1;
	}
	oldsize = filesize;
	if (enlarge(fsize)) return 1;
	if (read(fd, mem + filesize, fsize) == -1) {
		sysemsg(fname.string());
		return 1;
	}
	filesize += fsize;
	maxpos = mem + filesize;
	close(fd);
	setpage(mem + oldsize);
	return 0;
}
