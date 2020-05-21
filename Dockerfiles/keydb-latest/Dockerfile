# Use the latest KeyDB Docker Image
FROM eqalpha/keydb:latest
# Create module directory and install required additional dependencies for it
RUN mkdir -p /usr/lib/keydb/modules
# Copy over precompiled buster module
COPY modjs.so /usr/lib/keydb/modules
COPY bootstrap.js /usr/lib/keydb/modules
VOLUME /scripts
# Call module to load with redis-server
CMD ["keydb-server", "/etc/keydb/keydb.conf", "--loadmodule", "/usr/lib/keydb/modules/modjs.so"]
