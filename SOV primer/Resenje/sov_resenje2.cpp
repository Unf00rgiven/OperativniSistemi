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
#include <algorithm>

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

const size_t MEMORY_CAPACITY = 1 << 20; // 1 MB
const chrono::milliseconds STEP_INTERVAL(100);
const int PROCESSES = 4;
const size_t PAGE_SIZE = 1 << 12; // 4 KB

struct Fragment
{
    uint32_t loc;
    uint32_t len;
};

class Diagnostics
{
private:
    mutex m;

public:
    void allocateMessage(int pid, uint32_t loc, uint32_t len)
    {
        unique_lock<mutex> l(m);
        cout << "Process " << pid << " allocating from " << loc << " to " << loc + len << endl;
    }

    void deallocateMessage(int pid, uint32_t oLoc, uint32_t oLen, uint32_t loc, uint32_t len)
    {
        unique_lock<mutex> l(m);
        cout << "Process " << pid << " asked to deallocate from " << oLoc << " to " << oLoc + oLen << endl;
        cout << "Process " << pid << " deallocated from " << loc << " to " << loc + len << endl;
    }

    void readMessage(int pid, uint32_t loc, int seg)
    {
        unique_lock<mutex> l(m);
        cout << "Process " << pid << " reading from location " << loc << " in segment " << seg << endl;
    }

    void writeMessage(int pid, uint32_t loc, int seg)
    {
        unique_lock<mutex> l(m);
        cout << "Process " << pid << " writing to location " << loc << " in segment " << seg << endl;
    }

    void processStatusMessage(int pid, int seg)
    {
        unique_lock<mutex> l(m);
        cout << "Process " << pid << " has " << seg << " segments." << endl;
    }

    void printFreeMemoryMap(int pid, uint32_t amount, list<Fragment> &fm)
    {
        unique_lock<mutex> l(m);

        cout << "Process " << pid << " is waiting for " << amount << " bytes, which is " << amount / 4096 << " 4K pages." << endl;

        uint32_t start = 0;
        uint32_t totalFree = 0;
        uint32_t totalTaken = 0;
        uint32_t totalSpace = 0;

        for (Fragment f : fm)
        {
            if (start != f.loc)
            {
                cout << "Taken area from " << start << " to " << f.loc << " A total of " << ((f.loc - start) / 4096) << " 4K pages." << endl;
                totalTaken += (f.loc - start);
            }
            cout << "Free  area from " << f.loc << " to " << f.loc + f.len << " A total of " << (f.len / 4096) << " 4K pages."  << endl;
            totalFree += f.len;
            start = f.loc + f.len;
        }

        if (start != (uint32_t)MEMORY_CAPACITY)
        {
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

struct Request
{
    uint32_t size;
    bool terminated;
    condition_variable cv;

    Request(uint32_t s) : size(s), terminated(false) {}
};

class Memory
{
    Diagnostics &diagnostics;
    uint8_t *buffer;
    list<Fragment> free_list;
    map<int, Request*> requests;
    long segment_id;
    map<long, Fragment> segments;
    mutex m;

    Fragment insert_free_fragment(const Fragment &f)
    {
	for (auto it = free_list.begin(); it != free_list.end(); it++)
	{
	    if (it->loc + it->len == f.loc)
	    {
		it->len += f.len;

		if (it->loc + it->len == next(it)->loc)
		{
		    it->len += next(it)->len;
		    free_list.erase(next(it));
		}

		return *it;
	    }
	    else if (f.loc + f.len == it->loc)
	    {
		it->loc = f.loc;
		it->len += f.len;
		return *it;
	    }
	    else if (f.loc < it->loc)
	    {
		free_list.insert(it, f);
		return f;
	    }
	}

	free_list.push_back(f);
	return f;
    }

public:
    Memory(Diagnostics &d, uint32_t capacity) : diagnostics(d), segment_id(1)
    {
	buffer = new uint8_t[capacity];
	free_list.push_back({ 0, capacity });
    }

    ~Memory() { delete[] buffer; }

    long allocate(int pid, uint32_t size)
    {
	list<Fragment>::iterator it;
	unique_lock<mutex> l(m);

	while ((it = find_if(free_list.begin(), free_list.end(),
			     [size] (const Fragment &f) -> bool {
				 return f.len >= size;
			     })) == free_list.end())
	{
	    diagnostics.printFreeMemoryMap(pid, size, free_list);
	    Request r(size);
	    requests.insert(make_pair(pid, &r));
	    r.cv.wait(l);
	    requests.erase(pid);
	    if (r.terminated) return 0;
	}

	Fragment f = { it->loc, size };

	if (it->len > size)
	{
	    it->loc += size;
	    it->len -= size;
	}
	else
	{
	    free_list.erase(it);
	}

	diagnostics.allocateMessage(pid, f.loc, f.len);
	segments[segment_id] = f;
	return segment_id++;
    }
    
    void free(int pid, long segment)
    {
	unique_lock<mutex> l(m);
	auto it = segments.find(segment);
	if (it == segments.end()) return;

	Fragment f = insert_free_fragment(it->second);
	diagnostics.deallocateMessage(pid, it->second.loc, it->second.len, f.loc, f.len);
	segments.erase(it);

	// Sa ovakvim resenjem mozemo i da izbegnemo budjenje procesa koji
	// sigurno nece moci da alociraju jer znamo kolicinu za svaki zahtev.
	for (auto it = requests.begin(); it != requests.end(); it++)
	    it->second->cv.notify_one();
    }

    uint8_t read(int pid, long segment, uint32_t address)
    {
	unique_lock<mutex> l(m);
	auto it = segments.find(segment);
	if (it == segments.end() || address >= it->second.len) return 0;
	diagnostics.readMessage(pid, address, segment);
	return buffer[it->second.loc + address];
    }

    void write(int pid, long segment, uint32_t address, uint8_t value)
    {
	unique_lock<mutex> l(m);
	auto it = segments.find(segment);
	if (it == segments.end() || address >= it->second.len) return;
	diagnostics.writeMessage(pid, address, segment);
	buffer[it->second.loc + address] = value;
    }

    void terminate(int pid)
    {
	unique_lock<mutex> l(m);
	auto it = requests.find(pid);
	if (it == requests.end()) return;

	it->second->terminated = true;
	it->second->cv.notify_one();
    }
};

class Process
{
    Memory &memory;
    int pid;
    bool running;
    map<long, uint32_t> segments;

    long create_segment()
    {
	uint32_t size = (1 + rand() % 25) * PAGE_SIZE;
	long segment = memory.allocate(pid, size);
	if (segment) segments[segment] = size;
	return segment;
    }

    void delete_segment(long segment)
    {
	memory.free(pid, segment);
	segments.erase(segment);
    }

    long random_segment()
    {
	auto it = segments.begin();
	advance(it, rand() % segments.size());
	return it->first;
    }

public:
    Process(Memory &m, int id) : memory(m), pid(id) {}

    void execute()
    {
	running = true;
	long first_segment = create_segment();

	while (running)
	{
	    int rnd = rand() % 100;

	    if (rnd < 7)
	    {
		if (!create_segment()) break;
	    }
	    else if (rnd < 10)
	    {
		if (segments.size() > 1)
		{
		    long segment;
		    while ((segment = random_segment()) == first_segment) continue;
		    delete_segment(segment);
		}
	    }
	    else if (rnd < 80)
	    {
		long segment = random_segment();
		uint32_t address = rand() % segments[segment];
		memory.read(pid, segment, address);
	    }
	    else
	    {
		long segment = random_segment();
		uint32_t address = rand() % segments[segment];
		memory.write(pid, segment, address, rand() & 0xFF);
	    }

	    this_thread::sleep_for(STEP_INTERVAL);
	}

	for (auto it = segments.begin(); it != segments.end(); it++)
	    memory.free(pid, it->first);
	segments.clear();
    }

    void stop()
    {
	running = false;
	memory.terminate(pid);
    }
};

void wait_for_ctrl_c()
{
    int signum;
    sigset_t sigset;

    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    sigprocmask(SIG_BLOCK, &sigset, NULL);
    sigwait(&sigset, &signum);
}

int main()
{
    srand(time(NULL));

    Diagnostics d;
    Memory m(d, MEMORY_CAPACITY);
    Process p[PROCESSES] {
	Process(m, 1), Process(m, 2), Process(m, 3), Process(m, 4)
    };
    thread t[PROCESSES];

    for (int i = 0; i < PROCESSES; i++)
	t[i] = thread(&Process::execute, ref(p[i]));

    wait_for_ctrl_c();
    cout << "Terminating all threads... " << endl;

    for (int i = 0; i < PROCESSES; i++) p[i].stop();
    for (int i = 0; i < PROCESSES; i++) t[i].join();
    return 0;
}
