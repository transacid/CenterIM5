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

//TODO: configurable path using ./configure
#define CIM_CONFIG_PATH		".centerim"

#define CONF_PLUGIN_SAVE_PREF	"/centerim/plugins/loaded"

#define EXCEPTION_NONE			0
#define EXCEPTION_PURPLE_CORE_INIT	100

#define INPUT_BUF_SIZE			64	//TODO is this reasonable? (pasting needs a somewhat larger buffer to be efficient)

#include "CenterIM.h"

#include <cppconsui/WindowManager.h>

#include <libpurple/prefs.h>
#include <libpurple/core.h>
#include <libpurple/plugin.h>
#include <libpurple/util.h>
#include <libpurple/pounce.h>
#include <libpurple/debug.h>
#include <libpurple/savedstatuses.h>
#include <glib.h>

#include <libintl.h>

//TODO move inside CenterIM object
static PurpleDebugUiOps logbuf_debug_ui_ops =
{
	CenterIM::tmp_purple_print_,
	CenterIM::tmp_isenabled_,
	NULL,
	NULL,
	NULL,
	NULL
};

CenterIM* CenterIM::instance = NULL;
std::vector<CenterIM::logbuf_item>* CenterIM::logbuf = NULL;

CenterIM* CenterIM::Instance(void)
{
	if (!instance) instance = new CenterIM();
	return instance;
}

void CenterIM::Delete(void)
{
	if (instance) {
		delete instance;
		instance = NULL;
	}
}

void CenterIM::Run(void)
{
	g_main_loop_run(gmainloop);
}

//TODO: move next two static structs inside the CenterIM object
static PurpleCoreUiOps centerim_core_ui_ops =
{
	NULL, //CenterIM::ui_prefs_init_,
	NULL, //CenterIM::debug_ui_init_,   
	NULL, //CenterIM::ui_init_,
	CenterIM::ui_uninit_,
};

static PurpleEventLoopUiOps centerim_glib_eventloops =
{
	CenterIM::timeout_add,
	CenterIM::timeout_remove,
	CenterIM::input_add,
	CenterIM::input_remove,
	NULL
};

CenterIM::CenterIM()
: channel(NULL)
, channel_id(0)
{
	char *path;
	/* set the configuration file location */
	path = g_build_filename(purple_home_dir(), CIM_CONFIG_PATH, NULL);
	purple_util_set_user_dir(path);
	g_free(path);

	/* This does not disable debugging, but rather it disables printing to stdout */
	purple_debug_set_enabled(FALSE);

	/* This catches and buffers libpurple debug messages until the Log object
	 * can be instantiated
	 * */
	purple_debug_set_ui_ops(&logbuf_debug_ui_ops);

	/* Set the core-uiops, which is used to
	 *      - initialize the ui specific preferences.
	 *      - initialize the debug ui.
	 *      - initialize the ui components for all the modules.
	 *      - uninitialize the ui components for all the modules when the core terminates.
	 * */
	purple_core_set_ui_ops(&centerim_core_ui_ops);
	/* Set the uiops for the eventloop. */
	purple_eventloop_set_ui_ops(&centerim_glib_eventloops);

	/* In case we ever write centerim specific plugins */
	path = g_build_filename(purple_user_dir(), "plugins", NULL);
	purple_plugins_add_search_path(path);
	g_free(path);

	if (!purple_core_init("centerim")) {
		/* can't do much without libpurple */
		throw (EXCEPTION_PURPLE_CORE_INIT);
	}

	/* normally these three functions are called 
	 * by centerim_core_ui_ops(), but since we need
	 * a CenterIM object for them we have to call them
	 * manually after the core has been initialized
	 * */
	windowmanager = WindowManager::Instance();
	ui_prefs_init();
	debug_ui_init();
	ui_init();
	io_init();

	/* create a new loop */
	//TODO perhaps replace by Glib::Main ?? (does that even exist?)
	gmainloop = g_main_loop_new(NULL, FALSE);
}

CenterIM::~CenterIM()
{
	/* clean up */
	io_uninit();
	purple_core_quit();
	log->Delete();
	windowmanager->Delete();
}

void CenterIM::ui_prefs_init(void)
{
	conf = Conf::Instance();
}

void CenterIM::debug_ui_init(void)
{
	std::vector<logbuf_item>::iterator i;
	logbuf_item *item;
	
	windowmanager->Add(log = Log::Instance());

	if (logbuf) {
		for (i = logbuf->begin(); i != logbuf->end(); i++) {
			item = &(*i);
			log->purple_print(item->level, item->category, item->arg_s);
			g_free(item->category);
			g_free(item->arg_s);
		}

		delete logbuf;
		logbuf = NULL;
	}
}

void CenterIM::ui_init(void)
{
	//TODO when these objecs are windows, add them to the windowmanager
//	windowmanager->Add(accounts = new Accounts());
//	windowmanager->Add(connections = new Connections());
	windowmanager->Add(buddylist = BuddyList::Instance());
//	windowmanager->Add(conversations = Conversations::Instance());
//	windowmanager->Add(transfers = new Transfers());

	accounts = new Accounts();
	connections = new Connections();
//	buddylist = new BuddyList();
	conversations = Conversations::Instance();
	transfers = new Transfers();
}

void CenterIM::ui_uninit(void)
{
	//TODO when these objecs are windows, remove them from the windowmanager
//	windowmanager->Remove(accounts);
	delete accounts; accounts = NULL;

//	windowmanager->Remove(connections);
	delete connections; connections = NULL;

	windowmanager->Remove(buddylist);
	buddylist->Delete(); buddylist = NULL;

//	windowmanager->Remove(conversations);
	conversations->Delete(); conversations = NULL;

//	windowmanager->Remove(transfers);
	delete transfers; transfers = NULL;

	conf->Delete(); conf = NULL;
}

guint CenterIM::timeout_add(guint interval, GSourceFunc function, gpointer data)
{
	return g_timeout_add(interval, function, data);
}

gboolean CenterIM::timeout_remove(guint handle)
{
	return g_source_remove(handle);
}

#define PURPLE_GLIB_READ_COND  (G_IO_IN | G_IO_HUP | G_IO_ERR)
#define PURPLE_GLIB_WRITE_COND (G_IO_OUT | G_IO_HUP | G_IO_ERR | G_IO_NVAL)

//TODO move inside CenterIM object
typedef struct _CenterIMGLibIOClosure {
	PurpleInputFunction function;
	guint result;
	gpointer data;
} CenterIMGLibIOClosure;


guint CenterIM::input_add(int fd, PurpleInputCondition condition,
	PurpleInputFunction function, gpointer data)
{
	CenterIMGLibIOClosure *closure = g_new0(CenterIMGLibIOClosure, 1);
	GIOChannel *channel;
	int cond = 0;

	closure->function = function;
	closure->data = data;

	if (condition & PURPLE_INPUT_READ)
		cond |= PURPLE_GLIB_READ_COND;
	if (condition & PURPLE_INPUT_WRITE)
		cond |= PURPLE_GLIB_WRITE_COND;

	channel = g_io_channel_unix_new(fd);
	closure->result = g_io_add_watch_full(channel, G_PRIORITY_DEFAULT, (GIOCondition)cond,
				purple_glib_io_input, closure, purple_glib_io_destroy);

	g_io_channel_unref(channel);
	return closure->result;
}

gboolean CenterIM::input_remove(guint handle)
{
	return g_source_remove(handle);
}

gboolean CenterIM::purple_glib_io_input(GIOChannel *source, GIOCondition condition, gpointer data)
{
	CenterIMGLibIOClosure *closure = (CenterIMGLibIOClosure*)data;
	int purple_cond = 0;

	if (condition & PURPLE_GLIB_READ_COND)
		purple_cond |= PURPLE_INPUT_READ;
	if (condition & PURPLE_GLIB_WRITE_COND)
		purple_cond |= PURPLE_INPUT_WRITE;

	closure->function(closure->data, g_io_channel_unix_get_fd(source),
			  (PurpleInputCondition)purple_cond);

	return TRUE;
}

void CenterIM::purple_glib_io_destroy(gpointer data)
{
	g_free(data);
}

/* This catches and buffers libpurple debug messages until the Log object
 * can be instantiated
 * */
void CenterIM::tmp_purple_print_(PurpleDebugLevel level, const char *category, const char *arg_s)
{
	if (!logbuf)
		logbuf = new std::vector<logbuf_item>;

	logbuf_item item;
	item.level = level;
	item.category = g_strdup(category);
	item.arg_s = g_strdup(arg_s);
	logbuf->push_back(item);
}

/* Xerox (finch saves us a little of work here) */
void CenterIM::io_init(void)
{
	curs_set(0);
	nonl();
//        g_io_channel_set_encoding(channel, NULL, NULL); //TODO how to convert input to UTF-8 automatically? perhaps in io_input
//        g_io_channel_set_buffered(channel, FALSE); //TODO not needed?
//        g_io_channel_set_flags(channel, G_IO_FLAG_NONBLOCK, NULL ); //TODO not needed?

	channel = g_io_channel_unix_new(STDIN_FILENO);
	g_io_channel_set_close_on_unref(channel, TRUE);

	channel_id = g_io_add_watch_full(channel,  G_PRIORITY_HIGH,
		(GIOCondition)(G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_PRI), io_input_, this, NULL);

	g_io_add_watch_full(channel, G_PRIORITY_HIGH,
		(G_IO_NVAL), io_input_error_, this, NULL);

	g_io_channel_unref(channel);

	//g_printerr("gntmain: setting up IO\n"); //TODO is this an error??
}

void CenterIM::io_uninit(void)
{
	g_source_remove(channel_id);
	channel_id = 0;
	g_io_channel_unref(channel);
	channel = NULL;
}

gboolean CenterIM::io_input_error(GIOChannel *source, GIOCondition cond)
{
	g_source_remove(channel_id);
	g_io_channel_unref(source);

	//TODO log an error, apparantly we lost stdin, which shoudn't happen
	//we also try to reopen stdin

	channel = NULL;
	io_init();
	return TRUE;
}

gboolean CenterIM::io_input(GIOChannel *source, GIOCondition cond)
{
	char buf[INPUT_BUF_SIZE];
	int rd = read(STDIN_FILENO, buf, INPUT_BUF_SIZE-1);

	//TODO convert to UTF-8 here?

	/* Below this line we assume all input has been converted
	 * to and UTF-8 encoded multibyte string
	 * */

	if (rd < 0) {
		//TODO what to do on error?
	} else if (rd == 0) {
		//TODO this should not happen
	}

	buf[rd] = '\0'; //TODO remove
	log->Write(PURPLE_DEBUG_MISC, "input: %s (%02x %02x %02x)", buf, buf[0], buf[1], buf[2]); //TODO remove

	ProcessInput(buf, rd);

	return TRUE;
}

void CenterIM::ProcessInput(char *input, int bytes)
{
	int eaten;

	//while (bytes > 0) {
		/* Process overriding key-combo's */
		/*
		eaten = ProcessOverrides();
		keys += eaten;
		bytes -= eaten;
		if (bytes < 1) break;
		*/
		switch (input[0]) {
		case 'q':
			//TODO remove this case when keybindings are done
			g_main_loop_quit(gmainloop);
			//keys++; bytes--;
			break;//continue;
		case 0x0c: // ^L, form feed, redraw
			windowmanager->Draw();
			input++;
			bytes--;
			break;//continue;
		}

		/* Process keys */
		eaten = windowmanager->ProcessInput(input, bytes);
		/*keys += eaten;
		bytes -= eaten;
		if (bytes < 1) break;*/

		/* Process all key-combo's */
		/*
		eaten = ProcessOverrides();
		keys += eaten;
		bytes -= eaten;
		if (bytes < 1) break;

		eaten = ProcessKeyCombos();
		keys += eaten;
		bytes -= eaten;
		
		if (bytes < 1) break;
		*/
	//}

	if (bytes < 0)
		; //TODO throw exception about more processed input
		// than there was input available
}
