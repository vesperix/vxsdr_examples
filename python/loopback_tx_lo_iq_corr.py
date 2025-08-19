# Copyright (c) 2024 Vesperix Corporation
# SPDX-License-Identifier: GPL-3.0-or-later

# Uses a loopback connection to measure values for LO bias (LO feedthrough)
# and IQ corrections on a VXSDR radio. Numpy is required.

import os
import sys
import time
import datetime
import argparse
import math

import numpy as np

import vxsdr_py


def clip(x, xmin, xmax):
    if x < xmin:
        return xmin
    if x > xmax:
        return xmax
    return x


def rx_measure_power(radio, freq, n_data):
    n_skip = 1024  # skip any startup transients
    rx_data = np.zeros((n_data + n_skip,), dtype=np.complex64)
    radio.set_rx_freq(freq)
    radio.rx_start(datetime.datetime.now(tz=datetime.timezone.utc), n_data + n_skip)
    radio.get_rx_data(rx_data, n_data + n_skip)
    d = rx_data[n_skip:]
    if np.max(np.abs(d) >= 0.8):
        print("warning: near clipping on rx", file=sys.stderr)
    data_mean = np.mean(d)
    pwr = np.mean(np.abs(d - data_mean) ** 2)
    return 10 * math.log10(pwr)


def rx_measure_power_agc(radio, freq, rx_ndata):
    gain_inc = 20
    pwr_fs = 10 * math.log10(math.sqrt(0.5))
    pwr = rx_measure_power(radio, freq, rx_ndata)

    if pwr_fs - pwr > gain_inc + 3:
        current_gain = radio.get_rx_gain()
        radio.set_rx_gain(current_gain + gain_inc)
        pwr = rx_measure_power(radio, freq, rx_ndata) - gain_inc
        radio.set_rx_gain(current_gain)

    return pwr


if __name__ == "__main__":
    parser = argparse.ArgumentParser(prog=sys.argv[0], description="Measure TX LO and IQ corrections using loopback")

    parser.add_argument("-L", "--local_ip", default=None, help="the local IP address used to communicate with the VXSDR")
    parser.add_argument("-D", "--device_ip", default=None, help="the IP address of the VXSDR")

    parser.add_argument("-O", "--output_base", default="tx_lo_iq_corr", help="the output file base name (without extension)")
    parser.add_argument("-H", "--add_header", action="store_true", default=False, help="include a header in the output file with device info")
    parser.add_argument("-C", "--comment_delimiter", default="%", help="characters to place at start of header lines in output file")
    parser.add_argument("-X", "--add_unique_id", action="store_true", default=False, help="append device id to output file basename")
    parser.add_argument("-Y", "--add_timestamp", action="store_true", default=False, help="append date and time to output file basename")

    parser.add_argument("-S", "--minimum_frequency", type=float, default=5.0e9, help="the minimum frequency to measure")
    parser.add_argument("-E", "--maximum_frequency", type=float, default=22.0e9, help="the maximum frequency to measure")
    parser.add_argument("-T", "--frequency_increment", type=float, default=100.0e6, help="the measurement frequency increment")
    parser.add_argument("-N", "--data_length", type=int, default=20480, help="the length of data received")

    parser.add_argument("-G", "--tx_gain", type=float, default=0.0, help="the TX gain for the measurement")
    parser.add_argument("-R", "--rate", type=float, default=10.0e6, help="the TX and RX sample rate for the measurement")
    parser.add_argument("-Q", "--tx_port", type=int, default=0, help="the TX port for the measurement")

    args = parser.parse_args()

    my_config = {}
    if args.local_ip is not None and args.device_ip is not None:
        # using UDP transport
        import ipaddress

        local_addr = ipaddress.ip_address(args.local_ip)
        device_addr = ipaddress.ip_address(args.device_ip)
        my_config["udp_transport:local_address"] = local_addr
        my_config["udp_transport:device_address"] = device_addr
    elif args.local_ip is None and args.device_ip is None:
        # using PCIE transport
        my_config["data_transport"] = int(vxsdr_py.transport_type.PCIE)
        my_config["command_transport"] = int(vxsdr_py.transport_type.PCIE)
    else:
        print("unable to determine VXSDR transport: specify both local and device addresses for UDP, neither for PCIe")
        exit()

    meas_delay = 0.1
    rate = args.rate
    tx_gain = args.tx_gain
    rx_gain = -10.0

    f_min = args.minimum_frequency
    f_max = args.maximum_frequency
    f_inc = args.frequency_increment

    radio = vxsdr_py.vxsdr_py(my_config)
    radio.set_time_now(datetime.datetime.now(tz=datetime.timezone.utc))

    radio.set_tx_gain(tx_gain)
    radio.set_rx_gain(rx_gain)
    radio.set_tx_rate(rate)
    radio.set_rx_rate(rate)
    radio.set_tx_port(args.tx_port)

    if_freq = radio.get_tx_if_freq()

    lo_offset = if_freq
    iq_offset = 2 * if_freq

    tx_offset = 0
    # move signal away from DC on RX
    rx_offset = rate / 8

    tx_ndata = 20480
    t_tx = np.arange(tx_ndata, dtype=np.float32) / rate
    tx_data = 0.5 * np.exp(2j * math.pi * tx_offset * t_tx, dtype=np.complex64)

    rx_ndata = args.data_length
    t_rx = np.arange(rx_ndata, dtype=np.float32) / rate
    w = np.hanning(rx_ndata)
    demod_data = np.multiply(w, np.exp(-2j * math.pi * rx_offset * t_rx), dtype=np.complex64)

    base = args.output_base

    if args.add_unique_id:
        hresp = radio.hello()
        base = base + "_{:d}".format(hresp[3])

    if args.add_timestamp:
        timestr = datetime.datetime.now().astimezone().strftime("%Y-%m-%d-%H.%M.%S")
        base = base + "_{:s}".format(timestr)

    outfile_name = base + ".txt"

    if os.path.isfile(outfile_name):
        try:
            input("output file {:s} exists -- return to overwrite, ctrl-C to quit".format(outfile_name))
        except KeyboardInterrupt:
            sys.exit()

    save_data = False
    t0 = time.time()

    with open(outfile_name, "w") as outfile:

        if args.add_header:
            delimiter = args.comment_delimiter
            print("{:s} measurement data and time: {:s}".format(delimiter, datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")))
            print("{:s} input arguments:".format(delimiter))
            print("{:s} input arguments:".format(delimiter), file=outfile)
            for k, v in sorted(vars(args).items()):
                print("{0:s}   {1:24s} {2}".format(delimiter, k, v))
                print("{0:s}   {1:24s} {2}".format(delimiter, k, v), file=outfile)

            lgd = "{:s}    F (GHz)   Bias1      Bias2    LO uncorr LO corr    IQ1        IQ2        IQ3        IQ4     IQ uncorr IQ corr".format(
                delimiter
            )
            print(lgd)
            print(lgd, file=outfile)

        for tx_freq in np.arange(f_min, f_max + 1.0, f_inc):

            radio.set_tx_freq(tx_freq)
            radio.set_tx_iq_bias((0.0, 0.0))

            start_lo = rx_measure_power_agc(radio, tx_freq - lo_offset + rx_offset, rx_ndata)

            amax = 1.0
            amin = -1.0
            bmax = 1.0
            bmin = -1.0

            n_step = 5
            lo_tol = 5e-6
            iq_tol = 5e-6

            a_lo = []
            b_lo = []
            v_lo = []

            lo_best = radio.get_tx_iq_bias()
            best_lo = start_lo

            # uses very simple successive linear searches to find an optimum
            for its in range(15):
                for a in np.linspace(amin, amax, n_step):
                    radio.set_tx_iq_bias((a, lo_best[1]))

                    pwr_dbm = rx_measure_power_agc(radio, tx_freq - lo_offset + rx_offset, rx_ndata)

                    a_lo.append(a)
                    b_lo.append(lo_best[1])
                    v_lo.append(pwr_dbm)

                    if pwr_dbm < best_lo:
                        lo_best = radio.get_tx_iq_bias()
                        best_lo = pwr_dbm

                arng = amax - amin
                amax = lo_best[0] + arng / (n_step - 1)
                amin = lo_best[0] - arng / (n_step - 1)

                amax = clip(amax, -1.0, 1.0)
                amin = clip(amin, -1.0, 1.0)

                for b in np.linspace(bmin, bmax, n_step):
                    radio.set_tx_iq_bias((lo_best[0], b))

                    pwr_dbm = rx_measure_power_agc(radio, tx_freq - lo_offset + rx_offset, rx_ndata)

                    a_lo.append(lo_best[0])
                    b_lo.append(b)
                    v_lo.append(pwr_dbm)

                    if pwr_dbm < best_lo:
                        lo_best = radio.get_tx_iq_bias()
                        best_lo = pwr_dbm

                brng = bmax - bmin
                bmax = lo_best[1] + brng / (n_step - 1)
                bmin = lo_best[1] - brng / (n_step - 1)

                bmax = clip(bmax, -1.0, 1.0)
                bmin = clip(bmin, -1.0, 1.0)

                if (amax - amin) <= lo_tol and (bmax - bmin) <= lo_tol:
                    break

            radio.set_tx_iq_bias(lo_best)

            corr = (1.0, 0.0, 0.0, 1.0)

            radio.set_tx_iq_corr(corr)

            radio.tx_loop(datetime.datetime.now(tz=datetime.timezone.utc) + datetime.timedelta(milliseconds=50), rx_ndata, 0.0, 0)
            radio.put_tx_data(tx_data)
            time.sleep(0.1)

            pwr_carrier = rx_measure_power_agc(radio, tx_freq + rx_offset, rx_ndata)
            pwr_image = rx_measure_power_agc(radio, tx_freq - iq_offset + rx_offset, rx_ndata)
            start_iq = pwr_image - pwr_carrier

            alimhi = 1.9990
            alimlo = 0.5
            blimhi = 0.49975
            blimlo = -0.5

            amax = alimhi
            amin = alimlo
            bmax = blimhi
            bmin = blimlo

            n_step = 5
            iq_a_tol = 1 / 2 ** 11
            iq_b_tol = 1 / 2 ** 13

            a_iq = []
            b_iq = []
            v_iq = []

            iq_best = radio.get_tx_iq_corr()
            best_iq = start_iq

            for its in range(15):
                if (amax - amin) > iq_a_tol:
                    n_step_a = n_step
                else:
                    n_step_a = 1
                for a in np.linspace(amin, amax, n_step_a):
                    radio.set_tx_iq_corr((a, 0.0, iq_best[2], 1.0))

                    pwr_carrier = rx_measure_power_agc(radio, tx_freq + rx_offset, rx_ndata)
                    pwr_image = rx_measure_power_agc(radio, tx_freq - iq_offset + rx_offset, rx_ndata)
                    pwr_dbm = pwr_image - pwr_carrier

                    a_iq.append(a)
                    b_iq.append(iq_best[2])
                    v_iq.append(pwr_dbm)

                    if pwr_dbm < best_iq:
                        iq_best = radio.get_tx_iq_corr()
                        best_iq = pwr_dbm

                if n_step_a > 1:
                    arng = amax - amin
                    amax = iq_best[0] + arng / (n_step - 1)
                    amin = iq_best[0] - arng / (n_step - 1)
                    amax = clip(amax, alimlo, alimhi)
                    amin = clip(amin, alimlo, alimhi)

                if (bmax - bmin) > iq_b_tol:
                    n_step_b = n_step
                else:
                    n_step_b = 1
                for b in np.linspace(bmin, bmax, n_step_b):
                    radio.set_tx_iq_corr((iq_best[0], 0.0, b, 1.0))

                    pwr_carrier = rx_measure_power_agc(radio, tx_freq + rx_offset, rx_ndata)
                    pwr_image = rx_measure_power_agc(radio, tx_freq - iq_offset + rx_offset, rx_ndata)
                    pwr_dbm = pwr_image - pwr_carrier

                    a_iq.append(iq_best[0])
                    b_iq.append(b)
                    v_iq.append(pwr_dbm)

                    if pwr_dbm < best_iq:
                        iq_best = radio.get_tx_iq_corr()
                        best_iq = pwr_dbm

                if n_step_b > 1:
                    brng = bmax - bmin
                    bmax = iq_best[2] + brng / (n_step - 1)
                    bmin = iq_best[2] - brng / (n_step - 1)
                    bmax = clip(bmax, blimlo, blimhi)
                    bmin = clip(bmin, blimlo, blimhi)

                if (amax - amin) <= iq_tol and (bmax - bmin) <= iq_tol:
                    break

            radio.tx_stop()

            outstr = " {:10.3f} {:10.6f} {:10.6f} {:8.2f} {:8.2f} {:10.6f} {:10.6f} {:10.6f} {:10.6f} {:8.2f} {:8.2f}".format(
                1e-9 * tx_freq, lo_best[0], lo_best[1], start_lo, best_lo, iq_best[0], iq_best[1], iq_best[2], iq_best[3], start_iq, best_iq
            )
            print(outstr)
            print(outstr, file=outfile)

    radio.tx_stop()
    t1 = time.time()

    print("Done. Elapsed time = {:.1f} seconds".format(t1 - t0))
