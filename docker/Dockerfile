FROM iwhite5/nagioscore-base-centos7 
LABEL author="Isaac A. White <iwhite@nagios.com>"

WORKDIR /var/www/html
RUN ./configure
RUN make all
RUN make install-groups-users
RUN usermod -a -G nagios apache
RUN make install
RUN make install-daemoninit
RUN systemctl enable httpd.service
RUN make install-commandmode
RUN make install-config
RUN make install-webconf

RUN htpasswd -bc /usr/local/nagios/etc/htpasswd.users nagiosadmin admin
RUN echo "ServerName localhost" >> /etc/httpd/conf/httpd.conf

RUN /usr/sbin/httpd -k start
RUN /etc/init.d/nagios start

RUN ln -s /var/www/vim/.vim/ /root/.vim
RUN ln -s /var/www/vim/.vimrc /root/.vimrc

CMD [ "/bin/bash" , "-c", "vim +PluginInstall +qall" ]
# Make sure plugins directory is available (Commented for troubleshooting with search)
WORKDIR /var/www/html
RUN git config --global core.editor "/usr/bin/vim"
RUN git config --global user.email "iwhite@nagios.com"
RUN git config --global user.name  "Isaac A. White"

#WORKDIR /var/www/plugins/
#RUN git config --global core.editor "/usr/bin/vim"
#RUN git config --global user.email "iwhite@nagios.com"
#RUN git config --global user.name  "Isaac A. White"
#RUN ./tools/setup
#RUN ./configure
#RUN make
#RUN make install

#WORKDIR /var/www/html

EXPOSE 80 8080 3306 5555 22 2222 5693 5666
RUN yum update -y && yum install -y supervisor
RUN mkdir -p /etc/nagios
COPY ./docker/supervisord.conf /etc/nagios/supervisord.conf
CMD /usr/bin/supervisord -c /etc/nagios/supervisord.conf
