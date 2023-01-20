#define MY_DEBUG 1
#define _GNU_SOURCE
// run the program:
//   gcc server.c
//   ./a.out 0.0.0.0 8000
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/eventfd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/utsname.h>

#include "liburing.h"
#include "llhttp.h"

#define SERVER_PORT 8000
#define QUEUE_INIT 4
#define MY_MAX_LISTEN 100
#define MY_STR(x) #x
#define MY_STREXPAND(x) MY_STR(x)
#define MY_SPLICE_SIZE 4096
#define RECV_BUFFER_SIZE 1024
#define MY_INDEX "index.html" 
#define MY_INDEX_MINE "text/html; charset=utf-8"

static struct io_uring my_ring;
static int my_exitcode;
struct lazy_buffer {
  size_t length, cap;
  char *data;
};
struct lazy_buffer lazy_buffer_init(const void *data, size_t len) {
  struct lazy_buffer res;
  res.length = len;
  res.cap = len + 20;
  res.data = malloc(res.cap);
  memcpy(res.data, data, len);
  return res;
}
struct lazy_buffer lazy_buffer_create() {
  struct lazy_buffer res;
  res.length = 0;
  res.cap = 0;
  res.data = NULL;
  return res;
}
void lazy_buffer_cat_data(struct lazy_buffer *buffer, const void *data, size_t len) {
  size_t newLen = buffer->length + len;
  if (newLen > buffer->cap) {
    buffer->cap = newLen;
    buffer->cap *= 3;
    buffer->cap /= 2;
    buffer->data = realloc(buffer->data, buffer->cap);
  }
  memcpy(buffer->data + buffer->length, data, len);
  buffer->length = newLen;
}
void lazy_buffer_cat_cstr(struct lazy_buffer *buffer, const char *text) {
  return lazy_buffer_cat_data(buffer, text, strlen(text));
}
void lazy_buffer_destroy(struct lazy_buffer *buffer) {
  free(buffer->data);
}

enum client_state {
  ss_server_accept, // Wait for new clients, and call accept().
  ss_server_shutdown, // shutdown the server, leave event loop and call exit().
  ss_recv_header, // Waiting for EPOLLIN event, and call recv().
  ss_responce_file, // after call send() to send file contents to client.
  ss_responce_file2, 
  ss_readdir,
  ss_reparse, // after call send() to send http responce(header+body), and ready to re-use(keep-alive) the socket.
  ss_reparse2,
  ss_ws_handshake, // after call send() to send Sec-WebSocket-Key, ...etc.
  ss_ws_read6, // after call recv() to recv websocket frame (first 6 bytes).
  ss_ws_write, // after call send() to send websocket frame.
  ss_shutdown  // after call shutdown() to shutdown connection.
};
struct my_client {
  int fd;
  unsigned char so_type;
};
static struct my_server {
  struct my_client base;
  struct sockaddr_in addr;
  socklen_t addrlen;
} server; // so_type init to ss_server_accept(0)
static struct my_shutdowner {
  struct my_client base;
  uint64_t data;
} shutdowner;
struct http_reader {
  llhttp_t parser;
  llhttp_settings_t settings;
  char *url_str, *h_newname;
  unsigned char flags;
};
struct my_client_http {
  struct my_client base;
  char read_buffer[RECV_BUFFER_SIZE];
  int pfds[2];
  int fileid;
  off_t filesize;
  struct http_reader http_reader;
  struct lazy_buffer lzb;
  struct iovec io_vecs[2];
};
struct linux_dirent {
    unsigned long  d_ino;
    off_t          d_off;
    unsigned short d_reclen;
    char           d_name[];
};
struct linux_dirent64 {
    ino64_t        d_ino;
    off64_t        d_off;
    unsigned short d_reclen;
    unsigned char  d_type;
    char           d_name[];
};

#if MY_DEBUG
#define my_unreachable __builtin_unreachable
#else
#define my_unreachable abort
#endif
static void my_error(const char *msg);
static void my_fatal(const char *msg);

#define FILE_ATTRIBUTE_READONLY  0x00000001
#define FILE_ATTRIBUTE_HIDDEN  0x00000002
#define FILE_ATTRIBUTE_SYSTEM  0x00000004
#define FILE_ATTRIBUTE_DIRECTORY 0x00000010
#define FILE_ATTRIBUTE_ARCHIVE  0x00000020
#define FILE_ATTRIBUTE_DEVICE  0x00000040
#define FILE_ATTRIBUTE_NORMAL  0x00000080
#define FILE_ATTRIBUTE_TEMPORARY  0x00000100
#define FILE_ATTRIBUTE_COMPRESSED  0x00000800
