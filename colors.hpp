/* COLORS.HPP - Gruvbox-based UI / byte-class coloring
 *
 * Copyright 1996-2014 by Gerhard Buergmann
 * Copyright 2026 bvi contributors
 */

#pragma once

#include <string_view>

enum class ColorRole {
	Normal = 0,
	Address,
	Null,
	Control,
	Ascii,
	High,
	Search,
	Status,
	Error,
	Count
};

/* Call once after initscr(). Safe if the terminal has no color. */
void init_colors();

/* Re-read color settings and refresh pairs / background / screen. */
void apply_colors();

/* True when color output is active (terminal + :set colors). */
bool colors_enabled();

/* ncurses attribute for a semantic role (COLOR_PAIR and/or fallback attrs). */
int color_attr(ColorRole role);

/* Attribute for displaying a data byte in the hex/ASCII panes. */
int color_for_byte(unsigned char byte);

/* Validate #RRGGBB; returns false and leaves out untouched on failure. */
bool parse_hex_color(std::string_view text, int& r, int& g, int& b);
