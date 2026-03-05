################################################################################
# Copyright (C) 2019-2022 MemryX Limited. All rights reserved.
################################################################################
import threading, serial
from typing import Union

class MemxUart2ahb:
  '''This class provides utility methods to access MemryX device through debug
    UART port with proprietary UART2AHB protocol implemented.
  '''
  def __init__(self) -> None:
    self.lock = threading.Lock() # mutex
    self.ser = serial.Serial() # UART port

  def __del__(self) -> None:
    self.close()

  def __write(self, address: int, data: bytearray, length: int, increment: int) -> None:
    '''Writes burst data with given byte length to device. Based on UART2AHB,
      data should always be word (4 bytes) aligned with maximum transfer size
      limited to 256 words. This method will automatically split data whose
      length is larger than 256 words into multiple transfers. 
      ---
      address: int
        Target bus address which should always be aligned to 4 bytes
      data: bytearray
        Data byte buffer to be transferred
      length: int
        Data byte length to be transferred
      increment: int
        Post-write-increase-address-4-bytes enable
      ---
      Returns None
    '''
    address &= 0xffffffff; length = (length+3) & ~0x3 # ceil to 4 bytes
    chunk_size = 0x400 # upperbound = 256 words = 1024 btyes, 0-indexing
    data = data + bytearray(length-len(data)) if len(data) != length else data # add dummy padding
    if not self.ser.is_open:
      raise RuntimeError('Method \'open\' should be called in advance to setup port connection.')
    try:
      address |= ((increment<<1)|0x1) # b1: addr_inc, b0: 1=write/0=read
      if self.lock.acquire(timeout=1):
        offset = 0
        while offset < length-chunk_size:
          header = (0x00aaff|(((chunk_size>>2)-1)<<24)|(address<<32)).to_bytes(8, byteorder='little')
          self.ser.write(header + data[offset:offset+chunk_size])
          self.ser.read(16) # drop acknowledgement
          address += chunk_size if increment else 0
          offset += chunk_size
        if offset < length: # last transfer
          header = (0x00aaff|((((length-offset)>>2)-1)<<24)|(address<<32)).to_bytes(8, byteorder='little')
          self.ser.write(header + data[offset:length])
          self.ser.read(16) # drop acknowledgement
    finally:
      self.lock.release()

  def __read(self, address: int, length: int, increment: int) -> bytearray:
    '''Reads burst data with given byte length from device. Based on UART2AHB,
      data should always be word (4 bytes) aligned with maximum transfer size
      limited to 256 words. This method wil automatically split data whose
      length is larger than 256 words into multiple transfers.
      ---
      address: int
        Target bus address which should always be aligned to 4 bytes
      length: int
        Data byte length to be transferred
      increment: int
        Post-read-increase-address-4-bytes enable
      ---
      Returns data read from device in format 'bytearray'
    '''
    address &= 0xffffffff; length = (length+3) & ~0x3 # ceil to 4 bytes
    chunk_size = 0x400 # upperbound = 256 words = 1024 btyes, 0-indexing
    data = bytearray() # empty byte array in the beginning
    if not self.ser.is_open:
      raise RuntimeError('Method \'open\' should be called in advance to setup port connection.')
    try:
      address |= ((increment<<1)|0x0) # b1: addr_inc, b0: 1=write/0=read
      if self.lock.acquire(timeout=1):
        offset = 0
        while offset < length-chunk_size:
          header = (0x00aaff|(((chunk_size>>2)-1)<<24)|(address<<32)).to_bytes(8, byteorder='little')
          self.ser.write(header)
          data += self.ser.read(8 + chunk_size)[8:] # drop acknowledgement
          address += chunk_size if increment else 0
          offset += chunk_size
        if offset < length: # last transfer
          header = (0x00aaff|((((length-offset)>>2)-1)<<24)|(address<<32)).to_bytes(8, byteorder='little')
          self.ser.write(header)
          data += self.ser.read(8 + (length-offset))[8:] # drop acknowledgement
    finally:
      self.lock.release()
    return data

  def open(self, com_port: str='COM4', baudrate: int=921600) -> None:
    '''Opens Serial port connection. UART baudrate is fixed to 921600 based on
      reference clock 25M. COM port naming is related to Host OS, which should
      be something like 'COM4' on Windows and '/dev/ttyUSB0' on Linux.
      ---
      Returns None
    '''
    self.ser.port = com_port
    self.ser.baudrate = baudrate
    self.ser.timeout = 1 # UART read bytes milliseconds timeout protection
    self.ser.open()
    if not self.ser.is_open:
      raise Exception('Failed to open serial port={}, baudrate={}'.format(com_port, baudrate))

  def close(self) -> None:
    '''Closes current Serial port connection.
      ---
      Returns None
    '''
    if self.ser.is_open:
      self.ser.close()

  def write(self, address: int, data: Union[int, bytearray], length: int=4, increment: int=1) -> None:
    '''Writes data to specific address. If data is given as type 'int', data
      will be mask to 4 bytes. Otherwise, data should be given as bytearray and
      byte length should also be provided.
      ---
      address: int
        Target bus address which should always be aligned to 4 bytes
      data: int or bytearray
        Data to be transferred. Length should be provided if bytearray is given
      length: int - default: 4
        Data byte length to be transferred
      increment: int - default: 1
        Post-write-increase-address-4-bytes enable, default on
      ---
      Returns None
    '''
    data = data.to_bytes(length, byteorder='little') if isinstance(data, int) else data
    return self.__write(address, data, length, increment)

  def read(self, address: int, length: int=4, increment: int=1) -> Union[int, bytearray]:
    '''Reads data from specific address. If length is given as 4 bytes, data
      returned will be format 'int', otherwise 'bytearray'.
      ---
      address: int
        Target bus address which should always be aligned to 4 bytes
      length: int - default: 4
        Data byte length to be transferred
      increment: int -default: 1
        Post-read-increase-address-4-bytes enable, default on
      ---
      Returns data read as format 'int' if length equals to 4 bytes, otherwise
      'bytearray'
    '''
    data = self.__read(address, length, increment)
    return int.from_bytes(data, byteorder='little') & 0xffffffff if (length+3)&~0x3 == 4 else data

