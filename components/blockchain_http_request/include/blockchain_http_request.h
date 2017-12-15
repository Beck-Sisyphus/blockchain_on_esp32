#ifndef __BLOCKCHAIN_HTTP_REQUEST_H
#define __BLOCKCHAIN_HTTP_REQUEST_H


//////////////////////////////////////////////////////////////////////////////////	 
//created time: 2017/12/16
//creater: chongzi
////////////////////////////////////////////////////////////////////////////////// 	

/**
 *request the new block from its neighbor and return the block in cJSON out string format*/
char *http_request_new_block( void );

/**
 *request the whole chain and directly store the whole chain to unv_chain_partition*/
void http_request_whole_chain( void );

/**
 * request the bin file and directly store the file to the unv_data_partition */
void http_request_bin_data( void );

#endif





















