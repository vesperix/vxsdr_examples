// Copyright (c) 2024 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

// Provides simple utilities for VXSDR programs

#include <cctype>
#include <chrono>
#include <cmath>
#include <complex>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

std::string format_time(const std::chrono::time_point<std::chrono::system_clock> t, const std::string& fmt) {
    std::stringstream output;
    time_t n_seconds = std::chrono::system_clock::to_time_t(t);
    output << std::put_time(localtime(&n_seconds), fmt.c_str());
    int32_t ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(t - std::chrono::system_clock::from_time_t(n_seconds)).count();
    output << "." << std::setfill('0') << std::setw(9) << ns;
    return output.str();
}

size_t read_cplx_16(const std::string& name, std::vector<std::complex<int16_t> >& data) {
    std::ifstream infile(name, std::ios::in | std::ios::binary);

    if (infile.is_open()) {
        infile.seekg(0, std::ifstream::end);
        size_t num_elem = infile.tellg() / sizeof(std::complex<int16_t>);
        infile.seekg(0, std::ifstream::beg);

        if (num_elem > data.size()) {
            data.resize(num_elem);
        }

        if (infile.good()) {
            infile.read((char*)&data.front(), sizeof(std::complex<int16_t>) * num_elem);
        }
        infile.close();

        for (size_t i = num_elem; i < data.size(); i++) {
            data[i] = std::complex<int16_t>(0, 0);
        }
        return num_elem;
    }
    return 0;
}

size_t write_cplx_16(const std::string& name, std::vector<std::complex<int16_t> >& data) {
    std::fstream outfile(name, std::ios::out | std::ios::trunc | std::ios::binary);

    if (outfile.good()) {
        outfile.write((char*)&data.front(), sizeof(std::complex<int16_t>) * data.size());
        outfile.seekg(0, std::fstream::end);
        size_t num_elem = outfile.tellg() / sizeof(std::complex<int16_t>);
        outfile.close();
        return num_elem;
    }
    return 0;
}
