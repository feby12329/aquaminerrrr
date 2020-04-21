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

#include <stdlib.h>
#include <time.h>
#include <cli11/CLI11.hpp>
#include "miner.hpp"

#ifndef VERSION
#define VERSION "0.0.0-unknown"
#endif

using namespace std;
int main(int argc, char **argv) {
  const string appname = "Aquachain Miner v" VERSION " (GPLv3)";
  const string sourcelink = "(Source: https://github.com/aerth/aquaminer)";
  // flag defaults
  string filename = "aquaminer.conf";
  bool verbose = false;
  bool mkconfig = false;
  string poolurl = "http://127.0.0.1:8543";
  uint8_t numThreads = 1;
  int numCPU = 1;

  // flags
  CLI::App app{appname};
  app.allow_config_extras(true);
  app.add_flag("-v,--verbose", verbose, "Verbose logging");
  app.add_flag("--mkconf", mkconfig,
               "create config based on given flags and exit");
  app.add_option("-F,--pool", poolurl, "pool URL to mine to");
  app.add_option("-t,--threads", numThreads, "number of threads to start");
  app.add_option("-C,--cores", numCPU, "number of CPU cores (dont touch)");
  app.set_config("-c,--conf", filename, "Read a TOML config file", false);
  CLI11_PARSE(app, argc, argv);
  srand(time(NULL));

  if (mkconfig) {
    app.remove_option(app.get_option("--mkconf"));
    return std::cout << app.config_to_str(true, true) ? 0 : 111;
  }

  // print config
  app.remove_option(app.get_option("--mkconf"));
  std::cout << app.config_to_str(true, true);

  // start mining
  Miner *miner = new Miner;
  std::cout << appname << endl << sourcelink << endl;
  if (verbose) {
    spdlog::set_level(spdlog::level::debug);
    spdlog::debug("verbose logging enabled");
  }

  miner->poolUrl = poolurl;
  miner->numThreads = numThreads;
  miner->num_cpus = numCPU;
  miner->verbose = verbose;
  miner->start();
}
