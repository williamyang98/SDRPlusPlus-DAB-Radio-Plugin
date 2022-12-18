#pragma once

#include <stdint.h>

// DOC: ETSI EN 300 401
// Clause 10 - Energy dispersal
// Generates the pseudo random binary stream that is used for energy dispersal
// The polynomial: G(x) = 1 + x^-5 + x^-9 is hard coded in
// This is fine since it is used for both the FIC and MSC in the OFDM frame
class AdditiveScrambler 
{
private:
    uint16_t syncword;
    uint16_t reg;
public:
    uint8_t Process() {
        uint8_t b = 0x00;
        for (int i = 0; i < 8; i++) {
            uint8_t v = 0;
            // G(x) = 1 + x^-5 + x^-9
            v ^= ((reg >> 8) & 0b1);
            v ^= ((reg >> 4) & 0b1);
            // NOTE: scrambler is operating in bit reversed mode to match incoming data
            b |= (v << (7-i));
            reg = (reg << 1) | v;
        }
        return b;
    }

    void SetSyncword(const uint16_t _syncword) {
        syncword = _syncword;
    }
    void Reset() {
        reg = syncword;
    }
};