#include <cstdio>
#include <cstring>


// enable program trace mode
extern bool g_arg_trace;
// enable compliance mode
extern bool g_arg_compliance;
// show MIPS throughput
extern bool g_arg_show_mips;

// target executable
extern const char *g_arg_program;


void print_usage(const char *filename) {
  fprintf(stderr, R"(
  Usage: %s [options]
  Option:        | Description:
 ----------------+-----------------------------------
  program        | RV32IM ELF file to execute
  --compliance   | Generate a compliance signature
  --trace        | Print execution trace
  --show-mips    | Show MIPS throughput
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
        g_arg_compliance = true;
        continue;
      }
      if (0 == strcmp(arg, "--trace")) {
        g_arg_trace = true;
        continue;
      }
      if (0 == strcmp(arg, "--show-mips")) {
        g_arg_show_mips = true;
        continue;
      }
      // error
      fprintf(stderr, "Unknown argument '%s'\n", arg);
      return false;
    }
    // set the executable
    g_arg_program = arg;
  }
  // success
  return true;
}
