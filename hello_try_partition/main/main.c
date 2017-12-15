/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "esp_spi_flash.h"

#include <string.h>
#include <sys/socket.h>
#include <netdb.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_ota_ops.h"

#include "nvs.h"
#include "nvs_flash.h"
#include "Flash_RW.h"

static char GENESIS_BLOCK[] = "{\n"
        "\t\t\"index\":0,\n"
        "\t\t\"timestamp\":\t10,\n"
        "\t\t\"dataHash\":\t\"2c85fb3fa5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5\",\n"
        "\t\t\"previousHash\":\t\"2c85fb3fa5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5\",\n"
        "\t\t\"currentHash\":\t\"816534932c2b7154836da6afc367695e6337db8a921823784c14378abed4f7d7\"\n"
        "\t}";
static char SECOND_BLOCK[] = "{\n"
        "\t\t\"index\":\t1,\n"
        "\t\t\"timestamp\":\t10,\n"
        "\t\t\"dataHash\":\t\"6034fb3f2c2b7154836da6afc367695e6337db8a921823784c14378abed4f7d7\",\n"
        "\t\t\"previousHash\":\t\"816534932c2b7154836da6afc367695e6337db8a921823784c14378abed4f7d7\",\n"
        "\t\t\"currentHash\":\t\"00cbac8588784bf104e8ce5f1e570682ba6df979e33f1b1659208dcb4194e606\"\n"
        "\t}";

static char latest_block[ BLOCKSIZE+1 ] = { 0 };


void app_main()
{
    printf("Hello world!\n");

    /* Print chip information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("This is ESP32 chip with %d CPU cores, WiFi%s%s, ",
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    printf("silicon revision %d, ", chip_info.revision);

    printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
            (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    latest_block_write( &GENESIS_BLOCK );
    latest_block_read( &latest_block , BLOCKSIZE );
    if(latest_block[0])
    {
        printf("Latest block: %s",latest_block);
    }

    for (int i = 10; i >= 0; i--) {
        printf("Restarting in %d seconds...\n", i);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();
}
