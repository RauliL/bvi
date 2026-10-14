/* SET.HPP
 *
 * NOTE: Edit this file with tabstop=4 !
 *
 * Copyright 1996-2014 by Gerhard Buergmann
 * Gerhard.Buergmann@puon.at
 *
 * 1998-03-14 V 1.0.0
 * 1999-01-14 V 1.1.0
 * 1999-03-23 V 1.1.1
 * 1999-07-02 V 1.2.0 beta
 * 1999-08-14 V 1.2.0 final
 * 2000-08-21 V 1.3.0 final
 * 2010-06-02 V 1.3.4
 * 2013-08-23 V 1.4.0
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

#include <string>
#include <variant>
#include <vector>

struct	param {
	using value_type = std::variant<bool, off_t, std::string>;

	const char	*fullname;	/* full parameter name */
	const char	*shortname;	/* permissible abbreviation */
	value_type value;
	bool		changed;
};

extern	std::vector<param>	params;

/*
 * The following are the indices in the params array for each parameter
 */

/*
 * parameters
 */
#define	P_AW		0	/* Autowrite */
#define P_CM		1	/* Columns */
#define	P_EB		2	/* error bells */
#define	P_IC		3	/* ignore case in searches */
#define	P_MA		4	/* Magic characters in reg expr */
#define P_MM		5	/* move bytes in file */
#define	P_OF		6	/* address offset */
#define P_RO		7	/* Readonly */
#define	P_SS		8	/* scroll size */
#define	P_MO		9	/* show mode */
#define P_TT		10	/* Terminal type */
#define P_TE		11	/* Terse (short messages) */
#define P_US		12	/* Unix-Style of ASCII representation */
#define	P_LI		13	/* lines */
#define P_WL		14	/* Wordlength for w, W, b, B command */
#define	P_WS		15	/* wrapscan */
#define P_HL		16	/* highlight search enabled */
#define	P_CO		17	/* colors enabled */
#define P_COLOR_BG	18
#define P_COLOR_FG	19
#define P_COLOR_ADDRESS	20
#define P_COLOR_NULL	21
#define P_COLOR_CONTROL	22
#define P_COLOR_ASCII	23
#define P_COLOR_HIGH	24
#define P_COLOR_SEARCH	25
#define P_COLOR_STATUS	26
#define P_COLOR_ERROR	27

/*
 * Accessors for the active alternative in params[n].value
 */
#define	P(n)	(std::get<off_t>(params[n].value))
#define	B(n)	(std::get<bool>(params[n].value))
#define	S(n)	(std::get<std::string>(params[n].value))

/* First color_* string parameter index through last inclusive. */
#define P_COLOR_FIRST	P_COLOR_BG
#define P_COLOR_LAST	P_COLOR_ERROR
