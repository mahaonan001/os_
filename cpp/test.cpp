#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>

// Function to convert hexadecimal string to integer
int hexToDec(const std::string& hexStr) {
    int decimal;
    int arr[10];
    for(int i=0;i<hexStr.length();i++){
      if()
    }
    return decimal;
}

// Function to solve the Josephus problem
int josephus(int n) {
    std::vector<int> monkeys(n);
    for (int i = 0; i < n; ++i) {
        monkeys[i] = i + 1;
    }

    int index = 0;
    while (monkeys.size() > 1) {
        index = (index + 1) % monkeys.size();
        monkeys.erase(monkeys.begin() + index);
    }
    
    return monkeys[0];
}

int main() {
    std::string input;
    std::cin >> input;

    int n = hexToDec(input);

    int result = josephus(n);

    std::cout << result << std::endl;

    return 0;
}
