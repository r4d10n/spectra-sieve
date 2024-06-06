rm websocket_server.o spectra_sieve
g++ -o websocket_server.o -I./include  -DUSE_WEBSOCKET -c websocket_server.cpp 
g++ -o spectra_sieve spectra_sieve.cpp websocket_server.o lib/libcivetweb.a -lpthread -ldl -lrt -liio -lm

