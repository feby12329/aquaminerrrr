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

#include <bits/exception.h>       // for exception
#include <curl/curl.h>            // for curl_easy_setopt
#include <jsoncpp/json/config.h>  // for JSONCPP_STRING
#include <jsoncpp/json/reader.h>  // for CharReader, CharRea...
#include <jsoncpp/json/value.h>   // for Value, arrayValue
#include <jsoncpp/json/writer.h>  // for operator<<
#include <spdlog/fmt/fmt.h>       // for format_to
#include <stdint.h>               // for uint8_t
#include <string.h>               // for strcmp, strcpy, strlen

#include <atomic>    // for atomic_ullong, __at...
#include <chrono>    // for duration, high_reso...
#include <cstdio>    // for printf, sprintf
#include <iostream>  // for operator<<, endl
#include <memory>    // for __shared_ptr_access
#include <mutex>     // for mutex
#include <stdexcept>
#include <string>   // for string, operator<<
#include <thread>   // for sleep_for
#include <utility>  // for move

#include "aqua.hpp"                               // for decodeHex, computeD...
#include "miner.hpp"                              // for Miner, WorkPacket
#include "spdlog/details/log_msg-inl.h"           // for log_msg::log_msg
#include "spdlog/logger.h"                        // for logger
#include "spdlog/sinks/ansicolor_sink-inl.h"      // for ansicolor_sink::pri...
#include "spdlog/sinks/stdout_color_sinks-inl.h"  // for stdout_color_mt

#ifdef SCHEDPOL
#define handle_error_en(en, msg) \
  do {                           \
    errno = en;                  \
    perror(msg);                 \
    exit(EXIT_FAILURE);          \
  } while (0)
#endif

#define GETWORK 1
#define SUBMITWORK 2

using std::atomic_ullong;
using std::cout;
using std::endl;
using std::string;

atomic_ullong sharesSubmitted;
atomic_ullong sharesValid;
atomic_ullong errCount;

Miner::Miner(const std::string url, const uint8_t nThreads, const uint8_t nCPU,
             const bool verboseLogs, const bool bench) {
  poolUrl = url;
  numThreads = nThreads;
  num_cpus = nCPU;
  verbose = verboseLogs;
  benching = bench;
  this->currentWork = new WorkPacket();
  this->logger = spdlog::stderr_color_mt("MINER");
  this->getworklog = spdlog::stderr_color_mt("GETWORK");

  this->getworkcurl = curl_easy_init();
  this->submitcurl = curl_easy_init();
  this->initcurl(this->getworkcurl, GETWORK);
  this->initcurl(this->submitcurl, SUBMITWORK);
}

Miner::~Miner() {
  printf("Miner dead!\n");
  curl_easy_cleanup(this->getworkcurl);
  curl_easy_cleanup(this->submitcurl);
}

int aquahash_version(void *out, const void *in, uint32_t mem);

void Miner::getworkThread(const char *thread_id) {
  auto logger = spdlog::stdout_color_mt("HTTP");
#ifdef SCHEDPOL
  sched_param sch;
  const sched_param wantsch = {10};
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

  this->numTries = 0;
  typedef std::chrono::high_resolution_clock Time;
  auto t1 = Time::now();
  auto ltime = Time::now();
  unsigned long long totalHash = 0;
  unsigned long long numHashesSinceLast = 0;
  float fps = 0.0;
  char fpsbuf[100];

  // bench mark 1M and exit
  if (benching) {
    workmu.lock();
    uint8_t in[40];
    uint8_t out[32];
    this->currentWork->version = '2';
    aquahash_version(out, in, 1);
    printf("Aquahash v2 Benchmark zero[32]=");
    print_hex(out, 32);

    logger->info("Starting 1M hashes");
    for (int i = 0; i < 31; i = i + 2) {
      this->currentWork->inputStr[i] = '1';
      this->currentWork->inputStr[i + 2] = '1';
    }
    workmu.unlock();
    // t1
    t1 = std::chrono::high_resolution_clock::now();
    // wait for hashes
    while (totalHash < 1000000) {
      totalHash += this->numTries;
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    // t2
    std::chrono::duration<double> dur =
        std::chrono::high_resolution_clock::now() - t1;

    workmu.lock();
    this->currentWork->inputStr[0] = '!';  // triggers a work copy
    this->currentWork->version = '!';      // kills the miners (if benching)
    workmu.unlock();

    // print hashrate and duration
    double sec = dur.count();
    printf("benchmark completed %llu hashes in %4.4f seconds (%4.4f kHs/sec)\n",
           totalHash, sec, totalHash / sec / 1000);
    return;
  }
  while (true) {
    if (!this->getwork()) {
      logger->warn("getwork() failed");
    };
    // print hashrate
    t1 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> durationSinceLast = t1 - ltime;
    ltime = t1;

    numHashesSinceLast = this->numTries;
    this->numTries = 0;
    if (numHashesSinceLast == 0 && totalHash != 0) {
      logger->warn("miner threads have been sleeping?");
      std::this_thread::sleep_for(std::chrono::milliseconds(3000));
      continue;
    }

    // calculate hashrate
    totalHash += numHashesSinceLast;
    fps = static_cast<double>(numHashesSinceLast) / durationSinceLast.count();
    if (fps == 0) {
      if (totalHash != 0) {
        logger->warn("can't calculate hashrate?");
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(3000));
      continue;
    }
    unsigned long long submitted = sharesSubmitted;
    unsigned long long submittedValid = sharesValid;
    unsigned long long errs = errCount;
    unsigned long long rejected = submitted - submittedValid;
    sprintf(fpsbuf, "Aquahash v%c [%04.4f kH/s] (%010llu) Valid=%llu Bad=%llu",
            this->currentWork->version, fps / 1000.00, totalHash,
            submittedValid, rejected);
    this->logger->info("{}", fpsbuf);

    if (errs != 0) {
      logger->warn("Pool HTTP Errors = %lu\n", errs);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
  }
}
namespace {
std::size_t callback(const char *in, std::size_t size, std::size_t num,
                     std::string *out) {
  const std::size_t totalBytes(size * num);
  out->append(in, totalBytes);
  return totalBytes;
}
}  // namespace

// initcurl sets the curl handle for the getwork() calls
void Miner::initcurl(CURL *curl, int typ) {
  // headers

  struct curl_slist *headers = nullptr;
  headers = curl_slist_append(headers, "User-Agent: AquaMinerPro/2.0");
  headers = curl_slist_append(headers, "Content-Type: application/json");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  if (typ == GETWORK) {
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS,
                     "{\"jsonrpc\":\"2.0\",\"method\":\"aqua_getWork\","
                     "\"params\":[],\"id\":42}");
    logger->info("Initializing getwork curl handle");
  } else if (typ == SUBMITWORK) {
    logger->info("Initializing submitwork curl handle");
  } else {
    throw std::invalid_argument("INVALID HTTP REQUEST TYPE");
  }

  // method POST
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");

  // Set remote URL.
  curl_easy_setopt(curl, CURLOPT_URL, poolUrl.c_str());
  printf("poolURL:, %s\n", poolUrl.c_str());

  // Don't bother trying IPv6, which would increase DNS resolution time.
  curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);

  // Don't wait forever, time out after 10 seconds.
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);

  // Follow HTTP redirects if necessary.
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

  // Hook up data handling function.
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, callback);
}

bool Miner::getwork() {
  // Hook up data container (will be passed as the last parameter to the
  // callback handling function).  Can be any pointer type, since it will
  // internally be passed as a void pointer.
  std::unique_ptr<std::string> httpData(new std::string());
  curl_easy_setopt(getworkcurl, CURLOPT_WRITEDATA, httpData.get());
  char errbuf[CURL_ERROR_SIZE];
  curl_easy_setopt(getworkcurl, CURLOPT_ERRORBUFFER, errbuf);
  errbuf[0] = 0;  // empty string

  // Response information.
  CURLcode res;
  long httpCode(0);

  // Run our HTTP POST command, capture the HTTP response code
  res = curl_easy_perform(getworkcurl);
  curl_easy_getinfo(getworkcurl, CURLINFO_RESPONSE_CODE, &httpCode);

  // parse JSON response
  string rawJson = *httpData.get();
  int rawJsonLength = rawJson.length();
  if (res != CURLE_OK || httpCode != 200) {
    size_t len = strlen(errbuf);
    if (len) {
      getworklog->warn("{}{}", errbuf, ((errbuf[len - 1] != '\n') ? "\n" : ""));
    } else {
      getworklog->warn("{}", curl_easy_strerror(res));
    }
    if (rawJsonLength != 0) {
      std::cout << "HTTP data was:\n" << rawJson << std::endl;
    }
    return false;
  }

  // Response looks good - done using Curl now.  Try to parse the results
  // and print them out.
  Json::Value jsonData;
  Json::CharReaderBuilder builder;
  JSONCPP_STRING err;
  const std::unique_ptr<Json::CharReader> jsonReader(builder.newCharReader());
  if (!jsonReader->parse(rawJson.c_str(), rawJson.c_str() + rawJsonLength,
                         &jsonData, &err)) {
    std::cout << "error" << std::endl;
    return false;
  }
  Json::Value val = jsonData["result"];
  if (val.type() != Json::arrayValue) {
    std::cout << "\nGot successful response from " << poolUrl << std::endl;
    cout << "invalid response: " << jsonData << endl;
    return false;
  }

  this->workmu.lock();
  // got work, copy to currentWork
  if (0 == strcmp(currentWork->inputStr, val[0].asString().c_str())) {
    logger->debug("no new work {}", currentWork->inputStr);
    this->workmu.unlock();
    return true;
  }

  if (verbose) {
    logger->debug("Got successful response from {}", poolUrl);
    logger->debug("Successfully parsed JSON data:");
    std::cout << jsonData.toStyledString() << std::endl;
  }

  // do it

  // save input

  strcpy(currentWork->inputStr, val[0].asString().c_str());
  // get version
  currentWork->version = val[1].asString().c_str()[65];
  // get input byte
  hex0x2bin(val[0].asString().c_str(), currentWork->input);
  // decode hex difficulty
  decodeHex(val[3].asString().c_str(), currentWork->difficulty);

  // compute target
  decodeHex(val[2].asString().c_str(), currentWork->target);
  // compute difficulty
  computeDifficulty(currentWork->target, currentWork->difficulty);
  this->workmu.unlock();
  return true;
}
/*
static const char *submitfmt =
    "{\"jsonrpc\":\"2.0\", \"id\" : 42, \"method\" : \"aqua_submitWork\", "
    "\"params\" : "
    "[\"0x%s\",\"0x%s\","
    "\"0x0000000000000000000000000000000000000000000000000000000000000000\"]"
    "}";
    */

auto noncelog = spdlog::stdout_color_mt("SUBMIT");
bool submitwork(WorkPacket *work, string endpoint, const bool verbose,
                CURL *submitcurl) {
#ifdef DEBUG
  print_hex(&work->buf[32], 8);
#endif
  // flip endianness
  uint8_t noncebuf[8];
  int j = 0;
  for (int i = 7; i >= 0; i--) {
    noncebuf[i] = work->buf[32 + j];
    j++;
  }

  // to hex
  char noncehex[17];  // plus one for the zero
  to_hex(noncebuf, noncehex, 8);

  // curl request
  char buf[233];  // pool max is 256, but its always the same size (?)
  sprintf(
      buf,
      "{\"jsonrpc\":\"2.0\", \"id\" : 42, \"method\" : \"aqua_submitWork\", "
      "\"params\" : "
      "[\"0x%s\",\"%s\","
      "\"0x0000000000000000000000000000000000000000000000000000000000000000\""
      "]"
      "}",
      noncehex, work->inputStr);
  curl_easy_setopt(submitcurl, CURLOPT_POSTFIELDS, buf);

  // Response information.
  long httpCode(0);
  std::unique_ptr<std::string> httpData(new std::string());

  // Hook up data handling function.
  curl_easy_setopt(submitcurl, CURLOPT_WRITEFUNCTION, callback);

  // Hook up data container (will be passed as the last parameter to the
  // callback handling function).  Can be any pointer type, since it will
  // internally be passed as a void pointer.
  curl_easy_setopt(submitcurl, CURLOPT_WRITEDATA, httpData.get());

  // Run our HTTP GET command, capture the HTTP response code, and clean up.
  curl_easy_perform(submitcurl);

  curl_easy_getinfo(submitcurl, CURLINFO_RESPONSE_CODE, &httpCode);

  //
  string rawJson = *httpData.get();
  int rawJsonLength = rawJson.length();
  if (httpCode != 200) {
    noncelog->error("Pool returned bad status code: {}", httpCode);
    if (rawJsonLength != 0) {
      std::cout << "HTTP data was:\n" << rawJson << std::endl;
    }
    return false;
  }

  Json::Value jsonData;
  Json::CharReaderBuilder builder;
  JSONCPP_STRING err;
  const std::unique_ptr<Json::CharReader> jsonReader(builder.newCharReader());
  if (!jsonReader->parse(rawJson.c_str(), rawJson.c_str() + rawJsonLength,
                         &jsonData, &err)) {
    noncelog->error("error parsing response: {}", err);
    return false;
  }
#ifdef DEBUG
  noncelog->debug("Successfully parsed JSON data:");
  std::cout << jsonData.toStyledString() << std::endl;
#endif

  Json::Value val = jsonData["result"];
  sharesSubmitted++;
  if (val.type() != Json::booleanValue) {
    noncelog->error(
        "invalid pool response, wasn't true OR false! Maybe switch pools or "
        "check internet connection?");
    errCount++;
    return false;
  }
  if (val.asBool()) {
    noncelog->info("Pool confirmed a share!");
    sharesValid++;
  } else {
    noncelog->warn("Pool marked a share invalid :*(");
  }
  return val.asBool();
}
