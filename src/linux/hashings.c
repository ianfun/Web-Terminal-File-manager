#include "sha1.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
/*
* Base64 encoding/decoding (RFC1341)
* Copyright (c) 2005-2011, Jouni Malinen <j@w1.fi>
*
* This software may be distributed under the terms of the BSD license.
* See README for more details.
*/

// 2023-1-17 - ianfun : make out_len must be not null, and replace os_malloc with malloc


static const unsigned char base64_table[65] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

unsigned char * base64_encode(const unsigned char *src, size_t len,
            size_t *out_len)
{
  unsigned char *out, *pos;
  const unsigned char *end, *in;
  size_t olen;
  int line_len;

  olen = len * 4 / 3 + 4; /* 3-byte blocks to 4-byte */
  olen += olen / 72; /* line feeds */
  olen++; /* nul termination */
  out = malloc(olen);
  if (out == NULL)
    return NULL;

  end = src + len;
  in = src;
  pos = out;
  line_len = 0;
  while (end - in >= 3) {
    *pos++ = base64_table[in[0] >> 2];
    *pos++ = base64_table[((in[0] & 0x03) << 4) | (in[1] >> 4)];
    *pos++ = base64_table[((in[1] & 0x0f) << 2) | (in[2] >> 6)];
    *pos++ = base64_table[in[2] & 0x3f];
    in += 3;
    line_len += 4;
    if (line_len >= 72) {
      *pos++ = '\n';
      line_len = 0;
    }
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
    line_len += 4;
  }

  if (line_len)
    *pos++ = '\n';

  *pos = '\0';
  *out_len = pos - out;
  return out;
}
char* my_sha1(const void *data, size_t len, size_t *out_len) {
  const char *magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  size_t alloc_size = 36 + len;
  uint8_t results[20], *ptr = results;
  SHA1_CTX sha;

  char *mdata = (char*)malloc(alloc_size);
  memcpy(mdata, data, len);
  strcpy(mdata + len, magic);

  SHA1Init(&sha);
  SHA1Update(&sha, (const unsigned char*)mdata, alloc_size);
  SHA1Final(results, &sha);

  free(mdata);

  return (char*)base64_encode((const unsigned char*)results, 20, out_len);
}

int main(int argc, const char *argv[]) {
  if (argc == 1) {
    puts("Not enough arguments");
    return 1;
  }
  size_t out_len;
  puts(my_sha1(argv[1], strlen(argv[1]), &out_len));
}

