#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// http://www.uninformativ.de/blog/postings/2017-04-02/0/POSTING-en.html

const char *CLIPBOARD_SELECTION = "CLIPBOARD";
const char *MIDDLE_MOUSE_SELECTION = "PRIMARY";

typedef struct clipboard_data_t {
  char *buffer;
  int offset;
  int length;
  int capacity;
} clipboard_data;

clipboard_data g_clipboard = {0};

void deny_request(Display *display, XSelectionRequestEvent *selection_request) {
  XSelectionEvent selection_event;
  char *atom_name;
  atom_name = XGetAtomName(display, selection_request->target);
  // fprintf(stderr, "denying request of type %s\n", atom_name);
  if (atom_name == NULL)
    XFree(atom_name);
  selection_event.type = SelectionNotify;
  selection_event.requestor = selection_request->requestor;
  selection_event.selection = selection_request->selection;
  selection_event.target = selection_request->target;
  selection_event.property = None;
  selection_event.time = selection_request->time;
  XSendEvent(display, selection_request->requestor, True, NoEventMask,
             (XEvent *)&selection_event);
}

void send_data(Display *display, XSelectionRequestEvent *selection_request,
               Atom utf8_selection) {
  XSelectionEvent selection_event;
  // char *atom_name;
  // atom_name = XGetAtomName(display, selection_request->target);
  /*fprintf(stderr, "Sending data to window 0x%lx ,property %s\n",
          selection_request->requestor, atom_name);
  if (atom_name)
    XFree(atom_name);
  */
  XChangeProperty(display, selection_request->requestor,
                  selection_request->property, utf8_selection, 8,
                  PropModeReplace, (unsigned char *)g_clipboard.buffer,
                  g_clipboard.length);
  selection_event.type = SelectionNotify;
  selection_event.requestor = selection_request->requestor;
  selection_event.selection = selection_request->selection;
  selection_event.target = selection_request->target;
  selection_event.property = selection_request->property;
  selection_event.time = selection_request->time;
  XSendEvent(display, selection_request->requestor, True, NoEventMask,
             (XEvent *)&selection_event);
}

int main() {
  // init data buffer
  g_clipboard.buffer = (char *)malloc(1024);
  memset(g_clipboard.buffer, 0, 1024);
  g_clipboard.capacity = 1024;
  g_clipboard.length = 0;
  g_clipboard.offset = 0;
  // Connect to X Server
  Display *display = XOpenDisplay(NULL);
  if (display == NULL) {
    errno = ENOENT;
    perror("ERROR: Failed to connect to X server ::");
    return -1;
  }
  // Get Root Window
  Window root_window = RootWindow(display, DefaultScreen(display));
  // Create Our Window
  Window our_window =
      XCreateSimpleWindow(display, root_window, -10, -10, 1, 1, 0, 0, 0);
  // Create Atom For Clipboard
  Atom clipboard_selection = XInternAtom(display, CLIPBOARD_SELECTION, false);
  Atom utf8_selection = XInternAtom(display, "UTF8_STRING", false);
  Atom targets_selection = XInternAtom(display, "TARGETS", false);
  // Get Previous Owner
  Window previous_owner = XGetSelectionOwner(display, clipboard_selection);
  // Read From Stdin And Set Clipboard Data As That Data
  for (;;) {
    char tmp[1024];
    ssize_t nread = read(0, tmp, 1024);
    // EOF BREAK
    if (nread == 0)
      break;
    // update buffer length needed
    g_clipboard.length += nread;
    if (g_clipboard.capacity < g_clipboard.length) {
      g_clipboard.buffer = realloc(g_clipboard.buffer, g_clipboard.length * 2);
    }
    memcpy(g_clipboard.buffer + g_clipboard.offset, tmp, nread);
    g_clipboard.offset += nread;
  }
  // Claim Clipboard
  XSetSelectionOwner(display, clipboard_selection, our_window, CurrentTime);
  // fork and child becomes the server
  int pid = fork();
  if (pid == -1) {
    exit(EXIT_FAILURE);
  }
  if (pid != 0)
    exit(EXIT_SUCCESS);
  // Child serves the clipboard
  for (;;) {
    XEvent event;
    XNextEvent(display, &event);
    XSelectionRequestEvent *selection_req = NULL;
    switch (event.type) {
    case SelectionClear:
      goto Cleanup;
    case SelectionRequest:
      // we send back the data no matter the request type lol
      selection_req = (XSelectionRequestEvent *)&event.xselectionrequest;
      send_data(display, selection_req, utf8_selection);
      break;
    }
  }
Cleanup:
  free(g_clipboard.buffer);
  XCloseDisplay(display);
  return 0;
}
