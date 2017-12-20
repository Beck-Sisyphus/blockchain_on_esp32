# blockchain_on_esp32
develop an minimum implementation of a blockchain on esp32

## Dependancies
1. ESP-idf
https://github.com/espressif/esp-idf

## Communication test

### board A
make menuconfig

Partition Table->Custom partition table CSV
partitions_two_ota_blockchain.csv

1. cd ota_try_request; make flash monitor; locally launch "python -m SimpleHTTPServer 8070". Give the binary file and blocks to the ESP32.
2. next, cd web_try; make flash monitor; 

### board B

make menuconfig

Partition Table->Custom partition table CSV
partitions_two_ota_blockchain.csv

1. cd ota_try_request; make flash monitor; locally launch "python -m SimpleHTTPServer 8070". Give the binary file and blocks to the ESP32.
