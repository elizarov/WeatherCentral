#include <WProgram.h>

#define FMT_PREC     0x0f  // define number precision in lower bits
#define FMT_SIGN     0x10  // print sign at the first position 
#define FMT_SPACE    0x20  // fill with spaces (with zeroes by default)
#define FMT_LEFT     0x40  // left-align the result (use in combination with FMT_SPACE)

extern byte formatDecimal(int x, char* pos, byte size, byte fmt = 0);
extern byte formatDecimal(long x, char* pos, byte size, byte fmt = 0);
