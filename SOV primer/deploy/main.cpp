#include <iostream>
#include <cstdlib>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <list>
#include <random>
#include <chrono>
#include <map>
#include <cstring>
#include <cstdio>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <csignal>

using namespace std;

/*
    PRIMER SOV

    Implementirati simulator više procesa (4) koji interaguju sa sistemom memorije 
    baziranim na segmentima koji upravlja fizičkom memorijom. Simulirani operativni
    sistem upravlja sa 1MB memorije što je realizovano kao običan niz. Svaki proces
    alocira memoriju (u jedinicama od po 4KB) tako što je traži od simuliranog OS-a
    i on onda dodeljuje procesu novi segment sa traženom memorijom. Svaki proces se
    izvršava u svom thread-u gde prvo alocira jedan segment veličine od 4KB do 100KB
    čija je veličina deljiva sa 4096. Zatim ulazi u beskonačnu petlju gde svakih 
    100ms radi jednu od operacija, nasumično i to tako što U:
        7% slučajeva alocira nov segment koji se kreće od 4KB do 100KB u veličini
        (birano nasumično)
        3% slučajeva nasumično bira segment koji ima (izuzimajući prvi koji alocira)
        i dealocira ga. 
        70% slučajeva čita vrednost iz nasumičnog segmenta kome ima pristupa i
        nasumične lokacije unutar tog segmenta. 
        20% slučajeva piše nasumično generisanu vrednost u nasumičnu (validnu) adresu
        nasumično biranog segmenta. 
    Segmenti imaju baznu adresu i dužinu. 
    Pristup u slučaju čitanja i pisanja se vrši preko logičke vrednosti koja se
    translira u fizičku u skladu sa metodom koju smo radili na predavanjima. 
    Alokacija se vrši iz slobodne fizičke memorije (o kojoj se mora voditi evidencija)
    i to na takav način da se susedni regioni memorije koji su slobodni tretiraju kao
    jedan veći region memorije. 
    Alokacija iz slobodne fizičke memorije se vrši koristeći first-fit metodu. 
    Sinhronizacija se treba obavljati isključivo kada je to neophodno, inače je
    neophodno omogućiti maksimalan paralelizam. 
    Na signal CTRL-C sa tastature je neophodno prekinuti program ne-nasilno, zaustaviti
    sve niti i osloboditi svu memoriju koju je naš proces (ne simulirani procesi)
    zauzeo. 
    U slučaju sumnje, implementacija treba da prati primere i pravila data na 
    predavanjima. 
*/

typedef uint32_t u32;

const size_t MEMORY_CAPACITY = (1 << 20); //1MiB
const chrono::milliseconds stepInterval(100);
const int PROCESSES = 4;

struct Fragment{
    u32 loc;
    u32 len;
};


class Diagnostics{
    private:
        mutex m;
    public:
        void allocateMessage(int pid, u32 loc, u32 len){
            unique_lock<mutex> l(m);
            cout << "Process " << pid << " allocating from " << loc << " to " << loc + len << endl;
        }
        void deallocateMessage(int pid, u32 oLoc, u32 oLen, u32 loc, u32 len){
            unique_lock<mutex> l(m);
            cout << "Process " << pid << " asked to deallocate from " << oLoc << " to " << oLoc + oLen << endl;
            cout << "Process " << pid << " deallocated from " << loc << " to " << loc + len << endl;
        }
        void readMessage(int pid, u32 loc, int seg){
            unique_lock<mutex> l(m);
            cout << "Process " << pid << " reading from location " << loc << " in segment " << seg << endl;
        }

        void writeMessage(int pid, u32 loc, int seg){
            unique_lock<mutex> l(m);
            cout << "Process " << pid << " writing to location " << loc << " in segment " << seg << endl;
        }

        void processStatusMessage(int pid, int seg){
            unique_lock<mutex> l(m);
            cout << "Process " << pid << " has " << seg << " segments." << endl;
        }

        void printFreeMemoryMap(int pid, u32 amount, list<Fragment>& fm){
            unique_lock<mutex> l(m);
            cout << "Process " << pid << " is waiting for " << amount << " bytes, which is " << amount / 4096 << " 4K pages." << endl;
            u32 start = 0;
            u32 totalFree = 0;
            u32 totalTaken = 0;
            u32 totalSpace = 0;
            for(Fragment f : fm){
                if(start != f.loc){
                    cout << "Taken area from " << start << " to " << f.loc << " A total of " << ((f.loc - start) / 4096) << " 4K pages." << endl; 
                    totalTaken += (f.loc - start);
                }
                cout << "Free  area from " << f.loc << " to " << f.loc + f.len<< " A total of " << (f.len / 4096) << " 4K pages."  << endl;
                totalFree += f.len; 
                start = f.loc + f.len;
            }
            if(start != (u32)MEMORY_CAPACITY){
                cout << "Taken area from " << start << " to " << MEMORY_CAPACITY << " A total of " << ((MEMORY_CAPACITY - start) / 4096) << " 4K pages."  << endl;
                totalTaken += ((MEMORY_CAPACITY - start));
            }

            totalSpace = totalFree + totalTaken;
            cout << "Free " << totalFree << " bytes. Taken: " << totalTaken << " bytes. Total: " << totalSpace << " bytes." << endl;
        }
};

/*
    TODO
    Ahitektura programa je slobodna dok god uradi šta je neohpodno za zadatak,
    ali preporučena arhitektura uključuje objekat koji simulira operativni sistem
    odn. njegov sloj za rukovanje fizičkom memorijom i koji vodi računa o 
    memoriji, evidenciji slobodne memorije, tabelama segmenata za svaki proces, i
    i procesu alokacije i dealokacije. Zatim treba da bude objekat koji predstavlja
    proces koji treba da vodi računa o svom id-u, i takođe lokalnoj tabeli segmenata.
*/

void intHandler(int sig){
    cout << "Terminating all threads... " << endl;
    //TODO Ovde implementirati bezbedno gašenje celog programa
}

int main(){
    Diagnostics d;
}