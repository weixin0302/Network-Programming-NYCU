all: np_simple.cpp np_single_proc.cpp np_multi_proc.cpp
	g++ -o np_simple np_simple.cpp -g
	g++ -o np_single_proc np_single_proc.cpp -g
	g++ -o np_multi_proc np_multi_proc.cpp -g -lpthread
clean:
	rm np_simple
	rm np_single_proc
	rm np_multi_proc