#!/bin/bash
docker exec -w /var/www/html/ -it ncen ./configure
docker exec -w /var/www/html/ -it ncen make all install
./plugins.sh


