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
  default='COM3',
  type=str,
  help='''COM port used in UART2AHB connection, could be something like \'COM3\'
  in Windows or \'/dev/ttyACM1\' in Ubuntu''')

parser_.add_argument('-b', '--baudrate',
  default=230400,
  type=int,
  help='''COM port baud-rate used in UART2AHB connection. By default should be
    fixed to 230400 based on reference clock 25M''')

parser_.add_argument('-f', '--firmware',
  default='cascade.bin',
  type=str,
  help='''CPU firmware file path, which should be named as something like
    \'cascade.raw\' w/o CRC or \'cascade.bin\' w/ CRC by default.''')

parser_.add_argument('-w', '--wrloader',
  default='uart_WrLoader.raw',
  type=str,
  help='''uart write loader ./uart_WrLoader.raw''')

def main(args: argparse.Namespace) -> None:
  '''MemryX Cascade only. Use UART2AHB to download CPU firmware to SRAM.
  '''
  port = MemxUart2ahb(); data = bytearray(); wrldr = bytearray()
  port.open(com_port=args.com_port, baudrate=args.baudrate)

  with open(args.firmware, 'rb') as fp:
    port.write(0x2000_0204, 0x01) # chip reset
    data = fp.read()

  with open(args.wrloader, 'rb') as ldr:
    wrldr = ldr.read()

  if len(data) > 0:
    
    port.write(0x400C_0000, 0x55aa55aa)
    if not port.read(0x400C_0000) == 0x55aa55aa:
      print("Failed to access SRAM.")
    else:
      print("Start to download binary to address 0xA0000:", args.firmware)
      port.write(0x400C_0000, data, len(data), 1)
      print(len(data));
      
      print("Start to download wrloader:", args.wrloader)
      port.write(0x2000_0024, len(wrldr))
      port.write(0x4004_0000, wrldr, len(wrldr), 1)
      port.write(0x2000_0B04, 0x11) # setting UART2AHB boot complete regsiter

  port.close()
  pass

if __name__ == '__main__':
  main(parser_.parse_args()) # parse from sys.argv by default

