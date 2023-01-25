/*
Napraviti konkurentni program koji pokrece vise niti u petlji.
Svakoj niti proslediti njen redni broj prilikom kreiranja i 
svaka nit treba da ispise dati broj u okviru tela niti.
*/


#include <thread>
#include <iostream>

using namespace std;

void f(int rbr) {
   cout << rbr;
}

const int BROJ_NITI = 7;
int main() {
   thread t[BROJ_NITI];
   for(int i=0; i<BROJ_NITI; ++i)
      t[i] = thread(f, i);
   for(int i=0; i<BROJ_NITI; ++i)
      t[i].join();
   cout << endl;
}
