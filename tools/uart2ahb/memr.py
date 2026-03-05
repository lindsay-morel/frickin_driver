################################################################################
# Copyright (C) 2019-2022 MemryX Limited. All rights reserved.
################################################################################
import argparse
from lib.memx_uart2ahb import MemxUart2ahb

class RawTextAndArgumentDefaultsHelpFormatter(
  argparse.RawTextHelpFormatter,
  argparse.ArgumentDefaultsHelpFormatter):
  '''
  refer: https://stackoverflow.com/questions/61324536/python-argparse-with-argumentdefaultshelpformatter-and-rawtexthelpformatter
  '''

parser_ = argparse.ArgumentParser(
  description='MemryX Cascade firmware download helper',
  formatter_class=RawTextAndArgumentDefaultsHelpFormatter,
  epilog='''examples:
  $ python3 {name} -p /dev/ttyACM1
  $ python3 {name} -b 921600
  $ python3 {name} -f ../../firmware/cascade.raw
  $ python3 {name} -p /dev/ttyACM1 -b 921600 -f ../../firmware/cascade.raw
  '''.format(name=__file__))

parser_.add_argument('-p', '--com_port',
  default='/dev/ttyUSB1',
  type=str,
  help='''COM port used in UART2AHB connection, could be something like \'COM3\'
  in Windows or \'/dev/ttyACM1\' in Ubuntu''')

parser_.add_argument('-b', '--baudrate',
  default=230400,
  type=int,
  help='''COM port baud-rate used in UART2AHB connection. By default should be
    fixed to 921600 based on reference clock 25M''')

def auto_int(x):
    return int(x, 16)

parser_.add_argument('-a', '--address',
  default=0x4004_0000,
  type=auto_int,
  help='''memory dump address.''')

parser_.add_argument('-l', '--len',
  default=4,
  type=auto_int,
  help='''memory dump address.''')


def main(args: argparse.Namespace) -> None:
  '''MemryX Cascade only. Use UART2AHB to download CPU firmware to SRAM.
  '''
  port = MemxUart2ahb(); data = bytearray()
  port.open(com_port=args.com_port, baudrate=args.baudrate)
  addr = args.address;
  
  if args.len == 4:
    print(format(addr, '#010x'),":", format(port.read(addr), '#010x'))
  else:
    for step in range(0,args.len,16):
      print(format(addr+step+ 0, '#010x'),":", format(port.read(addr+step+ 0), '#010x'), format(port.read(addr+step+ 4), '#010x'), format(port.read(addr+step+ 8), '#010x'), format(port.read(addr+step+12), '#010x'))

  port.close()
  pass

if __name__ == '__main__':
  main(parser_.parse_args()) # parse from sys.argv by default

