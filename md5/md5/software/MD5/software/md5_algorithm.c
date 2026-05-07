#include "md5_algorithm.h"

unsigned leftRotate(unsigned x, int n) {
    return (x << n) | (x >> (32 - n));
}

void md5(unsigned int *message, unsigned int *result) {
    uint32_t A, B, C, D, i, F, g, Dtemp;
    const uint32_t a0 = 0x67452301;
    const uint32_t b0 = 0xefcdab89;
    const uint32_t c0 = 0x98badcfe;
    const uint32_t d0 = 0x10325476;
    
    A = a0;
    B = b0;
    C = c0;
    D = d0;
    
    for (i = 0; i < 64; i++) {
        if (i < 16) {
            F = (B & C) | (~B & D);
            g = i;
        } else if (i < 32) {
            F = (D & B) | (~D & C);
            g = (5 * i + 1) % 16;
        } else if (i < 48) {
            F = B ^ C ^ D;
            g = (3 * i + 5) % 16;
        } else {
            F = C ^ (B | ~D);
            g = (7 * i) % 16;
        }
        
        Dtemp = D;
        D = C;
        C = B;
        B += leftRotate((A + F + K[i] + message[g]), s[i]);
        A = Dtemp;
    }
    
    result[0] = a0 + A;
    result[1] = b0 + B;
    result[2] = c0 + C;
    result[3] = d0 + D;
}
