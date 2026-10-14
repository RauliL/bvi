/* COMM.C - Routines to parse and execute "command line" commands, such as
 * search or colon commands.
 *
 * 1996-01-16 created;
 * 1998-03-14 V 1.0.1
 * 1999-02-03 V 1.1.0
 * 1999-06-22 V 1.2.0 beta
 * 1999-09-10 V 1.2.0 final
 * 2000-03-03 V 1.3.0 beta
 * 2000-07-15 V 1.3.0 final
 * 2001-10-10 V 1.3.1
 * 2003-07-04 V 1.3.2
 * 2005-08-17 V 1.3.3
 * 2010-06-02 V 1.3.4
 * 2014-01-28 V 1.4.0
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
#include "colors.hpp"
#include "set.hpp"

#include <algorithm>
#include <functional>
#include <format>
#include <optional>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <peelo/xdg.hpp>

namespace fs = std::filesystem;

#ifdef HAVE_UNISTD_H
#	include <unistd.h>
#endif

#ifdef HAVE_FCNTL_H
#	include <fcntl.h>
#endif

#define WRITE (O_WRONLY|O_CREAT|O_TRUNC)
#define APPEND (O_WRONLY|O_APPEND)

#define MAXNAME	10		/* size of a : command name */

#define NO_ADDR			1
#define NO_ARG			8
#define ONE_ARG			16
#define	MAX_ONE_ARG		32
#define ONE_FILE		64
#define	MAX_ONE_FILE	128

static	fs::path	altfile;	/* alternate file */
static	std::string	oldbuf;		/** for :!! command **/

const char	*morefiles = "more files@to edit";
const char	*ambigous  = "Ambigous|Too many file names";
const char	*ambvalue  = "Ambigous|Too many values";
const char	*extra     = "Extra chars|Extra characters at end of command";
const char	*noaddr    = "No address allowed@on this command";
const char	*noval     = "No value@for binary operation";
int		repl_count = -1;
int		addr_flag;

PTR		start_addr;
PTR		end_addr;

extern	char	**files;		/* used for "next" and "rewind" */
extern	int		numfiles, curfile;
extern	int		errno;

struct CmdState {
	std::string name;			/* command name without ! */
	std::string rest;			/* text after command name / ! */
	std::string first_token;		/* first whitespace token of the command */
	std::vector<std::string> args;
	bool force = false;
};

using CmdHandler = std::function<void(CmdState&)>;

static bool save_chk(const fs::path&, const char*, const char*, int);
static off_t yd_addr(CmdState& st);
static int chk_comm(CmdState& st, int flag);

static void
split_args(CmdState& st, std::string text)
{
	st.args.clear();
	st.first_token.clear();

	auto pos = text.find_first_not_of(" \t");
	if (pos == std::string::npos) {
		return;
	}
	text.erase(0, pos);

	auto end = text.find_first_of(" \t");
	st.first_token = text.substr(0, end);
	if (end == std::string::npos) {
		return;
	}

	pos = text.find_first_not_of(" \t", end);
	while (pos != std::string::npos) {
		end = text.find_first_of(" \t", pos);
		st.args.push_back(text.substr(pos, end - pos));
		if (end == std::string::npos) {
			break;
		}
		pos = text.find_first_not_of(" \t", end);
	}
}

static void
cmd_substitute(CmdState& st)
{
	const char *rest = st.rest.c_str();
	repl_count = do_substitution(*rest, rest + 1, start_addr, end_addr);
	if (repl_count == -1) {
		emsg("No previous substitute re|No previous substitute regular expression");
		return;
	}
	repaint();
	if (!repl_count) {
		emsg("Fail|Substitute pattern matching failed");
	} else if (repl_count > 1) {
		msg(std::format("{} subs|{} substitutions", repl_count, repl_count).c_str());
	}
}

static void
cmd_global(CmdState& st)
{
	const char *rest = st.rest.c_str();
	current = start_addr - 1;
	repl_count = 0;
	addch('\n');
	while ((current = searching(*rest, rest + 1, current,
						end_addr, false|S_GLOBAL)) != 0L) {
		addch('\n');
		printline(current, maxy - 1);
		repl_count++;
		if (repl_count == LINES - 2) {
			if (wait_return(false)) return;
			repl_count = 0;
		}
	}
	if (repl_count) {
		wait_return(true);
	} else {
		repaint();
		emsg(notfound);
	}
}

static void
cmd_write(CmdState& st)
{
	int saveflag;
	int ok = 0;
	char *cmd = st.rest.data();

	SKIP_WHITE
	if (*cmd == '>') {
		if (*(++cmd) == '>') {
			cmd++;
			saveflag = APPEND;
		} else {
			emsg("Write forms are 'w' and 'w>>'");
			return;
		}
	} else if (*cmd == '!') {
		emsg("Not yet implemented");
		return;
	} else {
		saveflag = WRITE;
	}
	SKIP_WHITE

	st.args.clear();
	{
		std::string args{cmd};
		auto pos = args.find_first_not_of(" \t");
		while (pos != std::string::npos) {
			auto end = args.find_first_of(" \t", pos);
			st.args.push_back(args.substr(pos, end - pos));
			if (end == std::string::npos) {
				break;
			}
			pos = args.find_first_not_of(" \t", end);
		}
	}

	if (st.args.size() > 1) {
		emsg(ambigous);
		return;
	}
	if (st.args.size() == 1) {
		/* change '%' to Filename */
		std::string& arg = st.args[0];
		for (size_t i = 0; i < arg.size(); ++i) {
			if (arg[i] == '%' && (i == 0 || arg[i - 1] != '\\')) {
				if (name.empty()) {
					emsg("No filename@to substitute for %");
					return;
				}
				arg.replace(i, 1, name.string());
				i += name.string().size() - 1;
			}
		}
	}
	if (name.empty() && !st.args.empty()) name = st.args[0];
	if (st.force) {
		if (st.args.empty()) ok = save(name, start_addr, end_addr, saveflag);
			else	ok = save(st.args[0], start_addr, end_addr, saveflag);
	} else {
		if (st.args.empty()) {
			save_chk(name, start_addr, end_addr, saveflag);
		} else {
			std::error_code ec;
			const auto fstatus = fs::status(st.args[0], ec);
			if (fs::exists(fstatus)) {
				if (saveflag == WRITE) {
					emsg(std::format("File exists@- use \"{}! {}\" to overwrite", st.name, st.args[0]));
					return;
				} else {		/* APPEND */
/* We can only append to a regular file! */
					if (fs::is_regular_file(fstatus)) {
						if (filemode == PARTIAL) filemode = REGULAR;
					} else if (fs::is_block_file(fstatus)) {
						emsg("Cannot append to a block special file");
						return;
					}
					ok = save(st.args[0], start_addr, end_addr, saveflag);
				}
			} else {
				if (saveflag == APPEND) {
					emsg("No such file");
					return;
				} else {		/* WRITE */
/* If we write the block of a partial file to a new file, it will
 * become a regular file!
 */
					if (filemode == PARTIAL) filemode = REGULAR;
					ok = save(st.args[0], start_addr, end_addr, saveflag);
				}
			}
		}
	}
	if (st.name == "wq" && ok) {
		quit();
	}
}

static void
cmd_set(CmdState& st)
{
	if (chk_comm(st, NO_ADDR)) return;
	if (st.args.empty()) {
		doset(nullptr);
	} else {
		for (const auto& arg : st.args) {
			if (doset(arg.c_str())) return;
		}
	}
}

static void
cmd_args(CmdState& st)
{
	if (chk_comm(st, NO_ADDR|NO_ARG)) return;
	std::string args;
	for (int n = 0; n < numfiles; n++) {
		if (n == curfile) args += '[';
		args += files[n];
		if (n == curfile) args += ']';
		args += ' ';
	}
	msg(args.c_str());
}

static void
cmd_source(CmdState& st)
{
	if (chk_comm(st, NO_ADDR|ONE_FILE)) return;
	if (!read_rc(st.args[0])) sysemsg(st.args[0]);
	refresh();
}

static void
cmd_cd(CmdState& st)
{
	if (chk_comm(st, NO_ADDR|ONE_FILE)) return;
	if (!st.force) {
		if (edits) {
			if (B(P_AW)) {
				save(name, mem, maxpos, WRITE);
				edits = 0;
			} else {
				emsg(std::format("No write@since last change (:{}! overrides)", "cd").c_str());
				return;
			}
		}
	}
	std::error_code ec;
	fs::current_path(st.args[0], ec);
	if (ec) sysemsg(st.args[0]);
}

static void
cmd_edit(CmdState& st)
{
/*
 * The command ":e#" gets expanded to something like ":efile", so
 * detect that case here.
 */
	if (!st.first_token.empty() && st.first_token[0] == 'e' && st.args.empty()) {
		if (st.first_token.size() > 1 && st.first_token[1] == '!')
			(void) doecmd(st.first_token.c_str() + 2, true);
		else
			(void) doecmd(st.first_token.c_str() + 1, false);
		return;
	}
	if (chk_comm(st, NO_ADDR|MAX_ONE_FILE)) return;
	doecmd(st.args.empty() ? nullptr : st.args[0].c_str(), st.force);
}

static void
cmd_file(CmdState& st)
{
	if (chk_comm(st, NO_ADDR|MAX_ONE_FILE)) return;
	if (!st.args.empty()) name = st.args[0];
	fileinfo(name);
}

static void
cmd_read(CmdState& st)
{
	if (chk_comm(st, NO_ADDR|MAX_ONE_FILE)) return;
	/* read reads a file at EOF */
	if (!addfile(st.args.empty() ? name : fs::path(st.args[0]))) {
		edits = U_TRUNC;
		undosize = filesize;
		repaint();
	}
}

static void
cmd_xit(CmdState& st)
{
	if (chk_comm(st, NO_ADDR|NO_ARG)) return;
	do_exit();
}

static void
cmd_next(CmdState& st)
{
	if (chk_comm(st, NO_ADDR|NO_ARG)) return;
	if (!st.force) {
		if (edits) {
			if (B(P_AW)) {
				save(name, mem, maxpos, WRITE);
				edits = 0;
			} else {
				emsg(std::format("No write@since last change (:{}! overrides)", "next").c_str());
				return;
			}
		}
	}
	if ((curfile + 1) < numfiles) {
		if (st.force)  stuffin(":e! ");
		else		stuffin(":e ");
		stuffin(files[++curfile]);
		stuffin("\n");
	} else
		emsg("No more files@to edit!");
}

static void
cmd_rewind(CmdState& st)
{
	if (chk_comm(st, NO_ADDR|NO_ARG)) return;
	if (numfiles <= 1)		/* nothing to rewind */
		return;
	if (!st.force) {
		if (edits) {
			if (B(P_AW)) {
				save(name, mem, maxpos, WRITE);
				edits = 0;
			} else {
				emsg(std::format("No write@since last change (:{}! overrides)", "rewind").c_str());
				return;
			}
		}
	}
	curfile = 0;
	if (st.force)	stuffin(":e! ");
		else	stuffin(":e ");
	stuffin(files[0]);
	stuffin("\n");
}

static void
cmd_append(CmdState& st)
{
	if (chk_comm(st, NO_ADDR|MAX_ONE_ARG)) return;
	do_ins_chg(start_addr, st.args.size() == 1 ? st.args[0].c_str() : "a", U_APPEND);
}

static void
cmd_change(CmdState& st)
{
	if (chk_comm(st, MAX_ONE_ARG)) return;
	if (!addr_flag) start_addr = current;
	do_ins_chg(start_addr, st.args.size() == 1 ? st.args[0].c_str() : "a", U_EDIT);
}

static void
cmd_mark(CmdState& st)
{
	if (st.args.empty()) { emsg("Mark what?"); return; }
	if (st.args.size() > 1 || st.args[0].size() > 1) { emsg(extra); return; }
	if (!addr_flag) start_addr = current;
	do_mark(st.args[0][0], start_addr);
}

static void
cmd_yank(CmdState& st)
{
	if ((yanked = yd_addr(st)) == 0L) return;
	if ((yanked = alloc_buf(yanked, &yank_buf)) == 0L) return;
	std::memcpy(yank_buf, start_addr, yanked);
	msg(std::format("{} bytes", yanked).c_str());
}

static void
cmd_overwrite(CmdState& st)
{
	if (chk_comm(st, NO_ARG)) return;
	if (!addr_flag) start_addr = current;
	do_over(start_addr, yanked, yank_buf);
}

static void
cmd_undo(CmdState& st)
{
	if (chk_comm(st, NO_ADDR|NO_ARG)) return;
	do_undo();
}

static void
cmd_version(CmdState& st)
{
	if (chk_comm(st, NO_ADDR|NO_ARG)) return;
	msg(std::format("bvi version {} {}", VERSION, copyright).c_str());
}

static void
cmd_shell(CmdState& st)
{
	if (chk_comm(st, NO_ADDR|NO_ARG)) return;
	do_shell();
	clear();
	repaint();
	clearstr();
}

static void
cmd_quit(CmdState& st)
{
	if (chk_comm(st, NO_ADDR|NO_ARG)) return;
	if (!st.force) {
		if (edits) {
			emsg(std::format("No write@since last change (:{}! overrides)", "quit").c_str());
		} else {
			if ((curfile + 1) < numfiles) {
				emsg(std::format("{} {}", numfiles - curfile - 1, morefiles).c_str());
			} else
				quit();
		}
	} else quit();
}

static void
cmd_logic_op(CmdState& st, int mode, bool default_one, bool require_arg)
{
	if (st.args.size() == 1) {
		do_logic(mode, st.args[0].c_str());
	} else if (st.args.empty()) {
		if (default_one) {
			do_logic(mode, "1");
		} else if (require_arg) {
			emsg(noval);
		}
	} else {
		emsg(ambvalue);
	}
}

static void
cmd_neg(CmdState& st)
{
	if (!st.args.empty()) { emsg(extra); return; }
	do_logic(NEG, "255");
}

static void
cmd_not(CmdState& st)
{
	if (!st.args.empty()) { emsg(extra); return; }
	do_logic(NOT, "255");
}

static void
cmd_delete(CmdState& st)
{
	if (!B(P_MM)) {
		movebyte();
		return;
	}
	if ((undo_count = yd_addr(st)) == 0L) return;
	do_delete(undo_count, start_addr);
	msg(std::format("{} bytes", undo_count).c_str());
}

static void
cmd_insert(CmdState& st)
{
	if (!B(P_MM)) {
		movebyte();
		return;
	}
	if (chk_comm(st, MAX_ONE_ARG)) return;
	if (!addr_flag) start_addr = current;
	do_ins_chg(start_addr - 1, st.args.size() == 1 ? st.args[0].c_str() : "a", U_INSERT);
}

static void
cmd_put(CmdState& st)
{
	if (!B(P_MM)) {
		movebyte();
		return;
	}
	if (chk_comm(st, NO_ARG)) return;
	if (!addr_flag) start_addr = current;
	do_put(start_addr, yanked, yank_buf);
}

struct CmdTables {
	std::unordered_map<std::string, CmdHandler> map;
	std::vector<std::string> names;	/* canonical names for completion */
	std::unordered_set<std::string> file_arg_abbrevs;
};

/*
 * Build the colon-command table. Abbreviations are registered in the same
 * priority order as the old if/else chain: first registration wins.
 */
static CmdTables
make_cmd_tables()
{
	CmdTables tables;

	auto add = [&](const char* name, int min_len, CmdHandler fn, bool file_arg = false) {
		tables.names.emplace_back(name);
		const size_t n = std::strlen(name);
		for (size_t len = static_cast<size_t>(min_len); len <= n; ++len) {
			std::string abbrev(name, len);
			tables.map.emplace(abbrev, fn);
			if (file_arg) {
				tables.file_arg_abbrevs.insert(abbrev);
			}
		}
	};

	add("substitute", 1, cmd_substitute);
	add("global", 1, cmd_global);
	add("write", 1, cmd_write, true);
	tables.map.emplace("wq", cmd_write);
	tables.names.emplace_back("wq");
	tables.file_arg_abbrevs.insert("wq");
	add("set", 2, cmd_set);
	add("args", 2, cmd_args);
	add("source", 2, cmd_source, true);
	add("cd", 2, cmd_cd, true);
	add("edit", 1, cmd_edit, true);
	add("file", 1, cmd_file, true);
	add("read", 1, cmd_read, true);
	add("xit", 1, cmd_xit);
	add("next", 1, cmd_next);
	add("rewind", 3, cmd_rewind);
	add("append", 1, cmd_append);
	add("change", 1, cmd_change);
	add("mark", 2, cmd_mark);
	add("yank", 1, cmd_yank);
	add("overwrite", 1, cmd_overwrite);
	add("undo", 1, cmd_undo);
	add("version", 2, cmd_version);
	add("shell", 2, cmd_shell);
	add("quit", 1, cmd_quit);
	add("sleft", 2, [](CmdState& st){ cmd_logic_op(st, LSHIFT, true, false); });
	add("sright", 2, [](CmdState& st){ cmd_logic_op(st, RSHIFT, true, false); });
	add("rleft", 2, [](CmdState& st){ cmd_logic_op(st, LROTATE, true, false); });
	add("rright", 2, [](CmdState& st){ cmd_logic_op(st, RROTATE, true, false); });
	add("and", 3, [](CmdState& st){ cmd_logic_op(st, AND, false, true); });
	add("or", 2, [](CmdState& st){ cmd_logic_op(st, OR, false, true); });
	add("xor", 3, [](CmdState& st){ cmd_logic_op(st, XOR, false, true); });
	add("neg", 3, cmd_neg);
	add("not", 3, cmd_not);
	add("delete", 1, cmd_delete);
	add("insert", 1, cmd_insert);
	add("put", 2, cmd_put);

	std::sort(tables.names.begin(), tables.names.end());
	return tables;
}

static const CmdTables&
cmd_tables()
{
	static const auto tables = make_cmd_tables();
	return tables;
}

static const std::unordered_map<std::string, CmdHandler>&
cmd_map()
{
	return cmd_tables().map;
}

static size_t
skip_ex_white(std::string_view s, size_t i)
{
	while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) {
		++i;
	}
	return i;
}

static size_t
skip_ex_number(std::string_view s, size_t i, int base)
{
	if (i >= s.size()) {
		return i;
	}
	char *end = nullptr;
	std::string tmp{s.substr(i)};
	(void)std::strtoll(tmp.c_str(), &end, base);
	if (end && end != tmp.c_str()) {
		i += static_cast<size_t>(end - tmp.c_str());
	}
	return i;
}

/* Skip one address expression for completion purposes (no side effects). */
static size_t
skip_ex_addr(std::string_view s, size_t i)
{
	i = skip_ex_white(s, i);
	if (i >= s.size()) {
		return i;
	}

	const char ch = s[i];
	if (ch >= '1' && ch <= '9') {
		i = skip_ex_number(s, i, 10);
	} else if (ch == '0') {
		i = skip_ex_number(s, i, 16);
	} else if (ch == '.' || ch == '^' || ch == '$') {
		++i;
	} else if (ch == '\'') {
		++i;
		if (i < s.size()) {
			++i;
		}
	} else if (ch == '/' || ch == '\\' || ch == '?' || ch == '#') {
		const char delim = ch;
		++i;
		while (i < s.size() && s[i] != delim) {
			if (s[i] == '\\' && i + 1 < s.size()) {
				i += 2;
			} else {
				++i;
			}
		}
		if (i < s.size() && s[i] == delim) {
			++i;
		}
	} else {
		return i;
	}

	i = skip_ex_white(s, i);
	while (i < s.size() && (s[i] == '+' || s[i] == '-')) {
		++i;
		i = skip_ex_white(s, i);
		i = skip_ex_number(s, i, 0);
		i = skip_ex_white(s, i);
	}
	return i;
}

struct ExLineParts {
	size_t cmd_start = 0;
	size_t cmd_end = 0;
	size_t arg_start = std::string::npos;	/* last argument token, if any */
};

static ExLineParts
parse_ex_line(std::string_view line)
{
	ExLineParts parts;
	size_t i = skip_ex_white(line, 0);
	if (i < line.size() && line[i] == '%') {
		i = skip_ex_white(line, i + 1);
	} else {
		const size_t before = i;
		i = skip_ex_addr(line, i);
		if (i != before) {
			i = skip_ex_white(line, i);
			if (i < line.size() && line[i] == ',') {
				i = skip_ex_addr(line, i + 1);
				i = skip_ex_white(line, i);
			}
		}
	}

	parts.cmd_start = i;
	while (i < line.size() && line[i] >= 'a' && line[i] <= 'z') {
		++i;
	}
	parts.cmd_end = i;

	if (i < line.size() && line[i] == '!') {
		++i;
	}
	i = skip_ex_white(line, i);

	/* :w>>file / :w >> file */
	if (i + 1 < line.size() && line[i] == '>' && line[i + 1] == '>') {
		i += 2;
		i = skip_ex_white(line, i);
	}

	if (i < line.size()) {
		/* Complete the last whitespace-separated token. */
		size_t tok = i;
		for (size_t j = i; j < line.size(); ++j) {
			if (line[j] == ' ' || line[j] == '\t') {
				tok = j + 1;
			}
		}
		parts.arg_start = tok;
	}
	return parts;
}

static std::string
longest_common_prefix(const std::vector<std::string>& names)
{
	if (names.empty()) {
		return {};
	}
	std::string_view prefix = names[0];
	for (size_t n = 1; n < names.size(); ++n) {
		size_t k = 0;
		while (k < prefix.size() && k < names[n].size()
				&& prefix[k] == names[n][k]) {
			++k;
		}
		prefix = prefix.substr(0, k);
		if (prefix.empty()) {
			break;
		}
	}
	return std::string{prefix};
}

/* Apply matches to line[before]+prefix with LCP / unique / cycle semantics. */
static bool
apply_completion(std::string& line, const std::string& before,
		const std::string& prefix, std::vector<std::string> matches,
		std::string& saved_before, std::vector<std::string>& saved_matches,
		size_t& index, bool& cycling)
{
	std::sort(matches.begin(), matches.end());
	matches.erase(std::unique(matches.begin(), matches.end()), matches.end());

	const bool same_context = cycling && before == saved_before
		&& std::find(saved_matches.begin(), saved_matches.end(), prefix)
			!= saved_matches.end();

	if (!same_context) {
		saved_matches = std::move(matches);
		saved_before = before;
		index = 0;
		cycling = false;

		if (saved_matches.empty()) {
			return false;
		}
		if (saved_matches.size() == 1) {
			line = before + saved_matches[0];
			cycling = true;
			return true;
		}

		const std::string lcp = longest_common_prefix(saved_matches);
		if (lcp.size() > prefix.size()) {
			line = before + lcp;
			return true;
		}

		line = before + saved_matches[0];
		cycling = true;
		return true;
	}

	index = (index + 1) % saved_matches.size();
	line = before + saved_matches[index];
	return true;
}

static std::string
expand_tilde(std::string_view path)
{
	if (path.empty() || path[0] != '~') {
		return std::string{path};
	}

	if (const auto home_dir = peelo::xdg::home_dir()) {
		if (path.size() == 1) {
			return home_dir->string();
		} else if (path[1] == std::filesystem::path::preferred_separator) {
			return home_dir->string() + std::string(path.substr(1));
		}
	}

	return std::string{path};
}

static std::vector<std::string>
filename_matches(std::string_view typed)
{
	std::vector<std::string> matches;

	if (typed == "~") {
		matches.emplace_back("~/");
		return matches;
	}

	const std::string expanded = expand_tilde(typed);
	const bool trailing_sep = !expanded.empty()
		&& (expanded.back() == '/' || expanded.back() == '\\');

	fs::path partial{expanded};
	fs::path dir;
	std::string file_prefix;

	if (trailing_sep) {
		dir = partial;
		file_prefix.clear();
	} else if (typed.find('/') != std::string_view::npos
			|| typed.find('\\') != std::string_view::npos) {
		dir = partial.parent_path();
		if (dir.empty()) {
			dir = "/";
		}
		file_prefix = partial.filename().string();
	} else {
		dir = ".";
		file_prefix = expanded;
	}

	std::error_code ec;
	if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) {
		return matches;
	}

	/* Directory portion as the user typed it (for rebuilding matches). */
	std::string typed_dir;
	if (trailing_sep) {
		typed_dir = std::string{typed};
	} else {
		const auto slash = typed.find_last_of("/\\");
		if (slash != std::string_view::npos) {
			typed_dir = std::string{typed.substr(0, slash + 1)};
		}
	}

	for (const auto& entry : fs::directory_iterator(dir,
			fs::directory_options::skip_permission_denied, ec)) {
		if (ec) {
			break;
		}
		const std::string name = entry.path().filename().string();
		if (name.empty()) {
			continue;
		}
		if (name[0] == '.' && (file_prefix.empty() || file_prefix[0] != '.')) {
			continue;
		}
		if (name.size() < file_prefix.size()
				|| name.compare(0, file_prefix.size(), file_prefix) != 0) {
			continue;
		}

		std::error_code entry_ec;
		const bool is_dir = entry.is_directory(entry_ec);
		std::string candidate = typed_dir + name;
		if (is_dir) {
			candidate.push_back('/');
		}
		matches.push_back(std::move(candidate));
	}
	return matches;
}

static bool
ex_complete_filename(std::string& line, const ExLineParts& parts,
		std::string& saved_before, std::vector<std::string>& saved_matches,
		size_t& index, bool& cycling)
{
	if (parts.cmd_end == parts.cmd_start) {
		return false;
	}
	const std::string cmd{line.substr(parts.cmd_start,
		parts.cmd_end - parts.cmd_start)};
	if (!cmd_tables().file_arg_abbrevs.contains(cmd)) {
		return false;
	}
	/* Need to be in the argument (past the command name). */
	if (parts.arg_start == std::string::npos) {
		/* Command finished but no arg yet — complete from empty prefix. */
		if (parts.cmd_end >= line.size()) {
			return false;
		}
	}

	size_t arg_start = parts.arg_start;
	if (arg_start == std::string::npos) {
		/* e.g. "edit " with trailing space — parse left arg_start unset when
		 * only whitespace remains after optional ! / >>. Re-derive. */
		size_t i = parts.cmd_end;
		if (i < line.size() && line[i] == '!') {
			++i;
		}
		i = skip_ex_white(line, i);
		if (i + 1 < line.size() && line[i] == '>' && line[i + 1] == '>') {
			i += 2;
			i = skip_ex_white(line, i);
		}
		if (i > line.size()) {
			return false;
		}
		arg_start = i;
	}

	/* Only while editing the filename token through end of line. */
	if (arg_start > line.size()) {
		return false;
	}

	const std::string before{line.substr(0, arg_start)};
	const std::string prefix{line.substr(arg_start)};
	auto matches = filename_matches(prefix);
	return apply_completion(line, before, prefix, std::move(matches),
		saved_before, saved_matches, index, cycling);
}

/*
 * Tab-complete an ex command name or, for file-taking commands, a filename.
 * Repeated Tab cycles through matches.
 */
bool
ex_complete_command(std::string& line)
{
	static std::string saved_before;
	static std::vector<std::string> saved_matches;
	static size_t index = 0;
	static bool cycling = false;

	const ExLineParts parts = parse_ex_line(line);

	/* Command-name completion while still typing the name. */
	if (parts.cmd_end == line.size() && parts.cmd_start <= parts.cmd_end) {
		const std::string before{line.substr(0, parts.cmd_start)};
		const std::string prefix{line.substr(parts.cmd_start)};
		std::vector<std::string> matches;
		for (const auto& name : cmd_tables().names) {
			if (name.size() >= prefix.size()
					&& name.compare(0, prefix.size(), prefix) == 0) {
				matches.push_back(name);
			}
		}
		if (apply_completion(line, before, prefix, std::move(matches),
				saved_before, saved_matches, index, cycling)) {
			return true;
		}
		beep();
		return false;
	}

	if (ex_complete_filename(line, parts, saved_before, saved_matches,
			index, cycling)) {
		return true;
	}

	beep();
	return false;
}

/*
 * docmdline() - handle a colon command
 *
 * Handles a colon command received interactively by getcmdln() or from
 * the environment variable "BVIINIT" (or eventually .bvirc).
 */
void
docmdline(const std::string& cmdline)
{
	if (cmdline.empty() || cmdline[0] == '"') return; 	/** comment **/

	CmdState st;
	std::string buff = cmdline;
	char *cmd = buff.data();

/* With no address given, we start at the beginning of the file and
 * go to the end of the file (not line like in vi).
 */
	addr_flag = 0;
	start_addr = mem;
	end_addr = maxpos - 1;
	SKIP_WHITE
	if (*cmd == '%') {
		cmd++;
		addr_flag = 2;
	} else {
		if ((start_addr = calc_addr(&cmd, mem)) == nullptr) {
			return;
		}
		if (*cmd == ',') {
			cmd++;
			addr_flag = 1;
			SKIP_WHITE
			if ((end_addr = calc_addr(&cmd, maxpos - 1)) == nullptr) {
				return;
			}
		} else {
			if (addr_flag) {
				/* if we have only one address */
				end_addr = start_addr;
			}
		}
	}
	SKIP_WHITE

	if (start_addr > (end_addr + 1)) {
		emsg("Addr1 > addr2|First address exceeds second");
		return;
	}
	if ((end_addr + 1) > maxpos) {
		emsg("Not that many bytes@in buffer");
		return;
	}
	if (start_addr < mem) {
		emsg(std::format("Negative address@- first byte is {}", P(P_OF)).c_str());
		return;
	}

	SKIP_WHITE

/**** End of address range calculation ****/

	if (*cmd == '\0') {
		setpage(end_addr);
		return;
	}

	std::string cmdbuf = cmd;	/* save the unmodified command */

	if (*cmd == '!') {
		if (cmdbuf.size() == 1) {
			emsg("Incomplete shell escape command@- use 'shell' to get a shell");
			return;
		}
		if (cmdbuf[1] == '!') {
			if (!oldbuf.empty()) {
				cmdbuf = "!" + oldbuf;
				msg(oldbuf.c_str());
			} else {
				emsg("No previous command@to substitute for !");
				return;
			}
		} else {
			oldbuf = std::format("\n{}\n", cmdbuf.c_str() + 1);
		}

		if (B(P_AW)) {
			save(name, mem, maxpos, WRITE);
			edits = 0;
		}
		if (edits) msg("[No write]|[No write since last change]");
		savetty();
		endwin();
		std::system(cmdbuf.c_str() + 1);
		std::fprintf(stderr, "[Hit return to continue]");
		std::getchar();
		doupdate();
		repaint();
		clearstr();
		return;
	}

	while (*cmd >= 'a' && *cmd <= 'z') {
		st.name.push_back(*cmd++);
		if (st.name.size() == MAXNAME)  {
			emsg(std::format("What?|{}: Not an editor command", cmdbuf).c_str());
			return;
		}
	}
	if (*cmd == '!') {
		st.force = true;
		cmd++;
	}
	SKIP_WHITE
	st.rest = cmd;

	split_args(st, cmdbuf);

	const auto& map = cmd_map();
	const auto it = map.find(st.name);
	if (it != map.end()) {
		it->second(st);
		return;
	}

	/* :t{a-z} mark shortcut (any unmatched command name starting with t) */
	if (!st.name.empty() && st.name[0] == 't') {
		if (!addr_flag) start_addr = current;
		do_mark(st.name.size() > 1 ? st.name[1] : '\0', start_addr);
		return;
	}

	emsg(std::format("What?|{}: Not an editor command", st.first_token.empty()
		? st.name : st.first_token).c_str());
}


/* calculate address range for :yank and :delete command */
static off_t
yd_addr(CmdState& st)
{
	off_t	count = 0;

	if (st.args.empty()) {
		switch (addr_flag) {
		case 0: start_addr = current;
		case 1:	count = 1;
				break;
		case 2: count = end_addr - start_addr + 1;
				break;
		}
	} else if (st.args.size() == 1) {
		count = std::atoi(st.args[0].c_str());
		switch (addr_flag) {
		case 0: start_addr = current;
		case 1:	end_addr = start_addr + count - 1;
				break;
		case 2: start_addr = end_addr;
				end_addr = start_addr + count - 1;
				break;
		}
	} else {
		emsg(ambvalue);
		return 0;
	}
	return count;
}


/*********** Save file if not read only ********************/
static bool
save_chk(const fs::path& fname, const char *start, const char *end, int flags)
{
   if (B(P_RO)) {
       emsg(std::format("{}{}{} File is read only", '"', name.string(), '"'));
       return false;
   }
   return save(fname, start, end, flags) != 0;
}

void
do_exit()
{
	if (edits) {
		if (!save_chk(name, mem, maxpos - 1L, WRITE)) return;
	}
	if ((curfile + 1) < numfiles) {
		emsg(std::format("{} {}", numfiles - curfile - 1, morefiles).c_str());
	} else quit();
}


bool
doecmd(const char *arg, bool force)
{
	if (arg != nullptr && *arg == '\0') arg = nullptr;
	if (!force && edits) {
		emsg(std::format("No write@since last change (:{}! overrides)", "edit"));
		/*
		altfile = arg ? fs::path(arg) : fs::path{};
		*/
		return false;
	}
	if (arg != nullptr) {
		if (!name.empty() && name == arg) {
			if (!edits || (edits && !force))
				return true;
		}
		if (!name.empty() && std::strcmp(arg, "#") == 0) {
			if (altfile.empty()) {
				emsg("No alternate filename@to substitute for #");
				return false;
			}
			std::swap(name, altfile);
		} else {
			altfile = std::move(name);
			name = arg;
		}
	}
	if (name.empty()) {
		emsg("No file|No current filename");
		return false;
	}

	edits = 0;
	filesize = load(name);
	if (arg == nullptr) {
        setpage(current < maxpos ? current : maxpos - 1L);
    }
	return true;
}


void
clearstr()
{
	int n;

	move(maxy, 0);
	for (n = 0; n < maxx; n++) addch(' ');
	move(maxy, 0);
}


/**** displays an error message *****/
void
emsg(std::string_view s)
{
	int	cnt;
	int stchar;

	if (B(P_EB)) beep();
	if (B(P_MO)) {
		stchar = statsize;
	} else {
		stchar = 0;
	}

	clearstr();
	attrset(color_attr(ColorRole::Error));
	cnt = outmsg(s);
	attrset(color_attr(ColorRole::Normal));
	if (cnt >= (maxx - stchar)) {
		addch('\n');
		wait_return(true); }
}


/*** System error message *****/
void
sysemsg(std::string_view s)
{
	emsg(std::format("{}: {}", s, std::strerror(errno)));
}


/*** displays mode if showmode set *****/
void
smsg(std::string_view s)
{
	if (B(P_MO)) {
		msg(s);
		setcur();
	}
}


/************* displays s on status line *****************/
void
msg(std::string_view s)
{
	int stchar;

	if (B(P_MO)) {
		stchar = statsize;
	} else {
		stchar = 0;
	}
	clearstr();
	if (outmsg(s) >= (maxx - stchar)) {
		addch('\n');
		wait_return(true);
	}
}


int
outmsg(std::string_view s)
{
	int		cnt = 0;

	move(maxy, 0);
	const auto pipe = s.find('|');

	if (B(P_TE)) {
		for (char ch : s) {
			if (ch == '\0' || ch == '@' || ch == '|') break;
			addch(ch);
			cnt++;
		}
	} else {
		std::string_view out = (pipe == std::string_view::npos) ? s : s.substr(pipe + 1);
		for (char ch : out) {
			addch(ch == '@' ? ' ' : ch);
			cnt++;
		}
	}
	return cnt;
}

/* If flag == true we do a repaint
 *
 */
bool
wait_return(bool flag)
{
	int	c;

	std::signal(SIGINT, jmpproc);
	clearstr();
	attrset(A_REVERSE);
	mvaddstr(maxy, 0, "[Hit return to continue]");
	attrset(A_NORMAL);
	refresh();
	c = getch();
	if (flag) {
		clear();
		repaint();
	}
	clearstr();
	std::signal(SIGINT, SIG_IGN);
	if (c != CR && c != NL && c != ' ' && c != ':' && c != KEY_ENTER) return true;
	return false;
}


static int
chk_comm(CmdState& st, int flag)
{
	if ((flag & NO_ADDR) && (addr_flag > 0)) {
		emsg(noaddr); return 1; }
	if ((flag & NO_ARG) && !st.args.empty()) {
		emsg(extra); return 1; }
	if ((flag & MAX_ONE_ARG) && st.args.size() > 1) {
		emsg(ambvalue); return 1; }
	if ((flag & ONE_FILE) && st.args.empty()) {
		emsg("Missing filename"); return 1; }
	if ((flag & MAX_ONE_FILE) && st.args.size() > 1) {
		emsg(ambigous); return 1; }
	return 0;
}
