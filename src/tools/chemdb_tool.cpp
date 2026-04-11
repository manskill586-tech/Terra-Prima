#include "terra/chem/ChemDB.h"

#include <iostream>
#include <string>

namespace {

struct Options {
  std::string dataRoot = "data/elements";
  std::string outPath = "data/elements/chemdb.bin";
};

Options ParseArgs(int argc, char** argv) {
  Options opts;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    auto next = [&](std::string& out) {
      if (i + 1 < argc) {
        out = argv[++i];
      }
    };
    if (arg == "--data-root") {
      next(opts.dataRoot);
    } else if (arg == "--out") {
      next(opts.outPath);
    }
  }
  return opts;
}

} // namespace

int main(int argc, char** argv) {
  const Options opts = ParseArgs(argc, argv);
  terra::chem::ChemDB db;
  terra::chem::ChemDBConfig config{};
  config.dataRoot = opts.dataRoot;
  config.cachePath = opts.outPath;

  try {
    if (!terra::chem::ChemDB::BuildCacheFromRaw(config, db)) {
      std::cerr << "chemdb_tool: failed to build cache from " << opts.dataRoot << "\n";
      return 1;
    }
    if (!db.SaveCache(opts.outPath)) {
      std::cerr << "chemdb_tool: failed to save cache to " << opts.outPath << "\n";
      return 2;
    }
  } catch (const std::exception& ex) {
    std::cerr << "chemdb_tool: exception: " << ex.what() << "\n";
    return 3;
  } catch (...) {
    std::cerr << "chemdb_tool: unknown exception\n";
    return 4;
  }

  std::cout << "chemdb_tool: saved cache to " << opts.outPath
            << " (elements=" << db.Elements().size()
            << ", molecules=" << db.Molecules().size()
            << ", reactions=" << db.Reactions().size() << ")\n";
  return 0;
}
