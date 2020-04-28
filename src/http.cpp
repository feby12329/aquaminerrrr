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

#include <curl/curl.h>
#include <jsoncpp/json/json.h>
#include <chrono>
#include <condition_variable>
#include <cstdio>  // printf
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>
#include "aqua.hpp"
#include "miner.hpp"
using namespace std;
#define handle_error_en(en, msg) \
  do {                           \
    errno = en;                  \
    perror(msg);                 \
    exit(EXIT_FAILURE);          \
  } while (0)

atomic_ullong sharesSubmitted;
atomic_ullong sharesValid;
atomic_ullong errCount;

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
  auto t1 = std::chrono::high_resolution_clock::now();
  unsigned long long numHashesSinceLast = 0;
  auto ltime = std::chrono::high_resolution_clock::now();
  float fps;
  unsigned long long totalHash = 0;
  this->numTries = 0;
  char fpsbuf[100];
  while (true) {
    if (!this->getwork()) {
      logger->warn("getwork() failed");
    };
    // print hashrate
    t1 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> durationSinceLast = t1 - ltime;

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
    if (fps != 0) {
      unsigned long long submitted = sharesSubmitted;
      unsigned long long submittedValid = sharesValid;
      unsigned long long errs = errCount;
      unsigned long long rejected = submitted - submittedValid;
      sprintf(fpsbuf,
              "Aquahash v%c [%04.4f kH/s] (%010llu) Valid=%llu Bad=%llu",
              this->currentWork->version, fps / 1000.00, totalHash,
              submittedValid, rejected);
      this->logger->info("{}", fpsbuf);

      if (errs != 0) {
        logger->warn("Pool HTTP Errors = %lu\n", errs);
      }
    }
    ltime = t1;
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
bool Miner::getwork() {
  CURL *curl = curl_easy_init();
  struct curl_slist *headers = nullptr;
  headers = curl_slist_append(headers, "User-Agent: AquaMinerPro/2.0");
  headers = curl_slist_append(headers, "Content-Type: application/json");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS,
                   "{\"jsonrpc\":\"2.0\",\"method\":\"aqua_getWork\","
                   "\"params\":[],\"id\":42}");
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");

  // Set remote URL.
  curl_easy_setopt(curl, CURLOPT_URL, poolUrl.c_str());

  // Don't bother trying IPv6, which would increase DNS resolution time.
  curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);

  // Don't wait forever, time out after 10 seconds.
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);

  // Follow HTTP redirects if necessary.
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

  // Response information.
  long httpCode(0);
  std::unique_ptr<std::string> httpData(new std::string());

  // Hook up data handling function.
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, callback);
  char errbuf[CURL_ERROR_SIZE];
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
  errbuf[0] = 0;  // empty string

  // Hook up data container (will be passed as the last parameter to the
  // callback handling function).  Can be any pointer type, since it will
  // internally be passed as a void pointer.
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, httpData.get());

  // Run our HTTP GET command, capture the HTTP response code, and clean up.
  CURLcode res;

  res = curl_easy_perform(curl);

  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
  curl_easy_cleanup(curl);

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
bool submitwork(WorkPacket *work, string endpoint, const bool verbose) {
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
  char noncehex[17]; // plus one for the zero
  to_hex(noncebuf, noncehex, 8);

  // curl request
  CURL *curl = curl_easy_init();
  struct curl_slist *headers = nullptr;
  headers = curl_slist_append(headers, "User-Agent: AquaMinerPro/2.0");
  headers = curl_slist_append(headers, "Content-Type: application/json");
  char buf[233]; // pool max is 256, but its always the same size (?)
  printf("1\n");
  sprintf(
      buf,
      "{\"jsonrpc\":\"2.0\", \"id\" : 42, \"method\" : \"aqua_submitWork\", "
      "\"params\" : "
      "[\"0x%s\",\"%s\","
      "\"0x0000000000000000000000000000000000000000000000000000000000000000\""
      "]"
      "}",
      noncehex, work->inputStr);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, buf);

  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");

  // Set remote URL.
  curl_easy_setopt(curl, CURLOPT_URL, endpoint.c_str());

  // Don't bother trying IPv6, which would increase DNS resolution time.
  curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);

  // Don't wait forever, time out after 10 seconds.
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);

  // Follow HTTP redirects if necessary.
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

  // Response information.
  long httpCode(0);
  std::unique_ptr<std::string> httpData(new std::string());

  // Hook up data handling function.
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, callback);

  // Hook up data container (will be passed as the last parameter to the
  // callback handling function).  Can be any pointer type, since it will
  // internally be passed as a void pointer.
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, httpData.get());

  // Run our HTTP GET command, capture the HTTP response code, and clean up.
  curl_easy_perform(curl);

  printf("2\n");
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
  curl_easy_cleanup(curl);
  string rawJson = *httpData.get();
  int rawJsonLength = rawJson.length();
  if (httpCode != 200) {
    noncelog->error("Pool returned bad status code: {}");
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
