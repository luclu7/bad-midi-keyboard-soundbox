midi-soundbox: main.o miniaudio.o
	g++ main.o miniaudio.o -lportmidi -std=c++20 -o midi-soundbox

main.o: main.cpp
	g++ -c main.cpp -std=c++20

miniaudio.o: miniaudio.h miniaudio.c
	gcc -c miniaudio.c -std=c99

clean:
	rm main.o
	rm -f midi-soundbox

fullclean:
	rm -f midi-soundbox *.o