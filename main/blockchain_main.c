/*
 * Blockchain practice
 * Beck Pang, 2017/11/22
 * Use big endian throughout first
 * @https://github.com/lhartikk/naivechain/blob/master/main.js
*/
#include "stdint.h"
#include "stdio.h"
#include "string.h"
#include "stdbool.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_ota_ops.h"

#include "blockchain.h"
#include "mbedtls/sha256.h"
#include <time.h>
#include <esp_partition.h>
#include "cJSON.h"

#include "nvs.h"
#include "nvs_flash.h"

/**
 * Global cJSON struct for blockchain
 */
cJSON *chainRoot;

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
static void calculateHashBlock(block_t *block, uint8_t *dst)
{
    // index + previousHash + timestamp + dataHash, 1 + 32 + 8 + 32 = 73
    uint8_t input[73];
    * input       = block->index;
    *(input + 1)  = block->previousHash[0];
    *(input + 33) = block->timestamp[0];
    *(input + 41) = block->dataHash[0];

    mbedtls_sha256(input, 73, dst, 0);
}

static void generateBlock(block_t *prevBlock, block_t *block, uint8_t* datahash)
{
    time_t now;
    time(&now);
//    block_t preBlock = getLatestBlock();
    block->index = prevBlock->index + (uint8_t)1U;
    block->timestamp = malloc(8);
    *(uint64_t *)block->timestamp = (uint64_t)now;
    block->dataHash = malloc(32);
    block->previousHash = malloc(32);
    block->hash = malloc(32);

//    memcpy(block->dataHash, datahash,  (size_t) 32);
    memcpy(block->previousHash, prevBlock->hash, (size_t) 32);
}

static void generateGenesis(block_t *genesis, const char* hash_value)
{
    time_t now;
    time(&now);
    genesis->index = 0;
    genesis->timestamp = malloc(8);
    *(uint64_t *)genesis->timestamp = (uint64_t)now;
    genesis->dataHash = malloc(32);
    genesis->previousHash = malloc(32);
    genesis->hash = malloc(32);

    uint8_t *temp_hash;
    size_t  temp_len;
    temp_hash = alloc_hex(hash_value, &temp_len);
    memcpy(genesis->hash, temp_hash, temp_len);
    free(temp_hash);
}

static void freeBlock(block_t *blockToFree)
{
    free(blockToFree->dataHash);
    free(blockToFree->previousHash);
    free(blockToFree->hash);
    free(blockToFree);
}

static bool isValidNewBlock(block_t* prevBlock, block_t *newBlock)
{
    uint8_t verifyHash[32];
    calculateHashBlock(newBlock, verifyHash);
    if (prevBlock->index + 1 != newBlock->index)
    {
        ESP_LOGI(TAG, "block index error");
        return false;
    } else if (prevBlock->hash != newBlock->previousHash)
    {
        ESP_LOGI(TAG, "new block previous hash is not equal to old block hash");
        return false;
    } else if (verifyHash != newBlock->hash)
    {
        ESP_LOGI(TAG, "invalid new block hash, block corrupted.");
        return false;
    }
    return true;
}

/**
 * Add single block the the main cJSON root
 * @param block_to_add
 */
static void addBlockToJSONRoot(cJSON *root,block_t *block_to_add)
{
    cJSON *newBlock;
    cJSON_AddItemToObject(root, "genesis block", newBlock = cJSON_CreateObject());
    cJSON_AddNumberToObject(newBlock, "index",        block_to_add->index);
    cJSON_AddStringToObject(newBlock, "timestamp",    (const char *)block_to_add->timestamp);
    cJSON_AddStringToObject(newBlock, "dataHash",     (const char *)block_to_add->dataHash);
    cJSON_AddStringToObject(newBlock, "previousHash", (const char *)block_to_add->previousHash);
    cJSON_AddStringToObject(newBlock, "currentHash",  (const char *)block_to_add->hash);
}

/**
* Hash the partition data, done
* TODO: solve the memory size issues
*/
static void hashRunningPartitionToBlock(block_t *block)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    uint8_t *partition_data = (uint8_t *) malloc((size_t)(running->size/10));
    esp_partition_read(running, 0, partition_data, (size_t)(running->size/10));
    mbedtls_sha256(partition_data, (size_t)(running->size/10), block->dataHash, 0);
    free(partition_data);
}

static void create_block_task(void *pvParameter)
{
    chainRoot = cJSON_CreateObject();

    block_t *genesisBlock = malloc(sizeof(block_t));
    block_t *nextBlock = malloc(sizeof(block_t));

    const char *genesisHash = "816534932c2b7154836da6afc367695e6337db8a921823784c14378abed4f7d7";
    generateGenesis(genesisBlock, genesisHash);

    generateBlock(genesisBlock, nextBlock, 0x0);

    addBlockToJSONRoot(chainRoot, genesisBlock);

    char *blockInJSON = cJSON_PrintUnformatted(chainRoot);

    printf("%s \n", blockInJSON);
    cJSON_Delete(chainRoot);

    /**
     * Rehash the new block
     */
    hashRunningPartitionToBlock(nextBlock);
    calculateHashBlock(nextBlock, nextBlock->hash);
    addBlockToJSONRoot(chainRoot, nextBlock);

    isValidNewBlock(genesisBlock, nextBlock);

    /**
     * Debug process
     */
    print_hex("start time", genesisBlock->timestamp, 8);
    print_hex("data hash", genesisBlock->dataHash, 32);
    print_hex("genesis prev hash", genesisBlock->previousHash, 32);
    print_hex("genesis block hash", genesisBlock->hash, 32);
    print_hex("next data hash", nextBlock->dataHash, 32);
    print_hex("next prev hash", nextBlock->previousHash, 32);
    print_hex("next block hash", nextBlock->hash, 32);

    freeBlock(genesisBlock);
    freeBlock(nextBlock);
    genesisBlock = NULL;
    nextBlock = NULL;

    vTaskDelay(10000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "Restarting now.\n");
    fflush(stdout);

    esp_restart();
}

void app_main()
{
    xTaskCreate(&create_block_task, "create_block_task", 8192, NULL, 5, NULL);
}