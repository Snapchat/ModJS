# Use the latest Redis Docker Image (Buster)
FROM redis:buster
# Create module directory and install required additional dependencies for it
RUN mkdir -p /usr/lib/keydb/modules
# Copy over precompiled buster module
COPY modjs.so /usr/lib/keydb/modules
COPY bootstrap.js /usr/lib/keydb/modules
VOLUME /scripts
# Call module to load with redis-server
CMD ["redis-server", "--loadmodule", "/usr/lib/keydb/modules/modjs.so"]
