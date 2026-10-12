#ifndef NEURO_MANTLE_ECC_H
#define NEURO_MANTLE_ECC_H
#include "neuro_mantle.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Extended Hamming SECDED for one 64-bit word: 71 protected positions and one overall parity bit. */
void nm_ecc_encode64(uint64_t value, uint8_t codeword[9]);
/* Returns 0 for intact, 1 for corrected single bit, -1 for detected multiple bit error. */
int nm_ecc_decode64(const uint8_t codeword[9], uint64_t *value);
size_t nm_ecc_checkpoint_size(void);
int nm_ecc_capture(const nm_network *network, uint8_t *out, size_t length);
/* Restores atomically only if every codeword decodes; corrected receives correction count. */
int nm_ecc_restore(const uint8_t *data, size_t length, nm_network *network, size_t *corrected);
#ifdef __cplusplus
}
#endif
#endif
