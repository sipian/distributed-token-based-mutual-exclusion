#include<iostream>
using namespace std;


main(int argc, char const *argv[])
{
    int n;
    cin >> n;
    cout << n << " 10 0 0.5 1.5\n";
    for(int i = 0; i < n; i++) {
        cout << i << ' ';
        for(int j = 0; j < n; j++) {
            if(j!=i) {
                cout << j << ' ';
            }
        }
        cout << endl;
    }
    return 0;
}
