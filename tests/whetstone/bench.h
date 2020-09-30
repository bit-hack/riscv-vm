/*
 * provide timing and utility functions.
 *
 * identifiers starting with bm_ are reserved
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>

/* return the last result of TIME_CYCLES.  */
#define GET_CYCLES 1 /* bm_get_cycles () */

/* output the result of type double, higher is better.  Never returns.  */
#define RESULT(val) bm_set_result (val)

/* output an error and kill the program.  Never returns.  */
#define ERROR(msg) bm_error (msg)

/* make sure this value is calculated and not optimized away.  */
#define USE(value) bm_eat((int)(value))

/* return a zero without the compiler knowing it.  */
#define ZERO bm_return_zero ()

/* tag the next result with the given string.  */
#define TAG(s) bm_set_tag (s)

#define TIME_CYCLES

static int bm_return_zero (void) { return 0; }
static void bm_eat (int val) { }
static void bm_set_tag (const char *tag) { printf ("TAG %s\n", tag); }
static void bm_error(char* msg) { printf("ERROR %s\n", msg); }
static void bm_set_result(float val) { ; } 
static void bm_get_cycles() { ; }

/*
 * declares the main function for you, use it like this:
 * BEGIN_MAIN
 *   code;
 *   more code;
 * END_MAIN
 */
#define BEGIN_MAIN					\
	int								\
	main (int argc, char **argv)	\
	{

#define END_MAIN					\
	  return 0;						\
	}

/*
 * can be used instead of coding your own main(), if you
 * only have a single, simple test
 */
#define SINGLE_BENCHMARK(code)		\
	BEGIN_MAIN						\
	code; 							\
	END_MAIN

#ifdef __cplusplus
}
#endif

