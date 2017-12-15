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


#include "cJSON.h"

#include "Flash_RW.h"


#define BUFFER_SIZE 1024
#define DATA_HEADER_SIZE 100

static const char *TAG = "Flash Read&Write";

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

void latest_block_write( const void* src )
{ //write latest block to latest_block_partition
    if(stc == NULL){ESP_LOGE(TAG,"ERROR: NULL pointer in function latest_block_write!\n");return;}

    const esp_partition_t *latest_block_partition = NULL;
    esp_err_t err;

    latest_block_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA,
                                                      &LATEST_BLOCK_PARTITION_LABEL);

    assert(latest_block_partition != NULL);
    if(!latest_block_partition){ESP_LOGE(TAG,"CANNOT find latest_block_partition!!\n");}
    else{
        ESP_LOGE(TAG,"Partition: latest_block is found. address: 0x%08x, size: 0x%08x \n",
                 latest_block_partition->address, latest_block_partition->size);

        err = esp_partition_write(latest_block_partition, 0 , src , BLOCKSIZE );

        if(err == ESP_OK){ESP_LOGE(TAG,"esp_partition_write genesis_bolck to latest_block succeed!\n");}
        else{ESP_LOGE(TAG,"esp_partition_write genesis_bolck to latest_block failed! error = %d\n" , err);}
    }
    return;
}

char *latest_block_read( void )
{ //read latest block from latest_block_partition
    const esp_partition_t *latest_block_partition = NULL;
    char l_block[BLOCKSIZE] = { 0 };
    esp_err_t err;

    latest_block_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA,
                                                      &LATEST_BLOCK_PARTITION_LABEL);
    assert(latest_block_partition != NULL);
    if(!latest_block_partition){
        ESP_LOGE(TAG,"CANNOT find latest_block_partition!!\n");
    }else{
        ESP_LOGE(TAG,"Partition: latest_block is found. address: 0x%08x, size: 0x%08x \n",
               latest_block_partition->address, latest_block_partition->size);

        err = esp_partition_read(latest_block_partition, 0 , l_block , BLOCKSIZE );

        if(err == ESP_OK){ESP_LOGE(TAG,"esp_partition_read genesis_bolck from latest_block succeed!\n");}
        else{ESP_LOGE(TAG,"esp_partition_read genesis_bolck from latest_block failed! error = %d\n" , err);}
    }
    return l_block;
}

void write_block_on_chain(uint8_t index , const void* src )
{
    if(src == NULL){ESP_LOGE(TAG,"ERROR: NULL pointer!\n");return;}

    const esp_partition_t *chain_partition = NULL;
    esp_err_t err;

    chain_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA,
                                                      &VED_CHAIN_PARTITION_LABEL);

    assert(chain_partition != NULL);
    if(!chain_partition){
        ESP_LOGE(TAG,"CANNOT find ved_chain_partition!!\n");
    }else{
        ESP_LOGE(TAG,"Partition: ved_chain is found. address: 0x%08x, size: 0x%08x \n",
                 chain_partition->address, chain_partition->size);

        err = esp_partition_write(chain_partition, index*BLOCKSIZE , src , BLOCKSIZE );

        if(err == ESP_OK){ESP_LOGE(TAG,"Write %d th block on chain succeed!\n", index);}
        else{ESP_LOGE(TAG,"Write %d th block on chain failed! error = %d\n" , index , err);}
    }
    return;
}

char *read_block_on_chain(uint8_t index)
{
    const esp_partition_t *chain_partition = NULL;
    esp_err_t err;
    char block[BLOCKSIZE] = { 0 };

    chain_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA,
                                               &VED_CHAIN_PARTITION_LABEL);

    assert(chain_partition != NULL);
    if(!chain_partition){ESP_LOGE(TAG,"CANNOT find ved_chain_partition!!\n");}
    else
    {
        ESP_LOGE(TAG,"Partition: ved_chain is found. address: 0x%08x, size: 0x%08x \n",
                 chain_partition->address, chain_partition->size);

        err = esp_partition_read(chain_partition, index*BLOCKSIZE , block , BLOCKSIZE );

        if(err == ESP_OK){ESP_LOGE(TAG,"Write %d th block on chain succeed!\n", index);}
        else{ESP_LOGE(TAG,"Write %d th block on chain failed! error = %d\n" , index , err);}
    }
    return block;
}

esp_err_t verified_chain_copy(uint8_t num)
{
    const esp_partition_t *unv_chain_partition = NULL;
    const esp_partition_t *ved_chain_partition = NULL;
    esp_err_t err;
    char block[ BUFFERSIZE ] = { 0 };

    unv_chain_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA,
                                                   &UNV_CHAIN_PARTITION_LABEL);
    assert(unv_chain_partition != NULL);
    if(!unv_chain_partition){ESP_LOGE(TAG,"CANNOT find unv_chain_partition!!\n");}

    ved_chain_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA,
                                                   &VED_CHAIN_PARTITION_LABEL);
    assert(ved_chain_partition != NULL);
    if(!unv_chain_partition){ESP_LOGE(TAG,"CANNOT find unv_chain_partition!!\n");}

    for(uint8_t i=0 ; i <= num ; i++)
    {
        err = esp_partition_read(nuv_chain_partition, i*BLOCKSIZE , block , BLOCKSIZE );
        if(err != ESP_OK){ESP_LOGE(TAG,"Read %d th block on ved_chain failed! error = %d\n" , i , err);return err;}
        else
        {
            err = esp_partition_write(ved_chain_partition,i*BLOCKSIZE , block , BLOCKSIZE);
            if(err != ESP_OK){ESP_LOGE(TAG,"Write %d th block on ved_chain failed! error = %d\n" , i , err);return err;
            }else{ESP_LOGE(TAG,"Write %d th block on ved_chain succeed!\n");}
        }
    }
    ESP_LOGE(TAG,"Write whole blockchain to ved_chain_partition succeed!\n\n");
    return err;
}

esp_err_t verified_data_copy(void)
{
    const esp_partition_t *nuv_data_partition = NULL;
    const esp_partition_t *ved_data_partition = NULL;
    esp_err_t err;
    char buffer[BUFFER_SIZE] = { 0 };
    char data_header[DATA_HEADER_SIZE] = { 0 };

    nuv_data_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA,
                                                  &UNV_DATA_PARTITION_LABEL);
    assert(nuv_data_partition != NULL);
    if(!nuv_data_partition){ESP_LOGE(TAG,"CANNOT find nuv_data_partition!!\n");return ESP_ERR_NOT_FOUND;}

    ved_data_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA,
                                                  &VED_DATA_PARTITION_LABEL);
    assert(ved_data_partition != NULL);
    if(!ved_data_partition){ESP_LOGE(TAG,"CANNOT find ved_data_partition!!\n");return ESP_ERR_NOT_FOUND;}

    err = esp_partition_read(nuv_data_partition, 0 , data_header , DATA_HEADER_SIZE);
    if(err != ESP_OK){ESP_LOGE(TAG,"CANNOT read data header from nuv_data_partition!\n");return ESP_ERR_NOT_FOUND;}

    cJSON *root = NULL;//refer to cJSON library**
    root= cJSON_Parse(data_header);//turn string(data_header) to cJSON object**
    uint8_t length = cJSON_GetObjectItem(root,"length")->valueint;//give value int of "led" to led**
    cJSON_Delete(root);//release the area of root**

    for( uint8_t i=DATA_HEADER_SIZE ; i <= length ; i += BUFFER_SIZE )
    {
        err = esp_partition_read(nuv_data_partition, i , buffer , BUFFER_SIZE);
        if(err != ESP_OK){ESP_LOGE(TAG,"Read from nuv_data_partition failed!!\n");return err;}
        else
        {
            err = esp_partition_write(ved_data_partition, i , buffer , BUFFER_SIZE);
            if(err != ESP_OK){ESP_LOGE(TAG,"Data write failed at area [%d, %d]. err = %d \n", i, i+BUFFER_SIZE , err);return err;}
            else{ESP_LOGE(TAG,"Have transferred data length %d \n",i+BUFFER_SIZE-DATA_HEADER_SIZE);}
        }
    }

    err = esp_partition_write(ved_data_partition, 0 , data_header , DATA_HEADER_SIZE);
    if(err != ESP_OK){ESP_LOGE(TAG,"Write data_header failed! err= %d \n",err); return err;}

    ESP_LOGE(TAG,"COPY verified data to ved_data_partition finished!\n");
    return ESP_OK;
}


