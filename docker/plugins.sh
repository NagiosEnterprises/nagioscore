#!/bin/bash
docker exec -w /var/www/html/plugins -it ncen ./tools/setup
docker exec -w /var/www/html/plugins -it ncen ./configure
docker exec -w /var/www/html/plugins -it ncen make
docker exec -w /var/www/html/plugins -it ncen make install

