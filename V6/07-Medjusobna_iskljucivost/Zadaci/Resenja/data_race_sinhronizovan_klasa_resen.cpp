/*		
Napraviti konkurentni program koji modeluje klasu brojača. Interfejs klase sadrži sledeće metode: 

class Brojac {
   public:
      void inc();
      void dec();
      friend ostream& operator<<(ostream& , Brojac& );
};

Metode inc i dec povećavaću i smanjuju vrednost brojača respektivno. Operator << služi za ispis brojača na ekran.

Klasa treba da zaštiti konzistentnost brojača u datim metodama uz pomoć klasa mutex i unique_lock. 

Kreirati 1 globalni objekat brojača kome će pristupati 2 niti. 

Kreirati 2 niti pri čemu jedna nit poziva metodu uvećavanja brojača 1000000 puta a druga metodu smanjivanja brojača 1000000 puta. Na kraju programa ispisati konačnu vrednost brojača nakon uvećavanja i smanjivanja.
*/

#include <thread> 
#include <iostream> 
#include <mutex>

using namespace std;

const int ITERATIONS=100000000;

//klasa u sebi sadrzi podatke i obezbedjuje thread-safe pristup ovim podacima
//dakle medjusobna iskljucivost je enkapsulirana u klasu, pozivalac operacija ne mora da vodi racuna o medjusobnoj iskljucivosti
class Brojac {
   mutex m; //propusnica pripada klasi i obezbedjuje sprecavanje stetnog preplitanja pri vrsenju operacija nad objektima klase 
   int broj; //vrednost Brojaca
 public:
   Brojac() : broj(0) {} //inicijalno je Brojac nula
   void inc() { unique_lock<mutex> l(m); ++broj; } //operacija povecava Brojac, ali pre toga zakljucava propusnicu i na taj nacin sprecava stetno preplitanja pri izmeni Brojaca
   void dec() { unique_lock<mutex> l(m); --broj; }//operacija smanjuje Brojac, ali pre toga zakljucava propusnicu i na taj nacin sprecava stetno preplitanja pri izmeni Brojaca
   friend ostream& operator<<(ostream& os, Brojac& b) { //preklopljen operator za ispis objekta klase Brojac. Ispisuje se atribut "broj" iz objekta
      unique_lock<mutex> l(b.m); //pre pristupa broju, zakljucava se propusnica da bi se sprecilo stetno preplitanje
      os << b.broj <<endl;
      return os;
   }
};
Brojac br; //da bi obe niti menjale isti Brojac, on je definisan kao globalna promenljiva

void inkrement() {
   //specificirani broj puta se zatrazi povecavanje Brojaca. Pozivalac ne vodi racuna o stetnom preplitanju, to je odgovornost klase
   for(int i=0;i<ITERATIONS;++i) 
      br.inc();	
}

void dekrement() {
   for(int i=0;i<ITERATIONS;++i)  //specificirani broj puta se zatrazi smanjivanje Brojaca
      br.dec();
}

int main() {
   thread t1(inkrement);
   thread t2(dekrement);
   t1.join();
   t2.join();
   cout<<br<<endl; //moze ovako, posto je operator za ispis za klasu Brojac preklopljen
}
