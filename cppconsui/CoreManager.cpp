/*
 * Copyright (C) 2007 by Mark Pustjens <pustjens@dds.nl>
 * Copyright (C) 2009-2012 by CenterIM developers
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
 */

#include "CoreManager.h"

#include <stdio.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#include "gettext.h"

namespace CppConsUI
{

// based on glibmm code
class SourceConnectionNode
{
  public:
    explicit inline SourceConnectionNode(const sigc::slot_base& nslot);

    static void *notify(void *data);
    static void destroy_notify_callback(void *data);
    static gboolean source_callback(void *data);

    inline void install(GSource *nsource);
    inline sigc::slot_base *get_slot();

  protected:

  private:
    sigc::slot_base slot;
    GSource *source;
};

inline SourceConnectionNode::SourceConnectionNode(
    const sigc::slot_base& nslot)
: slot(nslot)
, source(0)
{
  slot.set_parent(this, &SourceConnectionNode::notify);
}

void *SourceConnectionNode::notify(void *data)
{
  SourceConnectionNode *self
    = reinterpret_cast<SourceConnectionNode*>(data);

  /* If there is no object, this call was triggered from
   * destroy_notify_handler(), because we set self->source to 0 there. */
  if (self->source) {
    GSource *s = self->source;
    self->source = 0;
    g_source_destroy(s);

    /* Destroying the object triggers execution of destroy_notify_handler(),
     * eiter immediately or later, so we leave that to do the deletion. */
  }

  return 0;
}

void SourceConnectionNode::destroy_notify_callback(void *data)
{
  SourceConnectionNode *self = reinterpret_cast<SourceConnectionNode*>(data);

  if (self) {
    /* The GLib side is disconnected now, thus the GSource* is no longer
     * valid. */
    self->source = 0;

    delete self;
  }
}

gboolean SourceConnectionNode::source_callback(void *data)
{
  SourceConnectionNode *conn_data
    = reinterpret_cast<SourceConnectionNode*>(data);

  // recreate the specific slot from the generic slot node
  return (*static_cast<sigc::slot<bool>*>(conn_data->get_slot()))();
}

inline void SourceConnectionNode::install(GSource *nsource)
{
  source = nsource;
}

inline sigc::slot_base *SourceConnectionNode::get_slot()
{
  return &slot;
}

CoreManager *CoreManager::Instance()
{
  static CoreManager instance;
  return &instance;
}

void CoreManager::StartMainLoop()
{
  g_main_loop_run(gmainloop);
}

void CoreManager::QuitMainLoop()
{
  g_main_loop_quit(gmainloop);
}

void CoreManager::AddWindow(FreeWindow& window)
{
  Windows::iterator i = FindWindow(window);

  if (i != windows.end()) {
    /* Window is already added, AddWindow() was called to bring the window to
     * the top. */
    windows.erase(i);
    windows.push_back(&window);
  }
  else {
    windows.push_back(&window);
    window.OnScreenResized();
  }

  FocusWindow();
  Redraw();
}

void CoreManager::RemoveWindow(FreeWindow& window)
{
  Windows::iterator i;

  for (i = windows.begin(); i != windows.end(); i++)
    if (*i == &window)
      break;

  g_assert(i != windows.end());

  windows.erase(i);

  FocusWindow();
  Redraw();
}

bool CoreManager::HasWindow(const FreeWindow& window) const
{
  for (Windows::const_iterator i = windows.begin(); i != windows.end(); i++)
    if (*i == &window)
      return true;

  return false;
}

FreeWindow *CoreManager::GetTopWindow()
{
  return dynamic_cast<FreeWindow*>(input_child);
}

void CoreManager::EnableResizing()
{
  OnScreenResized();

  // register resize handler
  struct sigaction sig;
  sig.sa_handler = SignalHandler;
  sigemptyset(&sig.sa_mask);
  sig.sa_flags = SA_RESTART;
  sigaction(SIGWINCH, &sig, NULL);
}

void CoreManager::DisableResizing()
{
  // unregister resize handler
  struct sigaction sig;
  sig.sa_handler = SIG_DFL;
  sigemptyset(&sig.sa_mask);
  sig.sa_flags = 0;
  sigaction(SIGWINCH, &sig, NULL);
}

void CoreManager::OnScreenResized()
{
  if (pipe_valid && !resize_pending) {
    write(pipefd[1], "@", 1);
    resize_pending = true;
  }
}

void CoreManager::Redraw()
{
  if (!redraw_pending) {
    redraw_pending = true;
    TimeoutOnceConnect(sigc::mem_fun(this, &CoreManager::Draw), 0);
  }
}

sigc::connection CoreManager::TimeoutConnect(const sigc::slot<bool>& slot,
    unsigned interval, int priority)
{
  SourceConnectionNode *conn_node = new SourceConnectionNode(slot);
  sigc::connection connection(*conn_node->get_slot());

  GSource *source = g_timeout_source_new(interval);

  if (priority != G_PRIORITY_DEFAULT)
    g_source_set_priority(source, priority);

  g_source_set_callback(source, &SourceConnectionNode::source_callback,
      conn_node, &SourceConnectionNode::destroy_notify_callback);

  g_source_attach(source, NULL);
  g_source_unref(source); // GMainContext holds a reference

  conn_node->install(source);
  return connection;
}

sigc::connection CoreManager::TimeoutOnceConnect(const sigc::slot<void>& slot,
    unsigned interval, int priority)
{
  return TimeoutConnect(sigc::bind_return(slot, FALSE), interval, priority);
}

CoreManager::CoreManager()
: top_input_processor(NULL), io_input_channel(NULL), io_input_channel_id(0)
, resize_channel(NULL), resize_channel_id(0), pipe_valid(false), tk(NULL)
, utf8(false), gmainloop(NULL), redraw_pending(false), resize_pending(false)
{
  InputInit();

  /**
   * @todo Check the return value. Throw an exception if we can't init curses.
   */
  Curses::screen_init();

  // create a new loop
  gmainloop = g_main_loop_new(NULL, FALSE);

  DeclareBindables();
}

CoreManager::~CoreManager()
{
  InputUnInit();

  // close all windows
  size_t i = 0;
  while (i < windows.size()) {
    FreeWindow *win = windows[i];
    /* There are two possibilities, either a window is in the Close() method
     * removed from the core manager or not. We don't increase i in the first
     * case. */
    win->Close();
    if (i < windows.size() && windows[i] == win)
      i++;
  }

  Curses::clear();
  Curses::noutrefresh();
  Curses::doupdate();
  Curses::screen_finalize();
}

bool CoreManager::ProcessInput(const TermKeyKey& key)
{
  if (top_input_processor && top_input_processor->ProcessInput(key))
    return true;

  return InputProcessor::ProcessInput(key);
}

gboolean CoreManager::io_input_error(GIOChannel * /*source*/,
    GIOCondition /*cond*/)
{
  // log a critical warning and bail out if we lost stdin
  g_critical("Stdin lost!");
  exit(1);

  return TRUE;
}

gboolean CoreManager::io_input(GIOChannel * /*source*/, GIOCondition /*cond*/)
{
  if (io_input_timeout_conn.connected())
    io_input_timeout_conn.disconnect();

  termkey_advisereadable(tk);

  TermKeyKey key;
  TermKeyResult ret;
  while ((ret = termkey_getkey(tk, &key)) == TERMKEY_RES_KEY) {
    if (key.type == TERMKEY_TYPE_UNICODE && !utf8) {
      gsize bwritten;
      GError *err = NULL;
      char *utf8;

      // convert data from user charset to UTF-8
      if (!(utf8 = g_locale_to_utf8(key.utf8, -1, NULL, &bwritten, &err))) {
        g_warning(_("Error converting input to UTF-8 (%s)."), err->message);
        g_clear_error(&err);
        continue;
      }

      memcpy(key.utf8, utf8, bwritten + 1);
      g_free(utf8);

      key.code.codepoint = g_utf8_get_char(key.utf8);
    }

    ProcessInput(key);
  }
  if (ret == TERMKEY_RES_AGAIN) {
    int wait = termkey_get_waittime(tk);
    io_input_timeout_conn = TimeoutOnceConnect(sigc::mem_fun(this,
          &CoreManager::io_input_timeout), wait);
  }

  return TRUE;
}

void CoreManager::io_input_timeout()
{
  TermKeyKey key;
  if (termkey_getkey_force(tk, &key) == TERMKEY_RES_KEY) {
    /* This should happen only for Esc key, so no need to do locale->utf8
     * conversion. */
    ProcessInput(key);
  }
}

gboolean CoreManager::resize_input(GIOChannel *source, GIOCondition /*cond*/)
{
  char buf[1024];
  gsize bytes_read;
  GError *err = NULL;
  g_io_channel_read_chars(source, buf, sizeof(buf), &bytes_read, &err);
  if (err)
    g_clear_error(&err);

  if (resize_pending)
    Resize();

  return TRUE;
}

void CoreManager::InputInit()
{
  // init libtermkey
  TERMKEY_CHECK_VERSION;
  if (!(tk = termkey_new(STDIN_FILENO, TERMKEY_FLAG_NOTERMIOS))) {
    g_critical(_("Libtermkey initialization failed."));
    exit(1);
  }
  termkey_set_canonflags(tk, TERMKEY_CANON_DELBS);
  utf8 = g_get_charset(NULL);

  io_input_channel = g_io_channel_unix_new(STDIN_FILENO);
  // set channel encoding to NULL so it can be unbuffered
  g_io_channel_set_encoding(io_input_channel, NULL, NULL);
  g_io_channel_set_buffered(io_input_channel, FALSE);
  g_io_channel_set_close_on_unref(io_input_channel, TRUE);

  io_input_channel_id = g_io_add_watch_full(io_input_channel, G_PRIORITY_HIGH,
      static_cast<GIOCondition>(G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_PRI),
      io_input_, this, NULL);
  g_io_add_watch_full(io_input_channel, G_PRIORITY_HIGH, G_IO_NVAL,
      io_input_error_, this, NULL);
  g_io_channel_unref(io_input_channel);

  // screen resizing
  if (!pipe(pipefd)) {
    pipe_valid = true;
    resize_channel = g_io_channel_unix_new(pipefd[0]);
    g_io_channel_set_encoding(resize_channel, NULL, NULL);
    g_io_channel_set_buffered(resize_channel, FALSE);
    g_io_channel_set_close_on_unref(resize_channel, TRUE);

    resize_channel_id = g_io_add_watch_full(resize_channel, G_PRIORITY_HIGH,
        G_IO_IN, resize_input_, this, NULL);
  }
}

void CoreManager::InputUnInit()
{
  termkey_destroy(tk);
  tk = NULL;

  g_source_remove(io_input_channel_id);
  io_input_channel_id = 0;
  g_io_channel_unref(io_input_channel);
  io_input_channel = NULL;

  if (pipe_valid) {
    g_source_remove(resize_channel_id);
    resize_channel_id = 0;
    g_io_channel_unref(resize_channel);
    resize_channel = NULL;
    close(pipefd[0]);
    close(pipefd[1]);
  }
}

void CoreManager::SignalHandler(int signum)
{
  if (signum == SIGWINCH)
    COREMANAGER->OnScreenResized();
}

void CoreManager::Resize()
{
  struct winsize size;

  resize_pending = false;

  if (ioctl(fileno(stdout), TIOCGWINSZ, &size) >= 0) {
    Curses::resizeterm(size.ws_row, size.ws_col);

    // make sure everything is redrawn from the scratch
    Curses::clear();
  }

  signal_resize();
  Redraw();
}

void CoreManager::Draw()
{
  if (!redraw_pending)
    return;

  Curses::erase();
  Curses::noutrefresh();

  // non-focusable -> normal -> top
  for (Windows::iterator i = windows.begin(); i != windows.end(); i++)
    if ((*i)->GetType() == FreeWindow::TYPE_NON_FOCUSABLE)
      (*i)->Draw();

  for (Windows::iterator i = windows.begin(); i != windows.end(); i++)
    if ((*i)->GetType() == FreeWindow::TYPE_NORMAL)
      (*i)->Draw();

  for (Windows::iterator i = windows.begin(); i != windows.end(); i++)
    if ((*i)->GetType() == FreeWindow::TYPE_TOP)
      (*i)->Draw();

  // copy virtual ncurses screen to the physical screen
  Curses::doupdate();

#ifdef DEBUG
  const Curses::Stats *stats = Curses::get_stats();
  g_debug("newpad calls: %u, newwin calls: %u, subpad calls: %u",
      stats->newpad_calls, stats->newwin_calls, stats->subpad_calls);
  Curses::reset_stats();
#endif // DEBUG

  redraw_pending = false;
}

CoreManager::Windows::iterator CoreManager::FindWindow(FreeWindow& window)
{
  return std::find(windows.begin(), windows.end(), &window);
}

void CoreManager::FocusWindow()
{
  // check if there are any windows left
  FreeWindow *win = NULL;
  Windows::reverse_iterator i;

  // try to find a top window first
  for (i = windows.rbegin(); i != windows.rend(); i++)
    if ((*i)->GetType() == FreeWindow::TYPE_TOP) {
      win = *i;
      break;
    }

  // normal windows
  if (!win)
    for (i = windows.rbegin(); i != windows.rend(); i++)
      if ((*i)->GetType() == FreeWindow::TYPE_NORMAL) {
        win = *i;
        break;
      }

  FreeWindow *focus = dynamic_cast<FreeWindow*>(GetInputChild());
  if (!win || win != focus) {
    // take the focus from the old window with the focus
    if (focus) {
      focus->UngrabFocus();
      ClearInputChild();
    }

    // give the focus to the window
    if (win) {
      SetInputChild(*win);
      win->RestoreFocus();
    }
    signal_top_window_change();
  }
}

void CoreManager::RedrawScreen()
{
  // make sure everything is redrawn from the scratch
  Curses::clear();

  Redraw();
}

void CoreManager::DeclareBindables()
{
  DeclareBindable("coremanager", "redraw-screen",
      sigc::mem_fun(this, &CoreManager::RedrawScreen),
      InputProcessor::BINDABLE_OVERRIDE);
}

} // namespace CppConsUI

/* vim: set tabstop=2 shiftwidth=2 textwidth=78 expandtab : */
