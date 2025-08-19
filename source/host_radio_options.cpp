// Copyright (c) 2024 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

// Provides a simple way to set commonly used VXSDR options

#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "option_utils.hpp"
#include "vxsdr.hpp"

std::vector<double> interpret_bracketed_list(const std::string& list, const char delim = ',') {
    // these list matching brackets in the same order
    const std::string left_brackets  = "[({";
    const std::string right_brackets = "])}";

    if (list.size() < 2) {
        std::cerr << "error parsing command line options (bracketed list): cannot interpret " << list << std::endl;
        exit(1);
    }

    unsigned i_bracket = left_brackets.size();
    for (unsigned i = 0; i < left_brackets.size(); i++) {
        if (list[0] == left_brackets[i] and list[list.size() - 1] == right_brackets[i]) {
            // found a matching pair of brackets
            i_bracket = i;
            break;
        }
    }

    if (i_bracket == left_brackets.size()) {
        std::cerr << "error parsing command line options (bracketed list): cannot find matching brackets in " << list << std::endl;
        exit(1);
    }

    // make a stringstream of the portion inside the brackets
    std::istringstream iss(list.substr(1, list.size() - 2));
    std::vector<double> ret;
    std::string number;
    while (std::getline(iss, number, delim)) {
        try {
            double x = std::stod(number.c_str(), NULL);
            ret.push_back(x);
        } catch (std::invalid_argument&) {
            std::cerr << "error parsing command line options (bracketed list): cannot interpret " << number << std::endl;
            exit(1);
        }
    }

    return ret;
}

void add_rx_1ch_options(option_utils::program_options& desc) {
    // clang-format off
    desc.add_option("rx_rate", "RX sample rate in Hz", option_utils::supported_types::REAL);
    desc.add_option("rx_freq", "RX center frequency in Hz", option_utils::supported_types::REAL);
    desc.add_option("rx_gain", "RX gain in dB", option_utils::supported_types::REAL, false, "0.0");
    desc.add_option("rx_ant", "RX antenna input selection", option_utils::supported_types::STRING);
    desc.add_option("rx_iq_corr", "RX iq correction in the format \"(corr_11,corr_12,corr_21,corr_22)\"", option_utils::supported_types::STRING);
    // clang-format on
}

void add_tx_1ch_options(option_utils::program_options& desc) {
    // clang-format off
    desc.add_option("tx_rate", "TX sample rate in Hz", option_utils::supported_types::REAL);
    desc.add_option("tx_freq", "TX center frequency in Hz", option_utils::supported_types::REAL);
    desc.add_option("tx_gain", "TX gain in dB", option_utils::supported_types::REAL, false, "0.0");
    desc.add_option("tx_ant", "TX antenna output selection", option_utils::supported_types::STRING);
    desc.add_option("tx_iq_bias", "TX iq bias in the format \"(bias_i,bias_q))\"", option_utils::supported_types::STRING);
    desc.add_option("tx_iq_corr", "TX iq correction in the format \"[corr_11,corr_12,corr_21,corr_22]\"", option_utils::supported_types::STRING);
    // clang-format on
}

void add_common_options(option_utils::program_options& desc) {
    // clang-format off
    desc.add_flag("help", "show help message");
    desc.add_option("config_file", "configuration file name", option_utils::supported_types::STRING);
    desc.add_option("prefix", "prefix for the output file", option_utils::supported_types::STRING, false, "test-");
    desc.add_option("suffix", "suffix for the output file", option_utils::supported_types::STRING, false, ".dat");
    desc.add_option("duration", "duration in seconds", option_utils::supported_types::REAL, false, "1.0");
    desc.add_option("clock_source", "source for frequency reference", option_utils::supported_types::STRING);
    desc.add_option("time_source", "source for time reference (host or pps)", option_utils::supported_types::STRING, false, "host");
    desc.add_option("rate", "TX/RX sample rate in Hz", option_utils::supported_types::REAL, true);
    desc.add_option("freq", "TX/RX center frequency in Hz", option_utils::supported_types::REAL, true);
    desc.add_flag("quit_on_error", "quit on errors");
    // clang-format on
}

void add_network_options(option_utils::program_options& desc) {
    // clang-format off
    desc.add_option("local_address", "IPv4 address of local interface", option_utils::supported_types::STRING, true);
    desc.add_option("device_address", "IPv4 address of device (including broadcast/multicast)", option_utils::supported_types::STRING, true);
    desc.add_option("netmask", "IPv4 netmask of local interface", option_utils::supported_types::STRING, false, "255.255.255.0");
    desc.add_option("payload_size", "maximum data packet payload size in bytes", option_utils::supported_types::INTEGER, false);
    desc.add_option("network_mtu", "network maximum UDP packet size in bytes", option_utils::supported_types::INTEGER, false, "9000");
    desc.add_option("network_send_buffer_bytes", "network transmit buffer size in bytes", option_utils::supported_types::INTEGER, false, "262144");
    desc.add_option("network_receive_buffer_bytes", "network receive buffer size in bytes", option_utils::supported_types::INTEGER, false, "8388608");
    desc.add_option("tx_data_queue_packets", "number of packets in the transmit packet queue", option_utils::supported_types::INTEGER, false, "512");
    desc.add_option("rx_data_queue_packets", "number of packets in the receive packet queue", option_utils::supported_types::INTEGER, false, "32768");
    desc.add_option("net_thread_priority", "priority to use for UDP handler threads when realtime priority is used (set to a negative number to not use realtime priority)",
            option_utils::supported_types::INTEGER, false, "1");
    desc.add_option("thread_affinity_offset", "offset in CPU number for UDP handler threads when CPU affinity is used (set to a negative number to not use CPU affinity)",
                option_utils::supported_types::INTEGER, false, "0");
    desc.add_option("network_bit_rate", "the bit rate of the network interface", option_utils::supported_types::REAL, false, "10e9");

    // clang-format on
}

int set_common_options(option_utils::parsed_options& vm, std::unique_ptr<vxsdr>& radio) {
    if (vm.count("time_source") > 0) {
        std::string ts_str = vm["time_source"].as<std::string>();
        for (unsigned i = 0; i < ts_str.size(); i++) {
            ts_str[i] = std::tolower(ts_str[i]);
        }
        if (ts_str.compare("host") == 0) {
            if (not radio->set_time_now(std::chrono::system_clock::now())) {
                std::cerr << "error in set_common_options: set_time_now" << std::endl;
            }
        } else if (ts_str.compare("pps") == 0) {
            constexpr unsigned max_host_clock_error_ms = 200;  // cannot be 500 or more!
            auto t_now                                 = std::chrono::system_clock::now();
            auto msecs                                 = std::chrono::time_point_cast<std::chrono::milliseconds>(t_now) -
                         std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::floor<std::chrono::seconds>(t_now));
            std::chrono::time_point<std::chrono::system_clock> t_set;
            if (msecs.count() < 1000 - max_host_clock_error_ms) {
                // set time at next second (i.e. ceil(t_now))
                t_set = std::chrono::ceil<std::chrono::seconds>(t_now);
            } else {
                // too close to second boundary, wait until second after next
                t_set = std::chrono::ceil<std::chrono::seconds>(t_now) + std::chrono::seconds(1);
            }
            // wait until nearly t_set, then send the command
            std::this_thread::sleep_until(t_set - std::chrono::milliseconds(max_host_clock_error_ms));
            if (not radio->set_time_next_pps(std::chrono::time_point_cast<vxsdr::duration>(t_set))) {
                std::cerr << "error in set_common_options: set_time_next_pps" << std::endl;
            }
        } else {
            std::cerr << "Error: unknown option value for --time_source: " << vm["time_source"].as<std::string>() << std::endl;
            exit(1);
        }
    }

    return 0;
}

int set_rx_1ch_options(option_utils::parsed_options& vm, std::unique_ptr<vxsdr>& radio) {
    if (vm.count("rate") > 0) {
        if (not radio->set_rx_rate(vm["rate"].as<double>())) {
            std::cerr << "error in set_rx_1ch_options: set_rx_rate" << std::endl;
        }
        if (vm.count("rx_rate") > 0) {
            std::cout << "Global option --rate overrides --rx_rate" << std::endl;
        }
    } else if (vm.count("rx_rate") > 0) {
        if (not radio->set_rx_rate(vm["rx_rate"].as<double>())) {
            std::cerr << "error in set_rx_1ch_options: set_rx_rate" << std::endl;
        }
    } else {
        std::cerr << "Please specify the global sample rate with --rate or the RX sample rate with --rx_rate" << std::endl;
        exit(1);
    }

    if (vm.count("freq") > 0) {
        if (not radio->set_rx_freq(vm["freq"].as<double>())) {
            std::cerr << "error in set_rx_1ch_options: set_rx_freq" << std::endl;
        }
        if (vm.count("rx_freq") > 0) {
            std::cout << "Global option --freq overrides --rx_freq" << std::endl;
        }
    } else if (vm.count("rx_freq") > 0) {
        if (not radio->set_rx_freq(vm["rx_freq"].as<double>())) {
            std::cerr << "error in set_rx_1ch_options: set_rx_freq" << std::endl;
        }
    } else {
        std::cerr << "Please specify the global center frequency with --freq or the RX center frequency with --rx_freq"
                  << std::endl;
        exit(1);
    }

    if (vm.count("rx_ant") > 0) {
        bool port_set = false;
        for (unsigned n = 0; n < radio->get_rx_num_ports().value_or(-1); n++) {
            if (radio->get_rx_port_name(n).value_or("") == vm["rx_ant"].as<std::string>()) {
                if (radio->set_rx_port(n)) {
                    port_set = true;
                    break;
                }
            }
        }
        if (not port_set) {
            std::cerr << "error in set_rx_1ch_options: set_rx_port" << std::endl;
        }
    }

    if (vm.count("rx_gain") > 0) {
        if (not radio->set_rx_gain(vm["rx_gain"].as<double>())) {
            std::cerr << "error in set_rx_1ch_options: set_rx_gain" << std::endl;
        }
    }

    if (vm.count("rx_iq_corr") > 0) {
        auto x = interpret_bracketed_list(vm["rx_iq_corr"].as<std::string>());
        if (x.size() != 4) {
            std::cerr << "error in set_rx_1ch_options: set_rx_iq_corr (requires 4 arguments)" << std::endl;
        } else {
            std::array<double, 4> y;
            for (int i = 0; i < 4; i++) {
                y[i] = x[i];
            }
            if (not radio->set_rx_iq_corr(y)) {
                std::cerr << "error in set_rx_1ch_options: set_rx_iq_corr" << std::endl;
            }
        }
    }

    return 0;
}

int set_tx_1ch_options(option_utils::parsed_options& vm, std::unique_ptr<vxsdr>& radio) {
    if (vm.count("rate") > 0) {
        if (not radio->set_tx_rate(vm["rate"].as<double>())) {
            std::cerr << "error in set_tx_1ch_options: set_tx_rate" << std::endl;
        }
        if (vm.count("tx_rate") > 0) {
            std::cout << "Global option --rate overrides --tx_rate" << std::endl;
        }
    } else if (vm.count("tx_rate") > 0) {
        if (not radio->set_tx_rate(vm["tx_rate"].as<double>())) {
            std::cerr << "error in set_tx_1ch_options: set_tx_rate" << std::endl;
        }
    } else {
        std::cerr << "Please specify the global sample rate with --rate or the TX sample rate with --tx_rate" << std::endl;
        exit(1);
    }

    if (vm.count("freq") > 0) {
        if (not radio->set_tx_freq(vm["freq"].as<double>())) {
            std::cerr << "error in set_tx_1ch_options: set_tx_freq" << std::endl;
        }
        if (vm.count("tx_freq") > 0) {
            std::cout << "Global option --freq overrides --tx_freq" << std::endl;
        }
    } else if (vm.count("tx_freq") > 0) {
        if (not radio->set_tx_freq(vm["tx_freq"].as<double>())) {
            std::cerr << "error in set_tx_1ch_options: set_tx_freq" << std::endl;
        }
    } else {
        std::cout << "Please specify the global center frequency with --freq or the TX center frequency with --tx_freq"
                  << std::endl;
        exit(1);
    }

    if (vm.count("tx_ant") > 0) {
        bool port_set = false;
        for (unsigned n = 0; n < radio->get_tx_num_ports().value_or(-1); n++) {
            if (radio->get_tx_port_name(n).value_or("") == vm["tx_ant"].as<std::string>()) {
                if (radio->set_tx_port(n)) {
                    port_set = true;
                    break;
                }
            }
        }
        if (not port_set) {
            std::cerr << "error in set_tx_1ch_options: set_tx_port" << std::endl;
        }
    }

    if (vm.count("tx_gain") > 0) {
        if (not radio->set_tx_gain(vm["tx_gain"].as<double>())) {
            std::cerr << "error in set_tx_1ch_options: set_tx_gain" << std::endl;
        }
    }

    if (vm.count("tx_iq_bias") > 0) {
        auto x = interpret_bracketed_list(vm["tx_iq_bias"].as<std::string>());
        if (x.size() != 2) {
            std::cerr << "error in set_tx_1ch_options: set_tx_iq_bias (requires 2 arguments)" << std::endl;
        } else {
            std::array<double, 2> y;
            for (int i = 0; i < 2; i++) {
                y[i] = x[i];
            }
            if (not radio->set_tx_iq_bias(y)) {
                std::cerr << "error in set_tx_1ch_options: set_tx_iq_bias" << std::endl;
            }
        }
    }

    if (vm.count("tx_iq_corr") > 0) {
        auto x = interpret_bracketed_list(vm["tx_iq_corr"].as<std::string>());
        if (x.size() != 4) {
            std::cerr << "error in set_tx_1ch_options: set_tx_iq_corr (requires 4 arguments)" << std::endl;
        } else {
            std::array<double, 4> y;
            for (int i = 0; i < 4; i++) {
                y[i] = x[i];
            }
            if (not radio->set_tx_iq_corr(y)) {
                std::cerr << "error in set_tx_1ch_options: set_tx_iq_corr" << std::endl;
            }
        }
    }

    return 0;
}

int set_network_options(option_utils::parsed_options& vm, std::unique_ptr<vxsdr>& radio) {
    if (vm.count("payload_size") > 0) {
        if (not radio->set_max_payload_bytes(vm["payload_size"].as<unsigned>())) {
            std::cerr << "error in set_network_options: set_max_payload_bytes" << std::endl;
        }
    }

    return 0;
}
