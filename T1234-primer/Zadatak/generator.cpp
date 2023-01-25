#include <iostream>
#include <random>
#include <functional>
#include <fstream>

using namespace std;

int main(int argc, char** argv){
    if(argc != 2){
        cerr << "Morate specificirati koliko slogova." << endl;
    }
    int n = atoi(argv[1]);
    default_random_engine gen;
    uniform_real_distribution<double> pdist(-5.0, 5.0);
    uniform_int_distribution<long> tdist(10, 30);

    auto pdie = bind(pdist, gen);
    auto tdie = bind(tdist, gen);

    double sum = 0.0;

    ofstream f("input.dat", ios::out | ios::binary);
    for(int i = 0; i < n; i++){
        double p = pdie();
        long t = tdie();
        sum += p;
        f.write((char*)&p, sizeof(double));
        f.write((char*)&t, sizeof(long));
    }
    f.close();
    cout << "Generisao sam fajl od " << n << " elemenata, sa prosecnom vrednoscu: " << sum / n << endl; 
    return 0;
}