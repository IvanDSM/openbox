#ifndef __openbox_h
#define __openbox_h

#include "render/render.h"
#include "render/theme.h"

#ifdef USE_LIBSN
#  define SN_API_NOT_YET_FROZEN
#  include <libsn/sn.h>
#endif

#include <glib.h>
#include <X11/Xlib.h>

extern RrInstance *ob_rr_inst;
extern RrTheme    *ob_rr_theme;

/*! The X display */
extern Display *ob_display; 

#ifdef USE_LIBSN
SnDisplay *ob_sn_display;
#endif

/*! The number of the screen on which we're running */
extern int      ob_screen;
/*! The root window */
extern Window   ob_root;

/*! States of execution for Openbox */
typedef enum {
    State_Starting,
    State_Exiting,
    State_Running
} State;

/* The state of execution of the window manager */
extern State ob_state;

/*! When set to true, Openbox will exit */
extern gboolean ob_shutdown;
/*! When set to true, Openbox will restart instead of shutting down */
extern gboolean ob_restart;
/*! When restarting, if this is not NULL, it will be executed instead of
  restarting Openbox. */
extern char *ob_restart_path;

/*! Runtime option to specify running on a remote display */
extern gboolean ob_remote;
/*! Runtime option to run in synchronous mode */
extern gboolean ob_sync;

typedef struct Cursors {
    Cursor ptr;
    Cursor busy;
    Cursor move;
    Cursor bl;
    Cursor br;
    Cursor tl;
    Cursor tr;
    Cursor t;
    Cursor r;
    Cursor b;
    Cursor l;
} Cursors;
extern Cursors ob_cursors;

/*! The path of the rc file. If NULL the default paths are searched for one. */
extern char *ob_rc_path;

/* cuz i have nowhere better to put it right now... */
gboolean ob_pointer_pos(int *x, int *y);

#endif
