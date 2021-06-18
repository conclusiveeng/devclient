#! env/bin/python3
# coding: utf8

from yaml.dumper import BaseDumper


# This tool automaticity generates YAML EEPROM configuration files based
# on template file with initial EEPROM values.
# On each generated file mac address is computed and serial number.
#
# Example: venv/bin/python3 gen-eeprom-yaml.py -n 100 -f ../kstr-sama5d27-rev3.0.yaml


import argparse
from yaml import load, dump
try:
    from yaml import CLoader as Loader, CDumper as Dumper
except ImportError:
    from yaml import Loader, Dumper
from netaddr import EUI

def parse_cli():
    my_parser = argparse.ArgumentParser()
    my_parser.add_argument('-n',
                           '--num',
                           action='store',
                           type=int,
                           dest='number_of_boards',
                           required=True)

    my_parser.add_argument('-f',
                           '--file',
                           action='store',
                           type=argparse.FileType('r', encoding='utf-8'),
                           dest='template_file_name',
                           required=True)

    my_parser.add_argument("-l", "--list", action="store_true")                           

    args = my_parser.parse_args()
    arguments = vars(args)
    return arguments

def update_yaml(input_yaml, eeprom_address, serial, mac):
    yaml_out = input_yaml
    yaml_out['address'] = eeprom_address
    for elem in yaml_out['eeprom']:
        if elem['name'] == 'serial-number':
            elem['value'] = serial
        if elem['name'] == 'mac-address':
            elem['value'] = mac
    return dump(yaml_out, encoding=('utf-8'))

def get_start_serial(input_yaml):
    for elem in input_yaml['eeprom']:
        if elem['name'] == 'serial-number':
            return elem['value']

def get_start_mac(input_yaml):
    for elem in input_yaml['eeprom']:
        if elem['name'] == 'mac-address':
            return elem['value'].replace(':', '-')

def get_num_mac_per_board(input_yaml):
    for elem in input_yaml['eeprom']:
        if elem['name'] == 'number-mac':
            return elem['value']

def get_eeprom_address(input_yaml):
        return hex(input_yaml['address'])

def save_file(file_name, yaml):
    full_file_name = f'{file_name}.yml'
    with open(full_file_name, 'wb') as f:
        f.write(yaml)

if __name__ == "__main__":
    args = parse_cli()
    input_file = load(args['template_file_name'], Loader=Loader)

    start_serial = get_start_serial(input_file)
    start_mac = EUI(get_start_mac(input_file)).value
    mac_per_board = get_num_mac_per_board(input_file)
    # EEPROM address must be read and resaved as the parser changes it to decimal
    eeprom_address = get_eeprom_address(input_file)

    for i in range(args['number_of_boards']):
        serial =  start_serial + i
        serial_number_text = f"0x{serial:08x}"

        mac_address_text = str(EUI(start_mac+i*mac_per_board)).replace('-', ':')

        yaml_out = update_yaml(input_file, eeprom_address, serial_number_text, mac_address_text)
        save_file(f"gen-board-{i}-{serial_number_text}", yaml_out)
        if args['list']:
            print(f"{i}: {serial_number_text}")
