#include <iostream>
#include <cstdlib>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
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

typedef chrono::high_resolution_clock hrc_t;//Radi seed-a PRNG-a.  
typedef uint32_t u32;//Radi kompaktnosti pisanja

const size_t MEMORY_CAPACITY = (1 << 22); //4MiB
const hrc_t::time_point start = hrc_t::now();
const chrono::milliseconds stepInterval(100);
const int PROCESSES = 4;

const char* outputName = "stateFifo";
const char* inputName = "commandFifo";

const int EF_FIRSTFIT = 0;
const int EF_LASTFIT = 1;
const int EF_BESTFIT = 2;
const int EF_WORSTFIT = 3;
const int EF_LENGTH = 4;

const int F_ALLOCATING = 1;
const int F_COMPACTING = 2;


bool allocationEnabled = true;
bool compactingActive = false;

int type = EF_FIRSTFIT;

void onAllocationChanged();
void onCompactionChanged();
void onTypeChanged();

void intHandler(int sig);

struct Fragment{ //Jedan fragment slobodne memorije
    u32 loc;
    u32 len;
};


class Diagnostics{ //Pomoćna klasa, slobodno je proširiti
    private:
        mutex m;
        int fdOutput;
        int fdInput;
        int waiting; 
        char outBuffer[8192];
        bool visual;
    public:
        Diagnostics(bool v) : waiting(0), visual(v) {
            if(visual){
                fdInput = open(inputName, O_RDONLY);
                fdOutput = open(outputName, O_WRONLY);
                *((int*)(outBuffer)) = 3075;
                outBuffer[4] = type;
                outBuffer[5] = waiting;
                outBuffer[6] = F_ALLOCATING;
                for(int i = 7; i < 3073;i++){
                    outBuffer[i] = 0;
                }
            }
        }
        void incWaiting() {
            unique_lock<mutex> l(m);
            waiting++;
        }
        void decWaiting() {
            unique_lock<mutex> l(m);
            waiting--;
        }
        void allocateMessage(int pid, u32 loc, u32 len, u32 seg){
            unique_lock<mutex> l(m);
            cout << "Process " << pid << " allocating from " << loc << " to " << loc + len << endl;
            if(visual){
                int x = loc / 4096; 
                int ll = len / 4096; 
                for(int i = 0;i < ll;i++){
                    outBuffer[7 + (x + i)*3 + 0] = 1;
                    outBuffer[7 + (x + i)*3 + 1] = (char)pid;
                    outBuffer[7 + (x + i)*3 + 2] = (char)seg;
                }
            }
        }
        void deallocateMessage(int pid, u32 oLoc, u32 oLen, u32 loc, u32 len){
            unique_lock<mutex> l(m);
            cout << "Process " << pid << " asked to deallocate from " << oLoc << " to " << oLoc + oLen << endl;
            cout << "Process " << pid << " deallocated from " << loc << " to " << loc + len << endl;

            if(visual){
                int x = loc / 4096; 
                int ll = len / 4096; 
                for(int i = 0;i < ll;i++){
                    outBuffer[7 + (x + i)*3 + 0] = 0;
                    outBuffer[7 + (x + i)*3 + 1] = 0;
                    outBuffer[7 + (x + i)*3 + 2] = 0;
                }
            }

        }

        void compactionDeallocateMessage(u32 oLoc, u32 oLen, u32 loc, u32 len){
            unique_lock<mutex> l(m);
            cout << "The compacter asked to deallocate from " << oLoc << " to " << oLoc + oLen << endl;
            cout << "The compacter deallocated from " << loc << " to " << loc + len << endl;

            if(visual){
                int x = loc / 4096; 
                int ll = len / 4096; 
                for(int i = 0;i < ll;i++){
                    outBuffer[7 + (x + i)*3 + 0] = 0;
                    outBuffer[7 + (x + i)*3 + 1] = 0;
                    outBuffer[7 + (x + i)*3 + 2] = 0;
                }
            }
        }

        void compactionMessage(int pid, int seg, u32 oBase, u32 len, u32 nBase){
            unique_lock<mutex> l(m);
            cout << "Process " << pid << " and segment " << seg << "of length " << len << " bytes is a compaction candidate. To be moved from " << oBase << " to " << nBase << endl;
            if(visual){
                int x = nBase / 4096; 
                int ll = len / 4096; 
                for(int i = 0;i < ll;i++){
                    outBuffer[7 + (x + i)*3 + 0] = 1;
                    outBuffer[7 + (x + i)*3 + 1] = (char)pid;
                    outBuffer[7 + (x + i)*3 + 2] = (char)seg;
                }
            }
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

        void runOutput(){
            while(1 && visual){                
                this_thread::sleep_for(stepInterval / 2);
                outBuffer[4] = type;
                outBuffer[5] = waiting;
                outBuffer[6] = 0;
                if(allocationEnabled) outBuffer[6] |= F_ALLOCATING;
                if(compactingActive) outBuffer[6] |= F_COMPACTING; 
                write(fdOutput, outBuffer, 3079);
            }
        }

        void runInput(){
            if(visual){
                while(1){
                    char inbuf[2];
                    read(fdInput, inbuf, 2);
                    if(inbuf[0] == 'q' && inbuf[1] == 'q'){
                        intHandler(0);
                        return;
                    }else if(inbuf[0] == 'c'){
                        type = (int)inbuf[1];
                        onTypeChanged();
                    }else if(inbuf[0] == 'a'){
                        if(inbuf[1] == 0){
                            allocationEnabled = false;
                        }else if(inbuf[2] == 1){
                            allocationEnabled = true;
                        }else{
                            allocationEnabled = !allocationEnabled;
                        }
                        onAllocationChanged();
                    }else if(inbuf[0] == 'd'){
                        if(inbuf[1] == 0){
                            compactingActive = false;
                        }else if(inbuf[2] == 1){
                            compactingActive = true;
                        }else{
                            compactingActive = !compactingActive;
                        }
                        onCompactionChanged();
                    }
                }       
            }else{
                //Rezervna ne-vizuelna tehnika kontrole
                while(1){
                    char c;
                    cin >> c;
                    if(c == 'q'){
                        intHandler(0);
                        return;
                    }else if(c == 'c'){
                        type = (type + 1) % EF_LENGTH;
                        {
                            unique_lock<mutex> l(m);
                            cout << "Mod je sada: " << type << endl;
                        }
                        onTypeChanged();
                    }else if(c == 'a'){
                        allocationEnabled = !allocationEnabled;
                        {
                            unique_lock<mutex> l(m);
                            cout << "Alokacije je sada: " << ((allocationEnabled) ? "ON" : "OFF") << endl;
                        }
                        onAllocationChanged();
                    }else if(c == 'd'){
                        compactingActive = !compactingActive;
                        {
                            unique_lock<mutex> l(m);
                            cout << "Kompakcija je sada: " << ((compactingActive) ? "ON" : "OFF") << endl;
                        }
                        onCompactionChanged();
                    }
                }
            }

        }
};

struct TableEntry{ //Jedna stavka u tabeli segmenta
    u32 base = 0;
    u32 len = 0;
};

class SegmentTable{ //Tabela segmenata za neki proces
    public:
        SegmentTable() {}
        int insertSegment(TableEntry tableEntry){
            int id = getNewID();
            table.insert(pair<int, TableEntry>(id, tableEntry));
            return id;
        }
        void deleteSegment(int id){
            table.erase(id);
        }
        TableEntry& getEntry(int id){
            return table[id];
        }
        /* 
            Vraća nasumični validni ID iz date tabele. Nije ključno za rešenje
            ali čini programiranje procesa lakšim. Prosleđujemo random_engine zato
            što je njegovo generisanje jako sporo, i stoga ne treba da se pravi tokom
            izvršavanja programa. Naša arhitektura ovde ima poseban generator za
            svaki proces budući da generisanje slučajnih brojeva predstavlja operaciju
            pisanja (menja interno stanje) te stoga se mora ili sinhronizovati ili
            obezbediti jedan takav po niti. Mi smo uradili ovo drugo, zbog brzine. 
        */
        int getRandomID(default_random_engine& gen){
            uniform_int_distribution<int> d(0, table.size() - 1);
            int x = d(gen);
            int i = 0;
            for(pair<int, TableEntry> p : table){
                if(i == x){
                    return p.first;
                }
                i++;
            }
            cerr << "Random segment generation failed" << endl;
            return 0;
        }
    private:
        map<int, TableEntry> table;
        /*
            Kod nađe najveći ID date tabele i vrati tu vrednost uvećanu za 1. To
            garantuje jedinstven ID. 
        */
        int getNewID(){
            if(table.empty()) return 0;
            int id = 0;
            for(pair<int, TableEntry> p : table){
                if(p.first > id) id = p.first;
            }
            return ++id;
        }
};

//Klasa koja simulira OS
class SystemMemory{
    public:
        SystemMemory(size_t capacity, Diagnostics& d) : terminal(false), cadh(d) {
            mem = (char*)malloc(capacity); //Alociramo 1MB memorije
            if(mem == NULL){
                perror("Could not initialize memory.");
                exit(1);
            }
            //Na početku je sva memorija slobodna što znači da se naša evidencija
            //slobodne memorije od 1 odsečka koji počinje od 0 i veliki je
            //koliko uopšte ima memorije. 
            freeMemory.push_back((Fragment) {.loc=0, .len=(u32)capacity});
        }
        ~SystemMemory(){
            free(mem);//Moramo osloboditi zauzeto
        }
        //Operacija čitanja. Primetite adresu koja je segment + logička adresa
        char read(int processID, int segmentID, u32 logicalAddress){
            cadh.readMessage(processID, logicalAddress, segmentID);//Ispisuje na ekran
            int loc = segmentTables[processID].getEntry(segmentID).base;
            if(logicalAddress >= segmentTables[processID].getEntry(segmentID).len){ //Provera prava pristupa
                cerr << "Internal segmentation violation." << endl;
                exit(2);
            }
            return mem[loc + logicalAddress]; //Očitavanje vrednosti uz translaciju
        }
        void write(int processID, int segmentID, u32 logicalAddress, char value){
            cadh.writeMessage(processID, logicalAddress, segmentID);
            int loc = segmentTables[processID].getEntry(segmentID).base;
            if(logicalAddress >= segmentTables[processID].getEntry(segmentID).len){
                cerr << "Internal segmentation violation." << endl;
                exit(2);
            }
            mem[loc + logicalAddress] = value;
        }
        
        int allocate(int processID, u32 amount){
            if(!segmentTables.count(processID)){ //U slučaju da ovo zovemo prvi put za dati proces, ubacujemo novu tabelu
                {
                    unique_lock<mutex> l(mAllocate);
                    segmentTables.insert(pair<int, SegmentTable>(processID, SegmentTable()));
                }//Ubacivanje menja deljenu klasu te je stavljamo u isključiv region.
                int loc = -1;
                {
                    unique_lock<mutex> l(mAllocate);//I ovde zaključavamo: rad sa evidencijom slobodne memorije je takođe rad sa deljenim resursom
                    while((loc = findFree(amount)) < 0){
                        cadh.printFreeMemoryMap(processID, amount, freeMemory); //U slučaju čekanja pišemo kako trenutno izgleda memorija.
                        cadh.incWaiting();
                        cvFree.wait(l);
                        cadh.decWaiting();
                        if(terminal) return -1; //Proveravamo da li nas je probudio ne uspeh u alokaciji, no proces gašenja.
                    }
                }
                //Sada kada imamo odakle počinje naša slobodna memorija (u loc) napravimo segment i stavimo ga u evidenciju
                int ret = segmentTables[processID].insertSegment((TableEntry) {.base = (u32)loc, .len  = amount});
                cadh.allocateMessage(processID, (u32)loc, amount, ret);
                return ret; //Vraćamo indeks segmenta koji smo alocirali
            }else{
                //Isto kao gore, ali bez potrebe da ubacijemo tabelu budući da već postoji
                int loc = -1;
                {
                    unique_lock<mutex> l(mAllocate);
                    while((loc = findFree(amount)) < 0){
                        cadh.printFreeMemoryMap(processID, amount, freeMemory);
                        cadh.incWaiting();
                        cvFree.wait(l);
                        cadh.decWaiting();
                        if(terminal) return -1;
                    }                    
                }
                int ret = segmentTables[processID].insertSegment((TableEntry) {.base = (u32)loc, .len  = amount});
                cadh.allocateMessage(processID, (u32)loc, amount, ret);
                return ret;
            }
        }
        void deallocate(int processID, int segmentID){
            int loc = segmentTables[processID].getEntry(segmentID).base;
            int len = segmentTables[processID].getEntry(segmentID).len;
            {
                    unique_lock<mutex> l(mAllocate);
                    list<Fragment>::iterator it;
                    /*
                        Ovde se implementira algoritam koji smo diskutovali
                        na času. Ako oslobađamo regione fizičke memorije
                        bitno je detektovati situaciju gde imamo fizičke regione
                        koji se dodiruju, budući da njih treba spojiti. 
                        Ako je novoslobođeni segment *, a stari slobodni segmenti
                        #, dok su zauzeti delovi predstavljeni praznim prostorom
                        naše opcije su
                        ###### **** ######## prelazi u ###### **** ########
                        ######****  ######## prelazi u ##########  ########
                        ######  ****######## prelazi u ######  ************
                        ######******######## prelazi u ####################
                    */
                    for(it = freeMemory.begin(); it != freeMemory.end();it++){
                        if(it->loc > loc) break;
                    }
                    if(it == freeMemory.begin()){
                        freeMemory.push_front((Fragment) {.loc = (u32)loc, .len = (u32)len});
                        it = freeMemory.begin();
                        list<Fragment>::iterator next = it;
                        next++;
                        if(next != freeMemory.end()){
                            if(it->loc + it->len == next->loc){
                                it->len = it->len + next->len;
                                freeMemory.erase(next);
                                cadh.deallocateMessage(processID, (u32)loc, len, (u32)loc, it->len);
                                cvFree.notify_all();                                   
                            }else{
                                cadh.deallocateMessage(processID, (u32)loc, len, (u32)loc, len);
                                cvFree.notify_all();                                   
                            }
                        }else{
                            cadh.deallocateMessage(processID, (u32)loc, len, (u32)loc, len);
                            cvFree.notify_all();
                        }
                    }else{
                        it--;
                        if((it->loc + it->len) == loc){
                            it->len = it->len + len;
                            list<Fragment>::iterator next = it;
                            next++;
                            if(next != freeMemory.end()){
                                if(it->loc + it->len == next->loc){
                                    it->len = it->len + next->len;
                                    freeMemory.erase(next);
                                    cadh.deallocateMessage(processID, loc, len, it->loc, it->len);
                                    cvFree.notify_all();
                                }else{
                                    cadh.deallocateMessage(processID, loc, len, it->loc, it->len);
                                    cvFree.notify_all();
                                }
                            }else{
                                cadh.deallocateMessage(processID, loc, len, it->loc, it->len);
                                cvFree.notify_all();
                            }
                        }else{
                            it++;
                            it = freeMemory.insert(it, (Fragment) {.loc = (u32)loc, .len = (u32)len});
                            list<Fragment>::iterator next = it;
                            next++;
                            if(next != freeMemory.end()){
                                if(it->loc + it->len == next->loc){
                                    it->len = it->len + next->len;
                                    freeMemory.erase(next);
                                    cadh.deallocateMessage(processID, loc, len, it->loc, it->len);
                                    cvFree.notify_all();                                    
                                }else{
                                    cadh.deallocateMessage(processID, loc, len, it->loc, it->len);
                                    cvFree.notify_all();
                                }
                            }else{
                                cadh.deallocateMessage(processID, loc, len, it->loc, it->len);
                                cvFree.notify_all();
                            }
                        }
                    }
                }
            segmentTables[processID].deleteSegment(segmentID);
        }
        //Ovo je ono što u stvari zove proces, ovo ga samo usmeri gde treba
        int getRandomID(int processID, default_random_engine& gen){
            return segmentTables[processID].getRandomID(gen);
        }
        //Ovim zaustavljamo niti koje čekaju u okviru OS. 
        void terminate(){
            terminal = true;
            cvFree.notify_all();
        }
    private:
        char *mem;
        bool terminal;
        Diagnostics& cadh;
        map<int, SegmentTable> segmentTables;
        list<Fragment> freeMemory;
        mutex mAllocate; 
        condition_variable cvFree;
        /*
            Implementacija first fit algoritma. Prvi odsečak koji je dovoljno veliki
            se odabira. Ako je taman veličine ceo se izvozi. Ako je veći, mrvi se
            tako što se početak odsečka pomera unapred za zauzet prostor i dužina
            se adekvatno smanjuje, a kao povratna vrednost se daje pređašnja adresa
            početka tog odsečka. 
        */
        int findFree(u32 amount){
            for(list<Fragment>::iterator it = freeMemory.begin();it != freeMemory.end();it++){
                if(it->len == amount){
                    int ret = (int)it->loc;
                    freeMemory.erase(it);
                    return ret;
                }else if(it->len > amount){
                    int ret = (int)it->loc;
                    it->loc = it->loc + amount; 
                    it->len = it->len - amount;
                    return ret; 
                }
            }
            return -1; 
        }   
};

//Sinhronizovani brojač da bi procesi dobili ID brojeve koji su jedinstveni
class IDManager{
    private:
        int process;
        mutex m;
    public:
        IDManager() : process(0) {}
        int getProcessID(){
            unique_lock<mutex> l(m);
            return process++; 
        }
};

class Process{
    public:
        Process(IDManager& idm, SystemMemory& sm, Diagnostics& dd) : systemMemory(sm), segmentSizeDistribution(1,25), stepDistribution(1,100), terminate(false), cadh(dd){
               id = idm.getProcessID();
               hrc_t::duration d = hrc_t::now() - start;
               auto x = d.count();
               x = x ^ (id << 7);//Dozvoljavamo bitovima id-a da utiču na seed vrednost
               //generatora slučajnih brojeva ovog procesa. Ovo omogućava da slučajne
               //vrednosti budu maksimalno različite čak i ako su početna vremena veoma
               //bliska. 
               generator.seed(x);
        }
        void run(){
            cout << "Running process with ID" << id << endl;
            //Pravljenje početnog, permanentnog segmenta. 
            u32 amount = segmentSizeDistribution(generator) * 4096;
            int permanentSegment = systemMemory.allocate(id, amount);
            localTable.insert(pair<int, u32>(permanentSegment, amount));
            while(1){
                if(terminate) return;
                int step = stepDistribution(generator);//Vrednost od 1 do 100
                //Što nam omogućava da precizno definišemo šanse za različite
                //korake simulacije. 
                if(step < 7){
                    //Veličina za alokaciju koja je umnožak 4096. 
                    u32 amount = segmentSizeDistribution(generator) * 4096;
                    int seg = systemMemory.allocate(id, amount);
                    if(seg < 0){ //Dobijamo -1 samo ako je neko pozvao terminate dok
                    //smo mi čekali. 
                        terminate = true;
                        continue;
                    }
                    localTable.insert(pair<int, u32>(seg, amount));
                }else if(step >= 7 && step < 10){
                    int seg = systemMemory.getRandomID(id, generator);
                    if(seg == permanentSegment) continue; //Ne dozvoljavamo
                    //da se oslobodi permanentni segment
                    systemMemory.deallocate(id, seg);
                    localTable.erase(seg);                    
                }else if(step >= 10 && step < 80){
                    int seg = systemMemory.getRandomID(id, generator);
                    uniform_int_distribution<u32> sd(0, localTable[seg] - 1);
                    u32 logicalAddress = sd(generator);
                    systemMemory.read(id, seg, logicalAddress);
                }else if(step >= 80 && step < 101){
                    int seg = systemMemory.getRandomID(id, generator);
                    uniform_int_distribution<u32> sd(0, localTable[seg] - 1);
                    uniform_int_distribution<int> dd(-1000000, 1000000);
                    u32 logicalAddress = sd(generator);
                    int data = dd(generator);
                    systemMemory.write(id, seg, logicalAddress, data);
                }
                else{
                    cerr << "Impossible step value" << endl;
                    exit(3);
                }
                cadh.processStatusMessage(id, localTable.size());
                this_thread::sleep_for(stepInterval);
            }
        }
        void doTerminate(){
            terminate = true;
        }
    private:
        int id;
        Diagnostics& cadh;
        default_random_engine generator;
        SystemMemory& systemMemory;
        map<int, u32> localTable;
        uniform_int_distribution<int> segmentSizeDistribution; 
        uniform_int_distribution<int> stepDistribution;
        bool terminate;
};

//Nit koja omogućava da se proces izvršava
void processRunner(Process& p){
    p.run();
}

void inputRunner(Diagnostics& d){
    d.runInput();
}

void outputRunner(Diagnostics& d){
    d.runOutput();
}

//Globalni pokazivači da bi mogli da pristupimo ključnim strukturama programa iz
//Obrađivača signala. 
SystemMemory* pMem = NULL;
Process* processes = NULL;

//Obrađivač signala koji se poziva kada se pritisne CTRL+C
void intHandler(int sig){
    cout << "Terminating all threads... " << endl;
    if(pMem == NULL) exit(10);
    if(processes == NULL) exit(11); 
    pMem->terminate();
    for(int i = 0; i < PROCESSES;i++){
        processes[i].doTerminate();
    }
}

void onAllocationChanged(){

}

void onCompactionChanged(){

}

void onTypeChanged(){

}

int main(int argc, char** argv){
    bool visual = true;
    if(argc == 2){
        if(argv[1][0] == 't'){
            visual = false;
        }
    }
    Diagnostics d(visual);
    SystemMemory mem(MEMORY_CAPACITY, d);
    pMem = &mem;
    signal(SIGINT, intHandler);
    IDManager idm;
    thread inThread(inputRunner, ref(d));
    thread outThread(outputRunner, ref(d));
    inThread.detach();
    outThread.detach();
    Process processList[PROCESSES] {Process(idm, mem, d), Process(idm, mem, d), Process(idm, mem, d), Process(idm, mem, d)}; 
    thread threadList[PROCESSES];
    for(int i = 0; i < PROCESSES;i++){
        threadList[i] = thread(processRunner, ref(processList[i]));
    }
    processes = processList;
    for(int i = 0; i < PROCESSES;i++){
        threadList[i].join();
    }
    return 0;
}