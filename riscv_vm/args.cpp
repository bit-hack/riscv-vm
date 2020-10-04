#include <cstdio>
#include <cstring>


extern bool g_arg_trace;
extern bool g_arg_compliance;
extern bool g_arg_show_mips;
extern bool g_fullscreen;

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
  --fullscreen   | Run in a fullscreen window
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
      if (0 == strcmp(arg, "--fullscreen")) {
        g_fullscreen = true;
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
