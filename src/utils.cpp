#include <stdint.h>
#include "utils.h"

char nibbleToHex(uint8_t nibble) {
  return ((nibble <= 9) ? ('0' + nibble) : ('A' + nibble - 10));
}

void hexToAscii(const uint8_t *input, char *output, uint8_t size) {
  while (size--) {
    *(output++) = nibbleToHex((*input) >> 4u);
    *(output++) = nibbleToHex((*input) & 0x0Fu);

    input++;
  }
}
