import sys, os, ctypes, struct, platform, threading, time
from ctypes import c_ubyte, c_int, c_void_p, c_char_p, POINTER, create_string_buffer
from lib.mxio import MxIo, any_to_bytes, bytes_to_int

#------------------------------------------------------------------------------
# load library for different platform dynamically
#------------------------------------------------------------------------------
_mxusb = None
try:
  # after python3.8, need to specify dynamic library searching path
  dirname = os.path.dirname(os.path.realpath(__file__))
  (major, minor, micro, rel, ser) = sys.version_info
  if major == 3 and minor > 8:
    os.add_dll_directory(dirname)
  # check 32bit or 64bit mode
  mode = struct.calcsize('P') << 3
  # windows
  if os.name == 'nt':
    if mode == 64:
      fpath = os.path.join(dirname,'bin','win','cyusb3','x64','mxusb.dll')
      _mxusb = ctypes.windll.LoadLibrary(fpath)
    else:
      fpath = os.path.join(dirname,'bin','win','cyusb3','x86','mxusb.dll')
      _mxusb = ctypes.windll.LoadLibrary(fpath)
  # linux
  elif os.name == 'posix':
    if platform.processor() == 'x86_64':
      fpath = os.path.join(dirname,'bin','linux','x86_64','mxusb.so')
      _mxusb = ctypes.cdll.LoadLibrary(fpath)
    elif platform.processor() == 'aarch64':
      fpath = os.path.join(dirname,'bin','linux','aarch64','mxusb.so')
      raise Exception('Not-supported platform = {}(x{})'.format(platform.uname(), mode))
    else:
      raise Exception('Not-supported platform = {}(x{})'.format(platform.uname(), mode))
  # other os
  else:
    raise Exception('Not-supported os = {}(x{})'.format(os.name, mode))

  # function reshape
  if _mxusb:
    # mxerr mxusb_open(void);
    _mxusb.mxusb_open.argtypes = []
    _mxusb.mxusb_open.restype = c_int
    # mxerr mxusb_close(void);
    _mxusb.mxusb_close.argtypes = []
    _mxusb.mxusb_close.restype = c_int
    # mxerr mxusb_show(void);
    _mxusb.mxusb_show.argtypes = []
    _mxusb.mxusb_show.restype = c_int
    # MxUsbHandle mxusb_open_handle(unsigned short vendor_id, unsigned short product_id, int interface);
    _mxusb.mxusb_open_handle.argtypes = [c_int, c_int, c_int]
    _mxusb.mxusb_open_handle.restype = c_void_p
    # mxerr mxusb_close_handle(MxUsbHandle handle);
    _mxusb.mxusb_close_handle.argtypes = [c_void_p]
    _mxusb.mxusb_close_handle.restype = c_int
    # unsigned short mxusb_get_vendor(MxUsbHandle handle);
    _mxusb.mxusb_get_vendor.argtypes = [c_void_p]
    _mxusb.mxusb_get_vendor.restype = c_int
    # unsigned short mxusb_get_product(MxUsbHandle handle);
    _mxusb.mxusb_get_product.argtypes = [c_void_p]
    _mxusb.mxusb_get_product.restype = c_int
    # mxerr mxusb_bulk_out(MxUsbHandle handle, byte *data, int len);
    _mxusb.mxusb_bulk_out.argtypes = [c_void_p, POINTER(c_ubyte), c_int]
    _mxusb.mxusb_bulk_out.restype = c_int
    # mxerr mxusb_bulk_in(MxUsbHandle handle, byte *data, int len);
    _mxusb.mxusb_bulk_in.argtypes = [c_void_p, POINTER(c_ubyte), c_int]
    _mxusb.mxusb_bulk_in.restype = c_int
    # mxerr mxusb_download_fx3(MxUsbHandle handle,  char *filename);
    _mxusb.mxusb_download_fx3.argtypes = [c_void_p, c_char_p]
    _mxusb.mxusb_download_fx3.restype = c_int
  else:
    raise Exception('What happenes here?')

except Exception as ex:
  print('Fail to load library mxusb: {}'.format(ex))
  exit()

#------------------------------------------------------------------------------
# function wrapper
#------------------------------------------------------------------------------
def mxusb_open() -> int:
  '''Low level driver initialization, should be called before any mxusb function
    is used. After this function returns success, mxusb_close should be called
    before application terminates. Returns 0 if success, otherwise error code.
  '''
  return _mxusb.mxusb_open()

def mxusb_close() -> int:
  '''Low level driver deinitialization, should be called after closing all open
    devices and before application terminates. Returns 0 if success, otherwise
    error code.
  '''
  return _mxusb.mxusb_close()

class MxUsbHandle:
  '''Wrapper class for mxusb handle. This class contains only one IO handle
    for each interface which means User should create multiple class instances
    if is required to do multiple interface control.
  '''
  def __init__(self, ) -> None:
    '''Creates a handle wrapper instance. Still need to call 'open' before bulk
      transfer is available.
    '''
    self.handle = None
    self.vendor_id = None
    self.product_id = None
    self.interface = None
    self.is_open = False

  def __del__(self) -> None:
    '''Always do the clean-up to destory IO handle.
    '''
    self.close()

  def open(self, vendor_id: int, product_id: int, interface: int) -> bool:
    '''Get USB IO handle for specified interface. If multiple devices exists,
      interface index will be increased.
    '''
    vendor_id &= 0xffff; product_id &= 0xffff; interface &= 0xffffffff
    self.handle = _mxusb.mxusb_open_handle(vendor_id, product_id, interface)
    if self.handle:
      self.vendor_id = vendor_id
      self.product_id = product_id
      self.interface = interface
      self.is_open = True
    return self.is_open

  def close(self) -> None:
    '''Releases given USB IO handle.
    '''
    if self.handle:
      _mxusb.mxusb_close_handle(self.handle)
    self.handle = None
    self.vendor_id = None
    self.product_id = None
    self.interface = None
    self.is_open = False

  def get_vendor(self) -> int:
    '''Returns Vendor ID for current device, but actually uses handle to look
      back to device and get device information.
    '''
    if self.handle:
      return _mxusb.mxusb_get_vendor(self.handle) & 0xffff
    return self.vendor_id

  def get_product(self) -> int:
    '''Returns Product ID for current device, but actually uses handle to look
      back to device and get device information.
    '''
    if self.handle:
      return _mxusb.mxusb_get_product(self.handle) & 0xffff
    return self.product_id

  def bulk_out(self, bylen: int, bybuf: bytearray) -> None:
    '''Write data out through given handle and corresponding endpoint.
    '''
    if self.handle:
      buf = (c_ubyte * bylen).from_buffer(bybuf)
      return _mxusb.mxusb_bulk_out(self.handle, buf, bylen)
    raise Exception('USB handle is not opened.')

  def bulk_in(self, bylen: int) -> bytearray:
    '''Read data from given handle and corresponding endpoint.
    '''
    if self.handle:
      buf = (c_ubyte * bylen)()
      _err = _mxusb.mxusb_bulk_in(self.handle, buf, bylen)
      return bytearray(buf)
    raise Exception('USB handle is not opened.')

  def download_fx3(self, fpath: str) -> int:
    '''Download firmware to Cypress FX3, can only be performed to FX3 which
      is in bootloader mode. User should guarantee FX3 is reseted before call
      the firmware download.
    '''
    if not os.path.exists(fpath):
      return -1
    if self.handle:
      buf = create_string_buffer(fpath.encode('utf-8'))
      return _mxusb.mxusb_download_fx3(self.handle, buf)
    return -1

#------------------------------------------------------------------------------
# USB to chip IO wrapper
#------------------------------------------------------------------------------
class MxUsb(MxIo):
  '''Usb to chip interface, currently is limited to proprietary CCIF interface.
    However, should be extend to support both M31 PHY and CCIF within low level
    driver.
  '''
  CMD_NOP = 0
  CMD_INIT = 1
  CMD_WRITE = 2
  CMD_READ = 3
  CMD_READ_DATA = 4
  CMD_FLOW = 5
  CMD_PAD = 6
  CMD_BYPASS = 8
  PKT_SIZE = 2048 # 2048B

  def __init__(self) -> None:
    self.lock = threading.Lock()
    self.mst = MxUsbHandle()
    self.slv = MxUsbHandle()
    self.last_chip_id = 1
    if mxusb_open(): # libusb init.
      raise Exception('Failed to init. libusb.')
    pass

  def __del__(self) -> None:
    self.close()
    mxusb_close() # libusb clean-up
    #time.sleep(1) # delay a while to wait OS to clean-up

  def open(self, mst_vendor: int=0x04b4, mst_product: int=0xf4, mst_interface: int=0, slv_vender: int=0x04b4, slv_product: int=0xf2, slv_interface: int=0, no_mst: int=0, no_slv: int=0) -> None:
    '''Currently there are two ports required to do chip asynchronized access.
      Master port is used as chip ingress port while slave port as egress port.
    '''
    # 0xf3 is Cypress FX3 default boot-loader product ID. Try to download
    # image to FX3 if there is no master or slave found.
    if not no_mst:
      self.mst.open(mst_vendor, mst_product, mst_interface)
    if not no_slv:
      self.slv.open(slv_vender, slv_product, slv_interface)
    if not no_mst and not self.mst.is_open:
      dev = MxUsbHandle()
      dev.open(0x04b4, 0xf3, 0)
      dev.download_fx3('img/AutoMaster.img')
      time.sleep(1)
      self.mst.open(mst_vendor, mst_product, mst_interface)
    if not no_slv and not self.slv.is_open:
      dev = MxUsbHandle()
      dev.open(0x04b4, 0xf3, 0)
      dev.download_fx3('img/AutoSlave.img')
      time.sleep(1)
      dev.open(slv_vender, slv_product, slv_interface)
      self.slv.open(slv_vender, slv_product, slv_interface)
    if (not no_mst and not self.mst.is_open) or (not no_slv and not self.slv.is_open):
      raise Exception('Failed to open USB interface, mst={}, slv={}'.format(self.mst.is_open, self.slv.is_open))
    if (not no_mst and self.mst.is_open) and (not no_slv and self.slv.is_open):
      # write init. command to chain chips,  use BYPASS command to squeeze for
      # the first time we don't know how many chips are chained in system.
      self.mst_cmd_init(chip_id=1)
      data = bytes_to_int(self.slv_get_pkt(self.PKT_SIZE-4)[:4]) & 0xffffffff
      assert (data>>28) & 0xf == self.CMD_INIT
      self.last_chip_id = ((data>>18) & 0x3f) - 1
      print('setup last chip id = {}'.format(self.last_chip_id))
    pass

  def close(self) -> None:
    '''Close both ingress master port and egress slave ports.
    '''
    if self.mst.is_open:
      self.mst.close()
    if self.slv.is_open:
      self.slv.close()
    pass

  def mst_cmd_nop(self, bylen: int=4, chip_id: int=1) -> None:
    '''Generate NOP command packet which should be dropped by any chip.
    '''
    bylen = (bylen+3)&~0x3
    hdr = any_to_bytes((self.CMD_NOP<<28)|(chip_id<<18), 4)
    self.mst.bulk_out(4+bylen, hdr)

  def mst_cmd_init(self, chip_id: int=1) -> None:
    '''Generate INIT command packet which will assign chip ID incrementally.
    '''
    hdr = any_to_bytes((self.CMD_INIT<<28)|(chip_id<<18), 4)
    self.mst.bulk_out(4, hdr)

  def mst_cmd_write(self, addr: int, bylen: int, bybuf: bytearray, chip_id: int=1) -> None:
    '''Generate WRITE command packet which payload size is limited to 2^18.
    '''
    addr &= 0xffffffff; bylen = (bylen+3)&~0x3; wlen = bylen>>2
    hdr = any_to_bytes((self.CMD_WRITE<<28)|(chip_id<<18)| wlen |(addr<<32), 8)
    self.mst.bulk_out(8+bylen, hdr+bybuf)

  def mst_cmd_read(self, addr: int, bylen: int, chip_id: int=1) -> None:
    '''Generate READ command packet which payload size is limited to 2^18.
    '''
    addr &= 0xffffffff; bylen = (bylen+3)&~0x3; wlen = bylen>>2
    hdr = any_to_bytes((self.CMD_READ<<28)|(chip_id<<18)| wlen |(addr<<32), 8)
    self.mst.bulk_out(8, hdr)

  def mst_cmd_read_data(self, addr: int, bylen: int, bybuf: bytearray, chip_id: int=0) -> None:
    '''Generate READ_DATA command packet which should be bypassed by all chips.
    '''
    addr &= 0xffffffff; bylen = (bylen+3)&~0x3; wlen = bylen>>2
    hdr = any_to_bytes((self.CMD_READ_DATA<<28)|(chip_id<<18)| wlen |(addr<<32), 8)
    self.mst.bulk_out(8+bylen, hdr+bybuf)

  def mst_cmd_flow(self, flow: int, bylen: int, bybuf: bytearray, chip_id: int=0) -> None:
    '''Generate FLOW command packet which is sent to MPU or HOST directly.
    '''
    flow &= 0xf; bylen = (bylen+3)&~0x3; wlen = bylen>>2
    hdr = any_to_bytes((self.CMD_FLOW<<28)|(flow<<24)|(chip_id<<18)|(wlen-1), 4)
    self.mst.bulk_out(4+bylen, hdr+bybuf)

  def mst_cmd_pad(self, bylen: int, chip_id: int=1) -> None:
    '''Generate PAD command packet which should trigger dummy padding from chip.
    '''
    bylen = (bylen+3)&~0x3; wlen = bylen>>2
    hdr = any_to_bytes((self.CMD_PAD<<28)|(chip_id<<18)|(wlen-1), 4)
    self.mst.bulk_out(4, hdr)

  def mst_cmd_bypass(self, data: int) -> None:
    '''Generate BYPASS command packet which should be always bypassed by chip.
    '''
    data &= 0xffffffff
    hdr = any_to_bytes((self.CMD_BYPASS<<28)|data, 4)
    self.mst.bulk_out(4, hdr)

  def mst_cmd_bcast(self, addr: int, bylen: int, bybuf: bytearray) -> None:
    '''Generate WRITE command packet which ID is fixed to 0x3f
    '''
    addr &= 0xffffffff; bylen = (bylen+3)&~0x3; wlen = bylen>>2
    hdr = any_to_bytes((self.CMD_WRITE<<28)|(0x3f<<18)| wlen |(addr<<32), 8)
    self.mst.bulk_out(8+bylen, hdr+bybuf)

  def slv_get_pkt(self, pad_bylen: int=0, pad_word: int=0xdeadbeef) -> bytearray:
    '''Read data from FX3 slave buffer with size limited to 2048B. Push
      additional padding to "squeeze" out data if is required.
    '''
    pad_word |= self.CMD_BYPASS; pad_bylen = (pad_bylen+3)&~0x3; wlen = pad_bylen>>2
    if wlen: # use BYPASS to do padding instead of PAD for debug purpose
      [self.mst_cmd_bypass(pad_word) for _ in range(wlen)]
    return self.slv.bulk_in(self.PKT_SIZE)

  def write(self, addr: int, data: int, chip_id: int=1) -> None:
    '''Write 4 bytes data to specific 4 bytes address through USB interface.
    '''
    addr &= 0xffffffff; data &= 0xffffffff
    if not self.mst.is_open:
      raise Exception('MxUsb:write, interface is not ready') 
    try:
      if self.lock.acquire(timeout=1):
        bybuf = any_to_bytes(data, 4)
        self.mst_cmd_write(addr, 4, bybuf, chip_id)
    finally:
      self.lock.release()
    pass

  def write_burst(self, addr: int, bylen: int, bybuf: bytearray, chip_id: int=1, inc: int=1) -> None:
    '''Write burst data given base address through USB interface. Since the word
      length field within CCIF header is atmost 18 bits, transfer will be split
      into multiple packets if header plus payload is larger than limitation.
    '''
    addr &= 0xffffffff; bylen = (bylen+3)&~0x3
    if not self.mst.is_open:
      raise Exception('MxUsb:write_burst, interface is not ready')
    try:
      if self.lock.acquire(timeout=1):
        ofs = 0; chunk = self.PKT_SIZE-8 # reserve 4 bytes for header and 4 bytes for address
        while ofs < bylen-chunk: # when size of data to be transfered is larger than chunk size
          self.mst_cmd_write(addr, chunk, bybuf[ofs:ofs+chunk], chip_id)
          addr += chunk if inc else 0; ofs += chunk
        if ofs < bylen: # last one with transfer size smaller than chunk
          self.mst_cmd_write(addr, bylen-ofs, bybuf[ofs:], chip_id)
    finally:
      self.lock.release()
    pass

  def read(self, addr: int, chip_id: int=1) -> int:
    '''Read 4 bytes data from specific 4 bytes address through USB interface.
      Will write 'Read' command through master port first than receive data from
      slave port.
    '''
    addr &= 0xffffffff; bybuf = bytearray()
    if not self.mst.is_open or not self.slv.is_open:
      raise Exception('MxUsb:read, interface is not ready')
    try:
      if self.lock.acquire(timeout=1):
        self.mst_cmd_read(addr, 4, chip_id)
        self.mst_cmd_pad(self.PKT_SIZE-12, self.last_chip_id) # need to add padding to squeeze out data
        bybuf += self.slv_get_pkt()[8:] # drop header and address echoed
    finally:
      self.lock.release()
    return bytes_to_int(bybuf) & 0xffffffff

  def read_burst(self, addr: int, bylen: int, chip_id: int=1, inc: int=1) -> bytearray:
    '''Read burst data with given length. Since the word length field within
      CCIF header is atmost 18 bits, transfer will be split into multiple
      packets if header plus payload is larger than limitation.
    '''
    addr &= 0xffffffff; bylen = (bylen+3)&~0x3; bybuf = bytearray()
    if not self.mst.is_open or not self.slv.is_open:
      raise Exception('MxUsb:read_burst, interface is not ready')
    try:
      if self.lock.acquire(timeout=1):
        ofs = 0; chunk = self.PKT_SIZE-8 # reserve 4 bytes for header and 4 bytes for address
        while ofs < bylen-chunk: # when size of data to be transfered is larger than chunk size
          self.mst_cmd_read(addr, chunk, chip_id)
          bybuf += self.slv_get_pkt()[8:] # drop header and address echoed
          addr += chunk if inc else 0; ofs += chunk
        if ofs < bylen: # last one with transfer size smaller than chunk
          self.mst_cmd_read(addr, bylen-ofs, chip_id)
          self.mst_cmd_pad(self.PKT_SIZE-(8+bylen-ofs), self.last_chip_id) # squeeze out data
          bybuf += self.slv_get_pkt()[8:]
    finally:
      self.lock.release()
    return bybuf

