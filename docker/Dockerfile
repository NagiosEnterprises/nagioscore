FROM cen
LABEL author="Isaac A. White <iwhite@nagios.com>"


# WORKDIR /var/www/html/plugins
# RUN ./tools/setup && ./configure

# Make sure the timestamps are not skewed
# RUN find . -type f -exec touch {} +
# RUN make && make install

WORKDIR /var/www/html
CMD /usr/bin/supervisord -c /var/www/html/supervisord.conf

