#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include <arpa/inet.h>

#include "../funcs.h"
#include "../structs.h"
#include <ragnar/api.h>

#define SOCKPATH "/tmp/ragnar_socket"
#define MSGSIZE 256

#include "sockets.h"

typedef void (*cmd_handler_t)(state_t* s, const uint8_t* data, int32_t clientfd);

typedef struct {
  cmd_handler_t handler;
  uint32_t len;
  RgCommandType type;
} cmd_data_t;

static client_t* extractclient(state_t* s, const uint8_t* data);
static void sendv2(int32_t clientfd, state_t* s, v2_t* v);

static void cmdterminate(state_t* s, const uint8_t* data, int32_t clientfd);
static void cmdgetwins(state_t* s, const uint8_t* data, int32_t clientfd);
static void cmdkillwin(state_t* s, const uint8_t* data, int32_t clientfd);
static void cmdfocuswin(state_t* s, const uint8_t* data, int32_t clientfd);
static void cmdnextwin(state_t* s, const uint8_t* data, int32_t clientfd);
static void cmdfirstwin(state_t* s, const uint8_t* data, int32_t clientfd);
static void cmdgetfocus(state_t* s, const uint8_t* data, int32_t clientfd);
static void cmdgetmonfocus(state_t* s, const uint8_t* data, int32_t clientfd);
static void cmdgetcursor(state_t* s, const uint8_t* data, int32_t clientfd);
static void cmdgetwinarea(state_t* s, const uint8_t* data, int32_t clientfd);

static void handlecmd(state_t* s, uint8_t cmdid, const uint8_t* data, 
                      size_t len, int32_t clientfd);

static cmd_data_t cmdhandlers[] = {
  { .handler = cmdterminate,    .len = sizeof(uint32_t),      .type = RgCommandTerminate },
  { .handler = cmdgetwins,      .len = 0,                     .type = RgCommandGetWindows },
  { .handler = cmdkillwin,      .len = sizeof(RgWindow),      .type = RgCommandKillWindow },
  { .handler = cmdfocuswin,     .len = sizeof(RgWindow),      .type = RgCommandFocusWindow },
  { .handler = cmdnextwin,      .len = sizeof(RgWindow),      .type = RgCommandNextWindow },
  { .handler = cmdfirstwin,     .len = 0,                     .type = RgCommandFirstWindow },
  { .handler = cmdgetfocus,     .len = 0,                     .type = RgCommandGetFocus },
  { .handler = cmdgetmonfocus,  .len = 0,                     .type = RgCommandGetMonitorFocus },
  { .handler = cmdgetcursor,    .len = 0,                     .type = RgCommandGetCursor },
  { .handler = cmdgetwinarea,   .len = sizeof(RgWindow),      .type = RgCommandGetWindowArea },
};

client_t*
extractclient(state_t* s, const uint8_t* data) {
  RgWindow win;
  memcpy(&win, data, sizeof(RgWindow));

  client_t* cl = clientfromwin(s, win);
  if(!cl) {
    logmsg(s, LogLevelError, 
           "ipc: window %i is invalid, not killing.");
    return NULL;
  }

  return cl;
}

void
sendv2(int32_t clientfd, state_t* s, v2_t* v) {
  Rgv2 v_rg = (Rgv2){
    .x = v->x, 
    .y = v->y
  };

  if(write(clientfd, &v_rg.x, sizeof(float)) == -1) {
    logmsg(s, LogLevelError, 
           "ipc: failed to send vec2 X position.");
  }

  if(write(clientfd, &v_rg.y, sizeof(float)) == -1) {
    logmsg(s, LogLevelError, 
           "ipc: failed to send vec2 Y position.");
  }
}

void 
cmdterminate(state_t* s, const uint8_t* data, int32_t clientfd) {
  (void)clientfd;
  uint32_t exitcode;
  memcpy(&exitcode, data, sizeof(uint32_t));

  logmsg(s, LogLevelTrace, "ipc: RgCommandTerminate: received command with exit code: %i", exitcode);
  terminate(s, exitcode);
}

void 
cmdgetwins(state_t* s, const uint8_t* data, int32_t clientfd) {
  (void)data;
  logmsg(s, LogLevelTrace, "ipc: RgCommandGetWindows: received command.\n"); 


  uint32_t numwins = 0;
  for(client_t* cl = s->clients; cl != NULL; cl = cl->next) {
    numwins++;
  }
  RgWindow wins[numwins];
  uint32_t i = 0;
  for(client_t* cl = s->clients; cl != NULL; cl = cl->next) {
    wins[i++] = cl->win;
  }

  if(write(clientfd, &numwins, sizeof(numwins)) == -1) {
    logmsg(s, LogLevelError, 
           "ipc: RgCommandGetWindows: failed to send number of client windows.");
  }
  if(write(clientfd, wins, sizeof(wins)) == -1) {
    logmsg(s, LogLevelError, 
           "ipc: RgCommandGetWindows: failed to send array of client windows.");
  }
}

void 
cmdkillwin(state_t* s, const uint8_t* data, int32_t clientfd) {
  (void)clientfd;
  client_t* cl;
  logmsg(s, LogLevelTrace, 
         "ipc: RgCommandKillWindow: received command.");
  if(!(cl = extractclient(s, data))) {
    logmsg(s, LogLevelError, 
           "ipc: RgCommandKillWindow: No client associated with window.");
    return;
  }
  killclient(s, cl);
}

void 
cmdfocuswin(state_t* s, const uint8_t* data, int32_t clientfd) {
  (void)clientfd;
  client_t* cl;
  logmsg(s, LogLevelTrace, 
         "ipc: RgCommandFocusWindow: received command.");
  if(!(cl = extractclient(s, data))) {
    logmsg(s, LogLevelError, 
           "ipc: RgCommandFocusWindow: No client associated with window.");
    return;
  }
  focusclient(s, cl);
}

void 
cmdnextwin(state_t* s, const uint8_t* data, int32_t clientfd) {
  (void)clientfd;
  client_t* cl;
  logmsg(s, LogLevelTrace, 
         "ipc: RgCommandNextWindow: received command.");
  if(!(cl = extractclient(s, data))) {
    logmsg(s, LogLevelError, 
           "ipc: RgCommandNextWindow: No client associated with window.");
    return;
  }

  RgWindow next = RG_INVALID_WINDOW;
  if(cl->next) {
    next = cl->next->win;
  }

  if(write(clientfd, &next, sizeof(next)) == -1) {
    logmsg(s, LogLevelError, 
           "ipc: RgCommandNextWindow: failed to send next window.");
  }
}

void 
cmdfirstwin(state_t* s, const uint8_t* data, int32_t clientfd) {
  (void)data;
  logmsg(s, LogLevelTrace, 
         "ipc: RgCommandFirstWindow: received command.");

  RgWindow first = RG_INVALID_WINDOW;
  if(s->clients) {
    first = s->clients->win ? (RgWindow)s->clients->win : RG_INVALID_WINDOW;
  }

  if(write(clientfd, &first, sizeof(first)) == -1) {
    logmsg(s, LogLevelError, 
           "ipc: RgCommandFirstWindow: failed to send first client window.");
  }
}

void 
cmdgetfocus(state_t* s, const uint8_t* data, int32_t clientfd) {
  (void)data;
  logmsg(s, LogLevelTrace, 
         "ipc: RgCommandGetFocus: received command.");
  
  RgWindow focus = s->focus ? (RgWindow)s->focus->win : RG_INVALID_WINDOW;

  if(write(clientfd, &focus, sizeof(focus)) == -1) {
    logmsg(s, LogLevelError, 
           "ipc: RgCommandGetFocus: failed to send first client window.");
  }
}

void 
cmdgetmonfocus(state_t* s, const uint8_t* data, int32_t clientfd) {
  (void)data;
  logmsg(s, LogLevelTrace, 
         "ipc: RgCommandGetMonitorFocus: received command.");
  

  int32_t focus = -1;
  if(s->monfocus) {
    focus = (int32_t)s->monfocus->idx;
  }

  if(write(clientfd, &focus, sizeof(focus)) == -1) {
    logmsg(s, LogLevelError, 
           "ipc: RgCommandGetMonitorFocus: failed to send first client window.");
  }
}

void 
cmdgetcursor(state_t* s, const uint8_t* data, int32_t clientfd) {
  (void)data;
  logmsg(s, LogLevelTrace, 
         "ipc: RgCommandGetCursor: received command.");

  bool success;
  v2_t cursor = cursorpos(s, &success);
  if(!success) {
    logmsg(s, LogLevelError, 
           "ipc: RgCommandGetCursor: failed to get cursor position.");
    return;
  }

  sendv2(clientfd, s, &cursor);
}
void 
cmdgetwinarea(state_t* s, const uint8_t* data, int32_t clientfd) {
  (void)data;
  logmsg(s, LogLevelTrace, 
         "ipc: RgCommandGetWindowArea: received command.");
  client_t* cl;
  if(!(cl = extractclient(s, data))) {
    logmsg(s, LogLevelError, 
           "ipc: RgCommandGetWindowArea: No client associated with window.");
    return;
  }

  sendv2(clientfd, s, &cl->area.pos);
  sendv2(clientfd, s, &cl->area.size);
}

void 
handlecmd(state_t* s, uint8_t cmdid, const uint8_t* data, size_t len, 
          int32_t clientfd) {
  bool exec = false;
  for(uint32_t i = 0; i < sizeof(cmdhandlers) / sizeof(cmd_data_t); i++) {
    if(cmdid == (uint8_t)cmdhandlers[i].type && len == cmdhandlers[i].len) {
      cmdhandlers[i].handler(s, data, clientfd);
      exec = true;
    }
  } 
  if(!exec) {
    logmsg(s, LogLevelWarn, 
           "ipc: Received invalid command with unknown command ID or invalid length (ID: %i).", cmdid);
  }
}

void* 
ipcserverthread(void* arg) {
  state_t* s = (state_t*)arg;

  int32_t serverfd, clientfd;
  struct sockaddr_un addr;
  uint8_t buf[MSGSIZE];
  ssize_t numread;

  // Create a Unix domain socket
  serverfd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (serverfd < 0) {
    logmsg(s, LogLevelError, "ipc: Failed to create unix domain socket for IPC.");
    terminate(s, EXIT_FAILURE);
  }

  // Set up the address structure
  memset(&addr, 0, sizeof(struct sockaddr_un));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, SOCKPATH, sizeof(addr.sun_path) - 1);

  // Remove any existing socket file
  unlink(SOCKPATH);

  // Bind the socket
  if (bind(serverfd, (struct sockaddr*)&addr, sizeof(struct sockaddr_un)) < 0) {
    logmsg(s, LogLevelError, "ipc: Failed to bind the IPC socket.");
    close(serverfd);
    terminate(s, EXIT_FAILURE);
  }

  // Listen for connections
  if (listen(serverfd, 5) < 0) {
    logmsg(s, LogLevelError, "ipc: Failed to listen for IPC connections.");
    close(serverfd);
    terminate(s, EXIT_FAILURE);
  }

  logmsg(s, LogLevelTrace, "ipc: Server is listening for IPC connections.");

  while (true) {
    // Accept a new client connection
    clientfd = accept(serverfd, NULL, NULL);
    if (clientfd < 0) {
      logmsg(s, LogLevelTrace, "ipc: Failed to accept IPC client connection.");
      close(serverfd);
      continue;
    }

    // Read the command ID 
    uint8_t command_id;
    numread = read(clientfd, &command_id, sizeof(command_id));
    if (numread != sizeof(command_id)) {
      logmsg(s, LogLevelTrace, "ipc: Failed to read command ID of IPC client with FD: %i", clientfd);
      close(clientfd);
      continue;
    }

    uint32_t len;
    numread = read(clientfd, &len, sizeof(len));
    if (numread != sizeof(len)) {
      logmsg(s, LogLevelTrace, "ipc: Failed to read data length of IPC client with FD: %i", clientfd);
      close(clientfd);
      continue;
    }

    len = ntohl(len); 

    if (len > sizeof(buf) - ((sizeof(command_id) + sizeof(len)))) {
      logmsg(s, LogLevelTrace, 
             "ipc: Data length of IPC client with FD: %i is too large to fit into the data buffer.", clientfd);
      close(clientfd);
      continue;
    }

    numread = read(clientfd, buf, len);
    if (numread != (ssize_t)len) {
      logmsg(s, LogLevelTrace, 
             "ipc: Failed to read data of client with FD: %i", clientfd);
      close(clientfd);
      continue;
    }

    // Handle the command
    handlecmd(s, command_id, buf, len, clientfd);

    // Close the client socket
    close(clientfd);
  }
}

