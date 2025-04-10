// Basic C++ test case for nifcpp

int globalVar = 10;

int add(int a, int b) {
    int result = a + b;
    return result;
}

int main() {
    int x = 5;
    int y = add(x, globalVar);
    if (y > 10) {
        y = y * 2;
    } else {
        y = y - 1;
    }
    return 0;
}
