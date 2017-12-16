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




static const char *TAG = "Flash Read&Write";


esp_err_t latest_block_write( const void* src )
{ //write latest block to latest_block_partition
    if(src == NULL){ESP_LOGE(TAG,"ERROR: NULL pointer in function latest_block_write!\n");return ESP_ERR_INVALID_ARG ;}

    const esp_partition_t *latest_block_partition = NULL;
    esp_err_t err;

    latest_block_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA,
                                                      &LATEST_BLOCK_PARTITION_LABEL);

    assert(latest_block_partition != NULL);
    if(!latest_block_partition){ESP_LOGE(TAG,"CANNOT find latest_block_partition!!\n");
        return;}
    //erase before write
    err = esp_partition_erase_range(latest_block_partition , 0 , latest_block_partition->size);
    if(err != ESP_OK) {ESP_LOGE(TAG, "Error: Erase partition failed! %d", err); return err;}

    ESP_LOGE(TAG,"Partition: latest_block is found. address: 0x%08x, size: 0x%08x \n",
             latest_block_partition->address, latest_block_partition->size);

    err = esp_partition_write(latest_block_partition, 0 , src , BLOCKSIZE );

    if(err == ESP_OK){ESP_LOGE(TAG,"esp_partition_write genesis_bolck to latest_block succeed!\n");}
    else{ESP_LOGE(TAG,"esp_partition_write genesis_bolck to latest_block failed! error = %d\n" , err);}

    return;
}

esp_err_t latest_block_read( void* dst, size_t size )
{ //read latest block from latest_block_partition
    const esp_partition_t *latest_block_partition = NULL;

    if(dst == NULL){ESP_LOGE(TAG,"ERROR: NULL pointer!\n");return ESP_ERR_INVALID_ARG ;}

    esp_err_t err;

    if(size != BLOCKSIZE){ESP_LOGE(TAG,"Invalid size!! size should be %d \n",BLOCKSIZE);return ESP_ERR_INVALID_SIZE;}

    latest_block_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA,
                                                      &LATEST_BLOCK_PARTITION_LABEL);
    assert(latest_block_partition != NULL);
    if(!latest_block_partition){
        ESP_LOGE(TAG,"CANNOT find latest_block_partition!!\n");return ESP_ERR_NOT_FOUND;
    }else{
        ESP_LOGE(TAG,"Partition: latest_block is found. address: 0x%08x, size: 0x%08x \n",
               latest_block_partition->address, latest_block_partition->size);

        err = esp_partition_read(latest_block_partition, 0 , dst , BLOCKSIZE );

        if(err == ESP_OK){ESP_LOGE(TAG,"esp_partition_read genesis_block from latest_block succeed!\n");}
        else{ESP_LOGE(TAG,"esp_partition_read genesis_block from latest_block failed! error = %d\n" , err); return err;}
    }
    return ESP_OK;
}

esp_err_t write_block_on_chain(uint8_t index , const void* src )
{
    if(src == NULL){ESP_LOGE(TAG,"ERROR: NULL pointer!\n");return ESP_ERR_INVALID_ARG;}

    const esp_partition_t *chain_partition = NULL;
    esp_err_t err;

    chain_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA,
                                                      &VED_CHAIN_PARTITION_LABEL);

    assert(chain_partition != NULL);
    if(!chain_partition){
        ESP_LOGE(TAG,"CANNOT find ved_chain_partition!!\n");
    }else{
        ESP_LOGI(TAG,"Partition: ved_chain is found. address: 0x%08x, size: 0x%08x \n",
                 chain_partition->address, chain_partition->size);

        err = esp_partition_write(chain_partition, index*BLOCKSIZE , src , BLOCKSIZE );

        if(err == ESP_OK){ESP_LOGI(TAG,"Write %d th block on chain succeed!\n", index);}
        else{ESP_LOGE(TAG,"Write %d th block on chain failed! error = %d\n" , index , err);
            return err;}
    }
    return ESP_OK;
}

esp_err_t read_block_on_chain(uint8_t index , void* dst, size_t size)
{
    const esp_partition_t *chain_partition = NULL;
    esp_err_t err;
    char block[BLOCKSIZE] = { 0 };

    if(dst == NULL){ESP_LOGE(TAG,"NULL pointer!");return ESP_ERR_INVALID_ARG;}

    if(size != BLOCKSIZE){ESP_LOGE(TAG,"Invalid size!! size should be %d \n",BLOCKSIZE);return ESP_ERR_INVALID_SIZE;}


    chain_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA,
                                               &UNV_CHAIN_PARTITION_LABEL);

    assert(chain_partition != NULL);
    if(!chain_partition){ESP_LOGE(TAG,"CANNOT find ved_chain_partition!!\n");return ESP_ERR_NOT_FOUND;}
    else
    {
        ESP_LOGE(TAG,"Partition: ved_chain is found. address: 0x%08x, size: 0x%08x \n",
                 chain_partition->address, chain_partition->size);

        err = esp_partition_read(chain_partition, index*BLOCKSIZE , block , BLOCKSIZE );

        if(err == ESP_OK){ESP_LOGE(TAG,"Write %d th block on chain succeed!\n", index);}
        else{ESP_LOGE(TAG,"Write %d th block on chain failed! error = %d\n" , index , err);return err;}
    }
    return ESP_OK;
}

esp_err_t verified_chain_copy(uint8_t num)
{
    const esp_partition_t *unv_chain_partition = NULL;
    const esp_partition_t *ved_chain_partition = NULL;
    esp_err_t err;
    char block[ BLOCKSIZE ] = { 0 };

    unv_chain_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA,
                                                   &UNV_CHAIN_PARTITION_LABEL);
    assert(unv_chain_partition != NULL);
    if(!unv_chain_partition){ESP_LOGE(TAG,"CANNOT find unv_chain_partition!!\n");}

    ved_chain_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA,
                                                   &VED_CHAIN_PARTITION_LABEL);
    assert(ved_chain_partition != NULL);
    if(!unv_chain_partition){ESP_LOGE(TAG,"CANNOT find unv_chain_partition!!\n");}

    //erase before write
    err = esp_partition_erase_range(ved_chain_partition , 0 , ved_chain_partition->size);
    if(err != ESP_OK) {ESP_LOGE(TAG, "Error: Erase partition failed! %d", err); return err;}

    //copy
    for(uint8_t i=0 ; i <= num ; i++)
    {
        err = esp_partition_read(unv_chain_partition, i*BLOCKSIZE , block , BLOCKSIZE );
        if(err != ESP_OK){ESP_LOGE(TAG,"Read %d th block on ved_chain failed! error = %d\n" , i , err);return err;}
        else
        {
            err = esp_partition_write(ved_chain_partition,i*BLOCKSIZE , block , BLOCKSIZE);
            if(err != ESP_OK){ESP_LOGE(TAG,"Write %d th block on ved_chain failed! error = %d\n" , i , err);return err;
            }else{ESP_LOGE(TAG,"Write %d th block on ved_chain succeed!\n",i);}
        }
    }
    ESP_LOGE(TAG,"Write whole blockchain to ved_chain_partition succeed!\n\n");
    return ESP_OK;
}

esp_err_t verified_data_copy(void)
{
    const esp_partition_t *unv_data_partition = NULL;
    const esp_partition_t *ved_data_partition = NULL;
    esp_err_t err;
    char buffer[DATA_COPY_BUFFER_SIZE] = { 0 };
    char data_header[DATA_HEADER_SIZE] = { 0 };
    //find partition
    unv_data_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA,
                                                  &UNV_DATA_PARTITION_LABEL);
    assert(unv_data_partition != NULL);
    if(!unv_data_partition){ESP_LOGE(TAG,"CANNOT find unv_data_partition!!\n");return ESP_ERR_NOT_FOUND;}
    //find partition
    ved_data_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA,
                                                  &VED_DATA_PARTITION_LABEL);
    assert(ved_data_partition != NULL);
    if(!ved_data_partition){ESP_LOGE(TAG,"CANNOT find ved_data_partition!!\n");return ESP_ERR_NOT_FOUND;}
    //read data header
    err = esp_partition_read(unv_data_partition, ved_data_partition->size - 1 - DATA_HEADER_SIZE , data_header , DATA_HEADER_SIZE);
    if(err != ESP_OK){ESP_LOGE(TAG,"CANNOT read data header from unv_data_partition!\n");return ESP_ERR_NOT_FOUND;}
    //get length
    cJSON *root = NULL;//refer to cJSON library**
    root= cJSON_Parse(data_header);//turn string(data_header) to cJSON object**
    int length = cJSON_GetObjectItem(root,"length")->valueint;//give value int of "led" to led**
    cJSON_Delete(root);//release the area of root**
    //check length
    if(length >= (ved_data_partition->size - 1 - DATA_HEADER_SIZE))
    {ESP_LOGE(TAG,"ved_data_partition is too small to copy the bin data file!!\n");return ESP_ERR_INVALID_SIZE;}
    //erase before write
    err = esp_partition_erase_range(ved_data_partition , 0 , ved_data_partition->size);
    if(err != ESP_OK) {ESP_LOGE(TAG, "Error: Erase partition failed! %d", err); return err;}
    //COPY
    for( uint8_t i=0 ; i <= length ; i += DATA_COPY_BUFFER_SIZE )
    {
        err = esp_partition_read(unv_data_partition, i , buffer , DATA_COPY_BUFFER_SIZE);
        if(err != ESP_OK){ESP_LOGE(TAG,"Read from unv_data_partition failed!!\n");return err;}
        else
        {
            err = esp_partition_write(ved_data_partition, i , buffer , DATA_COPY_BUFFER_SIZE);
            if(err != ESP_OK){ESP_LOGE(TAG,"Data write failed at area [%d, %d]. err = %d \n", i, i+DATA_COPY_BUFFER_SIZE , err);return err;}
            else{ESP_LOGE(TAG,"Have transferred data length %d \n",i+DATA_COPY_BUFFER_SIZE);}
        }
    }
    //copy data header
    err = esp_partition_write(ved_data_partition, ved_data_partition->size - 1 - DATA_HEADER_SIZE , data_header , DATA_HEADER_SIZE);
    if(err != ESP_OK){ESP_LOGE(TAG,"Write data_header failed! err= %d \n",err); return err;}

    ESP_LOGE(TAG,"COPY verified data to ved_data_partition finished!\n");
    return ESP_OK;
}

esp_err_t get_data_header_of_unv_data(void *dst)
{
    const esp_partition_t *unv_data_partition = NULL;
    esp_err_t err;

    if(dst == NULL){ESP_LOGE(TAG,"NULL pointer!");return ESP_ERR_INVALID_ARG;}

    unv_data_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA,
                                                  &UNV_DATA_PARTITION_LABEL);
    assert(unv_data_partition != NULL);
    if(!unv_data_partition){ESP_LOGE(TAG,"CANNOT find unv_data_partition!!\n");return ESP_ERR_NOT_FOUND;}

    err = esp_partition_read(unv_data_partition, unv_data_partition->size - 1 - DATA_HEADER_SIZE , dst , DATA_HEADER_SIZE);
    if(err != ESP_OK){ESP_LOGE(TAG,"CANNOT read data header from unv_data_partition!\n");return ESP_ERR_NOT_FOUND;}

    return ESP_OK;

}


