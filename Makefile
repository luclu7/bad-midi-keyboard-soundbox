testmidi: main.o miniaudio.o
	g++ main.o miniaudio.o -lportmidi -o testmidi -std=c++20

main.o: main.cpp
	g++ -c main.cpp -std=c++20

miniaudio.o: miniaudio.h miniaudio.c
	gcc -c miniaudio.c -std=c99

clean:
	rm main.o
	rm -f testmidi

fullclean:
	rm -f testmidi *.o