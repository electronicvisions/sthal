import pandas

# Connector: (analog, channel, tigger, FPGA slave/top, FPGA master/bottom)
# Extracted from:
CONNECTORS = {
    'P1': (
        (0, 7, 0, 23, 47),
        (0, 4, 0, 22, 46),
        (0, 2, 0, 21, 45),
        (0, 0, 0, 20, 44),
        (0, 1, 1, 19, 43),
        (0, 3, 1, 18, 42),
        (0, 5, 1, 17, 41),
        (0, 6, 1, 16, 40),
    ),
    'P2': (
        (0, 7, 0, 15, 39),
        (0, 4, 0, 14, 38),
        (0, 2, 0, 13, 37),
        (0, 0, 0, 12, 36),
        (0, 1, 1, 11, 35),
        (0, 3, 1, 10, 34),
        (0, 5, 1, 9, 33),
        (0, 6, 1, 8, 32),
    ),
    'P3': (
        (0, 7, 0, 7, 31),
        (0, 4, 0, 6, 30),
        (0, 2, 0, 5, 29),
        (0, 0, 0, 4, 28),
        (0, 1, 1, 3, 27),
        (0, 3, 1, 2, 26),
        (0, 5, 1, 1, 25),
        (0, 6, 1, 0, 24),
    ),
    'P4': (
        (1, 7, 0, 23, 47),
        (1, 4, 0, 22, 46),
        (1, 2, 0, 21, 45),
        (1, 0, 0, 20, 44),
        (1, 1, 1, 19, 43),
        (1, 3, 1, 18, 42),
        (1, 5, 1, 17, 41),
        (1, 6, 1, 16, 40),
    ),
    'P5': (
        (1, 7, 0, 15, 39),
        (1, 4, 0, 14, 38),
        (1, 2, 0, 13, 37),
        (1, 0, 0, 12, 36),
        (1, 1, 1, 11, 35),
        (1, 3, 1, 10, 34),
        (1, 5, 1, 9, 33),
        (1, 6, 1, 8, 32),
    ),
    'P6': (
        (1, 7, 0, 7, 31),
        (1, 4, 0, 6, 30),
        (1, 2, 0, 5, 29),
        (1, 0, 0, 4, 28),
        (1, 1, 1, 3, 27),
        (1, 3, 1, 2, 26),
        (1, 5, 1, 1, 25),
        (1, 6, 1, 0, 24),
    ),
}

def generate_data(ip, adcs, anab):
    columns = ['adc', 'ip', 'port', 'fpga', 'channel', 'analog', 'trigger']

    data = []
    for adc, port, connector in adcs:
        for analog, channel, trigger, fpga_top, fpga_bottom in CONNECTORS[connector]:
            fpga = fpga_top if anab == "top" else fpga_bottom
            data.append((
                adc, ip, port, fpga, channel, analog, trigger))
    return pandas.DataFrame(data, columns=columns)

def convert( aString ):
    if aString.startswith("0x") or aString.startswith("0X"):
        return int(aString,16)
    elif aString.startswith("0"):
        return int(aString,8)
    else:
        return int(aString)

def output_yaml(data):
    TEMPLATE = """  - fpga: {fpga}
    analog: {analog}
    adc: {adc}
    remote_ip: {ip}
    remote_port: {port}
    channel: {channel}
    trigger: {trigger}"""

    out = []
    data = data.sort_values(by=['fpga', 'analog'])
    for idx, row in data.iterrows():
        out.append(TEMPLATE.format(**row))
    return "\n".join(out)

def main():
    import argparse

    parser= argparse.ArgumentParser()
    parser.add_argument('--P1', nargs=2, help="Serial of ADC and port on server (e.g. B201249 0xADC0) on connector P1")
    parser.add_argument('--P2', nargs=2, help="Serial of ADC and port on server (e.g. B201248 0xADC1) on connector P2")
    parser.add_argument('--P3', nargs=2, help="Serial of ADC and port on server (e.g. B201247 0xADC2) on connector P3")
    parser.add_argument('--P4', nargs=2, help="Serial of ADC and port on server (e.g. B201246 0xADC3) on connector P4")
    parser.add_argument('--P5', nargs=2, help="Serial of ADC and port on server (e.g. B201245 0xADC4) on connector P5")
    parser.add_argument('--P6', nargs=2, help="Serial of ADC and port on server (e.g. B201244 0xADC5) on connector P6")
    parser.add_argument('--ip', required=True, help="IP of ADC remote server")
    parser.add_argument("--anab", choices=("top", "bottom"))

    args = parser.parse_args()

    ip = args.ip
    anab = args.anab

    adcs = []
    for connector in ('P1', 'P2', 'P3', 'P4', 'P5', 'P6'):
        arg = getattr(args, connector, None)
        if arg is not None:
            adcs.append((str(arg[0]), convert(arg[1]), connector))

    data = generate_data(ip, adcs, anab)
    print output_yaml(data)

if __name__ == '__main__':
    main()
