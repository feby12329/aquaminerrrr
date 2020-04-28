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

#include <aquahash.h>  // for argon2_context, arg...
#include <gmp.h>       // for mpz_init, mpz_cmp
#include <stdint.h>    // for uint8_t, uint32_t
#include <stdio.h>     // for printf
#include <stdlib.h>    // for malloc, exit, EXIT_...

#include <chrono>   // for milliseconds
#include <cstring>  // for memcpy
#include <random>   // for mt19937_64, random_...
#include <thread>   // for thread, sleep_for
//#include <utility>  // for move
#include <vector>  // for vector

#include "aqua.hpp"                               // for mpz_fromBytesNoInit
#include "miner.hpp"                              // for Miner
#include "spdlog/common.h"                        // for debug
#include "spdlog/logger.h"                        // for logger
#include "spdlog/sinks/ansicolor_sink-inl.h"      // for ansicolor_sink::pri...
#include "spdlog/sinks/stdout_color_sinks-inl.h"  // for stderr_color_mt

#ifdef DEBUG
#include <iostream>
#include <string>
#endif
#ifdef SCHEDPOL
#include <sched.h>
//#include <sys/sysctl.h>
#include <sys/types.h>
#endif

using std::move;
using std::thread;
using std::vector;

int aquahash_version(void *output, const void *input, uint32_t mem) {
  argon2_context context;
  context.out = static_cast<uint8_t *>(output);
  context.outlen = static_cast<uint32_t>(HASH_LEN);
  context.pwd = const_cast<uint8_t *>(static_cast<const uint8_t *>(input));
  context.pwdlen = static_cast<uint32_t>(HASH_INPUT_LEN);
  context.salt = nullptr;
  context.saltlen = 0;
  context.secret = nullptr;
  context.secretlen = 0;
  context.ad = nullptr;
  context.adlen = 0;
  context.allocate_cbk = nullptr;
  context.free_cbk = nullptr;
  context.flags = ARGON2_DEFAULT_FLAGS;
  context.m_cost = mem;
  context.lanes = 1;
  context.threads = 1;
  context.t_cost = 1;
  context.version = ARGON2_VERSION_13;
  return argon2_ctx(&context, Argon2_id);
}

#define handle_error_en(en, msg) \
  do {                           \
    errno = en;                  \
    perror(msg);                 \
    exit(EXIT_FAILURE);          \
  } while (0)

#ifdef AFFINE
static void affine_to_cpu_mask(int id, unsigned long mask, int num_cpus) {
  cpu_set_t set;
  CPU_ZERO(&set);
  for (uint8_t i = 0; i < num_cpus; i++) {
    // cpu mask
    if (mask & (1UL << i)) {
      CPU_SET(i, &set);
    }
  }
  if (id == -1) {
    // process affinity
    sched_setaffinity(0, sizeof(&set), &set);
  } else {
    // thread only
    // pthread_setaffinity_np(, sizeof(&set), &set);
  }
}
#endif
void Miner::minerThread(uint8_t thread_id) {
  static const bool verbose = this->verbose;
  logger->debug("thread {} started\n", thread_id);

#ifdef SCHEDPOL
  sched_param sch;
  const sched_param wantsch = {20};
  int policy;
  int s;
  s = pthread_getschedparam(pthread_self(), &policy, &sch);
  if (s != 0) {
    logger->error("Can't get thread priority!");
    handle_error_en(s, "pthread_attr_getschedpolicy #1");
  }
  logger->info("Thread {} was at priority {} ({})", thread_id,
               sch.sched_priority, policy);
  s = pthread_setschedparam(pthread_self(), SCHED_RR, &wantsch);
  if (s != 0) {
    logger->error("Can't set thread priority!");
    handle_error_en(s, "pthread_attr_setschedpolicy");
  }
  s = pthread_getschedparam(pthread_self(), &policy, &sch);
  if (s != 0) {
    handle_error_en(s, "pthread_attr_getschedpolicy #2");
  }
  logger->info("Thread {} is now at priority {} ({})", thread_id,
               sch.sched_priority, policy);
#endif

  logger->info("Binding thread {} to cpu {} (mask {})", thread_id,
               thread_id % num_cpus, (1 << ((thread_id - 1) % num_cpus)));

#ifdef AFFINE
  affine_to_cpu_mask(thread_id, 1UL << ((thread_id - 1) % num_cpus), num_cpus);
#endif

  // create new WorkPacket to store work variables
  WorkPacket *work = new WorkPacket();

  // random nonce
  std::random_device engine;
  std::mt19937_64 prng;
  uint64_t n = std::random_device{}();
  prng.seed(n);
  memcpy(&work->buf[32], &n, 4);
  prng.seed(n);
  memcpy(&work->buf[36], &n, 4);

  // initialize variables
  mpz_t mpz_result;
  mpz_init(mpz_result);

  uint64_t nonce_int = 0;
  uint64_t tries = 0;

  // so all the threads dont report at the same time
  uint64_t reportTriesMod = static_cast<uint64_t>(thread_id + (1 * 1000));
  // starting nonce
  memcpy(&nonce_int, &work->buf[32], 8);
  logger->info("Thread {} starting nonce: {}", thread_id, nonce_int);
  memcpy(&work->buf[32], &nonce_int, 8);

  // miner loop
  while (true) {
    // roll nonce
    memcpy(&nonce_int, &work->buf[32], 8);
    nonce_int++;
    memcpy(&work->buf[32], &nonce_int, 8);

#ifdef NONCEDEBUG
    printf("NEWNONCE:");
    print_hex(&work->buf[32], 8);
#endif

    if (tries % 20000 == 0) {
      // see if we got new work
      if (!this->getCurrentWork(work, thread_id)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        continue;
      }
    }

    // report hashrate every 10k hashes (per thread)
    if (tries % 20000 == reportTriesMod) {
      this->numTries += tries;
      tries = 0;
    }

    // Aquahash Version Switch (See Aquachain HF)
    //
    // TODO: move this to aquahash_version()
    uint32_t mem = 1;
    if (work->version == '2') {
    } else if (work->version == '3') {
      mem = 16;
    } else if (work->version == '4') {
      mem = 32;
    } else if (work->version == 0 || work->version == '0') {
      printf("thread %d going to sleep for 1 sec (no work yet)\n", thread_id);
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      continue;
    } else if (benching && work->version == '!') {
      break;
    } else {
      printf("thread %d going to sleep for 1 sec (no work: '%c')\n", thread_id,
             work->version);
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      continue;
    }

    // hash it
    //
    // TODO: accept work->version char as 3rd arg
    if (ARGON2_OK != aquahash_version(work->output, work->buf, mem)) {
      printf("argon2 failed\n");
      exit(111);
    }
    tries++;

    // TODO: memcmp
    mpz_fromBytesNoInit(work->output, HASH_LEN, mpz_result);
    if (mpz_cmp(mpz_result, work->target) > 0) {
      // reloop
      continue;
    }
#ifdef DEBUG
    printf("thread %d mining version %c (input=%s)\n", thread_id, work->version,
           work->inputStr);
    printf("input from thread %d: ", thread_id);
    print_hex(work->buf, 40);
    printf("\n");
    printf("output from thread %d: ", thread_id);
    print_hex(work->output, 32);
    printf("\n");
    printf("nonce from thread %d:", thread_id);
    print_hex(&work->buf[32], 8);
    printf("\n");
    printf("diff target from thread %d:", thread_id);
    std::string diff = mpzToString(work->difficulty);
    std::cout << diff << std::endl;
#endif
    logger->info("thread {} found new solution", thread_id);
    // std::thread(submitwork, work, poolUrl).detach();
    if (!submitwork(work, poolUrl, verbose, this->submitcurl)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(400));
      if (!this->getwork()) {
        printf(
            "submit work() failed, getwork() failed, not able to fetch "
            "work. sleeping for 1s\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
      } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(60));

    // if invalid diff, increase nonce
    // roll nonce
    // (*(uint64_t *)&work->buf[32])++;
  }
}
