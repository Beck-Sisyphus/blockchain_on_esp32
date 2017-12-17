#ifndef __BLOCKCHAIN_HTTP_REQUEST_H
#define __BLOCKCHAIN_HTTP_REQUEST_H


//////////////////////////////////////////////////////////////////////////////////	 
//created time: 2017/12/16
//creater: chongzi
////////////////////////////////////////////////////////////////////////////////// 	
#define EXAMPLE_SERVER_IP   CONFIG_SERVER_IP
#define EXAMPLE_SERVER_PORT CONFIG_SERVER_PORT
#define BIN_DATA_FILENAME CONFIG_EXAMPLE_FILENAME
#define CHAIN_FILENAME "/chain.c"
#define LATEST_BLOCK_FILENAME "/latest_block.c"

//bool connect_to_http_server( void );

/**
 *request the new block from its neighbor and return the block in cJSON out string format*/
esp_err_t http_request_new_block( void *dst );

/**
 *request the whole chain and directly store the whole chain to unv_chain_partition*/
esp_err_t http_request_whole_chain( void );

/**
 * request the bin file and directly store the file to the unv_data_partition */
esp_err_t http_request_bin_data(  void  ); // will close this socket_id

#endif





















