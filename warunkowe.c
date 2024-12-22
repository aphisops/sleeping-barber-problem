#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

#define LICZBA_KRZESEL 4
#define CZAS_STRZYZENIA 3
#define LICZBA_KLIENTOW 15

pthread_cond_t fryzjer_gotowy = PTHREAD_COND_INITIALIZER;    // Zmienna warunkowa informująca o gotowości fryzjera
pthread_cond_t klient_gotowy = PTHREAD_COND_INITIALIZER;     // Zmienna warunkowa informująca o gotowości klienta
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;           // Mutex do synchronizacji dostępu do współdzielonych zasobów

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
        pthread_mutex_lock(&mutex);

        // Fryzjer czeka na sygnał o gotowości klienta lub na zakończenie pracy
        while (rozmiar_kolejki == 0 && laczna_liczba_klientow < LICZBA_KLIENTOW) {
            pthread_cond_wait(&klient_gotowy, &mutex);
        }

        // Jeśli liczba klientów jest równa oczekiwanej i nie ma nikogo w kolejce, to kończymy pracę
        if (laczna_liczba_klientow == LICZBA_KLIENTOW && rozmiar_kolejki == 0) {
            pthread_mutex_unlock(&mutex);
            break;
        }

        // Fryzjer obsługuje kolejnego klienta z poczekalni
        aktualny_klient = kolejka_poczekalnia[przod_kolejki];
        przod_kolejki = (przod_kolejki + 1) % LICZBA_KRZESEL;
        rozmiar_kolejki--;
        wyswietl_stan(aktualny_klient, 1);

        // Informacja o gotowości fotela po zakończeniu strzyżenia
        pthread_cond_signal(&fryzjer_gotowy);
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
    int id_klienta = *(int*)arg;  // Pobierz wartość argumentu jako int
    free(arg);  // Zwolnienie dynamicznie alokowanej pamięci

    pthread_mutex_lock(&mutex);
    // Sprawdzenie, czy jest miejsce w poczekalni
    if (rozmiar_kolejki >= LICZBA_KRZESEL) {
        liczba_odrzuconych++;
        odrzuceni_klienci[liczba_odrzuconych - 1] = id_klienta;  // Poprawne przypisanie do tablicy
        wyswietl_stan(id_klienta, 2);
        pthread_mutex_unlock(&mutex);
        return NULL;
    }

    // Klient zajmuje miejsce w poczekalni
    kolejka_poczekalnia[koniec_kolejki] = id_klienta;
    koniec_kolejki = (koniec_kolejki + 1) % LICZBA_KRZESEL;
    rozmiar_kolejki++;
    wyswietl_stan(id_klienta, 0);

    // Powiadomienie fryzjera o gotowości klienta
    pthread_cond_signal(&klient_gotowy);
    pthread_mutex_unlock(&mutex);

    // Klient czeka na gotowość fotela do strzyżenia
    pthread_mutex_lock(&mutex);
    while (aktualny_klient != id_klienta) {
        pthread_cond_wait(&fryzjer_gotowy, &mutex);
    }
    pthread_mutex_unlock(&mutex);

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "-info") == 0) {
        tryb_info = 1;  // Włączenie trybu informacyjnego
    }

    srand(time(NULL));  // Inicjalizacja generatora liczb pseudolosowych

    pthread_t fryzjer_watek;  // Wątek fryzjera
    pthread_create(&fryzjer_watek, NULL, fryzjer, NULL);  // Tworzenie wątku fryzjera

    // Tworzenie wątków klientów
    int id_klienta = 1;
    for (int i = 0; i < LICZBA_KLIENTOW; i++) {
        int* id = malloc(sizeof(int));
        if (id == NULL) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }
        *id = id_klienta; 
        sleep(rand() % 2 + 1); 
        pthread_t klient_watek;
        pthread_create(&klient_watek, NULL, klient, id);
        pthread_detach(klient_watek);  // Odłączenie wątku klienta

        id_klienta++;
        laczna_liczba_klientow++;  
    }

    // Czekanie na zakończenie wątku fryzjera
    pthread_join(fryzjer_watek, NULL);

    // Usunięcie zmiennych warunkowych i mutexu
    pthread_cond_destroy(&fryzjer_gotowy);
    pthread_cond_destroy(&klient_gotowy);
    pthread_mutex_destroy(&mutex);

    return 0;
}
