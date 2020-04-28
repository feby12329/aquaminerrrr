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

#ifndef SMALLTHINGS_H
#define SMALLTHINGS_H
#include <gmp.h>

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
static const unsigned char hex_digits[] = {'0', '1', '2', '3', '4', '5',
                                           '6', '7', '8', '9', 'A', 'B',
                                           'C', 'D', 'E', 'F'};
void mpz_maxBest(mpz_t mpz_n);
int char2int(char input);
void __bin2hex(char *s, const unsigned char *p, size_t len);
char *bin2hex(const unsigned char *p, size_t len);
void hex2bin(const char *src, uint8_t *target);
void print_hex(const unsigned char *src, size_t len);
void to_hex(const unsigned char *src, char *target, size_t len);
void mpz_maxBest(mpz_t mpz_n);
void hex0x2bin(const char *src, uint8_t *target);
void mpz_fromBytesNoInit(uint8_t *bytes, size_t count, mpz_t mpz_result);

void mpz_fromBytes(uint8_t *bytes, size_t count, mpz_t mpz_result);
void decodeHex(const char *encoded, mpz_t mpz_res);
std::string decodeHex(const std::string &encoded);

void encodeHex(mpz_t mpz_num, std::string &res);
// target = 2 ^ 256 / difficulty
void computeTarget(mpz_t mpz_difficulty, mpz_t &mpz_target);
// difficulty = 2 ^ 256 / target
void computeDifficulty(mpz_t mpz_target, mpz_t &mpz_difficulty);
std::string mpzToString(mpz_t num);
typedef unsigned char byte;
typedef std::vector<byte> Bytes;

#endif  // SMALLTHINGS_H
