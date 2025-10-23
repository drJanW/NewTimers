1. add (get-ready-for-ota)trigger to web interface
2. set timer as "wait for 5 minutes for for some form of confirmation", e.g. by serial
confirmation for ota written in flash memory
on restart: read ota message in flash
3n: no conformation: restart program via normal setup
3y: confirmation: restart program via ota setup

ota setup: minimal setup, wait for 5 minutes for succesful  OTA download 

4y: OTA succesful: remove OTA confirmation in flash and restart
4n: restart  (risk of endless loop??)