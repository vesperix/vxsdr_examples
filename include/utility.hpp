// Copyright (c) 2024 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

// Provides simple utilities for VXSDR programs

#pragma once

#include <chrono>

std::string format_time(const std::chrono::time_point<std::chrono::system_clock> t, const std::string& fmt = "%Y-%m-%d %H:%M:%S");

size_t read_cplx_16(const std::string& name, std::vector<std::complex<int16_t> >& data);
size_t write_cplx_16(const std::string& name, std::vector<std::complex<int16_t> >& data);
