#include "server.h"
static const char *get_mime_type(const char *path) {
  const char *s;
  if (!path)
    return MY_INDEX_MINE;
  if (!(s = strrchr(path, '.')))
    return "text/plain";
  s++;
#include "mine.c"
}
static int http_on_url(llhttp_t* parser, const char* at, size_t length) {
  struct http_reader *self = (struct http_reader*)parser;
  if (length <= 1) {
    self->url_str = NULL;
    return 0;
  }
  --length;
  ++at;
  char *str = (char*)malloc(length + 1), *strptr = str;
  const char *ptr = at, *end = at + length;
  while (ptr != end) {
    unsigned char c = *ptr;
    if (c == '%') {
      unsigned char c1 = ptr[1];
      if (isxdigit(c1)) {
        unsigned char c2 = ptr[2];
        if (isxdigit(c2)) {
          if (c1 >= 'a')
                  c1 -= 'a'-'A';
          if (c1 >= 'A')
                  c1 -= ('A' - 10);
          else
                  c1 -= '0';
          if (c2 >= 'a')
                  c2 -= 'a'-'A';
          if (c2 >= 'A')
                  c2 -= ('A' - 10);
          else
                  c2 -= '0';
          *strptr++ =  c1* 16 + c2;
          ptr += 3;
        }
      }
    } else if (c == '+') {
      *strptr++ = (ptr++, ' ');
    } else {
      *strptr++ = *ptr++;
    }
  }
  *strptr = '\0';
  self->url_str = str;
  return 0;
}
static int http_on_header_field(llhttp_t* parser, const char* at, size_t length) {
  struct http_reader *self = (struct http_reader*)parser;
  if (length == 13 && *at == 'X' && at[1] == '-') {
    if (memcmp(at + 2, "Recursively", 11) == 0) {
      self->flags |= 2;
    }
  }
  if (length == 9 && *at == 'X' && at[1] == '-') {
    if (memcmp(at + 2, "NewName", 7) == 0) {
      self->flags |= 4;
    }
  }
  return 0;
}
static int http_on_header_value(llhttp_t* parser, const char* at, size_t length) {
  struct http_reader *self = (struct http_reader*)parser;
  if (self->flags & 4) {
    self->h_newname = malloc(length + 1);
    memcpy(self->h_newname, at, length);
    self->h_newname[length] = '\0';
  }
  return 0;
}
static int http_on_header_complete(llhttp_t* parser) {
  struct http_reader *self = (struct http_reader*)parser;
  self->flags |= 1; // mark as finished
  return 0;
}
static void http_reader_start(struct http_reader *reader) {
  llhttp_settings_init(&reader->settings);
  reader->settings.on_url = http_on_url;
  reader->settings.on_header_field = http_on_header_field;
  reader->settings.on_header_value = http_on_header_value;
  reader->settings.on_headers_complete = http_on_header_complete;
  llhttp_init(&reader->parser, HTTP_REQUEST, &reader->settings);
}
static void my_accept_start() {
  struct io_uring_sqe *sqe = io_uring_get_sqe(&my_ring);
  server.addrlen = sizeof(server.addr);
  io_uring_prep_accept(sqe, server.base.fd, (struct sockaddr *)&server.addr, &server.addrlen, 0);
  io_uring_sqe_set_data(sqe, &server);
  io_uring_submit(&my_ring);
}
static void my_error(const char *msg) {
  if (errno) perror(msg); else fprintf(stderr, "error: %s\n", msg);
}
static void my_fatal(const char *msg) {
  if (errno) perror(msg); else fprintf(stderr, "fatal: %s\n", msg);
  exit(EXIT_FAILURE);
}
static void my_delete_client(struct my_client *self) {
  close(self->fd);
  free(self);
}
static void my_delete_client_http(struct my_client_http *self) {
  if (self->http_reader.url_str)
    free(self->http_reader.url_str);
  if (self->pfds[0]) {
    close(self->pfds[0]);
    close(self->pfds[1]);
  }
  if (self->fileid)
    close(self->fileid);
  if (self->http_reader.h_newname)
    free(self->http_reader.h_newname);
  return my_delete_client(&self->base);
}
static void my_recv_header_once(int fd) {
    struct io_uring_sqe *sqe;
    struct my_client_http *ev;

    ev = (struct my_client_http*)malloc(sizeof(struct my_client_http));
    ev->base.so_type = ss_recv_header;
    ev->base.fd = fd;
    ev->http_reader.url_str = NULL;
    ev->http_reader.settings.on_url = NULL;
    ev->http_reader.flags = 0;
    ev->http_reader.h_newname = NULL;
    ev->pfds[0] = 0;
    ev->fileid = 0;

    sqe = io_uring_get_sqe(&my_ring);
    io_uring_prep_recv(sqe, fd, ev->read_buffer, sizeof(ev->read_buffer), 0);
    io_uring_sqe_set_data(sqe, ev);
    io_uring_submit(&my_ring);
}
static void my_recv_header_start(struct my_client_http *self) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(&my_ring);
    io_uring_prep_recv(sqe, self->base.fd, self->read_buffer, sizeof(self->read_buffer), 0);
    io_uring_sqe_set_data(sqe, self);
    io_uring_submit(&my_ring);
}
static void my_send_header_once(struct my_client_http *self, const void *text, size_t len, enum client_state state) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(&my_ring);
    self->base.so_type = state;
    io_uring_prep_send(sqe, self->base.fd, text, len, 0);
    io_uring_sqe_set_data(sqe, self);
    io_uring_submit(&my_ring);
}
static void my_splice_start(struct my_client_http *self) {
    struct io_uring_sqe *sqe;
    off_t offset = lseek(self->fileid, 0, SEEK_CUR);
    assert(offset >= 0);
    off_t remain = self->filesize - offset;
    if (remain > MY_SPLICE_SIZE)
      remain = MY_SPLICE_SIZE;
    else if (remain == 0) {
      self->base.so_type = ss_recv_header;
      close(self->pfds[0]);
      close(self->pfds[1]);
      self->pfds[0] = 0;
      close(self->fileid);
      self->fileid = 0;
      free(self->http_reader.url_str);
      self->http_reader.url_str = NULL;
      my_recv_header_start(self);
      return;
    }
    self->base.so_type = ss_responce_file2;
    sqe = io_uring_get_sqe(&my_ring);
    io_uring_prep_splice(sqe, self->fileid, -1, self->pfds[1], -1, remain, SPLICE_F_MOVE);
    io_uring_sqe_set_data(sqe, self);
    io_uring_submit(&my_ring);
}
static void my_splice_start2(struct my_client_http *self, off_t len) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(&my_ring);
    self->base.so_type = ss_responce_file;
    io_uring_prep_splice(sqe, self->pfds[0], -1, self->base.fd, -1, len, SPLICE_F_MOVE);
    io_uring_sqe_set_data(sqe, self);
    io_uring_submit(&my_ring);
}
static int my_request_delete_recursive(const char *path) {
  DIR *d = opendir(path);
  if (!d) return -1;

   size_t path_len = strlen(path);
   if (d) {
      struct dirent *p;
      while ((p=readdir(d))) {
        struct stat statbuf;
          char *buf;
          size_t i;
          if (p->d_name[0] == '.' && (p->d_name[1] == '\0' || (p->d_name[1] == '.' && p->d_name[2] == '\0')))
            continue;
          buf = (char*)malloc(path_len + strlen(p->d_name) + 2);
          for (i = 0;path[i];++i)
            buf[i] = path[i];
          buf[i] = '/';
          for (i = 0;p->d_name[i];++i)
            buf[i] = p->d_name[i];
          buf[i] = '\0';
          if (stat(buf, &statbuf))
            return -1;
          if (S_ISDIR(statbuf.st_mode))
            my_request_delete_recursive(buf);
          else
            unlink(buf);
          free(buf);
      }
      closedir(d);
   }
   rmdir(path);
   return 0;
}
static void my_error_errno(struct my_client_http *self) {
  int o = errno;
    const char *err = strerror(o);
    int len = sprintf(self->read_buffer, 
"HTTP/1.1 500 Internal Server Error\r\n"
"Content-Length: %zu\r\n"
"Content-Type: text/plain\r\n"
"X-errno: %d\r\n"
"%s", strlen(err), o, err);
  return my_send_header_once(self, self->read_buffer, len, ss_reparse);
}
static void my_request_delete(struct my_client_http *self, struct stat st) {
  errno = 0;
  if (!self->http_reader.url_str) // we might not delete our home page ...
    goto D_ERROR;
  if (self->http_reader.h_newname) {
    if (rename(self->http_reader.url_str, self->http_reader.h_newname))
      goto D_ERROR;

    const char *text =
"HTTP/1.1 200 OK\r\n"
"Content-Type: text/plain\r\n"
"Content-Length: 17\r\n"
"\r\n"
"OK, File Renamed!";
    return my_send_header_once(self, text, strlen(text), ss_reparse);
  }
  if (S_ISDIR(st.st_mode)) {
    if (rmdir(self->http_reader.url_str)) {
      if (errno != ENOTEMPTY)
        goto D_ERROR;
      if (self->http_reader.flags & 2) {
        int ret = my_request_delete_recursive(self->http_reader.url_str);
        printf("%d\n", ret);
        if (ret)
          goto D_ERROR;
        goto D_OK;
      }
      const char *text = 
      "HTTP/1.1 202 Accepted\r\n"
      "Content-Type: text/plain\r\n"
      "Content-Length: 66\r\n"
      "\r\n"
      "The directory is not not empty.Are you sure to remove recursively?";
      return my_send_header_once(self, text, strlen(text), ss_reparse);
    }
    goto D_OK;
  } else {
    if (unlink(self->http_reader.url_str))
      goto D_ERROR;
D_OK: ;
    const char *text = 
"HTTP/1.1 200 OK\r\n"
"Content-Length: 17\r\n"
"Content-Type: text/plain\r\n"
"\r\n"
"OK, Item Deleted!";
     return my_send_header_once(self, text, strlen(text), ss_reparse);
  }
D_ERROR: ;
  if (errno)
    return my_error_errno(self);
  const char *text = 
"HTTP/1.1 500 Internal Server Error\r\n"
"Content-Length: 26\r\n"
"Content-Type: text/plain\r\n"
"\r\n"
"Sorry, no errno available.";
  return my_send_header_once(self, text, strlen(text), ss_reparse);
}
static void my_create_dir(struct my_client_http *self) {
  errno = 0;
  if (!self->http_reader.url_str) {
    errno = EEXIST;
    goto D_ERROR;
  }
  mode_t mode = 0700;
  if (mkdir(self->http_reader.url_str, mode))
    goto D_ERROR;
  {
  const char *text = 
"HTTP/1.1 201 Created\r\n"
"Content-Type: text/plain\r\n"
"Content-Length: 22\r\n"
"\r\n"
"OK, Directory created!";
  return my_send_header_once(self, text, strlen(text), ss_reparse);
 }
D_ERROR: ;
  if (errno)
    return my_error_errno(self);
  const char *text = 
"HTTP/1.1 500 Internal Server Error\r\n"
"Content-Length: 25\r\n"
"Content-Type: text/plain\r\n"
"\r\n"
"mkdir() returns an error.";
  return my_send_header_once(self, text, strlen(text), ss_reparse);
}
static void my_loop() {
  struct io_uring_cqe *cqe;
  struct my_client *self;
  int res;
  for (;;) {
    int num = io_uring_wait_cqe(&my_ring, &cqe);
    if (num < 0) {
      my_exitcode = 1;
      return;
    }
    io_uring_cqe_seen(&my_ring, cqe);
    self = (struct my_client*)io_uring_cqe_get_data(cqe);
    res = cqe->res;
    switch (self->so_type) {
    case ss_server_accept:
    {
      my_accept_start();
      if (res < 0) {
        my_delete_client(self);
        errno = -res;
        my_error("ss_server_accept");
        break;
      }
      my_recv_header_once(res);
      break;
    }
    case ss_reparse:
   {
RE_PARSE: ;
        struct my_client_http *mc = (struct my_client_http*)self;
        if (cqe->res <= 0) {
          my_delete_client_http(mc);
          break;
        }
        mc->http_reader.flags = 0;
        mc->http_reader.h_newname = NULL;
        mc->fileid = 0;
        mc->filesize = 0;
        mc->pfds[0] = 0;
        mc->base.so_type = ss_recv_header;
        my_recv_header_start(mc);
        break;
    }
    case ss_readdir:
    {
        struct my_client_http *mc = (struct my_client_http*)self;
        if (cqe->res <= 0) {
          my_delete_client_http(mc);
          break;
        }
        my_send_header_once(mc, mc->lzb.data, mc->lzb.length, ss_reparse2);
        break;
    }
    case ss_reparse2:
    {
        struct my_client_http *mc = (struct my_client_http*)self;
        lazy_buffer_destroy(&mc->lzb);
        goto RE_PARSE;
    }
    case ss_recv_header:
    {
        struct my_client_http *mc = (struct my_client_http*)self;
        if (cqe->res <= 0) {
          my_delete_client_http(mc);
          break;
        }
        if (!mc->http_reader.settings.on_url)
          http_reader_start(&mc->http_reader);
        llhttp_errno_t err = llhttp_execute(&mc->http_reader.parser, mc->read_buffer, cqe->res);
        if (err != HPE_OK) {
          printf("[llhttp parse error] code=%s, reason=%s\n", llhttp_errno_name(err), mc->http_reader.parser.reason);
          my_delete_client_http(mc);
          break;
        }
        if (mc->http_reader.flags & 1) {
          struct stat statbuf;
          const llhttp_method_t method = mc->http_reader.parser.method;
          if (method == HTTP_PUT) {
            my_create_dir(mc);
            break;
          }
          int file_fd = open(mc->http_reader.url_str ? mc->http_reader.url_str : MY_INDEX, O_RDONLY);
          if (file_fd == -1) {
            ERROR_404: ;
            const char *text = 
"HTTP/1.1 404 Not Found\r\n"
"Content-Type: text/html; charset=utf-8\r\n"
"Content-Length: 199\r\n"
"\r\n"
"<!DOCTYPE html><html><head><title>404 Not Found</title></head><body><h1 align=\"center\">404 Not Found</h1><hr><p style=\"text-align: center\">The requested file is not found in server.<p/></body></html>";
          // llhttp_reset(mc->http_reader);
            my_send_header_once(mc, text, strlen(text), ss_reparse);
            break;
          }
          if (fstat(file_fd, &statbuf))
            goto ERROR_404;
          if (method == HTTP_DELETE) {
            close(file_fd);
            my_request_delete(mc, statbuf);
            break;
          }
          if (method == HTTP_HEAD) {
            break;
          }
          if (method != HTTP_GET) {
            const char *text = 
"HTTP/1.1 405 Method Not Allowed\r\n"
"Content-Length: 230\r\n"
"Allow: GET, HEAD, DELETE, PUT\r\n"
"Content-Type: text/html; charset=utf-8\r\n"
"\r\n"
"<!DOCTYPE html><html><head><title>405 Method Not Allowed</title></head><body><h1 align=\"center\">405 Method Not Allowed</h1><hr><p style=\"text-align: center\">Only HTTP method GET, HEAD, PUT and DELETE are allowed.</p></body></html>";
            my_send_header_once(mc, text, strlen(text), ss_reparse);
          }
          if (S_ISDIR(statbuf.st_mode)) {
            const char *init_str = "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><link rel=\"stylesheet\" href=\"/folderattr.css\"><script src=\"/upload.js\"></script><title>Directory Listing For</title></head>"
"<body><h1></h1>"
"<div id=\"upload\"><input type=\"file\" id=\"f\" multiple><button onclick=\"fsubmit()\">Upload</button></div>"
"<base href=\"/";
             mc->lzb = lazy_buffer_init(init_str, strlen(init_str));
             assert(mc->http_reader.url_str); // '/' should be index.html/index.php or something else, not a directory.
             lazy_buffer_cat_cstr(&mc->lzb, mc->http_reader.url_str);
             lazy_buffer_cat_cstr(&mc->lzb, "/*\"><d d=\"");
             {
               struct utsname uname_buf;
               uname(&uname_buf);
               lazy_buffer_cat_cstr(&mc->lzb, uname_buf.machine);
             }
             lazy_buffer_cat_cstr(&mc->lzb, "\">");

             for (char buf[1024], buf2[64];;) {
               long nread = syscall(SYS_getdents64, file_fd, buf, sizeof(buf));
               if (nread == -1)
                   goto ERROR_404;
               if (nread == 0)
                   break;
               for (long bpos = 0; bpos < nread;) {
                   int len;
                   unsigned long attr = 0;
                   struct linux_dirent64 *d = (struct linux_dirent64 *) (buf + bpos);
                   struct stat statbuf;
                   if (!fstatat(file_fd, d->d_name, &statbuf, 0)) {
                    if (S_ISDIR(statbuf.st_mode))
                      attr |= FILE_ATTRIBUTE_DIRECTORY;
                    if (S_ISCHR(statbuf.st_mode) || S_ISBLK(statbuf.st_mode))
                      attr |= FILE_ATTRIBUTE_DEVICE;
                   }
                   len = sprintf(buf2, "<z z=\"%llu;%llu;%lu\">", (unsigned long long)statbuf.st_size, (unsigned long long)statbuf.st_mtime, attr);
                   lazy_buffer_cat_data(&mc->lzb, buf2, len);
                   lazy_buffer_cat_cstr(&mc->lzb, d->d_name);
                   lazy_buffer_cat_cstr(&mc->lzb, "</z>");
                   bpos += d->d_reclen;
               }
            }
            lazy_buffer_cat_cstr(&mc->lzb, "</d></body></html>");
            int len = sprintf(mc->read_buffer,
                          "HTTP/1.1 200 OK\r\n"
                          "Content-Type: text/html; charset=utf-8\r\n"
                          "Content-Length: %zu\r\n"
                          "\r\n", mc->lzb.length);
            my_send_header_once(mc, mc->read_buffer, len, ss_readdir);
            break;
          }
          if (!S_ISREG(statbuf.st_mode))
            goto ERROR_404;
          int len = sprintf(mc->read_buffer,
              "HTTP/1.1 200 OK\r\n"
              "Content-Type: %s\r\n"
              "Content-Length: %llu\r\n"
              "\r\n", get_mime_type(mc->http_reader.url_str), (unsigned long long)statbuf.st_size);
          mc->filesize = statbuf.st_size;
          mc->fileid = file_fd;
          my_send_header_once(mc, mc->read_buffer, len, ss_responce_file);
          break;
        }
        my_recv_header_start(mc);
        break;
    }
    case ss_responce_file:
    {
        struct my_client_http *mc = (struct my_client_http*)self;
        if (cqe->res <= 0) {
          my_delete_client_http(mc);
          break;
        }
        if (!mc->pfds[0] && pipe(mc->pfds) < 0)
          my_fatal("pipe()");
        my_splice_start(mc);
        break;
    }
    case ss_responce_file2:
    {
        struct my_client_http *mc = (struct my_client_http*)self;
        if (cqe->res <= 0) {
          my_delete_client_http(mc);
          break;
        }
        my_splice_start2(mc, cqe->res);
        break;
    }
    case ss_server_shutdown:
      return;
    default: my_unreachable();
   }
 }
}
static void my_cleanup() {
  io_uring_queue_exit(&my_ring);
  close(server.base.fd);
  close(shutdowner.base.fd);
  printf("[shutdown server] code=%d\n", my_exitcode);
}
static void my_signal_handler(int sig) {
  uint64_t one = 1;
  ssize_t res;
  do {
    res = write(shutdowner.base.fd, &one, sizeof(one));
  } while (res == -1 && errno == EAGAIN);
  if (res == -1)
    abort();
}
static void display_useage(const char *hint) {
  const char *useage = "A HTTP+WebSocket server(io_uring)\nUsage: [ip] [port] [directory]\n\n  For example: ./server 0.0.0.0 8000 ./public\n\n";
  write(STDERR_FILENO, useage, strlen(useage));
  my_error(hint);
  my_exitcode = 1;
}
static void init_error(const char *msg) {
  my_error(msg);
  my_exitcode = 1;
}
static void my_init(int argc, const char *argv[]) {
  struct sockaddr_in server_addr;

  memset(&server_addr, 0, sizeof(server_addr));

  server.base.fd = socket(AF_INET, SOCK_STREAM, 0);
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(SERVER_PORT);
  const char *ip4 = "0.0.0.0";
  const char *port = MY_STREXPAND(SERVER_PORT);
  if (argc != 1) {
    if (inet_pton(AF_INET, argv[1], &(server_addr.sin_addr)) <= 0)
      return display_useage("inet_pton()");
    if (argc > 2) {
      char *endptr;
      errno = 0;
      long val = strtol(argv[2], &endptr, 10);
      if (argv[2] == endptr)
        return display_useage("invalid port number: expect digits");
      if (*endptr != '\0')
        return display_useage("invalid port number: extra characters after digits");
      if (errno) // ERANGE
        return display_useage("strtol()");
      if (val > 65535 || val < 0)
        return display_useage("port number not in 0-65535");
      server_addr.sin_port = htons(val);
      port = argv[2];
      if (argc > 3) {
        const char *dir = argv[3];
        if (chdir(dir))
          return display_useage("chdir()");
      }
    }
    ip4 = argv[1];
  }
  if (setsockopt(server.base.fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
    init_error("setsockopt(SO_REUSEADDR)");
  if (bind(server.base.fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
    return display_useage("bind()");
  if (listen(server.base.fd, MY_MAX_LISTEN) < 0)
    return init_error("listen()");
  if ((errno = -io_uring_queue_init(QUEUE_INIT, &my_ring, 0)))
    return init_error("io_uring_queue_init()");
  if ((shutdowner.base.fd = eventfd(0, 0)) == -1)
    return init_error("eventfd()");
  {
    struct io_uring_sqe *sqe = io_uring_get_sqe(&my_ring);
    io_uring_prep_read(sqe, shutdowner.base.fd, &shutdowner.data, sizeof(shutdowner.data), 0);
    io_uring_sqe_set_data(sqe, &shutdowner);
  }
  {
    struct sigaction sh;
    sh.sa_handler = my_signal_handler;
    sigemptyset(&sh.sa_mask);
    sh.sa_flags = 0;
    sigaction(SIGINT, &sh, NULL);
  }
  my_accept_start();
  printf("[server start] url=http://%s:%s\n", ip4, port);
}
int main(int argc, const char *argv[]) {
  my_init(argc, argv);
  if (!my_exitcode)
    my_loop();
  my_cleanup();
  return my_exitcode;
}
