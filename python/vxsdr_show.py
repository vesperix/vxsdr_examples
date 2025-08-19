# Copyright (c) 2024 Vesperix Corporation
# SPDX-License-Identifier: GPL-3.0-or-later

# Provides a simple example getting VXSDR device info in Python

import sys
import argparse
import datetime

import vxsdr_py


def version_string(n):
    major = n // 10000
    minor = (n // 100) % 100
    patch = n % 100
    return f"{major:d}.{minor:d}.{patch:d}"


def wire_format_string(n):
    length = n % 256
    if (n & 0x100) != 0:
        format = "cplx"
    else:
        format = "real"
    if (n & 0x200) != 0:
        type = "flt"
    else:
        type = "int"
    return f"{format:s} {type:s} {length:d}"


def get_hello_info(radio, lines=[]):
    hresp = radio.hello()
    lines.append("device information:")
    lines.append("   device type           {:16d}".format(hresp[0]))
    lines.append("   FPGA  version         {:>16s}".format(version_string(hresp[1])))
    lines.append("   MCU code_version      {:>16s}".format(version_string(hresp[2])))
    lines.append("   unique id             {:16d}".format(hresp[3]))
    lines.append("   packet version        {:>16s}".format(version_string(hresp[4])))
    lines.append("   wire format           {:>16s}".format(wire_format_string(hresp[5])))
    lines.append("   sample granularity    {:16d}".format((hresp[5] & 0xFF000000) >> 24))
    lines.append("   number of subdevices  {:16d}".format(hresp[6]))
    lines.append("   max payload bytes     {:16d}".format(hresp[7]))
    return lines


def get_sensor_info(radio, lines=[]):
    lines.append(f"sensor readings:")
    for i in range(radio.get_num_sensors()):
        name = radio.get_sensor_name(i)
        reading = radio.get_sensor_reading(i)
        lines.append(f"   {name:20s} {reading:16.3f}")
    return lines


def get_tx_settings(radio, lines=[]):
    lines.append(f"tx settings:")
    gain_limits = radio.get_tx_gain_range()
    lines.append("   minimum transmit gain {:16.2f}".format(gain_limits[0]))
    lines.append("   maximum transmit gain {:16.2f}".format(gain_limits[1]))
    lines.append("   current transmit gain {:16.2f}".format(radio.get_tx_gain()))
    rate_limits = radio.get_tx_rate_range()
    lines.append("   minimum transmit rate {:16.3e}".format(rate_limits[0]))
    lines.append("   maximum transmit rate {:16.3e}".format(rate_limits[1]))
    lines.append("   current transmit rate {:16.3e}".format(radio.get_rx_rate()))
    lines.append("   available transmit ports:")
    for i in range(radio.get_tx_num_ports()):
        lines.append("                         {:>16s}".format(radio.get_tx_port_name(i)))
    tx_port_name = radio.get_tx_port_name(radio.get_tx_port())
    lines.append("   current transmit port {:>16s}".format(tx_port_name))
    lines.append("   transmit if           {:16.3e}".format(radio.get_tx_if_freq()))
    return lines


def get_rx_settings(radio, lines=[]):
    lines.append(f"rx settings:")
    gain_limits = radio.get_rx_gain_range()
    lines.append("   minimum receive gain  {:16.2f}".format(gain_limits[0]))
    lines.append("   maximum receive gain  {:16.2f}".format(gain_limits[1]))
    lines.append("   current receive gain  {:16.2f}".format(radio.get_rx_gain()))
    rate_limits = radio.get_rx_rate_range()
    lines.append("   minimum receive rate  {:16.3e}".format(rate_limits[0]))
    lines.append("   maximum receive rate  {:16.3e}".format(rate_limits[1]))
    lines.append("   current receive rate  {:16.3e}".format(radio.get_rx_rate()))
    lines.append("   available receive ports:")
    for i in range(radio.get_rx_num_ports()):
        lines.append("                         {:>16s}".format(radio.get_rx_port_name(i)))
    rx_port_name = radio.get_rx_port_name(radio.get_rx_port())
    lines.append("   current receive port  {:>16s}".format(rx_port_name))
    lines.append("   receive if            {:16.3e}".format(radio.get_rx_if_freq()))
    return lines


def get_all_info(radio, lines=[]):
    lines = get_hello_info(radio, lines)
    lines = get_tx_settings(radio, lines)
    lines = get_rx_settings(radio, lines)
    lines = get_sensor_info(radio, lines)
    return lines


if __name__ == "__main__":
    parser = argparse.ArgumentParser(prog=sys.argv[0], description="show information about a VXSDR")

    parser.add_argument("-L", "--local_ip", default=None, help="the local IP address used to communicate with the VXSDR")
    parser.add_argument("-D", "--device_ip", default=None, help="the IP address of the VXSDR")

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

    radio = vxsdr_py.vxsdr_py(my_config)

    for line in get_all_info(radio):
        print(line)
