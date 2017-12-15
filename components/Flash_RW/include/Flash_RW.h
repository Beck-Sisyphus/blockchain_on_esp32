#ifndef __FLASH_RW_H
#define __FLASH_RW_H


//////////////////////////////////////////////////////////////////////////////////	 
//created time: 2017/12/16
//creater: chongzi
////////////////////////////////////////////////////////////////////////////////// 	

#define BLOCKSIZE 200

#define LATEST_BLOCK_PARTITION_LABEL "latest_block"
#define VED_CHAIN_PARTITION_LABEL "ved_chain"
#define UNV_CHAIN_PARTITION_LABEL "nuv_chain"
#define VED_BIN_DATA_PARTITION_LABEL "ved_data"
#define UNV_BIN_DATA_PARTITION_LABEL "unv_data"


void latest_block_write( const void* src );

char *latest_block_read( void );

void write_block_on_chain(uint8_t index , const void* src );




#endif





















