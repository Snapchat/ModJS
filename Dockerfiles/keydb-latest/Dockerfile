# Use the latest KeyDB Docker Image and add ModJS layer
FROM eqalpha/keydb:latest
RUN mkdir -p /usr/lib/keydb/modules
# Copy over precompiled ubuntu18.04 module along with latest bootstrap.js
COPY modjs.so /usr/lib/keydb/modules
COPY bootstrap.js /usr/lib/keydb/modules
VOLUME /scripts
# Call module to load with keydb-server
CMD ["keydb-server", "/etc/keydb/keydb.conf", "--loadmodule", "/usr/lib/keydb/modules/modjs.so"]
