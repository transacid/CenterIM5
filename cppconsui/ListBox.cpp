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

#include "ListBox.h"
#include "Keys.h"

#include "ScrollPane.h"

ListBox::ListBox(Widget& parent, int x, int y, int w, int h)
: AbstractListBox(parent, x, y, w, h)
, movingwidget(false)
{

}

ListBox::~ListBox()
{
}

void ListBox::AddWidget(Widget *widget)
{
	int y;

	y = GetScrollHeight();
	
	movingwidget = true;
	widget->Move(0, y);
	movingwidget = false;
	SetScrollHeight(y + widget->Height());
	AbstractListBox::AddWidget(widget);
}

void ListBox::RemoveWidget(Widget *widget)
{
	Children::iterator i;
	Widget *w = NULL;
	int y = 0;

	g_return_if_fail(widget != NULL);

	AbstractListBox::RemoveWidget(widget);

	for (i = ChildrenBegin(); i != ChildrenEnd(); i++) {
		w = (*i).first;
		widget->Move(0, y);
	}

	SetScrollHeight(y + widget->Height());
}
