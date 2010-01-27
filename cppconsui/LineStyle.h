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

#ifndef __LINESTYLE_H__
#define __LINESTYLE_H__

#include <glib.h>

struct LineElements;

// @todo add line drawing functions?
class LineStyle
{
	public:
		enum Type {
			DEFAULT,
			ASCII,
			ASCII_ROUNDED,
			LIGHT,
			LIGHT_ROUNDED,
			HEAVY
		};

		LineStyle(Type t);
		LineStyle(const LineStyle &other);
		LineStyle &operator=(LineStyle &other);
		virtual ~LineStyle();

		void SetStyle(Type t);
		Type GetStyle();

		const gchar *H();
		const gchar *HBegin();
		const gchar *HEnd();
		const gchar *HUp();
		const gchar *HDown();
		const gchar *V();
		const gchar *VBegin();
		const gchar *VEnd();
		const gchar *VLeft();
		const gchar *VRight();
		const gchar *Cross();
		const gchar *CornerTL();
		const gchar *CornerTR();
		const gchar *CornerBL();
		const gchar *CornerBR();

	protected:
		Type type;

	private:
		LineElements *GetCurrentElems();
};

#endif /* __LINESTYLE_H__ */
