// Copyright (c) 2024 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

// Provides a simple example of looped transmit from a file

#include <arpa/inet.h>
#include <chrono>
#include <cmath>
#include <complex>
#include <csignal>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <thread>

#include <vxsdr.hpp>

#include "host_radio_options.hpp"
#include "utility.hpp"

using namespace std::chrono_literals;

int main(int argc, char* argv[]) {
    try {
        std::cout << argv[0] << " started" << std::endl;

        // set up options and read from command line and/or configuration file
        option_utils::program_options desc("vxsdr_tx_loop_file", "test loop transmit using data from a file");

        add_common_options(desc);
        add_network_options(desc);
        add_tx_1ch_options(desc);

        desc.add_option("tx_waveform_file", "file containing the transmit waveform", option_utils::supported_types::STRING, true);
        desc.add_option("pri", "pulse repetition interval in seconds (zero for continuous loop)",
                        option_utils::supported_types::REAL, false, "0.0");

        auto vm = desc.parse(argc, argv);

        // get the duration and pri from the command line
        auto duration_sec = vm["duration"].as<double>();
        if (duration_sec <= 0.0) {
            std::cerr << "duration must be positive" << std::endl;
            return 1;
        }

        auto pri_sec = vm["pri"].as<double>();
        if (pri_sec < 0.0) {
            std::cerr << "pri must be nonnegative" << std::endl;
            return 1;
        }

        // find number of repetitions
        size_t n_pulses = 0;
        if (pri_sec > 0.0) {
            n_pulses = std::llround(duration_sec / pri_sec);
        }

        std::vector<std::complex<int16_t>> tx_wf;

        // check that the given file exists and read it
        if (read_cplx_16(vm["tx_waveform_file"].as<std::string>(), tx_wf) == 0) {
            std::cerr << "unable to read tx waveform file " << vm["tx_waveform_file"].as<std::string>() << std::endl;
            return 1;
        }
        std::cout << "loaded tx waveform file " << vm["tx_waveform_file"].as<std::string>() << std::endl;

        size_t n_samples = tx_wf.size();
        if (n_samples == 0) {
            std::cerr << "tx waveform file contains " << n_samples << " samples" << std::endl;
            return 1;
        }
        std::cout << "tx waveform file contains " << n_samples << " samples" << std::endl;

        // get radio settings from command line arguments
        uint32_t local_addr  = ntohl(inet_addr(vm["local_address"].as<std::string>().c_str()));
        uint32_t device_addr = ntohl(inet_addr(vm["device_address"].as<std::string>().c_str()));

        std::map<std::string, int64_t> settings = {
            {"udp_transport:local_address", local_addr},
            {"udp_transport:device_address", device_addr},
            {"tx_data_queue_packets", vm["tx_data_queue_packets"].as<unsigned>()},
            {"rx_data_queue_packets", vm["rx_data_queue_packets"].as<unsigned>()},
            {"network_send_buffer_bytes", vm["network_send_buffer_bytes"].as<unsigned>()},
            {"network_receive_buffer_bytes", vm["network_receive_buffer_bytes"].as<unsigned>()},
            {"net_thread_priority", vm["net_thread_priority"].as<int>()},
            {"thread_affinity_offset", vm["thread_affinity_offset"].as<int>()}};

        if (vm.count("network_mtu") > 0) {
            settings["udp_data_transport:mtu_bytes"] = vm["network_mtu"].as<unsigned>();
        }

        // set up the radio
        auto radio = std::make_unique<vxsdr>(settings);

        set_common_options(vm, radio);
        set_network_options(vm, radio);
        set_tx_1ch_options(vm, radio);

        // the radio is now set up, so we can query it for settings
        double waveform_duration = n_samples / radio->get_tx_rate().value_or(-1);
        if (pri_sec > 0 and waveform_duration > pri_sec) {
            std::cerr << "duration of waveform is longer than pri (" << waveform_duration << ", " << pri_sec << "), check tx_rate"
                      << std::endl;
            return 1;
        }

        // check that the looped waveform will fit in the FPGA buffer
        size_t tx_buffer_size_bytes = 0;
        auto bsize                  = radio->get_buffer_info();
        if (bsize.has_value()) {
            tx_buffer_size_bytes = bsize->at(1);
        } else {
            std::cerr << "unable to get buffer info" << std::endl;
            return 1;
        }

        size_t tx_buffer_samps = tx_buffer_size_bytes / sizeof(vxsdr::wire_sample);
        if (tx_buffer_samps < n_samples) {
            std::cerr << "file data will not fit in tx buffer (" << tx_buffer_samps << " available, " << n_samples << " needed"
                      << std::endl;
            return 1;
        }

        // the buffer ram bus width sets the sample granularity of the radio when looping
        // if there is dead time between loops, this doesn't matter, but if samples are looped end-to-end
        // (pri == 0), the waveform looped must match the sample granularity, or gaps will occur
        auto hello_info = radio->hello();
        if (hello_info.has_value()) {
            uint32_t wire_format = hello_info->at(5);
            auto granularity     = radio->compute_sample_granularity(wire_format);
            if (pri_sec == 0.0 and n_samples % granularity != 0) {
                std::cerr << "waveform length does not match granularity -- gaps will occur" << std::endl;
            }
        }

        // start and stop times are synched to the radio clock
        // command line options control whether the radio clock is set by the host clock or from the pps
        // setting from pps uses date, hour, minute, and second from host clock, which must be within +/- 100 ms of pps
        auto t1 = radio->get_time_now();
        if (t1.has_value()) {
            std::cout << "radio time: " << format_time(t1.value()) << std::endl;
        } else {
            std::cerr << "unable to get radio time" << std::endl;
            return 1;
        }

        std::cout << "using frequency " << radio->get_tx_freq().value_or(-1) << " Hz" << std::endl;
        std::cout << "using rate      " << radio->get_tx_rate().value_or(-1) << " samples/s" << std::endl;
        std::cout << "using tx_gain   " << radio->get_tx_gain().value_or(-1) << " dB" << std::endl;
        std::cout << "using pri       " << pri_sec << " s" << std::endl;
        std::cout << "using duration  " << duration_sec << " s" << std::endl;

        // start 1-2 seconds in the future
        auto t_start = std::chrono::ceil<std::chrono::seconds>(radio->get_time_now().value()) + 1s;
        std::cout << "start time: " << format_time(t_start) << std::endl;

        // send the data
        auto n_sent = radio->put_tx_data(tx_wf);
        if (n_sent != n_samples) {
            std::cerr << "error sending waveform data" << std::endl;
        }

        // round pri to nearest nanosecond
        vxsdr::duration pri = std::chrono::duration(std::chrono::nanoseconds(std::llround(1e9 * pri_sec)));
        if (not radio->tx_loop(t_start, n_samples, pri, n_pulses)) {
            std::cerr << "tx_loop() failed" << std::endl;
            return 1;
        }

        vxsdr::duration duration = std::chrono::duration(std::chrono::milliseconds(std::llround(1e3 * duration_sec)));
        std::this_thread::sleep_until(t_start + duration + 100ms);

        std::cout << "transmit complete" << std::endl;
    } catch (std::exception& e) {
        std::cerr << "exception caught: " << e.what() << std::endl;
        return 3;
    }
}
