//Pavle Vukovic RA135/2019

#include <iostream>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <chrono>
#include <list>
#include <fstream>
#include <limits>

using namespace std;
using namespace chrono;
using namespace this_thread;

const milliseconds READ_INTERVAL(2);

/*
    T1234 Zadatak
    Vaš program na komandnoj liniji treba da učita, redom sledeće parametre:
        Kako se zove fajl iz koga se učitava ulaz.
    Fajl iz koga se učitava ulaz je binaran i sastoji se od slogova. Svaki
    slog se sastoji od:
        1 long vrednosti (vreme) i
        15 double vrednosti (niz)
    Fajl je garantnovan da je korektno formatiran i sastoji se od barem jednog
    sloga: ne morate pisati kod koji kontroliše za taj problem.
    Niti koje postoje u programu su:
        1 nit učitavač
        4 niti obrađivača
    Nit učitavač učitava jedan slog iz ulaznog fajla, čeka period obrade od
    2ms, a zatim zatraži da se sadržaj sloga obradi. Ako ima slobodnih niti
    obrađivača, to se desi odmah. Ako nema, nit učitavač ubaci nit u red
    čekanja. Red čekanja ima 7 mesta. Ako je i mesto broj 7 zauzeto, nit učitavač
    pauzira učitavanje i čeka dok se ne oslobodi mesto.
    Svako čekanje se mora prijaviti korisniku preko
    standardnog izlaza.
    Svaka nit obrađivača mora čekati vreme specificirano u long vrednosti
    (koja predstavlja broj milisekundi), a onda mora iz ulaznog niza sloga
    odrediti minimalnu vrednost. Program mora tako raditi da postoji deljena
    promenljiva koja sadrži u svakom trenutku najveći minimum nađen iz svih
    nizova. Kada obrađivač nađe minimalnu vrednost poredi je sa deljenom promenljivom
    i vrši zamenu ako je ovaj minimum veći. Ovo mora biti urađeno na način koji
    ne dozvoljava štetno preplitanje.

    Kada se obrade svi slogovi iz fajla, program ispiše maksimum svih minimuma i
    terminira program. Osim u slučaju greške, program se mora terminirati tako što
    stigne do kraja main funkcije.

    Ceo zadatak treba da stane u ovaj .cpp i kompajliraće se sa
    g++ -pthread --std=c++14 -o main main.cpp
    komandom.

    Očekivani output za input.dat koji je dat je -0.881422.

*/

struct Slog{
    milliseconds vreme;                                 //long vrednost
    double niz[15];
    bool kraj;
};

class SinhronaVrednost{
    private:
        mutex m;
        double univerzalni_minimum;
    public:
        SinhronaVrednost(){
            univerzalni_minimum =  -numeric_limits<double>::max();      //najmanja vrednost za double, jer ako stavim 0 npr. ona ispada univerzalni minimum, sto nije dobro resenje
        }

        void poredjenje(double minimum){                                //porede se vrednosti univerzalnog minimuma i minimuma svakog sloga ponaosob
            unique_lock<mutex> lock(m);

            if(minimum > univerzalni_minimum){                          //postavljanje vrednosti univerzalnog minimuma
                univerzalni_minimum = minimum;
            }
        }

        double getUniverzalniMinimum() const{                           //obican geter za univerzalni minimum
            return univerzalni_minimum;
        }
};

class RedCekanja{
    private:
        mutex m;                                                //propusnica
        list<Slog> red_cekanja;                                 //lista sacinjena od slogova
        condition_variable slobodno_mesto;                      //govori mi da li na listi ima slobodnog mesta
        condition_variable popunjeno;                           //govori mi da li je lista prazna ili ne
        condition_variable gotovo;                              //govori mi da je doslo do kraja obrade
        int cekaju;                                             //broj niti obradjivaca koje cekaju
        int n;                                                  //ukupan broj niti obradjivaca
    public:
        RedCekanja(){
            cekaju = 0;
            n = 4;
        }

        void stavi(Slog s){                                     //stavlja slog na kraj liste
            unique_lock<mutex> lock(m);
            while(red_cekanja.size() >= 7){                     //maksimalan broj mesta na listi je 7, ako je taj broj ostvaren ceka se prazno mesto na listi
                cout << "Nema mesta u redu cekanja!!! Ceka se mesto..." << endl;
                slobodno_mesto.wait(lock);
            }
            red_cekanja.push_back(s);                           //kada na listi ima mesta, stavljam na kraj liste slog prosledjen kao parametar
            popunjeno.notify_one();                             //obavestavam nit, koja ceka na preuzimanje sloga, da lista nije prazna, tj. da moze preuzeti slog
        }

        Slog uzmi(){                                            //skida slog sa pocetka liste, vraca ga kao povratnu vrednost
            unique_lock<mutex> lock(m);
            while(red_cekanja.empty()){                         //kada je lista prazna ne mogu da vratim slog sa pocetka iste
                cout << "Red cekanja je prazan!!! Ceka se da se on popuni necim..." << endl;
                cekaju++;                                       //broj niti koje cekaju se povecava
                gotovo.notify_one();                            //moguce je da je doslo do kraja
                popunjeno.wait(lock);                           
                cekaju--;                                       //broj niti koje cekaju se smanjuje
            }
            Slog s = red_cekanja.front();                       //slog s uzima vrednost sloga sa pocetka liste
            red_cekanja.pop_front();                            //taj slog se izbacuje iz liste
            slobodno_mesto.notify_one();                        //obavestavam 1 nit obradjivaca koja je cekala da ubaci slog na listu da to moze uciniti

        return s;                                               //vracam slog kao povratnu vrednost
        }

        void WaitUntilEnd(){                                    //cekam da se niti obradjivaci zavrse obradu
            unique_lock<mutex> lock(m);
            while(cekaju != n){                                    
                cout << "Cekam da se proracuni zavrse!!!" << endl;
                gotovo.wait(lock);
            }
        }

};

void ucitava(char* fname, RedCekanja& rc, SinhronaVrednost& v){     //funcija niti ucitavaca
    ifstream fajl(fname, ios::binary | ios::in);                    //otvaram ulazni fajl
    while(1){
        sleep_for(READ_INTERVAL);
        long vremee;
        double nizz[15];

        fajl.read((char*)& vremee, sizeof(long));                   //citanje iz fajla---ocitava se prvi long
        for(int i = 0; i < 15; i++){
            fajl.read((char*)& nizz[i], sizeof(double));            //citanje iz fajla---ocitaju se 15 double vrednosti
        }

        if(fajl.eof()) break;                                       //provera da li je doslo do kraja fajla! break se odnosi na while!

        milliseconds mili(vremee);                                  //vreme parsirano iz long u milliseconds

        Slog s;                                                     //Lokalna struktura slog, koju popunjavam ucitanim vrednostima
        s.vreme = mili;
        for(int j = 0; j < 15; j++){
            s.niz[j] = nizz[j];
        }
        s.kraj = false;
        rc.stavi(s);                                                //Ubaci lokalni slog na listu
    }
    fajl.close();                                                   //Zatvaram ulazni fajl

    rc.WaitUntilEnd();                                              //funkcija koja sluzi da budem siguran da su sve niti zavrsile obradu
    for(int i = 0; i < 4; i++){
        Slog s;
        s.kraj = true;
        rc.stavi(s);
    }

    cout << "Maksimum svih minimuma je: " << v.getUniverzalniMinimum() << "!!!" << endl;
}

void obrada(RedCekanja& rc, SinhronaVrednost& v){                   //funkcija niti obradjivaca
    double minimum;

    while(1){
        Slog s = rc.uzmi();                                         //preuzima se slog sa pocetka liste
        if(s.kraj){                                                 //ako je vrednost flag-a kraj == true, onda sledi kraj programa
            return;
        }
        sleep_for(s.vreme);
        minimum = s.niz[0];

        for(int i = 1; i < 15; i++){                                //trazenje minimuma za svaki slog ponaosob
            if(s.niz[i] < minimum){
                minimum = s.niz[i];
            }
        }

        v.poredjenje(minimum);                                      //pozivanje funkcije u kojoj se porede minimumi slogova sa univerzalnim minimumom
    }
}

int main(int argc, char** argv){
    if(argc != 2){
        cout << "Navesti ime ulaznog fajla." << endl;
        exit(1);
    }

    RedCekanja rc;
    SinhronaVrednost sv;

    thread t1(ucitava,argv[1],ref(rc),ref(sv));
    thread niz_niti[4];
    for(int i = 0; i < 4; i++){
        niz_niti[i] = thread(obrada, ref(rc), ref(sv));
    }

    t1.join();
    for(int i = 0; i < 4; i++){
        niz_niti[i].join();
    }

    return 0;
}
