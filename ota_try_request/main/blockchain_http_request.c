/* OTA example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
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

#include "blockchain_http_request.h"
#include "Flash_RW.h"



#define BUFFSIZE 1024
#define TEXT_BUFFSIZE 1024

static const char *TAG = "HTTP REQUEST";
/*an data write buffer ready to write to the flash*/
static char write_data[BUFFSIZE + 1] = { 0 };
/*an packet receive buffer*/
static char text[BUFFSIZE + 1] = { 0 };
/* an image total length*/
static int binary_file_length = 0;
/*socket id*/
extern int socket_id;



/*read buffer by byte still delim ,return read bytes counts*/
static int read_until(char *buffer, char delim, int len)
{
//  /*TODO: delim check,buffer check,further: do an buffer length limited*/
    int i = 0;
    while (buffer[i] != delim && i < len) {
        ++i;
    }
    return i + 1;
}

/* resolve a packet from http socket
 * return true if packet including \r\n\r\n that means http packet header finished,start to receive packet body
 * otherwise return false
 * */
static bool read_past_http_header(char text[], int total_len, esp_partition_t *unv_data_partition)
{
    /* i means current position */
    esp_err_t err;
    int i = 0, i_read_len = 0;
    while (text[i] != 0 && i < total_len) {
        i_read_len = read_until(&text[i], '\n', total_len);
        // if we resolve \r\n line,we think packet header is finished
        if (i_read_len == 2) {
            int i_write_len = total_len - (i + 2);
            memset(write_data, 0, BUFFSIZE);
            /*copy first http packet body to write buffer*/
            memcpy(write_data, &(text[i + 2]), i_write_len);
            //erase before write
            err = esp_partition_erase_range(unv_data_partition , 0 , unv_data_partition->size);
            if(err != ESP_OK) {ESP_LOGE(TAG, "Error: Erase partition failed! %d", err); return false;}
            //write to partition
            err = esp_partition_write( unv_data_partition, 0 , (const void *)write_data, i_write_len);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Error: write failed! err=0x%x", err);
                return false;
            } else {
                ESP_LOGI(TAG, "write header OK");
                binary_file_length += i_write_len;
            }
            return true;
        }
        i += i_read_len;
    }
    return false;
}


static void __attribute__((noreturn)) task_fatal_error()
{
    ESP_LOGE(TAG, "Exiting task due to fatal error...");
    close(socket_id);
    (void)vTaskDelete(NULL);

    while (1) {
        ;
    }
}


esp_err_t http_request_bin_data(void)
{
    esp_err_t err;
    const esp_partition_t *unv_data_partition = NULL;

    unv_data_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA,
                                                  &UNV_DATA_PARTITION_LABEL);
    assert(unv_data_partition != NULL);
    if(!unv_data_partition){ESP_LOGE(TAG,"CANNOT find nuv_data_partition!!\n");return ESP_ERR_NOT_FOUND;}

    ESP_LOGI(TAG, "Starting http request of bin data\n");


    /*send GET request to http server*/
    const char *GET_FORMAT =
        "GET %s HTTP/1.0\r\n"
        "Host: %s:%s\r\n"
        "User-Agent: esp-idf/1.0 esp32_block_chain_node\r\n\r\n";

    char *http_request = NULL;
    int get_len = asprintf(&http_request, GET_FORMAT, EXAMPLE_FILENAME, EXAMPLE_SERVER_IP, EXAMPLE_SERVER_PORT);
    if (get_len < 0) {
        ESP_LOGE(TAG, "Failed to allocate memory for GET request buffer");
        task_fatal_error();
    }
    int res = send(socket_id, http_request, get_len, 0);
    free(http_request);

    if (res < 0) {
        ESP_LOGE(TAG, "Send GET request to server failed");
        task_fatal_error();
    } else {
        ESP_LOGI(TAG, "Send GET request to server succeeded");
    }


    bool resp_body_start = false, flag = true;
    /*deal with all receive packet*/
    while (flag) {
        memset(text, 0, TEXT_BUFFSIZE);
        memset(write_data, 0, BUFFSIZE);
        int buff_len = recv(socket_id, text, TEXT_BUFFSIZE, 0);
        if (buff_len < 0) { /*receive error*/
            ESP_LOGE(TAG, "Error: receive data error! errno=%d", errno);
            task_fatal_error();
        } else if (buff_len > 0 && !resp_body_start)
        { /*deal with response header*/
            memcpy(write_data, text, buff_len);
            resp_body_start = read_past_http_header(text, buff_len, unv_data_partition);
        } else if (buff_len > 0 && resp_body_start)
        { /*deal with response body*/
            memcpy(write_data, text, buff_len);
            err = esp_partition_write( unv_data_partition, binary_file_length , (const void *)write_data, buff_len);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Error: write failed! err=0x%x", err);
                task_fatal_error();
            }
            binary_file_length += buff_len;
            ESP_LOGI(TAG, "Have written binary length %d", binary_file_length);
        } else if (buff_len == 0) {  /*packet over*/
            flag = false;
            ESP_LOGI(TAG, "Connection closed, all packets received");
            close(socket_id);
        } else {
            ESP_LOGE(TAG, "Unexpected recv result");
        }
    }

    cJSON *root=NULL;
    root=cJSON_CreateObject();
    if(root==NULL){ESP_LOGI(TAG,"cjson root create failed\n");return NULL;}
    cJSON_AddNumberToObject(root,"length",binary_file_length);
    char* out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    err = esp_partition_write(unv_data_partition, unv_data_partition->size - 1 - DATA_HEADER_SIZE , out , DATA_HEADER_SIZE);
    if(err != ESP_OK){ESP_LOGE(TAG,"Write data_header failed! err= %d \n",err); return err;}

    ESP_LOGI(TAG, "Writing succeed! Total written binary data length : %d", binary_file_length);
    return ;
}
