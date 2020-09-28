#include <cstdio>
#include <cstring>



// enable program trace mode
extern bool g_trace;
// enable compliance mode
extern bool g_compliance;
// target executable
extern const char *g_program;


void print_usage(const char *filename) {
  fprintf(stderr, R"(
  Usage: %s [options]
  Option:        | Description:
 ----------------+-----------------------------------
  program        | RV32IM ELF file to execute
  --compliance   | Generate a compliance signature
  --trace        | Print execution trace
)", filename);
}

bool parse_args(int argc, char **args) {
  // parse each argument in turn
  for (int i = 1; i < argc; ++i) {
    const char *arg = args[i];
    // parse flags
    if (arg[0] == '-') {
      if (0 == strcmp(arg, "--help")) {
        return false;
      }
      if (0 == strcmp(arg, "--compliance")) {
        g_compliance = true;
        continue;
      }
      if (0 == strcmp(arg, "--trace")) {
        g_trace = true;
        continue;
      }
      // error
      fprintf(stderr, "Unknown argument '%s'\n", arg);
      return false;
    }
    // set the executable
    g_program = arg;
  }
  // success
  return true;
}
