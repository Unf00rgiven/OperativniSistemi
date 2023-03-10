/*
Izmeniti program data_race.cpp i realizovati zaštitu pristupa brojaču uz pomoć klase 
mutex i njenih operacija lock() i unlock(). Primetiti koliko sada traje izvršavanje programa.
*/

#include <thread> 
#include <iostream> 
#include <mutex>

using namespace std;

const int ITERATIONS=100000000;

int brojac=0; //brojac je globalna promenljiva da bi obe niti menjale isti brojac

mutex m; //globalna propusnica za sprecavanje stetnog preplitanja pri pristupu brojacu

void inkrement() {
   for(int i=0;i<ITERATIONS;++i) {
	  m.lock(); //zakljuca se propusnica pre pristupa deljenoj promenljivoj
      ++brojac;	
	  m.unlock();
	}
}

void dekrement() {
   for(int i=0;i<ITERATIONS;++i) {
	  m.lock(); //zakljuca se propusnica pre pristupa deljenoj promenljivoj
      --brojac;	
      m.unlock();
   }
}

int main() {
   thread t1(inkrement);
   thread t2(dekrement);
   t1.join();
   t2.join();
   cout<<brojac<<endl;
}
