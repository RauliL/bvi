/* COLORS.C - Gruvbox-based UI / byte-class coloring
 *
 * Copyright 2026 bvi contributors
 */

#include "bvi.hpp"
#include "colors.hpp"
#include "set.hpp"

#include <algorithm>
#include <array>
#include <string>

namespace {

constexpr int kPairBase = 1;	/* pair 0 is reserved */
constexpr int kColorBase = 16;	/* try to stay above the basic 16 */

struct Rgb {
	int r = 0;
	int g = 0;
	int b = 0;
};

bool g_started = false;
bool g_active = false;
std::array<int, static_cast<size_t>(ColorRole::Count)> g_attrs{};

int
hex_digit(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

Rgb
clamp_rgb(int r, int g, int b)
{
	return {
		std::clamp(r, 0, 255),
		std::clamp(g, 0, 255),
		std::clamp(b, 0, 255)
	};
}

/* Map an RGB triple to the nearest basic curses color constant. */
short
nearest_basic_color(const Rgb& rgb)
{
	struct Named {
		short id;
		int r, g, b;
	};
	static const Named basics[] = {
		{ COLOR_BLACK,   0,   0,   0 },
		{ COLOR_RED,   205,   0,   0 },
		{ COLOR_GREEN,   0, 205,   0 },
		{ COLOR_YELLOW, 205, 205,  0 },
		{ COLOR_BLUE,    0,   0, 238 },
		{ COLOR_MAGENTA,205,  0, 205 },
		{ COLOR_CYAN,    0, 205, 205 },
		{ COLOR_WHITE, 229, 229, 229 },
	};

	long best = -1;
	short best_id = COLOR_WHITE;
	for (const auto& n : basics) {
		const long dr = rgb.r - n.r;
		const long dg = rgb.g - n.g;
		const long db = rgb.b - n.b;
		const long d = dr * dr + dg * dg + db * db;
		if (best < 0 || d < best) {
			best = d;
			best_id = n.id;
		}
	}
	return best_id;
}

const std::string&
role_setting(ColorRole role)
{
	switch (role) {
	case ColorRole::Normal:  return S(P_COLOR_FG);
	case ColorRole::Address: return S(P_COLOR_ADDRESS);
	case ColorRole::Null:    return S(P_COLOR_NULL);
	case ColorRole::Control: return S(P_COLOR_CONTROL);
	case ColorRole::Ascii:   return S(P_COLOR_ASCII);
	case ColorRole::High:    return S(P_COLOR_HIGH);
	case ColorRole::Search:  return S(P_COLOR_SEARCH);
	case ColorRole::Status:  return S(P_COLOR_STATUS);
	case ColorRole::Error:   return S(P_COLOR_ERROR);
	default:                 return S(P_COLOR_FG);
	}
}

bool
rgb_from_setting(ColorRole role, Rgb& out)
{
	int r = 0, g = 0, b = 0;
	if (!parse_hex_color(role_setting(role), r, g, b)) {
		return false;
	}
	out = clamp_rgb(r, g, b);
	return true;
}

bool
rgb_bg(Rgb& out)
{
	int r = 0, g = 0, b = 0;
	if (!parse_hex_color(S(P_COLOR_BG), r, g, b)) {
		return false;
	}
	out = clamp_rgb(r, g, b);
	return true;
}

short
palette_index(ColorRole role)
{
	return static_cast<short>(kColorBase + static_cast<int>(role));
}

short
palette_bg_index()
{
	return static_cast<short>(kColorBase + static_cast<int>(ColorRole::Count));
}

void
set_curses_color(short id, const Rgb& rgb)
{
	/* ncurses wants 0..1000 */
	init_color(id,
		static_cast<short>(rgb.r * 1000 / 255),
		static_cast<short>(rgb.g * 1000 / 255),
		static_cast<short>(rgb.b * 1000 / 255));
}

int
fallback_attr(ColorRole role)
{
	switch (role) {
	case ColorRole::Search:
		return A_STANDOUT;
	case ColorRole::Status:
		return A_BOLD;
	case ColorRole::Error:
		return A_REVERSE;
	case ColorRole::Address:
	case ColorRole::Ascii:
		return A_BOLD;
	default:
		return A_NORMAL;
	}
}

void
install_pairs(bool use_palette)
{
	Rgb bg{};
	if (!rgb_bg(bg)) {
		bg = { 0x28, 0x28, 0x28 };
	}

	short bg_id;
	if (use_palette) {
		bg_id = palette_bg_index();
		set_curses_color(bg_id, bg);
	} else {
		bg_id = nearest_basic_color(bg);
	}

	for (int i = 0; i < static_cast<int>(ColorRole::Count); ++i) {
		const auto role = static_cast<ColorRole>(i);
		Rgb fg{};
		if (!rgb_from_setting(role, fg)) {
			fg = { 0xeb, 0xdb, 0xb2 };
		}

		short fg_id;
		if (use_palette) {
			fg_id = palette_index(role);
			set_curses_color(fg_id, fg);
		} else {
			fg_id = nearest_basic_color(fg);
		}

		const short pair = static_cast<short>(kPairBase + i);
		init_pair(pair, fg_id, bg_id);

		int attr = COLOR_PAIR(pair);
		if (role == ColorRole::Search || role == ColorRole::Status
				|| role == ColorRole::Error) {
			attr |= A_BOLD;
		}
		g_attrs[static_cast<size_t>(i)] = attr;
	}
}

void
setup_color_state(bool do_repaint)
{
	for (auto& a : g_attrs) {
		a = A_NORMAL;
	}

	if (!has_colors() || !B(P_CO)) {
		g_active = false;
		bkgdset(A_NORMAL | ' ');
		bkgd(A_NORMAL | ' ');
		if (do_repaint) {
			repaint();
		}
		return;
	}

	const bool use_palette = can_change_color()
		&& COLORS > palette_bg_index();

	install_pairs(use_palette);
	g_active = true;

	bkgdset(color_attr(ColorRole::Normal) | ' ');
	bkgd(color_attr(ColorRole::Normal) | ' ');
	if (do_repaint) {
		repaint();
	}
}

} // namespace

bool
parse_hex_color(std::string_view text, int& r, int& g, int& b)
{
	if (text.size() != 7 || text[0] != '#') {
		return false;
	}
	int vals[6];
	for (int i = 0; i < 6; ++i) {
		vals[i] = hex_digit(text[static_cast<size_t>(i + 1)]);
		if (vals[i] < 0) {
			return false;
		}
	}
	r = (vals[0] << 4) | vals[1];
	g = (vals[2] << 4) | vals[3];
	b = (vals[4] << 4) | vals[5];
	return true;
}

int
color_attr(ColorRole role)
{
	if (!g_active) {
		return fallback_attr(role);
	}
	const auto i = static_cast<size_t>(role);
	if (i >= g_attrs.size()) {
		return A_NORMAL;
	}
	return g_attrs[i];
}

int
color_for_byte(unsigned char byte)
{
	if (byte == 0x00) {
		return color_attr(ColorRole::Null);
	}
	if (byte < 0x20 || byte == 0x7f) {
		return color_attr(ColorRole::Control);
	}
	if (byte < 0x80) {
		return color_attr(ColorRole::Ascii);
	}
	return color_attr(ColorRole::High);
}

void
init_colors()
{
	g_started = true;
	if (!has_colors()) {
		g_active = false;
		return;
	}
	start_color();
	use_default_colors();
	setup_color_state(false);
}

void
apply_colors()
{
	if (!g_started) {
		return;
	}
	setup_color_state(true);
}
