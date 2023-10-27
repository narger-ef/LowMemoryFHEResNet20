//
// Created by Lorenzo on 24/10/23.
//

#ifndef LOWMEMORYFHERESNET20_UTILS_H
#define LOWMEMORYFHERESNET20_UTILS_H

#include <iostream>
using namespace std;
using namespace std::chrono;

namespace utils {

    static inline chrono::time_point<steady_clock, nanoseconds> start_time() {
        return high_resolution_clock::now();
    }

    static duration<long long, ratio<1, 1000>> total_time;

    static inline void print_duration(chrono::time_point<steady_clock, nanoseconds> start, const string &title) {
        auto ms = duration_cast<milliseconds>(high_resolution_clock::now() - start);

        total_time += ms;

        auto secs = duration_cast<seconds>(ms);
        ms -= duration_cast<milliseconds>(secs);
        auto mins = duration_cast<minutes>(secs);
        secs -= duration_cast<seconds>(mins);

        if (mins.count() < 1) {
            cout << "⌛(" << title << "): " << secs.count() << ":" << ms.count() << "s" << " (Total: " << duration_cast<seconds>(total_time).count() << "s)" << endl;
        } else {
            cout << "⌛(" << title << "): " << mins.count() << "." << secs.count() << ":" << ms.count() << endl;
        }
    }

    static inline vector<double> read_values_from_file(const string& filename, double scale = 1) {
        vector<double> values;
        ifstream file(filename);

        if (!file.is_open()) {
            std::cerr << "Can not open " << filename << std::endl;
            return values; // Restituisce un vettore vuoto in caso di errore
        }

        string row;
        while (std::getline(file, row)) {
            istringstream stream(row);
            string value;
            while (std::getline(stream, value, ',')) {
                try {
                    double num = stod(value);
                    values.push_back(num * scale);
                } catch (const invalid_argument& e) {
                    cerr << "Can not convert: " << value << endl;
                }
            }
        }

        file.close();
        return values;
    }
}

#endif //LOWMEMORYFHERESNET20_UTILS_H
