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
    double parametar;
    milliseconds vreme;
    bool terminate;
};

class SinhronaVrednost{
    private:
        double sum;
        int n;
        mutex m;
    public:
        SinhronaVrednost() : sum(0.0), n(0) {}
        void dodaj(double x){
            unique_lock<mutex> l(m);
            sum += x;
            n++;
        }
        double racunaj(){
            unique_lock<mutex> l(m);
            return sum / (double)n;            
        }
};

class RedCekanja{
    private:
        list<Slog> red; 
        mutex m;
        condition_variable available;
        condition_variable hasRoom;
        condition_variable completion;
        int waiting;
        int n;
    public:
        RedCekanja(int _n) : waiting(0), n(_n) {}
        Slog get()
        {
            unique_lock<mutex> l(m);
            while(red.empty()){
                cout << "Cekam slog..." << endl;
                waiting++;
                completion.notify_one();
                available.wait(l);
                waiting--;
            }
            Slog s = red.front();
            red.pop_front();
            hasRoom.notify_one();
            return s;
        }
        void put(Slog s){
            unique_lock<mutex> l(m);
            while(red.size() >= 5){
                cout << "Red pun, cekam..." << endl;
                hasRoom.wait(l);                
            }
            red.push_back(s);
            available.notify_one();
        }
        void waitUntilEnd(){
            unique_lock<mutex> l(m);
            while(waiting != n){
                cout << "Cekam da se proracuni zavrse" << endl;
                completion.wait(l);        
            }
        } 
};

void ucitava(char* fname, RedCekanja& rc, SinhronaVrednost& v, int n){
    ifstream f(fname, ios::binary | ios::in);
    int i = 0;
    while(1){
        sleep_for(READ_INTERVAL);
        double p;
        long m; 
        f.read((char*)&p, sizeof(double));
        f.read((char*)&m, sizeof(long));
        if(f.eof()) break;        
        milliseconds ms(m);
        Slog s;
        s.parametar = p;
        s.vreme = ms;
        s.terminate = false;
        rc.put(s);
        i++;
    }
    f.close();
    rc.waitUntilEnd();
    for(int i = 0; i < n; i++){
        Slog s;
        s.terminate = true;
        rc.put(s);
    }
    cout << "Srednja vrednost: " << v.racunaj() << endl;
}

void obrada(RedCekanja& rc, SinhronaVrednost& v){
    while(1){
        Slog s = rc.get();
        if(s.terminate) return;
        sleep_for(s.vreme);
        v.dodaj(s.parametar);
    }
}

int main(int argc, char** argv){
    if(argc != 3){
        cout << "Navesti ime ulaznog fajla pa onda broj niti za obradu." << endl;
        exit(1);
    }
    int n = atoi(argv[2]);
    if(n <= 0) n = 1;
    if(n > 8) n = 8;
    RedCekanja rc(n);
    SinhronaVrednost v;
    thread u(ucitava, argv[1], ref(rc), ref(v), n);
    thread o[8];
    for(int i = 0; i < n; i++){
        o[i] = thread(obrada, ref(rc), ref(v));
    }
    u.join();
    for(int i = 0; i < n; i++){
        o[i].join();
    }
    return 0;
}