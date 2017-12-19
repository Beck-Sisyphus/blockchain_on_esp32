//
// Created by beck on 21/11/2017.
//

#ifndef ESP_BLOCKCHAIN_H
#define ESP_BLOCKCHAIN_H

#include "stdint.h"

typedef struct {
    uint8_t  index;         // JSON: int
//    uint64_t timestamp;     // JSON: string
    uint8_t* timestamp;     // JSON: string
    uint8_t*  dataHash;         // JSON: string, TODO: change to dataHash for firmware
    uint8_t*  previousHash; // JSON: string
    uint8_t*  hash;         // JSON: string
} __attribute__((packed))block_t;

/* Bitcoin block
 *
 * #include <uint256.h>
 *
 * int32_t nVersion;
 * uint256 hashPrevBlock;
 * uint256 hashMerkleRoot;
 * uint32_t nTime;
 * uint32_t nBits;
 * uint32_t nNonce;
 *
 * */
#endif //ESP_BLOCKCHAIN_H
