compile:
	gcc Projekt2Zmienne.c -o zmienne -pthread
	gcc Projekt2Semafory.c -o semafory -pthread

runZM: zmienne
	./zmienne -k 100 -c 5
runSEM: semafory
	./semafory -k 100 -c 5

clean: semafory zmienne
	rm -f semafory zmienne