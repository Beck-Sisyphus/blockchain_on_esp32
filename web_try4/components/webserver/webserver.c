/* Standard includes. */
#include "string.h"
#include "esp_err.h"
/* lwIP core includes */
#include "lwip/opt.h"
#include "lwip/sockets.h"
#include <netinet/in.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"
/* Utils includes. */
#include "esp_log.h"
#include "event.h"
#include "http.h"
#include "webserver.h"
#include "cJSON.h"
#define TAG "webserver:"

#include "../../main/blockchain_http_request.h"
#include "../../main/Flash_RW.h"

#define GPIO_OUTPUT_IO_0    16
#define GPIO_OUTPUT_PIN_SEL  ((1<<GPIO_OUTPUT_IO_0))
#define BUFFSIZE 1024

extern char latest_block[BLOCKSIZE+1];
extern char some_block[BLOCKSIZE+1];
extern char new_block[BLOCKSIZE+1];
extern char data_header[DATA_HEADER_SIZE+1];

static uint32_t http_url_length=0;
static uint32_t http_body_length=0;

static char* http_body=NULL;
static char* http_url=NULL;

static int32_t socket_fd, client_fd;
#define RES_HEAD "HTTP/1.1 200 OK\r\nContent-type: %s\r\nTransfer-Encoding: chunked\r\n\r\n"
#define RES_HEAD2 "HTTP/1.1 200 OK\r\nContent-type: %s\r\nContent-Length:%d\r\n\r\n"
static char chunk_len[15];


const static char not_find_page[] = "<!DOCTYPE html>"
      "<html>\n"
      "<head>\n"
      "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
      "	 <link href=\"https://cdn.bootcss.com/bootstrap/4.0.0-alpha.6/css/bootstrap.min.css\" rel=\"stylesheet\">\n"
      "  <script src=\"https://cdn.bootcss.com/bootstrap/4.0.0-alpha.6/js/bootstrap.min.js\"></script>\n"
      "<title>WhyEngineer-ESP32</title>\n"
      "</head>\n"
      "<body>\n"
      "<h1 class=\"container\">Page not Found!</h1>\n"
      "</body>\n"
      "</html>\n";

static int header_value_callback(http_parser* a, const char *at, size_t length){
	// for(int i=0;i<length;i++)
	// 	putchar(at[i]);
	return 0;
}
static int body_callback(http_parser* a, const char *at, size_t length){
    // static uint32_t cnt=0;
    // printf("cnt:%d\n",cnt++);
    //spiRamFifoWrite(at,length);
    http_body=realloc(http_body,http_body_length+length);
    memcpy(http_body+http_body_length,at,length);
    http_body_length+=length;
  //   for(int i=0;i<length;i++)
		// putchar(at[i]);
    return 0;
}
static int url_callback(http_parser* a, const char *at, size_t length){
    // static uint32_t cnt=0;
    // printf("cnt:%d\n",cnt++);
    //spiRamFifoWrite(at,length);
    http_url=realloc(http_url,http_url_length+length);
    memcpy(http_url+http_url_length,at,length);
    http_url_length+=length;
  //   for(int i=0;i<length;i++)
		// putchar(at[i]);
    return 0;
}
static void chunk_end(int socket){
	write(socket, "0\r\n\r\n", 7);
}

typedef struct
{
  char* url;
  void(*handle)(http_parser* a,char*url,char* body);
}HttpHandleTypeDef;

void web_index(http_parser* a,char*url,char* body);
void led_ctrl(http_parser* a,char*url,char* body);
void load_logo(http_parser* a,char*url,char* body);
void load_esp32(http_parser* a,char*url,char* body);
void web_index(http_parser* a,char*url,char* body);
void http_response_new_block(http_parser* a,char*url,char* body);
void http_response_whole_chain(http_parser* a,char*url,char* body);
void http_response_data_file(http_parser* a,char*url,char* body);

static void not_find();
const HttpHandleTypeDef http_handle[]={
	{"/", http_response_data_file},
	{"/api/led/",http_response_whole_chain},
	{"/static/logo.png",not_find},
	{"/static/esp32.png",not_find},
};

static void return_bin_file( void )
{
    uint32_t total_len = 0;
    uint32_t read_len = 0;
    char* read_buf = malloc(BUFFSIZE+1);
    char* data_header = malloc(DATA_HEADER_SIZE+1);
    const esp_partition_t *ved_data_partition = NULL;

    //get the total_len of bin file
    esp_err_t err = get_data_header_of_ved_data(  data_header  );
    if(err != ESP_OK){ESP_LOGE(TAG,"Get data header failed! error type: %d",err);}

    cJSON *root = NULL;//refer to cJSON library**
    root= cJSON_Parse(data_header);//turn string(data_header) to cJSON object**
    total_len = cJSON_GetObjectItem(root,"length")->valueint;
    cJSON_Delete(root);//release the area of root**
    free(data_header);

    //get ved_data_partition
    ved_data_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA,
                                                  &VED_DATA_PARTITION_LABEL);
    assert(ved_data_partition != NULL);
    if(!ved_data_partition){ESP_LOGE(TAG,"CANNOT find ved_data_partition!!\n");return ESP_ERR_NOT_FOUND;}

    while(1)
    {
        err = esp_partition_read(ved_data_partition, read_len , read_buf , BUFFSIZE);
        if(err != ESP_OK){ESP_LOGI(TAG,"patition read error. error type: %d",err);}

        if(read_len < total_len)
        {
			if(total_len-read_len >= BUFFSIZE)
			{

				//sprintf(chunk_len , "%x\r\n" , BUFFSIZE);
				//write(client_fd , chunk_len , strlen(chunk_len));
				write(client_fd , read_buf , BUFFSIZE);
				//write(client_fd , "\r\n" , 2);

				read_len += BUFFSIZE;
				ESP_LOGI(TAG,"total_len == %d ,read_len = %d",total_len , read_len);
			}
			else
			{

				//sprintf(chunk_len , "%x\r\n" , total_len - read_len);
				//write(client_fd , chunk_len , strlen(chunk_len));
				write(client_fd , read_buf , total_len - read_len);
				//write(client_fd , "\r\n" , 2);

				read_len = total_len;
				ESP_LOGI(TAG,"total_len == %d ,read_len = %d",total_len , read_len);
			}

        }
        else break;
    }

    //chunk_end(client_fd);
    free(read_buf);
}


static void return_new_block( void )
{

	uint32_t i=0;
	char* read_buf = malloc(BLOCKSIZE+1);

	while(1)
	{
		if(i < 1)
		{
			esp_err_t err = latest_block_read( read_buf , BLOCKSIZE);
			if(err != ESP_OK){ESP_LOGE(TAG,"Read block %d on chain failed. error type:%d", i , err);}
			else
			{

				//sprintf(chunk_len,"%x\r\n",BLOCKSIZE);
				//write(client_fd, chunk_len, strlen(chunk_len));
				write(client_fd, read_buf, BLOCKSIZE);
				//write(client_fd, "\r\n", 2);

				i++;
			}
		}
		else break;
	}
	//chunk_end(client_fd);
	free(read_buf);
}

static void return_whole_chain( void )
{
	uint32_t i=0;
	char *read_buf = malloc(BLOCKSIZE);
	char *request = malloc(BLOCKSIZE);

	cJSON *root = NULL;//refer to cJSON library**
	root= cJSON_Parse(latest_block);//turn string(data_header) to cJSON object**
	uint32_t la_index = cJSON_GetObjectItem(root,"index")->valueint;//give value int of "led" to led**
	cJSON_Delete(root);//release the area of root**

	while(1)
	{
		if(i <= la_index)
		{
			esp_err_t err = read_block_on_ved_chain(i , read_buf , BLOCKSIZE);
			if(err != ESP_OK){ESP_LOGE(TAG,"Read block %d on chain failed. error type:%d", i , err);}
			else
			{
				ESP_LOGI(TAG,"Read block %d on chain succeed!. content: %s", i , read_buf);

				//sprintf(chunk_len,"%x\r\n",BLOCKSIZE);
				//write(client_fd, chunk_len, strlen(chunk_len));
				write(client_fd, read_buf, BLOCKSIZE);
				//write(client_fd, "\r\n", 2);
				ESP_LOGI(TAG,"string length of request: %d ", strlen(chunk_len));

				i++;
			}
		}
		else break;
	}
	//chunk_end(client_fd);
	free(read_buf);
	free(request);
}


void http_response_new_block(http_parser* a,char*url,char* body)
{
    char *request;
    asprintf(&request,RES_HEAD2,"text/plain",BLOCKSIZE);//html
    write(client_fd, request, strlen(request));
    free(request);
    return_new_block();

	ESP_LOGI(TAG,"write latest_block : %s", latest_block);
}

void http_response_whole_chain(http_parser* a,char*url,char* body)
{
    char *request;

    //get the index of latest_block -> la_index
	cJSON *root = NULL;//refer to cJSON library**
	root= cJSON_Parse(latest_block);//turn string(data_header) to cJSON object**
	uint32_t la_index = cJSON_GetObjectItem(root,"index")->valueint;//give value int of "led" to led**
	cJSON_Delete(root);//release the area of root**

    //send header
	asprintf(&request,RES_HEAD2,"text/plain", (la_index+1)*BLOCKSIZE);
    write(client_fd, request, strlen(request));
	ESP_LOGI(TAG,"string length of request: %d ", strlen(request));
    free(request);

	//send body part
    return_whole_chain();
}

void http_response_data_file(http_parser* a,char*url,char* body)
{
    char *request;
	char* data_header = malloc(DATA_HEADER_SIZE+1);

	//get the total_len of bin file
	esp_err_t err = get_data_header_of_ved_data(  data_header  );
	if(err != ESP_OK){ESP_LOGE(TAG,"Get data header failed! error type: %d",err);}
	cJSON *root = NULL;//refer to cJSON library**
	root= cJSON_Parse(data_header);//turn string(data_header) to cJSON object**
	uint32_t total_len = cJSON_GetObjectItem(root,"length")->valueint;
	cJSON_Delete(root);//release the area of root**
	free(data_header);
	ESP_LOGI(TAG,"The length of data file is: %d", total_len);

	//send header
    asprintf(&request,RES_HEAD2,"text/plain",total_len);//html
    write(client_fd, request, strlen(request));
    free(request);

	//send body part
    return_bin_file();
}


static void return_file(char* filename){
	uint32_t r;
	char* read_buf=malloc(1024);
  	FILE* f = fopen(filename, "r");
  	if(f==NULL){
  		ESP_LOGE(TAG,"not find the file");
  		return;
  	}
  	while(1){
    	r=fread(read_buf,1,1024,f);
    	if(r>0){
    		sprintf(chunk_len,"%x\r\n",r);
    		write(client_fd, chunk_len, strlen(chunk_len));
	    	//printf("%s",dst_buf);
	    	write(client_fd, read_buf, r);
	    	write(client_fd, "\r\n", 2);
    	}else
    		break;
    }
    fclose(f);
  	chunk_end(client_fd);
}
static void not_find(){
	char *request;
  	asprintf(&request,RES_HEAD,"text/html");//html
  	write(client_fd, request, strlen(request));
  	free(request);
  	sprintf(chunk_len,"%x\r\n",strlen(not_find_page));
  	write(client_fd, chunk_len, strlen(chunk_len));
  	write(client_fd, not_find_page, strlen(not_find_page));
  	write(client_fd,"\r\n",2);
  	chunk_end(client_fd);
}
void load_logo(http_parser* a,char*url,char* body){
	char *request;
  	asprintf(&request,RES_HEAD,"image/png");//html
  	write(client_fd, request, strlen(request));
  	free(request);
  	return_file("/sdcard/www/static/logo.png");
}
void load_esp32(http_parser* a,char*url,char* body){
	char *request;
  	asprintf(&request,RES_HEAD,"image/png");//html
  	write(client_fd, request, strlen(request));
  	free(request);
  	return_file("/sdcard/www/static/esp32.png");
}
void web_index(http_parser* a,char*url,char* body){
	char *request;
  	asprintf(&request,RES_HEAD,"text/html");//html
  	write(client_fd, request, strlen(request));
  	free(request);
  	return_file("/sdcard/www/index.html");
}

void led_ctrl(http_parser* a,char*url,char* body){
	char *request;
  	asprintf(&request,RES_HEAD,"application/json");//json
  	write(client_fd, request, strlen(request));
  	free(request);
  	cJSON *root=NULL;
    root= cJSON_Parse(http_body);
    uint8_t led=cJSON_GetObjectItem(root,"led")->valueint;
    cJSON_Delete(root);
    gpio_set_level(GPIO_OUTPUT_IO_0,led);
  	root=NULL;
	root=cJSON_CreateObject();
	if(root==NULL){
		ESP_LOGI(TAG,"cjson root create failed\n");
		return NULL;
	}
	cJSON_AddNumberToObject(root,"err",0);
	// cJSON_AddStringToObject(root,"cuid","esp32_whyengineer");
	// cJSON_AddStringToObject(root,"token",access_token);
	// cJSON_AddNumberToObject(root, "rate", 8000);
	// cJSON_AddNumberToObject(root, "channel", 1);
	// cJSON_AddNumberToObject(root, "len", len);
	// cJSON_AddStringToObject(root,"speech",speech);
	char* out = cJSON_PrintUnformatted(root);
	sprintf(chunk_len,"%x\r\n",strlen(out));
	write(client_fd, chunk_len, strlen(chunk_len));
	write(client_fd, out, strlen(out));
  	write(client_fd,"\r\n",2);
  	chunk_end(client_fd);
	//send(client,out,strlen(out),MSG_WAITALL);
	//printf("handle_return: %s\n", out);
	cJSON_Delete(root);
}

static int body_done_callback (http_parser* a){
	http_body=realloc(http_body,http_body_length+1);
    http_body[http_body_length]='\0';
  	ESP_LOGI(TAG,"body:%s",http_body);
  	for(int i=0;i<sizeof(http_handle)/sizeof(http_handle[0]);i++){
  		if(strcmp(http_handle[i].url,http_url)==0){
  			http_handle[i].handle(a,http_url,http_body);
  			return 0;
  		}
  	}
  	not_find();
  	// char *request;
  	// asprintf(&request,RES_HEAD,HTML);
  	// write(client_fd, request, strlen(request));
   //  free(request);
   //  sprintf(chunk_len,"%x\r\n",strlen(not_find_page));
   //  //ESP_LOGI(TAG,"chunk_len:%s,length:%d",chunk_len,strlen(http_index_hml));
   //  write(client_fd, chunk_len, strlen(chunk_len));
   //  write(client_fd, http_index_hml, strlen(http_index_hml));
   //  write(client_fd, "\r\n", 2);
   //  chunk_end(client_fd);
    //close( client_fd );	
    return 0;
}
static int begin_callback (http_parser* a){
  	//ESP_LOGI(TAG,"message begin");
    return 0;
}
static int header_complete_callback(http_parser* a){
	http_url=realloc(http_url,http_url_length+1);
    http_url[http_url_length]='\0';
	ESP_LOGI(TAG,"url:%s",http_url);
    return 0;
}

static http_parser_settings settings =
{   .on_message_begin = begin_callback
    ,.on_header_field = 0
    ,.on_header_value = header_value_callback
    ,.on_url = url_callback
    ,.on_status = 0
    ,.on_body = body_callback
    ,.on_headers_complete = header_complete_callback
    ,.on_message_complete = body_done_callback
    ,.on_chunk_header = 0
    ,.on_chunk_complete = 0
};

/* Dimensions the buffer into which input characters are placed. */

static struct sockaddr_in server, client;


int creat_socket_server(in_port_t in_port, in_addr_t in_addr)
{
	int socket_fd, on;
	//struct timeval timeout = {10,0};

	server.sin_family = AF_INET;
	server.sin_port = in_port;
	server.sin_addr.s_addr = in_addr;

	if((socket_fd = socket(AF_INET, SOCK_STREAM, 0))<0) {
		perror("listen socket uninit\n");
		return -1;
	}
	on=1;
	//setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
	setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int) );
	//CALIB_DEBUG("on %x\n", on);
	if((bind(socket_fd, (struct sockaddr *)&server, sizeof(server)))<0) {
		perror("cannot bind srv socket\n");
		return -1;
	}

	if(listen(socket_fd, 1)<0) {
		perror("cannot listen");
		close(socket_fd);
		return -1;
	}

	return socket_fd;
}



void webserver_task( void *pvParameters ){
	int32_t lBytes;
	esp_err_t err;
	ESP_LOGI(TAG,"webserver start");
	uint32_t request_cnt=0;
	//init gpio
	gpio_config_t io_conf;
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
	//init sd card
	sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 10
    };
    sdmmc_card_t* card;
    err = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);
    if (err != ESP_OK) {
        if (err == ESP_FAIL) {
            printf("Failed to mount filesystem. If you want the card to be formatted, set format_if_mount_failed = true.");
        } else {
            printf("Failed to initialize the card (%d). Make sure SD card lines have pull-up resistors in place.", err);
        }
        return;
    }
    sdmmc_card_print_info(stdout, card);

    //check things in flash
    latest_block_read( latest_block , BLOCKSIZE );//get the latest block
    ESP_LOGI(TAG, "The latest_block is: %s",latest_block);

    read_block_on_unv_chain(1 , &some_block , BLOCKSIZE);
    ESP_LOGI(TAG, "The block with index 1 is: %s",some_block+1);

    read_block_on_unv_chain(0 , &some_block , BLOCKSIZE);
    ESP_LOGI(TAG, "The block with index 0 is: %s",some_block+1);

    read_block_on_ved_chain(1 , &some_block , BLOCKSIZE);
    ESP_LOGI(TAG, "The block with index 1 is: %s",some_block+1);

    read_block_on_ved_chain(0 , &some_block , BLOCKSIZE);
    ESP_LOGI(TAG, "The block with index 0 is: %s",some_block+1);

    get_data_header_of_ved_data(&data_header);
    ESP_LOGI(TAG, "Get Data Header!!!! %s \n",data_header);

    //server
    (void) pvParameters;
	http_parser parser;
    http_parser_init(&parser, HTTP_REQUEST);
    parser.data = NULL;
    socklen_t client_size=sizeof(client);

	socket_fd = creat_socket_server(htons(80),htonl(INADDR_ANY));
	if( socket_fd >= 0 ){
		/* Obtain the address of the output buffer.  Note there is no mutual
		exclusion on this buffer as it is assumed only one command console
		interface will be used at any one time. */
		char recv_buf[64];
		esp_err_t nparsed = 0;
		/* Ensure the input string starts clear. */
		for(;;){
			client_fd=accept(socket_fd,(struct sockaddr*)&client,&client_size);
			if(client_fd>0L){
				// strcat(outbuf,pcWelcomeMessage);
				// strcat(outbuf,path);
				// lwip_send( lClientFd, outbuf, strlen(outbuf), 0 );
				do{
					lBytes = recv( client_fd, recv_buf, sizeof( recv_buf ), 0 );
					if(lBytes==0){
						close( client_fd );	
						break;
					}
						
					 // for(int i=0;i<lBytes;i++)
						// putchar(recv_buf[i]);
					nparsed = http_parser_execute(&parser, &settings, recv_buf, lBytes);
					
					//while(xReturned!=pdFALSE);
					//lwip_send( lClientFd,path,strlen(path), 0 );
				}while(lBytes > 0 && nparsed >= 0);
				free(http_url);
				free(http_body);
				http_url=NULL;
				http_body=NULL;
				http_url_length=0;
				http_body_length=0;		
			}
			ESP_LOGI(TAG,"request_cnt:%d,socket:%d",request_cnt++,client_fd);
			close( client_fd );
		}
	}

}