build:
	gcc -o server main.c
run:
	./server 8181 ./file
pfrun:
	perf stat ./perf-server 8181 ./file/
clean:
	rm -r ./server
	rm -r ./file/webserver.log