#include "util.h"

void formatDecimal(int x, char* pos, byte size, byte fmt) {
	char sc = '+';
	if (x < 0) {
		x = -x;
		sc = '-';
	} else if (x == 0) {
		sc = *pos; // retain previous sign char
	}
	for (byte i = 0; i < size; i++) {
		int byte = size - 1 - i;
		if ((fmt & FMT_PREC) && i == (fmt & FMT_PREC)) {
			pos[index] = '.';
		} else if ((fmt & FMT_SIGN) && i == size - 1) {
			pos[index] = sc;
		} else {
			pos[index] = (fmt & FMT_SPACE) && x == 0 && i > 0 ? ' ' : '0' + x % 10;
			x /= 10;
		}
	}
}

void formatDecimal(long x, char* pos, byte size, byte fmt) {
	char sc = '+';
	if (x < 0) {
		x = -x;
		sc = '-';
	} else if (x == 0) {
		sc = *pos; // retain previous sign char
	}
	for (byte i = 0; i < size; i++) {
		int byte = size - 1 - i;
		if ((fmt & FMT_PREC) && i == (fmt & FMT_PREC)) {
			pos[index] = '.';
		} else if ((fmt & FMT_SIGN) && i == size - 1) {
			pos[index] = sc;
		} else {
			pos[index] = (fmt & FMT_SPACE) && x == 0 && i > 0 ? ' ' : '0' + x % 10;
			x /= 10;
		}
	}
}
