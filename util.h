#include <WProgram.h>

#define FMT_PREC  0x0f
#define FMT_SIGN  0x10
#define FMT_SPACE 0x20

extern void formatDecimal(int x, char* pos, byte size, byte fmt = 0);
extern void formatDecimal(long x, char* pos, byte size, byte fmt = 0);
