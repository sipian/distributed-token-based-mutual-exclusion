#include<iostream>
using namespace std;

void generateCompleteGraphs(int n) {
    for(int i = 0; i < n; i++) {
        cout << i << ' ';
        for(int j = 0; j < n; j++) {
            if(j!=i) {
                cout << j << ' ';
            }
        }
        cout << endl;
    }
}

void generateLineGraph(int n) {
    cout << "0 1\n";
    for(int i = 1; i < n-1; i++) {
        cout << i << ' ' << i-1 << ' ' << i+1 << endl;
    }
    cout << n-1 << " " << n-2 << endl;
}

main(int argc, char const *argv[])
{
    int n;
    cin >> n;
    cout << n << " 6 3 1.55 0.65\n";
    generateCompleteGraphs(n);
    return 0;
}
