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

static void create_block_task(void *pvParameter)
{
    time_t now;
    time(&now);
    block_t genesisBlock = { 0, (uint64_t) now, "genesis block", {0x0}, {0x0} };
    block_t nextBlock    = { 1, (uint64_t) now, "new block", {0x0}, {0x0} };

    /**
     * parsing an string to hex
     */
    const char *genesisHash = "816534932c2b7154836da6afc367695e6337db8a921823784c14378abed4f7d7";
    uint8_t *temp_hash;
    size_t  temp_len;
    temp_hash = alloc_hex(genesisHash, &temp_len);
    memcpy(genesisBlock.hash, temp_hash, temp_len);
    free(temp_hash);

    /**
     * Serialize the inputs to big-endian
     * @http://davidederosa.com/basic-blockchain-programming/serialization-part-one/
     */
    // index + previousHash + timestamp + data, 1 + 32 + 8 + 32 = 73
    uint8_t input[73];
    * input                     = genesisBlock.index;
    *(input + 1)                = genesisBlock.hash[0];
    *(uint64_t *)(input + 33)   = genesisBlock.timestamp;
    *(input + 41)               = genesisBlock.data[0];

    mbedtls_sha256(input, 73, nextBlock.hash, 0);

    /**
     * Debug process
     * */
    print_hex("start time", (input+33), 8);
    print_hex("genesis block hash", genesisBlock.hash, 32);
    print_hex("next block hash", nextBlock.hash, 32);
    vTaskDelay(10000 / portTICK_PERIOD_MS);

    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();
}

void app_main()
{
    xTaskCreate(&create_block_task, "create_block_task", 8192, NULL, 5, NULL);
}