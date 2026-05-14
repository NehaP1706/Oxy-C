all: client server

client:
	gcc utils.c client.c -o client.out -lssl -lcrypto

server:
	gcc utils.c server.c -o server.out -lssl -lcrypto

clean:
	rm -f client.out server.out