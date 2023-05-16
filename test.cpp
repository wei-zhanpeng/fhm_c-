#include <stdio.h>
#include <stdlib.h>
#include <iostream>
using namespace std;

class ele{
    public:
        int a;
        int b;
        char c;

    ele(int a){
        this->a = a;
    }
};

int main(){

   // ele e2;
    ele e1(10);
    cout << &e1 <<endl;
    cout << sizeof(e1) <<endl;
    
}