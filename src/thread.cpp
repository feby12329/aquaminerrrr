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

#include <gmp.h>             // for mpz_init
#include <spdlog/fmt/fmt.h>  // for format_to
#include <spdlog/logger.h>   // for logger::set_level
#include <stdint.h>          // for uint8_t
#include <stdio.h>           // for printf
#include <stdlib.h>          // for malloc

#include <algorithm>  // for max
#include <memory>     // for shared_ptr, __share...
#include <thread>     // for thread
#include <utility>    // for move
#include <vector>     // for vector

#include "miner.hpp"
#include "spdlog/common.h"                        // for debug
#include "spdlog/logger.h"                        // for logger
#include "spdlog/sinks/ansicolor_sink-inl.h"      // for ansicolor_sink::pri...
#include "spdlog/sinks/stdout_color_sinks-inl.h"  // for stderr_color_mt

#ifdef SCHEDPOL
#include <sched.h>
//#include <sys/sysctl.h>
#include <sys/types.h>
#endif

using std::move;
using std::thread;
using std::vector;

WorkPacket::WorkPacket() {
  this->input =
      static_cast<uint8_t *>(malloc(HASH_INPUT_LEN * sizeof(uint8_t)));
  this->output = static_cast<uint8_t *>(malloc(HASH_LEN * sizeof(uint8_t)));
  this->nonce = 0;
  this->noncebuf = static_cast<uint8_t *>(malloc(8 * sizeof(uint8_t)));
  mpz_init(this->difficulty);
  mpz_init(this->target);
}

void Miner::start(void) {
  if (verbose) {
    logger->set_level(spdlog::level::debug);
  }

  if (numThreads == 0) {
    numThreads = std::thread::hardware_concurrency();
    logger->info("detected {} CPU cores", numThreads);
  }

  if (num_cpus == 0) {
    num_cpus = numThreads;
  }

  thread gwt(&Miner::getworkThread, this, "getwork()");
  // start threads
  logger->info("starting {} threads..", numThreads);
  vector<thread *> threads;
  threads.resize(numThreads);
  vector<thread> threadList(numThreads);
  for (uint8_t i = 0; i < numThreads; i++) {
    threads[static_cast<int>(i)] = new thread(&Miner::minerThread, this, i + 1);
  }

  logger->info("waiting for getwork to finish");
  gwt.join();
  int i = 0;
  for (auto t : threads) {
    t->join();
    logger->info("miner thread {} ended", i + 1);
    i++;
  }
  logger->info("all threads finished");
}
