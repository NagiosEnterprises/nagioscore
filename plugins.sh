#!/bin/bash
docker exec -w /var/www/html/plugins -it nvim ./tools/setup
docker exec -w /var/www/html/plugins -it nvim ./configure
docker exec -w /var/www/html/plugins -it nvim make 
docker exec -w /var/www/html/plugins -it nvim make install

