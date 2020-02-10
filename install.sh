#!/bin/bash
docker exec -w /var/www/html/ -it nvim ./configure
docker exec -w /var/www/html/ -it nvim make all install
./plugins.sh


