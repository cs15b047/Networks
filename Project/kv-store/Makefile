CXX=g++
LDFLAG= -lpthread -lrdmacm

client: client.cpp
	$(CXX) -o client client.cpp $(LDFLAG)

server: kv_store.cpp
	$(CXX) -o server kv_store.cpp $(LDFLAG)

all: client server

clean:
	rm -f client server