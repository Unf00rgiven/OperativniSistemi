#include <iostream>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <chrono>
#include <list>
#include <fstream>

using namespace std;
using namespace chrono;
using namespace this_thread;

const milliseconds READ_INTERVAL(5);

/*
    T1234 PRIMER
    Vaš program na komandnoj liniji treba da učita, redom sledeće parametre:
        Kako se zove fajl iz koga se učitava ulaz. 
        Koliko niti obrađivača treba da postoji (maksimum 8)
    Fajl iz koga se učitava ulaz je binaran i sastoji se od slogova. Svaki
    slog se sastoji od:
        1 double vrednosti (parametar) i
        1 long vrednosti (vreme)
    Fajl je garantnovan da je korektno formatiran i sastoji se od barem jednog
    sloga: ne morate pisati kod koji kontroliše za taj problem. 
    Niti koje postoje u programu su:
        1 nit učitavač 
        n niti obrađivača
    Nit učitavač učitava jedan slog iz ulaznog fajla, čeka period obrade od 
    5ms, a zatim zatraži da se sadržaj sloga obradi. Ako ima slobodnih niti
    obrađivača, to se desi odmah. Ako nema, nit učitavač ubaci nit u red 
    čekanja. Red čekanja ima 5 mesta. Ako je i mesto broj 5 zauzeto, nit učitavač
    pauzira učitavanje i čeka dok se ne oslobodi mesto.
    Svako čekanje se mora prijaviti korisniku preko
    standardnog izlaza. 
    Svaka nit obrađivača mora čekati vreme specificirano u long vrednosti
    (koja predstavlja broj milisekundi), a onda mora dodati parametar vrednost
    na sumu koju dele sve niti obrađivači i povećati jednako deljen brojač za 
    1. Ovo mora biti urađeno na način koji ne dozvoljava štetno preplitanje. 
    Kada više nema ulaza, nit učitavač sračuna srednju vrednost svih obrađenih 
    parametara, ispiše je na ekran, i terminira program. 

    Ceo zadatak treba da stane u ovaj .cpp i kompajliraće se sa 
    g++ -pthread --std=c++14 -o main main.cpp 
    komandom. 
*/

struct Slog{
    //TODO ubaciti slog
};

class SinhronaVrednost{
    //TODO suma i srednja vrednost ovde
};

class RedCekanja{
    //TODO struktura za sinhronizaciju izmedju niti
};

void ucitava(){
    //TODO nit za ucitavanje, izmeniti i parametre
}

void obrada(RedCekanja& rc, SinhronaVrednost& v){
    //TODO nit za cekanje
}

int main(int argc, char** argv){
    if(argc != 3){
        cout << "Navesti ime ulaznog fajla pa onda broj niti za obradu." << endl;
        exit(1);
    }
    int n = atoi(argv[2]);
    if(n <= 0) n = 1;
    if(n > 8) n = 8;
    //TODO Napraviti i pokrenuti niti
    return 0;
}