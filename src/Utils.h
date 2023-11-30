//
// Created by Lorenzo on 24/10/23.
//

#ifndef LOWMEMORYFHERESNET20_UTILS_H
#define LOWMEMORYFHERESNET20_UTILS_H

#include <iostream>
#include <openfhe.h>



using namespace std;
using namespace std::chrono;
using namespace lbcrypto;

namespace utils {

    static inline chrono::time_point<steady_clock, nanoseconds> start_time() {
        return steady_clock::now();
    }

    static duration<long long, ratio<1, 1000>> total_time;

    static inline void print_duration(chrono::time_point<steady_clock, nanoseconds> start, const string &title) {
        auto ms = duration_cast<milliseconds>(steady_clock::now() - start);

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
                    //num = std::floor(num * 10) / 10; //1 decimal
                    values.push_back(num * scale);
                } catch (const invalid_argument& e) {
                    cerr << "Can not convert: " << value << endl;
                }
            }
        }

        file.close();
        return values;
    }

    static inline vector<double> read_fc_weight (const string& filename) {
        vector<double> weight = read_values_from_file("../weights/fc.bin");
        vector<double> weight_corrected;

        for (int i = 0; i < 64; i++) {
            for (int j = 0; j < 10; j++) {
                weight_corrected.push_back(weight[(10 * i) + j]);
            }
            for (int j = 0; j < 64 - 10; j++) {
                weight_corrected.push_back(0);
            }
        }

        return weight_corrected;
    }

    static inline double compute_approx_error(Plaintext expected, Plaintext bootstrapped) {
        vector<complex<double>> result;
        vector<complex<double>> expectedResult;

        result = bootstrapped->GetCKKSPackedValue();
        expectedResult = expected->GetCKKSPackedValue();


        if (result.size() != expectedResult.size())
            OPENFHE_THROW(config_error, "Cannot compare vectors with different numbers of elements");

        // using the infinity norm
        double maxError = 0;
        for (size_t i = 0; i < result.size(); ++i) {
            double error = std::abs(result[i].real() - expectedResult[i].real());
            if (maxError < error)
                maxError = error;
        }

        return std::abs(std::log2(maxError));
    }

    static inline int get_relu_depth(int degree) {
        //Check: https://github.com/openfheorg/openfhe-development/blob/main/src/pke/examples/FUNCTION_EVALUATION.md
        switch (degree) {
            case 5:
                return 3;
            case 13:
                return 4;
            case 27:
                return 5;
            case 59:
                return 6;
            case 119:
                return 7;
            case 200:
            case 247:
                return 8;
            case 495:
                return 9;
            case 1007:
                return 10;
            case 2031:
                return 11;
        }

        cerr << "Set a valid degree for ReLU" << endl;
        exit(1);
    }


}

#endif //LOWMEMORYFHERESNET20_UTILS_H
