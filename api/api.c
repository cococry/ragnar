#include "include/ragnar/api.h"

#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdint.h>
#include <arpa/inet.h>

#define SOCKPATH "/tmp/ragnar_socket"

typedef struct {
  int32_t sock;
  struct sockaddr_un addr;
} socket_client_t;

static int32_t clientconnect(socket_client_t* cl);
static int32_t senddata(socket_client_t* cl, const void* data, size_t size);
static int32_t recvdata(socket_client_t* cl, void* data, size_t size);
static int32_t setcmdtype(socket_client_t* cl, rg_command_type_t type);
static int32_t setdatalength(socket_client_t* cl, uint32_t len); 
static int32_t sendcmd(socket_client_t* cl, rg_command_type_t type, uint8_t* data, uint32_t len);
static int32_t clientinit(socket_client_t* cl); 
static int32_t clientclose(socket_client_t* cl); 
static void establishconn(socket_client_t* cl);
static void closeconn(socket_client_t* cl);

static bool s_logging = false;

int32_t 
clientconnect(socket_client_t* cl) {
  int code = connect(cl->sock, 
                     (struct sockaddr*)&cl->addr, 
                     sizeof(struct sockaddr_un));
  return code < 0 ? 1 : 0;
}

int32_t 
senddata(socket_client_t* cl, const void* data, size_t size) {
  int code = write(cl->sock, data, size);
  return code == -1 ? 1 : 0;
}

int32_t
recvdata(socket_client_t* cl, void* data, size_t size) {
  int code = read(cl->sock, data, size);
  return code == -1 ? 1 : 0;
}

int32_t 
setcmdtype(socket_client_t* cl, rg_command_type_t type) {
  uint8_t typecode = (uint8_t)type;
  return senddata(cl, &typecode, sizeof(typecode));
}

int32_t 
setdatalength(socket_client_t* cl, uint32_t len) {
  uint32_t len_net = htonl(len);
  return senddata(cl, &len_net, sizeof(len_net));
}

int32_t
sendcmd(socket_client_t* cl, rg_command_type_t type, uint8_t* data, uint32_t len) {
  if(setcmdtype(cl, type) != 0) { 
    fprintf(stderr, "ragnar api: failed to upload command type.\n");
    return 1;
  }

  if(setdatalength(cl, len) != 0) {
    fprintf(stderr, "ragnar api: failed to upload data length.\n");
    return 1;
  }

  if(senddata(cl, data, len) != 0) {
    fprintf(stderr, "ragnar api: failed to upload command data.\n");
    return 1;
  }

  return 0;
}

int32_t 
clientinit(socket_client_t* cl) {  
  cl->sock = socket(AF_UNIX, SOCK_STREAM, 0);
  if(cl->sock < 0) {
    fprintf(stderr, "ragnar api: failed to create client socket.\n");
    return 1;
  }

  memset(&cl->addr, 0, sizeof(struct sockaddr_un));
  cl->addr.sun_family = AF_UNIX;
  strncpy(cl->addr.sun_path, SOCKPATH, sizeof(cl->addr.sun_path) - 1);

  if(s_logging) {
    printf("ragnar api: created unix domain socket on: '%s'\n", SOCKPATH);
  }
  return 0;
}

int32_t 
clientclose(socket_client_t* cl) {
  int32_t code = close(cl->sock);
  return code == -1 ? 1 : 0;
}

void
establishconn(socket_client_t* cl) {
  if(clientinit(cl) != 0) {
    fprintf(stderr, "ragnar api: failed to open client connection.\n");
    return;
  }

  if(clientconnect(cl) != 0) {
    fprintf(stderr, "ragnar api: client failed to connect to ragnar API.\n");
    return;
  }

  if(s_logging) {
    printf("ragnar api: successfully connected to server.\n");
  }
}

void
closeconn(socket_client_t* cl) {
  if(clientclose(cl) != 0) {
    fprintf(stderr, "ragnar api: failed to close client connection.\n");
    return;
  }
  if(s_logging) {
    printf("ragnar api: closed client connection.\n");
  }
}

void rg_set_trace_logging(bool logging) {
  s_logging = logging;
}

int32_t 
rg_cmd_terminate(uint32_t exitcode) {
  socket_client_t cl;
  establishconn(&cl);

  uint32_t len = sizeof(uint32_t);
  uint8_t data[len]; 
  memcpy(data, &exitcode, sizeof(exitcode));

  if(sendcmd(&cl, RgCommandTerminate, data, len) != 0) {
    fprintf(stderr, "ragnar api: RgCommandTerminate: failed to send command.\n");
    closeconn(&cl);
    return 1;
  }

  if(s_logging) {
    printf("ragnar api: RgCommandTerminate: successfully sent command.\n");
  }
  closeconn(&cl);
  return 0;
}

int32_t 
rg_cmd_get_windows(rg_window_t** wins, uint32_t* numwins) {
  (void)wins;
  (void)numwins;
  uint32_t len = 0;
  uint8_t* data = NULL;

  socket_client_t cl;
  establishconn(&cl);

  if(sendcmd(&cl, RgCommandGetWindows, data, len) != 0) {
    fprintf(stderr, "ragnar api: RgCommandGetWindows: failed to send command.\n");
    closeconn(&cl);
    return 1;
  }
 
  uint32_t num;
  if (recvdata(&cl, &num, sizeof(num)) < 0) {
    perror("read size");
    close(cl.sock);
    exit(EXIT_FAILURE);
  }

  rg_window_t* buf = malloc(num * sizeof(rg_window_t));
  if (!buf) {
    fprintf(stderr, "ragnar api: RgCommandGetWindows: failed to allocate memory for windows.\n");
    closeconn(&cl);
    return 1;
  }

  if (recvdata(&cl, buf, num * sizeof(rg_window_t)) != 0) {
    fprintf(stderr, "ragnar api: RgCommandGetWindows: failed to receive client windows.\n");
    closeconn(&cl);
    return 1;
  }

  if(s_logging) {
    printf("ragnar api: RgCommandGetWindows: successfully sent command.\n");
  }

  closeconn(&cl);

  *numwins = num;
  *wins = buf;

  return 0;
}

int32_t 
rg_cmd_kill_window(rg_window_t win) {
  socket_client_t cl;
  establishconn(&cl);

  uint32_t len = sizeof(rg_window_t);

  uint8_t data[len]; 
  memcpy(data, &win, sizeof(win));

  if(sendcmd(&cl, RgCommandKillWindow, data, len) != 0) {
    fprintf(stderr, "ragnar api: RgCommandKillWindow: failed to send command.\n");
    closeconn(&cl); 
    return 1;
  }

  if(s_logging) {
    printf("ragnar api: RgCommandKillWindow: successfully sent command.\n");
  }
  closeconn(&cl);
  return 0;
}

int32_t 
rg_cmd_focus_window(rg_window_t win) {
  socket_client_t cl;
  establishconn(&cl);

  uint32_t len = sizeof(rg_window_t);

  uint8_t data[len]; 
  memcpy(data, &win, sizeof(win));

  if(sendcmd(&cl, RgCommandFocusWindow, data, len) != 0) {
    fprintf(stderr, "ragnar api: RgCommandFocusWindow: failed to send command.\n");
    closeconn(&cl); 
    return 1;
  }

  if(s_logging) {
    printf("ragnar api: RgCommandFocusWindow: successfully sent command.\n");
  }
  closeconn(&cl);

  return 0;
}

int32_t 
rg_cmd_next_window(rg_window_t win, rg_window_t* next) {
  socket_client_t cl;
  establishconn(&cl);

  uint32_t len = sizeof(rg_window_t);

  uint8_t data[len]; 
  memcpy(data, &win, sizeof(win));

  if(sendcmd(&cl, RgCommandNextWindow, data, len) != 0) {
    fprintf(stderr, "ragnar api: RgCommandNextWindow: failed to send command.\n");
    closeconn(&cl);
    return 1;
  }

  if (recvdata(&cl, next, sizeof(rg_window_t)) != 0) {
    fprintf(stderr, "ragnar api: RgCommandNextWindow: failed to receive next window.\n");
    closeconn(&cl);
    return 1;
  }

  if(s_logging) {
    printf("ragnar api: RgCommandNextWindow: successfully sent command.\n");
  }
  closeconn(&cl);
  
  return 0;
}

int32_t 
rg_cmd_first_window(rg_window_t* first) {
  socket_client_t cl;
  establishconn(&cl);
  
  if(sendcmd(&cl, RgCommandFirstWindow, NULL, 0) != 0) {
    fprintf(stderr, "ragnar api: RgCommandFirstWindow: failed to send command.\n");
    closeconn(&cl);
    return 1;
  }

  if (recvdata(&cl, first, sizeof(rg_window_t)) != 0) {
    fprintf(stderr, "ragnar api: RgCommandFirstWindow: failed to receive first window.\n");
    closeconn(&cl);
    return 1;
  }

  if(s_logging) {
    printf("ragnar api: RgCommandFirstWindow: successfully sent command.\n");
  }
  closeconn(&cl);
  
  return 0;
}
int32_t 
rg_cmd_get_focus(rg_window_t* focus) {
  socket_client_t cl;
  establishconn(&cl);
  
  if(sendcmd(&cl, RgCommandGetFocus, NULL, 0) != 0) {
    fprintf(stderr, "ragnar api: RgCommandGetFocus: failed to send command.\n");
    closeconn(&cl);
    return 1;
  }

  if (recvdata(&cl, focus, sizeof(rg_window_t)) != 0) {
    fprintf(stderr, "ragnar api: RgCommandGetFocus: failed to receive first window.\n");
    closeconn(&cl);
    return 1;
  }

  if(s_logging) {
    printf("ragnar api: RgCommandGetFocus: successfully sent command.\n");
  }
  closeconn(&cl);
  
  return 0;
}

int32_t rg_cmd_get_monitor_focus(int32_t* idx) {
  socket_client_t cl;
  establishconn(&cl);
  
  if(sendcmd(&cl, RgCommandGetMonitorFocus, NULL, 0) != 0) {
    fprintf(stderr, "ragnar api: RgCommandGetMonitorFocus: failed to send command.\n");
    closeconn(&cl);
    return 1;
  }

  if (recvdata(&cl, idx, sizeof(int32_t)) != 0) {
    fprintf(stderr, "ragnar api: RgCommandGetMonitorFocus: failed to receive first window.\n");
    closeconn(&cl);
    return 1;
  }

  if(s_logging) {
    printf("ragnar api: RgCommandGetMonitorFocus: successfully sent command.\n");
  }

  closeconn(&cl);
  
  return 0;
}
