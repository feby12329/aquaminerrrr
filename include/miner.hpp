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

#ifndef M_MINER_H
#define M_MINER_H
#include <curl/curl.h>
#include <gmp.h>
#include <spdlog/spdlog.h>

#include <cstring>
#include <mutex>
#include <string>

#include "aqua.hpp"
#include "spdlog/sinks/stdout_color_sinks.h"
#define HASH_LEN (32)
#define HASH_INPUT_LEN (40)
#define zero32 \
  "0x0000000000000000000000000000000000000000000000000000000000000000"

class WorkPacket {
 public:
  WorkPacket();
  uint8_t *input;
  char inputStr[67];  // copied from getWork
  char version = 0;
  mpz_t difficulty;
  mpz_t target;
  uint8_t *output;
  uint64_t nonce = 0;
  uint8_t *noncebuf;
  uint8_t buf[40];  // input + nonce
 private:
};

bool getwork(const std::string endpoint, WorkPacket *work, const bool verbose);
bool submitwork(WorkPacket *work, std::string endpoint, const bool verbose,
                CURL *curl);

// Miner Class
class Miner {
 public:
  Miner(const std::string url, const uint8_t nThreads, const uint8_t nCPU,
        const bool verboseLogs, const bool benching);
  ~Miner();
  void start(void);

 private:
  bool verbose;
  bool benching;
  std::string poolUrl;
  uint8_t numThreads;
  int num_cpus;
  bool getwork();
  CURL *getworkcurl;
  CURL *submitcurl;
  void initcurl(CURL *, int);                  // int typ defined in http.cpp
  std::shared_ptr<spdlog::logger> logger;      // for miner
  std::shared_ptr<spdlog::logger> getworklog;  // for getwork
  std::mutex workmu;
  bool getCurrentWork(WorkPacket *work_t, uint8_t thread_id) {
    workmu.lock();
    if (currentWork->version == 0) {
      workmu.unlock();
      spdlog::debug("no work yet...");
      return false;
    }
    if (strcmp(currentWork->inputStr, work_t->inputStr) == 0) {
      workmu.unlock();
      return true;
    }
    // print new work
    // TODO: move to getwork thread
    logger->info("CPU {} new work: algo '{}' diff: {} input: {}", thread_id,
                 currentWork->version,
                 mpzToString(currentWork->difficulty).c_str(),
                 std::string(currentWork->inputStr).substr(0, 8));
    work_t->version = currentWork->version;
    strcpy(work_t->inputStr, currentWork->inputStr);
    memcpy(work_t->input, currentWork->input, 32);
    memcpy(work_t->buf, currentWork->input, 32);
    work_t->input = currentWork->input;
    work_t->version = currentWork->version;
    mpz_set(work_t->difficulty, currentWork->difficulty);
    mpz_set(work_t->target, currentWork->target);
    workmu.unlock();
    return true;
  };
  void minerThread(uint8_t id);
  void getworkThread(const char *id);
  //  void submitworkThread(const char *id);
  WorkPacket *currentWork;

  std::atomic<unsigned long long> numTries;
  void submitTries(uint8_t thread_id, uint64_t numTries);
};

// bool getwork(const std::string endpoint, WorkPacket *work, const bool
// verbose);

std::string mpzToString(mpz_t num);

#endif  // header
