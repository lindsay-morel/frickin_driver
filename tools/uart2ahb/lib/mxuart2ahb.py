import threading, serial
from lib.mxio import MxIo, any_to_bytes, bytes_to_int

class MxUart2ahb(MxIo):
  '''Proprietary command interface. Proprietary protocol above general UART
    interface with fixed baudrate divider within chip design.
  '''
  def __init__(self) -> None:
    self.lock = threading.Lock()
    self.ser = serial.Serial()

  def __del__(self) -> None:
    self.close()

  def open(self, com_port: str='COM4', baudrate: int=921600) -> None:
    '''Open Serial port connection. Currently baudrate is fixed to 921600
      using reference clock and has no option to adjust.
    '''
    # COM port name is related to PC Host OS. For example, it should be
    # something like 'COM4' on windows and '/dev/ttyUSB0' on linux
    self.ser.port = com_port
    self.ser.baudrate = baudrate
    self.ser.timeout = 1 # read until specified number of bytes specified received timeout protection
    self.ser.open()
    if not self.ser.is_open:
      raise Exception('MxUart2ahb:open, fail to open serial port={}'.format(com_port))
    self.write(0x2001_fffc, 0x55aa55aa)
    return self.read(0x2001_fffc) == 0x55aa55aa

  def close(self) -> None:
    '''Close current Serial port connection.
    '''
    if self.ser.is_open:
      self.ser.close()

  def write(self, addr: int, data: int, chip_id: int=1) -> None:
    '''Write 4 bytes data to specific 4 bytes address through UART2AHB.
    '''
    addr &= 0xffffffff; data &= 0xffffffff
    if not self.ser.is_open:
      raise Exception('MxUart2ahb:write, interface is not ready')
    try:
      addr |= 0x3 # b1: addr_inc, b0: 1=write/0=read
      if self.lock.acquire(timeout=1):
        hdr = any_to_bytes(0x00aaff|(0<<24)|(addr<<32), 8)
        bybuf = any_to_bytes(data, 4)
        self.ser.write(hdr+bybuf)
        self.ser.read(16) # (0xff + 0xaa + 0x00 + word_length + address(4 bytes) = 8 bytes) * 2 times
    finally:
      self.lock.release()
    pass

  def write_burst(self, addr: int, bylen: int, bybuf: bytearray, chip_id: int=1, inc: int=1) -> None:
    '''Write burst data with length at most 256 words once to given base address
      through UART2AHB. This method will automatically split data larger than
      256 words into multiple transmissions.
    '''
    addr &= 0xffffffff; bylen = (bylen+3)&~0x3
    if not self.ser.is_open:
      raise Exception('MxUart2ahb:write_burst, interface is not ready')
    try:
      addr |= ((inc<<1)|0x1) # b1: addr_inc, b0: 1=write/0=read
      if self.lock.acquire(timeout=1):
        ofs = 0; chunk = 0x400 # upperbound = 256 words = 1024B, 0-indexing
        while ofs < bylen-chunk:
          hdr = any_to_bytes(0x00aaff|(((chunk>>2)-1)<<24)|(addr<<32), 8)
          self.ser.write(hdr+bybuf[ofs:ofs+chunk])
          self.ser.read(16)
          addr += chunk if inc else 0; ofs += chunk
        if ofs < bylen: # last transfer
          hdr = any_to_bytes(0x00aaff|((((bylen-ofs)>>2)-1)<<24)|(addr<<32), 8)
          self.ser.write(hdr+bybuf[ofs:bylen])
          self.ser.read(16)
    finally:
      self.lock.release()
    pass

  def read(self, addr: int, chip_id: int=1) -> int:
    '''Read 4 bytes data from specific 4 bytes address through UART2AHB.
    '''
    addr &= 0xffffffff; bybuf = bytearray()
    if not self.ser.is_open:
      raise Exception('MxUart2ahb:read, interface is not ready')
    try:
      addr = (addr & ~0x3) | 0x2 # b1: addr_inc, b0: 1=write/0=read
      if self.lock.acquire(timeout=1):
        hdr = any_to_bytes(0x00aaff|(0<<24)|(addr<<32), 8)
        self.ser.write(hdr)
        bybuf += self.ser.read(12)[8:] # drop preamble, length and address echoed
    finally:
      self.lock.release()
    return bytes_to_int(bybuf) & 0xffffffff

  def read_burst(self, addr: int, bylen: int, chip_id: int=1, inc: int=1) -> bytearray:
    '''Read burst data with length at most 256 words once from given base
      address through UART2AHB. This method will automatically split data larger
      than 256 words into multiple transmissions.
    '''
    addr &= 0xffffffff; bylen = (bylen+3)&~0x3; bybuf = bytearray()
    if not self.ser.is_open:
      raise Exception('MxUart2ahb:read_burst, interface is not ready')
    try:
      addr = (addr&~0x3)|((inc<<1)|0x0) # b1: addr_inc, b0: 1=write/0=read
      if self.lock.acquire(timeout=1):
        ofs = 0; chunk = 0x400 # upperbound = 256 words = 1024B, 0-indexing
        while ofs < bylen-chunk:
          hdr = any_to_bytes(0x00aaff|(((chunk>>2)-1)<<24)|(addr<<32), 8)
          self.ser.write(hdr)
          bybuf += self.ser.read(8+chunk)[8:] # drop preamble, length and address echoed
          addr += chunk if inc else 0; ofs += chunk
        if ofs < bylen: # last transfer
          hdr = any_to_bytes(0x00aaff|((((bylen-ofs)>>2)-1)<<24)|(addr<<32), 8) 
          self.ser.write(hdr)
          bybuf += self.ser.read(8+(bylen-ofs))[8:]
    finally:
      self.lock.release()
    return bybuf

