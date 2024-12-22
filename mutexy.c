#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#define LICZBA_KRZESEL 4
#define CZAS_STRZYZENIA 3
#define LICZBA_KLIENTOW 15  

sem_t fryzjer_gotowy;         // Semafor informujący o gotowości fryzjera
sem_t klient_gotowy;         // Semafor informujący o gotowości klienta
pthread_mutex_t mutex;       // Mutex do synchronizacji dostępu do współdzielonych zasobów

int kolejka_poczekalnia[LICZBA_KRZESEL];   // Tablica reprezentująca poczekalnię
int przod_kolejki = 0;                     // Indeks początkowy kolejki
int koniec_kolejki = 0;                    // Indeks końcowy kolejki
int rozmiar_kolejki = 0;                   // Aktualny rozmiar kolejki

int liczba_odrzuconych = 0;                // Liczba odrzuconych klientów
int odrzuceni_klienci[100];                // Tablica przechowująca ID odrzuconych klientów
int aktualny_klient = -1;                  // ID klienta aktualnie obsługiwanego przez fryzjera
int laczna_liczba_klientow = 0;            // Całkowita liczba klientów

// Flaga dla trybu info
int tryb_info = 0;		

// Funkcja do wyświetlania aktualnego stanu
void wyswietl_stan(int id_klienta, int akcja) {
    printf("Rezygnacja:%d Poczekalnia: %d/%d [Fotel: %d]\n", 
           liczba_odrzuconych, rozmiar_kolejki, LICZBA_KRZESEL, 
           (akcja == 1) ? id_klienta : aktualny_klient);

    if (tryb_info) {
        printf("Poczekalnia: ");
        for (int i = 0; i < rozmiar_kolejki; i++) {
            int index = (przod_kolejki + i) % LICZBA_KRZESEL;
            printf("%d ", kolejka_poczekalnia[index]);
        }
        printf("\nOdrzuceni klienci: ");
        for (int i = 0; i < liczba_odrzuconych; i++) {
            printf("%d ", odrzuceni_klienci[i]);
        }
        printf("\n");
    }
}

// Wątek fryzjera
void* fryzjer(void* arg) {
    (void)arg;  

    while (1) {
        if (laczna_liczba_klientow == LICZBA_KLIENTOW && rozmiar_kolejki == 0) break;
    	
        // Fryzjer czeka na sygnał o gotowości klienta
        sem_wait(&klient_gotowy);
        pthread_mutex_lock(&mutex);
	
        // Fryzjer obsługuje kolejnego klienta z poczekalni
        aktualny_klient = kolejka_poczekalnia[przod_kolejki];
        przod_kolejki = (przod_kolejki + 1) % LICZBA_KRZESEL;
        rozmiar_kolejki--;
        wyswietl_stan(aktualny_klient, 1);
        
        // Informacja o gotowości fotela po zakończeniu strzyżenia
        sem_post(&fryzjer_gotowy);
        pthread_mutex_unlock(&mutex);

        sleep(CZAS_STRZYZENIA);  // Czas strzyżenia
	
        // Zakończenie obsługi klienta
        pthread_mutex_lock(&mutex);
        aktualny_klient = -1;
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

// Wątek klienta
void* klient(void* arg) {
    int id_klienta = *(int*)arg;
    free(arg);  		// Zwolnienie dynamicznie alokowanej pamięci

    pthread_mutex_lock(&mutex);
    // Sprawdzenie, czy jest miejsce w poczekalni
    if (rozmiar_kolejki < LICZBA_KRZESEL) {
        // Klient zajmuje miejsce w poczekalni
        kolejka_poczekalnia[koniec_kolejki] = id_klienta;
        koniec_kolejki = (koniec_kolejki + 1) % LICZBA_KRZESEL;
        rozmiar_kolejki++;
        wyswietl_stan(id_klienta, 0);
        
        // Powiadomienie fryzjera o gotowości klienta
        sem_post(&klient_gotowy);
        pthread_mutex_unlock(&mutex);
        
        // Klient czeka na gotowość fotela do strzyżenia
        sem_wait(&fryzjer_gotowy);
    } else {
        // Klient zostaje odrzucony z powodu braku miejsc
        odrzuceni_klienci[liczba_odrzuconych++] = id_klienta;
        wyswietl_stan(id_klienta, 2);
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "-info") == 0) {
        tryb_info = 1;	
    }

    srand(time(NULL));	
	
    sem_init(&fryzjer_gotowy, 0, 0);     // Inicjalizacja semafora fryzjera
    sem_init(&klient_gotowy, 0, 0);      // Inicjalizacja semafora klienta
    pthread_mutex_init(&mutex, NULL);    // Inicjalizacja mutexu

    pthread_t fryzjer_watek;		    // Wątek fryzjera
    pthread_create(&fryzjer_watek, NULL, fryzjer, NULL);		// Tworzenie wątku fryzjera
	
    // Tworzenie wątków klientów
    int id_klienta = 1;
    for (int i = 0; i < LICZBA_KLIENTOW; i++) {
        int* id = malloc(sizeof(int));
        if (id == NULL) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }
        *id = id_klienta;	// Numer klienta
	    sleep(rand() % 2 + 1);	// Losowy czas przed przyjściem kolejnego klienta
        pthread_t klient_watek;
        pthread_create(&klient_watek, NULL, klient, id);
        pthread_detach(klient_watek);		// Odłączenie wątku klienta

        id_klienta++;
        laczna_liczba_klientow++;		
    }
    
    pthread_join(fryzjer_watek, NULL);
    // Usunięcie semaforów i mutexu
    sem_destroy(&fryzjer_gotowy);
    sem_destroy(&klient_gotowy);
    pthread_mutex_destroy(&mutex);

    return 0;
}
