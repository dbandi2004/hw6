#ifndef HASH_H
#define HASH_H

#include <iostream>
#include <cctype>  // for std::tolower
#include <random>
#include <chrono>
#include <cstring> // for std::size_t

typedef unsigned long long HASH_INDEX_T;

struct MyStringHash {
    HASH_INDEX_T rValues[5] { 983132572, 1468777056, 552714139, 984953261, 261934300 };

    MyStringHash(bool debug = true) {
        if (!debug) {
            generateRValues();
        }
    }

    HASH_INDEX_T operator()(const std::string& k) const {
        unsigned long long w[5] = {0};
        int len = k.length();
        int pos = len % 6; // start position for the first group
        int index = 4;    // start filling w from the end

        while (index >= 0) {
            w[index] = computeSubstringValue(k, pos - 6, pos);
            pos -= 6;
            index--;
        }

        // Compute final hash
        HASH_INDEX_T hash = 0;
        for (int i = 0; i < 5; ++i) {
            hash += rValues[i] * w[i];
        }
        return hash;
    }

    HASH_INDEX_T letterDigitToNumber(char letter) const {
        if (isdigit(letter)) {
            return 26 + (letter - '0');
        } else {
            letter = tolower(letter);  // Ensure character is lowercase
            return letter - 'a';
        }
    }

    unsigned long long computeSubstringValue(const std::string& k, int start, int end) const {
        unsigned long long value = 0;
        unsigned long long base = 1;

        for (int i = end - 1; i >= start; i--) {
            if (i >= 0 && i < k.length()) {
                value += letterDigitToNumber(k[i]) * base;
            }
            base *= 36;
        }

        return value;
    }

    void generateRValues() {
        // Obtain a seed from the system clock:
        unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
        std::mt19937 generator(seed);  // mt19937 is a standard mersenne twister random number generator

        // Generate random numbers for rValues
        for (int i = 0; i < 5; ++i) {
            rValues[i] = generator();
        }
    }
};

#endif
