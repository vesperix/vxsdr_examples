// Copyright (c) 2024 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

// Provides a simple way to set commonly used VXSDR options

#pragma once

#include "option_utils.hpp"
#include "vxsdr.hpp"

void add_rx_1ch_options(option_utils::program_options& desc);
void add_tx_1ch_options(option_utils::program_options& desc);
void add_common_options(option_utils::program_options& desc);
void add_network_options(option_utils::program_options& desc);

int set_rx_1ch_options(option_utils::parsed_options& vm, std::unique_ptr<vxsdr>& radio);
int set_tx_1ch_options(option_utils::parsed_options& vm, std::unique_ptr<vxsdr>& radio);
int set_common_options(option_utils::parsed_options& vm, std::unique_ptr<vxsdr>& radio);
int set_network_options(option_utils::parsed_options& vm, std::unique_ptr<vxsdr>& radio);
