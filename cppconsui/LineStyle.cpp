/*
 * Copyright (C) 2007 by Mark Pustjens <pustjens@dds.nl>
 *
 * This file is part of CenterIM.
 *
 * CenterIM is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * CenterIM is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * */

#include "LineStyle.h"

struct LineElements {
	const gchar *h;		// Horizontal line
	const gchar *h_begin;	// "        " line begin
	const gchar *h_end;	// "        " line end
	const gchar *h_up;	// "             " and line up
	const gchar *h_down;	// "             " and line down
	const gchar *v;		// Vertical line
	const gchar *v_begin;	// "      " line begin
	const gchar *v_end;	// "      " line end
	const gchar *v_left;	// "           " and line left
	const gchar *v_right;	// "           " and line right
	const gchar *cross;	// Horizontal and Vertical line crossed
	const gchar *corner_tl;	// Top-left corner
	const gchar *corner_tr;	// Top-right corner
	const gchar *corner_bl;	// Bottom-left corner
	const gchar *corner_br;	// Bottom-right corner
};

//TODO add styles for double lines, block elements
static LineElements line_elements_ascii = {
	"-", "-", "-", "+", "+",
	"|", "|", "|", "+", "+",
	"+", "+", "+", "+", "+",
};

static LineElements line_elements_ascii_rounded = {
	"-", "-", "-", "-", "-",
	"|", "|", "|", "|", "|",
	"+", "/", "\\", "\\", "/",
};

static LineElements line_elements_light = {
	"\342\224\200", "\342\225\266", "\342\225\264", "\342\224\264", "\342\224\254",
	"\342\224\202", "\342\225\267", "\342\225\265", "\342\224\244", "\342\224\234",
	"\342\224\274", "\342\224\214", "\342\224\220", "\342\224\224", "\342\224\230",
};

static LineElements line_elements_light_rounded = {
	"\342\224\200", "\342\225\266", "\342\225\264", "\342\224\264", "\342\224\254",
	"\342\224\202", "\342\225\267", "\342\225\265", "\342\224\244", "\342\224\234",
	"\342\224\274", "\342\225\255", "\342\225\256", "\342\225\257", "\342\225\260",
};

static LineElements line_elements_heavy = {
	"\342\224\201", "\342\225\272", "\342\225\270", "\342\224\273", "\342\224\263",
	"\342\224\203", "\342\225\273", "\342\225\271", "\342\224\253", "\342\224\243",
	"\342\225\213", "\342\224\217", "\342\224\223", "\342\224\227", "\342\224\233",
};

LineStyle::LineStyle(Type t)
{
	switch (t) {
		case ASCII:
			elems = &line_elements_ascii;
			break;
		case ASCII_ROUNDED:
			elems = &line_elements_ascii_rounded;
			break;
		case DEFAULT:
		case LIGHT:
			elems = &line_elements_light;
			break;
		case LIGHT_ROUNDED:
			elems = &line_elements_light_rounded;
			break;
		case HEAVY:
			elems = &line_elements_heavy;
			break;
	}
}

LineStyle::LineStyle(const LineStyle &other)
{
	elems = other.elems;
}

LineStyle &LineStyle::operator=(LineStyle &other)
{
	elems = other.elems;
	return *this;
}

LineStyle::~LineStyle()
{
}

const gchar *LineStyle::H()
{
	return elems->h;
}

const gchar *LineStyle::HBegin()
{
	return elems->h_begin;
}

const gchar *LineStyle::HEnd()
{
	return elems->h_end;
}

const gchar *LineStyle::HUp()
{
	return elems->h_up;
}

const gchar *LineStyle::HDown()
{
	return elems->h_down;
}

const gchar *LineStyle::V()
{
	return elems->v;
}

const gchar *LineStyle::VBegin()
{
	return elems->v_begin;
}

const gchar *LineStyle::VEnd()
{
	return elems->v_end;
}

const gchar *LineStyle::VLeft()
{
	return elems->v_left;
}

const gchar *LineStyle::VRight()
{
	return elems->v_right;
}

const gchar *LineStyle::Cross()
{
	return elems->cross;
}

const gchar *LineStyle::CornerTL()
{
	return elems->corner_tl;
}

const gchar *LineStyle::CornerTR()
{
	return elems->corner_tr;
}

const gchar *LineStyle::CornerBL()
{
	return elems->corner_bl;
}

const gchar *LineStyle::CornerBR()
{
	return elems->corner_br;
}
