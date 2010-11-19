#include "util.h"

void formatDecimal(int x, char* pos, int size, byte prec, boolean sign) {
	char sc = '+';
	if (x < 0) {
		x = -x;
		sc = '-';
	} else if (x == 0) {
		sc = *pos; // retain previous sign char
	}
	for (int i = 0; i < size; i++) {
		int index = size - 1 - i;
		if (prec != 0 && i == prec) {
			pos[index] = '.';
		} else if (sign && i == size - 1) {
			pos[index] = sc;
		} else {
			pos[index] = '0' + x % 10;
			x /= 10;
		}
	}
}
