// Aquachain CPU Miner

// Copyright (C) 2020 aerth <aerth@riseup.net>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "aqua.hpp"

#include <assert.h>
#include <gmp.h>

#include <cstdio>  /* printf, NULL */
#include <cstdlib> /* strtoull */
#include <cstring>

void mpz_maxBest(mpz_t mpz_n) {
  mpz_t mpz_two, mpz_exponent;
  mpz_init_set_str(mpz_two, "2", 10);
  mpz_init_set_str(mpz_exponent, "256", 10);
  mpz_init_set_str(mpz_n, "0", 10);
  mpz_pow_ui(mpz_n, mpz_two, mpz_get_ui(mpz_exponent));
}

// target = 2 ^ 256 / difficulty
void computeTarget(mpz_t mpz_difficulty, mpz_t &mpz_target) {
  mpz_t mpz_numerator;
  mpz_maxBest(mpz_numerator);
  mpz_init_set_str(mpz_target, "0", 10);
  mpz_div(mpz_target, mpz_numerator, mpz_difficulty);
}

// difficulty = 2 ^ 256 / target
void computeDifficulty(mpz_t mpz_target, mpz_t &mpz_difficulty) {
  mpz_t mpz_numerator;
  mpz_maxBest(mpz_numerator);
  mpz_init_set_str(mpz_difficulty, "0", 10);
  mpz_div(mpz_difficulty, mpz_numerator, mpz_target);
}

int char2int(char input) {
  if (input >= '0' && input <= '9') return input - '0';
  if (input >= 'A' && input <= 'F') return input - 'A' + 10;
  if (input >= 'a' && input <= 'f') return input - 'a' + 10;
  return -1;
}

/* Adequate size s==len*2 + 1 must be alloced to use this variant */
void __bin2hex(char *s, const unsigned char *p, size_t len) {
  int i;
  static const char hex[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                               '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

  for (i = 0; i < (int)len; i++) {
    *s++ = hex[p[i] >> 4];
    *s++ = hex[p[i] & 0xF];
  }
  *s++ = '\0';
}

/* Returns a malloced array string of a binary value of arbitrary length. The
 * array is rounded up to a 4 byte size to appease architectures that need
 * aligned array  sizes */
char *bin2hex(const unsigned char *p, size_t len) {
  size_t slen;
  char *s = nullptr;

  slen = len * 2 + 1;
  if (slen % 4) slen += 4 - (slen % 4);
  s = static_cast<char *>(calloc(slen, 1));
  __bin2hex(s, p, len);

  return s;
}

void hex2bin(const char *src, uint8_t *target) {
  while (*src && src[1]) {
    *(target++) = char2int(*src) * 16 + char2int(src[1]);
    src += 2;
  }
}
void hex0x2bin(const char *src, uint8_t *target) {
  src += 2;  // skip '0' and 'x'
  while (*src && src[1]) {
    *(target++) = char2int(*src) * 16 + char2int(src[1]);
    src += 2;
  }
}
void print_hex(const uint8_t *src, size_t len) {
  char *hstr = bin2hex(src, len);
  printf("%s\n", hstr);
  free(hstr);
}

void to_hex(const unsigned char *src, char *target, size_t len) {
  __bin2hex(target, src, len);
}

void mpz_fromBytesNoInit(uint8_t *bytes, size_t count, mpz_t mpz_result) {
  const int ORDER = 1;
  const int ENDIAN = 1;
  // assert(count % 4 == 0);
  mpz_import(mpz_result, count >> 2, ORDER, 4, ENDIAN, 0, bytes);
}

void mpz_fromBytes(uint8_t *bytes, size_t count, mpz_t mpz_result) {
  // mpz_init(mpz_result);  // must init before import
  mpz_fromBytesNoInit(bytes, count, mpz_result);
}

std::string mpzToString(mpz_t num) {
  char buf[256];  // must be at least 64 (big numbers ...)
  int ret = gmp_snprintf(buf, sizeof(buf), "%Zd", num);
  assert(ret < static_cast<int>(sizeof(buf)));
  return buf;
}

void decodeHex(const char *encoded, mpz_t mpz_res) {
  auto pStart = encoded;
  if (strncmp(encoded, "0x", 2) == 0) pStart += 2;
  mpz_init_set_str(mpz_res, pStart, 16);
}
