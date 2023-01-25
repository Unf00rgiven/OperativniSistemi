// Ognjen Stojisavljevic RA 155/2019

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

const milliseconds READ_INTERVAL(2);

#define velicinaniza 15

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
    double vrednosti[velicinaniza];
    milliseconds vreme;
    bool terminate;

};

class SinhronaVrednost{
    private:
        double minimumSloga;
        double maxMinimuma;
        mutex m;
    public:
        SinhronaVrednost()
        {
            maxMinimuma = -numeric_limits<double>::max();
        }
        void minSloga(double niz[])
        {
            unique_lock<mutex> l(m);
            minimumSloga=niz[0];
            for(int i=0;i<velicinaniza;i++)
            {
                if(niz[i]<minimumSloga)
                    minimumSloga=niz[i];
            }
            if(minimumSloga>maxMinimuma) maxMinimuma = minimumSloga;

        }

        double getmaxMinumuma()
        {
            unique_lock<mutex> l(m);
            return maxMinimuma;
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
        RedCekanja() : waiting(0), n(4){}

        Slog get()
        {
            unique_lock<mutex> l(m);
            while(red.empty())
            {
                cout<<"Cekam slog..."<< endl;
                waiting++;
                completion.notify_one();
                available.wait(l);
                waiting--;
            }

            Slog s=red.front();
            red.pop_front();
            hasRoom.notify_one();
            return s;
        }

        void put(Slog s)
        {
            unique_lock<mutex> l(m);
            while(red.size() >= 7)
            {
                cout << "Red pun,cekam." << endl;
                hasRoom.wait(l);
            }
            red.push_back(s);
            available.notify_one();
        }

        void waitUntilEnd()
        {
            unique_lock<mutex> l(m);
            while(waiting!=n)
            {
                cout << "Cekam da se proracuni zavrse" << endl;
                completion.wait(l);
            }

        }


};

void ucitava(char* fname, RedCekanja& rc, SinhronaVrednost& v){
    ifstream f(fname, ios::binary | ios::in);

    while(1)
    {
        sleep_for(READ_INTERVAL);
        double nizdouble[velicinaniza];
        long v;

        f.read((char*)& v, sizeof(long));
        for(int k=0;k<velicinaniza;k++)
        {
            f.read((char*)& nizdouble[k], sizeof(double));
        }

        if(f.eof()) break;

        milliseconds ms(v);
        Slog s;
        s.vreme = ms;
        for(int k=0;k<velicinaniza;k++)
        {
            s.vrednosti[k] = nizdouble[k];
        }

        s.terminate = false;
        rc.put(s);
    }
    f.close();
    rc.waitUntilEnd();

    for(int i=0;i<4;i++)
    {
        Slog s;
        s.terminate = true ;
        rc.put(s);
    }

    cout<< "Maximum minimuma je: " << v.getmaxMinumuma() << endl;

}

void obrada(RedCekanja& rc, SinhronaVrednost& v){
    while(true)
    {
        Slog s= rc.get();
        if(s.terminate) return;
        sleep_for(s.vreme);
        v.minSloga(s.vrednosti);
    }
}

int main(int argc, char** argv){
    if(argc != 2){
        cout << "Navesti ime ulaznog fajla." << endl;
        exit(1);
    }

    RedCekanja rc;
    SinhronaVrednost v;
    thread u(ucitava,argv[1], ref(rc), ref(v));
    thread o[4];
    for(int i=0 ; i<4 ; i++)
    {
        o[i]=thread(obrada, ref(rc), ref(v));
    }
    u.join();
    for(int i=0 ; i<4 ; i++)
    {
        o[i].join();
    }
    return 0;
}
