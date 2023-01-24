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
#include <termios.h>
#include <pthread.h>
#include <pty.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/eventfd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <sys/wait.h>

#include "liburing.h"
#include "llhttp.h"
#include "sha1.h"

#define SERVER_PORT 8000
#define QUEUE_INIT 4
#define MY_MAX_LISTEN 100
#define MY_STR(x) #x
#define MY_STREXPAND(x) MY_STR(x)
#define MY_SPLICE_SIZE 4096
#define RECV_BUFFER_SIZE 1024
#define MY_INDEX "index.html" 
#define MY_INDEX_MINE "text/html; charset=utf-8"
#define MY_DEFAULT_SHELL "bash"
#define PTY_READ_SIZE 512
#define WAIT_CHILD_DELAY_US 1000

static const char *my_shell;
static struct io_uring my_ring;
static int my_exitcode;
static struct termios term_attrs;
static void *pidset;
pthread_mutex_t my_mutex = PTHREAD_MUTEX_INITIALIZER;
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
  ss_ws_open,
  ss_ws_read_data,
  ss_ws_read6,
  ss_ws_read_pty,
  ss_ws_send_data,
  ss_ws_write,
  ss_shutdown  // after call shutdown() to shutdown connection.
};
struct my_client {
  int fd;
  unsigned ops_pending;
  unsigned char so_type;
  unsigned char closing;
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
  char *url_str, *h_newname, *sec_ws_key, *ws_pro;
  unsigned ws_len;
  unsigned char flags;
};
struct my_client_ws {
  struct my_client base;
  struct my_client_http *mc;
  unsigned char header[4];
  struct iovec vecs[2];
  char buffer[PTY_READ_SIZE];
};
struct my_client_http {
  struct my_client base;
  char read_buffer[RECV_BUFFER_SIZE];
  int pfds[2];
  int fileid;
  off_t filesize;
  struct http_reader http_reader;
  struct lazy_buffer lzb;
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
static void my_error(const char *msg) {
  if (errno) perror(msg); else fprintf(stderr, "error: %s\n", msg);
}
static void my_error2(const char *msg) {
  fprintf(stderr, "error: %s\n", msg);
}
#else
#define my_unreachable abort
#define my_error(x)
#define my_error2(x)
#define printf(...)
#define puts(x) 
#endif
static void my_fatal(const char *msg) {
  if (errno) perror(msg); else fprintf(stderr, "fatal: %s\n", msg);
  exit(EXIT_FAILURE);
}
#define FILE_ATTRIBUTE_READONLY  0x00000001
#define FILE_ATTRIBUTE_HIDDEN  0x00000002
#define FILE_ATTRIBUTE_SYSTEM  0x00000004
#define FILE_ATTRIBUTE_DIRECTORY 0x00000010
#define FILE_ATTRIBUTE_ARCHIVE  0x00000020
#define FILE_ATTRIBUTE_DEVICE  0x00000040
#define FILE_ATTRIBUTE_NORMAL  0x00000080
#define FILE_ATTRIBUTE_TEMPORARY  0x00000100
#define FILE_ATTRIBUTE_COMPRESSED  0x00000800

static const unsigned char base64_table[65] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/*
* Base64 encoding/decoding (RFC1341)
* Copyright (c) 2005-2011, Jouni Malinen <j@w1.fi>
*
* This software may be distributed under the terms of the BSD license.
* See README for more details.
*/

// 2023-1-17 - Some custom edits.

void base64_encode(const unsigned char *src, unsigned char *out)
{
  unsigned char *pos;
  const unsigned char *end, *in;

  end = src + 20;
  in = src;
  pos = out;
  while (end - in >= 3) {
    *pos++ = base64_table[in[0] >> 2];
    *pos++ = base64_table[((in[0] & 0x03) << 4) | (in[1] >> 4)];
    *pos++ = base64_table[((in[1] & 0x0f) << 2) | (in[2] >> 6)];
    *pos++ = base64_table[in[2] & 0x3f];
    in += 3;
  }

  if (end - in) {
    *pos++ = base64_table[in[0] >> 2];
    if (end - in == 1) {
      *pos++ = base64_table[(in[0] & 0x03) << 4];
      *pos++ = '=';
    } else {
      *pos++ = base64_table[((in[0] & 0x03) << 4) |
                (in[1] >> 4)];
      *pos++ = base64_table[(in[1] & 0x0f) << 2];
    }
    *pos++ = '=';
  }

  *pos = '\0';
}
static uint64_t ntohll(uint64_t x) {
  uint64_t H = ntohl(x);
  uint64_t L = ntohl(x >> 32);
  return (H << 32) + L;
}
static void queue_kill(pid_t pid);

void *pidset_create();
int pidset_add(void *set, pid_t pid);
void pidset_remove(void *set, pid_t pid);
void pidset_destroy(void *set);
