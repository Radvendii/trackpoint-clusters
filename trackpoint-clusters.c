#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include <linux/input.h>
#include <fcntl.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>
#include <xdo.h>

// switch the DEBUG(...) definitions to enable / disable debugging logs
#define DEBUG(...) fprintf(stderr, __VA_ARGS__) // DEBUG ON
#undef  DEBUG
#define DEBUG(...)                              // DEBUG OFF

// sprinkle this into the code to see where things are going wrong
#define LINENO DEBUG("%d\n", __LINE__)

/*
 * The codes sent to the /dev/input file
 * This is true on my computer, your mileage may vary
 * You can check by writing a small script that reads
 * from the /dev/input file and prints the relevant values
 */
#define TYPE_BUTTON 1
#define TYPE_MOVE 2
#define CODE_LEFT 0x110
#define CODE_RIGHT 0x111
#define CODE_MIDDLE 0x112
#define VALUE_DOWN 1
#define VALUE_UP 0


// The period of inactivity (no mouse movements) before
// trackpoint-cluster takes control of the mouse buttons
#define ACTIVATE_DELAY ((struct timeval) { .tv_sec = 0, .tv_usec = 300000 })

// the maximum number of buttons the mouse can take
// (took this from MAXBUTTONCODES in xmodmap)
#define MAX_POINTER_MAP 256

// the keys we want to simulate
typedef enum { KEY_CONTROL, KEY_SHIFT, KEY_LEVEL3, KEY_LEVEL5 } key;

// The xdotool string associated with a key
const char *key2str (key k) {
  switch(k) {
  case KEY_CONTROL:
    return "Control_L";
  case KEY_SHIFT:
    return "Shift_R";
  case KEY_LEVEL3:
    return "ISO_Level3_Shift";
  case KEY_LEVEL5:
    return "ISO_Level5_Shift";
  }
}

// List of all keys
const key keys[] = { KEY_CONTROL,
                     KEY_SHIFT,
                     KEY_LEVEL3,
                     KEY_LEVEL5 };

bool active = false; // when false, this program is inactive and shouldn't do anything

typedef struct {
  bool left, middle, right; // is the given mousebutton pressed
  bool just_moved; // was the last event a mouse motion
} state;

// advance declaration of functions
void handle_fatal_sig(int sig);
int swallow_x_error(Display *d, XErrorEvent *e);
void init();
void deinit();
void xdotool(int, key);
bool key_should_down(state, key);
void update_state(state *s, struct input_event ie);
char *deviceFile(const char *);
char *trackpointFile();
int disable_pointer_map();
int restore_pointer_map();
void show_cursor(bool show);
bool enable_click(bool enable);
void apply_state_change(state old, state new);
void activate(bool);

// global variables because fuck it, this is a small program
// no seriously, come at me.

// last signal received which should cause program termination
// 0 signifies we have not received a fatal signal
volatile sig_atomic_t fatalSig = 0;
state s = { 0 };
Display *dpy = NULL; // needed to run xdotool commands, and XGetPointerMapping/XSetPointerMapping
xdo_t *xdo = NULL; // needed to run xdotool commands
int device_trackpoint = 0; // the trackpoint device. Use read() to get events

// the way the (first three) mouse buttons are mapped in X (a la xmodmap -e 'pointer=...')
// used for storing / disabling / restoring mouse buttons when we disable them
unsigned char pointerMap[MAX_POINTER_MAP] = { 0 };
// a constant pointer map for disabling all mouse buttons
const char disablePointerMap[MAX_POINTER_MAP] = { 0 };
// the number of pointers the mouse actually takes
int nPointerMap = 0;

struct timeval time_til_activate = { 0 };

void main() {
  init();

  for(;;) {
    struct input_event ie;
    fd_set rfds;
    int retval;

    FD_ZERO(&rfds);
    FD_SET(device_trackpoint, &rfds);

    /*
     * If we are active, this will just wait until we get a mouse event
     * If we are inactive (becaues the mouse is moving), we will stay
     * inactive, and reset the timer as long as the mouse is moving, and
     * when it stops we will wait time_til_active time, ignoring other events,
     * and then activate.
     *
     * reusing time_til_activate like this only works on linux
     * where it will be updated to reflect the amount of time not slept.
     * But this code is super linux dependent anyways, so ¯\_(ツ)_/¯
     */
    retval = select(device_trackpoint+1, &rfds, NULL, NULL, active ? NULL : &time_til_activate);

    if (retval == -1) { // error
      if (errno == EINTR) { // there was an interrupt
        if (fatalSig) { // fatal interrupt
          DEBUG("fatal interrupt; exiting\n");
          break; // break out of program loop and exit
        }
        else { // non-fatal interrupt
          DEBUG("non-fatal interrupt; continuing\n");
          continue; // retry select
        }
      }
      else { // some other error
        perror("select"); // log error and continue
      }
    }
    else if (retval == 0) { // timeout
      activate(true);
    }
    else { // device ready to read
      retval = read(device_trackpoint, &ie, sizeof(struct input_event));

      if (retval == -1) {
        perror("read");
      }

      state s_old = s;

      update_state(&s, ie);

      apply_state_change(s_old, s);
    }
  }
  deinit();
}

// handles signals which should cause termination
// this will prompt the program to clean up and exit
void handle_fatal_sig(int sig) {
  fatalSig = sig;
}

// X11 will complain if we try to show / hide the cursor twice in a row.
// We don't care. We'll still print errors for debugging though.
int swallow_x_error(Display *d, XErrorEvent *e) {
  DEBUG("got X error %d", e->error_code);
  return 0;
}

void init() {

  // Ignore X errors
  XSetErrorHandler(swallow_x_error);

  // setup X
  dpy = XOpenDisplay(NULL);

  // setup xdotool
  xdo = xdo_new(NULL);

  // start deactivated
  activate(false);

  // open input device for reading
  if ((device_trackpoint = open(trackpointFile(), O_RDONLY)) == -1) {
    perror("Error opening trackpoint device");
    exit(EXIT_FAILURE);
  }

  // make sure we cleanup from (usual) unusual termination
  signal(SIGINT, handle_fatal_sig);  // C-c
  signal(SIGTERM, handle_fatal_sig); // kill's default signal
  signal(SIGHUP, handle_fatal_sig);  // controlling terminal exits

  /*
   * Note we do not catch SIGQUIT, because IMO C-\ should be guaranteed to kill
   * the process, without any fuckery. SIGKILL also, though we don't have a
   * choice about that.
   *
   * There are lots of additional ways that you could kill the program (SIGALRM,
   * SIGUSR1, etc). But if you're doing some weird fuckery, that's on you.
   */

}

void deinit() {
  activate(false);
  close(device_trackpoint);
  XCloseDisplay(dpy);

  // after cleanup, if this was an abnormal exit, then re-raise the original signal with default handling
  if (fatalSig) {
    signal(fatalSig, SIG_DFL);
    raise(fatalSig);
  }
}

// sends a keypress event for the given key
// keydown or up depending on dir 1/-1
void xdotool(int dir, key k) {
  const char *keysym = key2str(k);
  switch(dir) {
    case 0:
      break;
    case -1:
      DEBUG("Sending %s down\n", keysym);
      xdo_send_keysequence_window_down(xdo, CURRENTWINDOW, keysym, 0);
      break;
    case 1:
      DEBUG("Sending %s up\n", keysym);
      xdo_send_keysequence_window_up(xdo, CURRENTWINDOW, keysym, 0);
      break;
  }
}

// given a state of the mouse buttons
// says whether a given key should be simulated
bool key_should_down(state s, key k) {
  switch(k) {
  case KEY_CONTROL:
    return           s.left && !s.middle;
  case KEY_SHIFT:
    return                     !s.middle &&  s.right;
  case KEY_LEVEL3:
    return          !s.left &&  s.middle &&  s.right;
  case KEY_LEVEL5:
    return           s.left &&  s.middle && !s.right;
  }
}

void update_state(state *s, struct input_event ie) {
  bool *button = NULL;

  switch(ie.type) {
    case TYPE_MOVE:
      s->just_moved = true;
      break;
    case TYPE_BUTTON:
      s->just_moved = false;

      switch (ie.code) {
        case CODE_LEFT:
          button = &s->left;
          break;
        case CODE_RIGHT:
          button = &s->right;
          break;
        case CODE_MIDDLE:
          button = &s->middle;
          break;
      }
      if (button) {
        *button = ie.value == VALUE_DOWN;
      }
      break;
  }
}

// Find the device which includes the search string
char *deviceFile(const char *searchstr) {
  char command[512];
  sprintf(command, "/usr/bin/env cat /proc/bus/input/devices | /usr/bin/env perl -00 -ne'/%s.*event(\\d+)/s && print $1'", searchstr);
  FILE *fp = popen(command, "r");
  if (fp == NULL) {
    perror("Failed to find device file (can't run command)");
    pclose(fp);
    return NULL;
  }

  char fnumstr[8];
  if (!fgets(fnumstr, sizeof(fnumstr), fp)) {
    perror("Failed to find device file (can't get output of command)");
    pclose(fp);
    return NULL;
  }

  pclose(fp);

  int fnum;
  fnum = atoi(fnumstr);

  DEBUG("%s device file number is %d\n", searchstr, fnum);

  char *fname = malloc(sizeof("/dev/input/event") + 8);
  sprintf(fname, "/dev/input/event%d", fnum);

  DEBUG("%s device file is %s\n", searchstr, fname);

  return fname;
}

char *trackpointFile() {
  return deviceFile("TrackPoint");
}

/*
 * If the mouse pointer buttons are mapped to anything (i.e. not disabled),
 * we store what they're mapped to
 * Calling this from enable_click() covers the following three cases:
 * 1) when we are first starting the program, this will initialize pointerMap
 * 2) whenever we disable the mouse, we first record what it was mapped to
 * 3) whenever we enable the mouse, we check to see if it was already enabled,
 *    and record what it was mapped to (and then remap it to that same thing)
 */
void store_pointer_map() {
  unsigned char tmpPointerMap[MAX_POINTER_MAP];

  nPointerMap = XGetPointerMapping(dpy, tmpPointerMap, MAX_POINTER_MAP);

  for (int i=0; i<nPointerMap; i++) {
    if (tmpPointerMap[i] != 0) {
      memcpy(pointerMap, tmpPointerMap, sizeof(pointerMap));
      break;
    }
  }
}

int disable_pointer_map() {
  return XSetPointerMapping(dpy, disablePointerMap, nPointerMap);
}

int restore_pointer_map() {
  return XSetPointerMapping(dpy, pointerMap, nPointerMap);
}

void show_cursor(bool show) {
  if (show) {
    XFixesShowCursor(dpy, DefaultRootWindow(dpy));
  }
  else {
    XFixesHideCursor(dpy, DefaultRootWindow(dpy));
  }
  XFlush(dpy);
}

// enable/disable the mouse buttons as normal mouse buttons
// returns whether the operation succeeded
bool enable_click(bool enable) {
  bool ret;
  store_pointer_map();

  ret = MappingSuccess == (enable ? restore_pointer_map() : disable_pointer_map());

  DEBUG("%s trackpoint button clicks: %s\n", enable ? "enabled" : "disabled", ret ? "succeeded" : "failed");

  if (ret) {
    show_cursor(enable);
  }

  return ret;
}

// Simulates keypresses based on the change of mouse button presses
void apply_state_change(state old, state new) {
  if (new.just_moved) {
    // reset timer
    time_til_activate = ACTIVATE_DELAY;
    if (active) {
      activate(false);
    }
  }
  if (active) {
    for (int i=0; i < sizeof(keys)/sizeof(keys[0]); i++) {
      int dir = key_should_down(old, keys[i]) - key_should_down(new, keys[i]);
      xdotool(dir, keys[i]);
    }
  }
}

// activate/deactivate this script
void activate(bool activate) {
  // if we are deactivating, reset the state and all the keypresses
  if (!activate) {
    state old_s = s;
    s.left = false;
    s.right = false;
    s.middle = false;
    s.just_moved = false;

    apply_state_change(old_s, s);
  }

  active = activate;

  // try to enable the mouse until we succeed
  // (user takes their hands off the mouse buttons)
  while(!enable_click(!active)) {
    sleep(1);
  }
}
