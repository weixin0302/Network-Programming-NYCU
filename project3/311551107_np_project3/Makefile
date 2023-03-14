all:
	g++ -std=c++14 -pedantic -pthread -lboost_system http_server.cpp -o http_server
	g++ -std=c++14 -pthread console.cpp -o console.cgi 

part1:
	g++ -std=c++14 -pedantic -pthread -lboost_system http_server.cpp -o http_server
	g++ -std=c++14 -pthread console.cpp -o console.cgi 

part2:
	g++ cgi_server.cpp -o cgi_server -lws2_32 -lwsock32 -std=c++14