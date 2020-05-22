# Use the latest Redis Docker Image (Buster) and add ModJS as a layer
FROM redis:buster
RUN mkdir -p /usr/lib/keydb/modules
# Copy over precompiled buster module along with latest bootstrap.js
COPY modjs.so /usr/lib/keydb/modules
COPY bootstrap.js /usr/lib/keydb/modules
VOLUME /scripts
# Call module to load with redis-server
CMD ["redis-server", "--loadmodule", "/usr/lib/keydb/modules/modjs.so"]
