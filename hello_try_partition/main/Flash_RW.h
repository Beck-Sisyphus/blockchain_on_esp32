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
#define VED_DATA_PARTITION_LABEL "ved_data"
#define UNV_DATA_PARTITION_LABEL "unv_data"


void latest_block_write( const void* src );//write latest block to latest block partition

esp_err_t latest_block_read( void* dst, size_t size );//read latest block from latest block partition to dst

void write_block_on_chain(uint8_t index , const void* src );//write the block use index in ved_chain_partition (please use it only for latest block)

esp_err_t read_block_on_chain(uint8_t index , void* dst, size_t size);//read the block use index in unv_chain_partition to dst

/*copy verified chain in unv_chain_partition to ved_chain_partition*/
esp_err_t verified_chain_copy(uint8_t num);//num is the maximum index of the chain that should be copied

/*copy verified data in nuv_data_partition to ved_data_partition*/
esp_err_t verified_data_copy(void);


#endif





















