/*
 * Blockchain practice
 * Beck Pang, 2017/11/22
 * Use big endian throughout first
 * @https://github.com/lhartikk/naivechain/blob/master/main.js
*/
#include "stdint.h"
#include "stdio.h"
#include "string.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "blockchain.h"
#include "mbedtls/sha256.h"
#include <time.h>
#include "cJSON.h"

static const char *TAG = "blockchain";

void print_hex(const char *label, const uint8_t *v, size_t len)
{
    size_t i;
    printf("%s: ", label);
    for (i = 0; i < len; ++i) {
        printf("%02x", v[i]);
    }
    printf("\n");
}

uint8_t bc_hex2byte(const char ch)
{
    if ( (ch >= '0') && (ch <= '9') ) {
        return ch - '0';
    }
    if ( (ch >= 'a') && (ch <= 'f') ) {
        return ch - 'a' + 10;
    }
    return 0;
}

/**
 * parsing an string to hex
 */
uint8_t *alloc_hex(const char *str, size_t *len)
{
    const size_t count = strlen(str) / 2;
    size_t i;

    uint8_t *v = malloc(count);

    for (i = 0; i < count; ++i) {
        const char high = bc_hex2byte(str[i * 2]);
        const char low  = bc_hex2byte(str[i * 2 + 1]);

        v[i] = high * 16 + low;
    }

    *len = count;
    return v;
}

/**
 * Serialize the inputs to big-endian
 * @http://davidederosa.com/basic-blockchain-programming/serialization-part-one/
 *
 * @param prevBlock
 * @param nextBlock
 */
static void calculateHash(block_t* prevBlock, block_t* nextBlock)
{
    // index + previousHash + timestamp + dataHash, 1 + 32 + 8 + 32 = 73
    uint8_t input[73];
    * input                     = prevBlock->index;
    *(input + 1)                = prevBlock->hash[0];
    *(uint64_t *)(input + 33)   = prevBlock->timestamp;
    *(input + 41)               = prevBlock->dataHash[0];

    mbedtls_sha256(input, 73, nextBlock->hash, 0);
}

static void generateBlock(block_t *prevBlock, block_t *block, uint8_t* datahash)
{
    time_t now;
    time(&now);
//    block_t preBlock = getLatestBlock();
    block->index = prevBlock->index + (uint8_t)1U;
    block->timestamp = (uint64_t)now;
    block->dataHash = malloc(32);
    block->previousHash = malloc(32);
    block->hash = malloc(32);

//    memcpy(block->dataHash, datahash,  (size_t) 32);
    memcpy(block->previousHash, prevBlock->hash, (size_t) 32);
    calculateHash(prevBlock, block);
}

static void generateGenesis(block_t* genesis, const char* hash_value)
{
    time_t now;
    time(&now);
    genesis->index = 0;
    genesis->timestamp = (uint64_t)now;
    genesis->dataHash = malloc(32);
    genesis->previousHash = malloc(32);
    genesis->hash = malloc(32);

    uint8_t *temp_hash;
    size_t  temp_len;
    temp_hash = alloc_hex(hash_value, &temp_len);
    memcpy(genesis->hash, temp_hash, temp_len);
    free(temp_hash);
}

static void create_block_task(void *pvParameter)
{
    block_t genesisBlock, nextBlock;

    const char *genesisHash = "816534932c2b7154836da6afc367695e6337db8a921823784c14378abed4f7d7";
    generateGenesis(&genesisBlock, genesisHash);

    generateBlock(&genesisBlock, &nextBlock, 0x0);

    /**
     * parse to cJSON
     */
    cJSON *root = NULL;
    cJSON *block1, *block2;
    root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "genesis block", block1 = cJSON_CreateObject());
    cJSON_AddNumberToObject(block1, "index",        genesisBlock.index);
    cJSON_AddNumberToObject(block1, "timestamp",    genesisBlock.timestamp);
    cJSON_AddStringToObject(block1, "dataHash",     (const char *)genesisBlock.dataHash);
    cJSON_AddStringToObject(block1, "previousHash", (const char *)genesisBlock.previousHash);
    cJSON_AddStringToObject(block1, "currentHash",  (const char *)genesisBlock.hash);
    cJSON_AddItemToObject(root, "second block", block2 = cJSON_CreateObject());
    cJSON_AddNumberToObject(block2, "index",        nextBlock.index);
    cJSON_AddNumberToObject(block2, "timestamp",    nextBlock.timestamp);
    cJSON_AddStringToObject(block2, "dataHash",     (const char *)nextBlock.dataHash);
    cJSON_AddStringToObject(block2, "previousHash", (const char *)nextBlock.previousHash);
    cJSON_AddStringToObject(block2, "currentHash",  (const char *)nextBlock.hash);

    char* print_out = cJSON_Print(root);
//    char* print_out = cJSON_PrintUnformatted(root);
    printf("%s \n", print_out);
    cJSON_Delete(root);

    /**
     * Debug process
     */
    print_hex("start time", (uint8_t *)&genesisBlock.timestamp, 8);
    print_hex("data hash", genesisBlock.dataHash, 32);
    print_hex("genesis prev hash", genesisBlock.previousHash, 32);
    print_hex("genesis block hash", genesisBlock.hash, 32);
    print_hex("next data hash", nextBlock.dataHash, 32);
    print_hex("next prev hash", nextBlock.previousHash, 32);
    print_hex("next block hash", nextBlock.hash, 32);

    free(genesisBlock.dataHash);
    free(genesisBlock.previousHash);
    free(genesisBlock.hash);

    free(nextBlock.dataHash);
    free(nextBlock.previousHash);
    free(nextBlock.hash);
    vTaskDelay(10000 / portTICK_PERIOD_MS);

    printf("Restarting now.\n");
    fflush(stdout);

    esp_restart();
}

void app_main()
{
    xTaskCreate(&create_block_task, "create_block_task", 8192, NULL, 5, NULL);
}