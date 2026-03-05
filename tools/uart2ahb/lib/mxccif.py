################################################################################
# Copyright (C) 2019-2021 MemryX Limited. All rights reserved.
################################################################################
import os, sys, ctypes, struct, platform
from ctypes import c_void_p, c_int, c_uint8, c_uint16, c_uint32, byref, POINTER
from lib.mxio import MxIo

# load library based on platform dynamically
# --
_mx_ccif = None
try:
  # after python3.8, need to specify dynamic library searching path
  dirname = os.path.dirname(os.path.realpath(__file__))
  (major, minor, micro, rel, ser) = sys.version_info
  if major == 3 and minor > 8 and os.name == 'nt':
    os.add_dll_directory(dirname)
  # check 32bit or 64bit mode
  mode = struct.calcsize('P') << 3
  # linux
  if os.name == 'posix':
    if platform.uname().machine in ['x86_64','aarch64']:
      libpath = os.path.join(dirname,'bin','linux',platform.uname().machine,'libmxccif.so')
      _mx_ccif = ctypes.cdll.LoadLibrary(libpath)
    else:
      raise Exception('Not-supported platform = {}(x{})'.format(platform.uname(), mode))
  # windows
  elif os.name == 'nt':
    raise Exception('Not-supported os = {}(x{})'.format(os.name, mode))
  # other os
  else:
    raise Exception('Not-supported os = {}(x{})'.format(os.name, mode))

  # function reshape
  # need to re-assign precise parameter type otherwise there will be some issue between 64-bits to 32-bits transition 
  if _mx_ccif:
    # MxCcifHandle mx_ccif_open(uint16_t mst_vendor_id, uint8_t mst_product_id, uint8_t mst_ep_out, int mst_interface, uint16_t slv_vendor_id, uint8_t slv_product_id, uint8_t slv_ep_in, int slv_interface);
    _mx_ccif.mx_ccif_open.argtypes = [c_uint16, c_uint8, c_uint8, c_int, c_uint16, c_uint8, c_uint8, c_int]
    _mx_ccif.mx_ccif_open.restype = c_void_p
    # mx_err_t mx_ccif_close(MxCcifHandle handle, int timeout);
    _mx_ccif.mx_ccif_close.argtypes = [c_void_p, c_int]
    _mx_ccif.mx_ccif_close.restype = c_int
    # mx_err_t mx_ccif_flush(MxCcifHandle handle);
    _mx_ccif.mx_ccif_flush.argtypes = [c_void_p]
    _mx_ccif.mx_ccif_flush.restype = c_int
    # mx_err_t mx_ccif_wait_ready(MxCcifHandle handle, int timeout);
    _mx_ccif.mx_ccif_wait_ready.argtypes = [c_void_p, c_int]
    _mx_ccif.mx_ccif_wait_ready.restype = c_int
    # mx_err_t mx_ccif_ctrl_init(MxCcifHandle handle, uint8_t chip_id, int timeout);
    _mx_ccif.mx_ccif_ctrl_init.argtypes = [c_void_p, c_uint8, c_int]
    _mx_ccif.mx_ccif_ctrl_init.restype = c_int
    # mx_err_t mx_ccif_ctrl_write(MxCcifHandle handle, uint8_t chip_id, uint32_t addr, uint32_t data, int timeout);
    _mx_ccif.mx_ccif_ctrl_write.argtypes = [c_void_p, c_uint8, c_uint32, c_uint32, c_int]
    _mx_ccif.mx_ccif_ctrl_write.restype = c_int
    # mx_err_t mx_ccif_ctrl_write_burst(MxCcifHandle handle, uint8_t chip_id, uint32_t addr, uint8_t *data, int length, int increment, int timeout);
    _mx_ccif.mx_ccif_ctrl_write_burst.argtypes = [c_void_p, c_uint8, c_uint32, POINTER(c_uint8), c_int, c_int, c_int]
    _mx_ccif.mx_ccif_ctrl_write_burst.restype = c_int
    # mx_err_t mx_ccif_ctrl_read(MxCcifHandle handle, uint8_t chip_id, uint32_t addr, uint32_t *data, int timeout);
    _mx_ccif.mx_ccif_ctrl_read.argtypes = [c_void_p, c_uint8, c_uint32, POINTER(c_uint32), c_int]
    _mx_ccif.mx_ccif_ctrl_read.restype = c_int
    # mx_err_t mx_ccif_ctrl_read_burst(MxCcifHandle handle, uint8_t chip_id, uint32_t addr, uint8_t *data, int length, int increment, int timeout);
    _mx_ccif.mx_ccif_ctrl_read_burst.argtypes = [c_void_p, c_uint8, c_uint32, POINTER(c_uint8), c_int, c_int, c_int]
    _mx_ccif.mx_ccif_ctrl_read_burst.restype = c_int
    # mx_err_t mx_ccif_data_write(MxCcifHandle handle, uint8_t chip_id, int flow_id, uint8_t *data, int length, int timeout);
    _mx_ccif.mx_ccif_data_write.argtypes = [c_void_p, c_uint8, c_int, POINTER(c_uint8), c_int, c_int]
    _mx_ccif.mx_ccif_data_write.restype = c_int
    # mx_err_t mx_ccif_data_read(MxCcifHandle handle, uint8_t chip_id, int flow_id, uint8_t *data, int length, int *transferred, int timeout);
    _mx_ccif.mx_ccif_data_read.argtypes = [c_void_p, c_uint8, c_int, POINTER(c_uint8), c_int, POINTER(c_int), c_int]
    _mx_ccif.mx_ccif_data_read.restype = c_int
    # mx_err_t mx_ccif_diag_dump_shared_ring_buffer(MxCcifHandle handle, uint8_t *data, int length);
    _mx_ccif.mx_ccif_diag_dump_shared_ring_buffer.argtypes = [c_void_p, POINTER(c_uint8), c_int]
    _mx_ccif.mx_ccif_diag_dump_shared_ring_buffer.restype = c_int
  else:
    raise Exception('What happenes here?')

except Exception as ex:
  print('Fail to load library mxccif: {}'.format(ex))
  exit()

# class wrapper
# --
class MxCcif(MxIo):
  '''There class is a reflection of driver mxccif.h
  '''
  def __init__(self, mst_vendor_id: int=0x04b4, mst_product_id: int=0xf4, mst_ep_out: int=0x01, mst_interface: int=0, slv_vendor_id: int=0x04b4, slv_product_id: int=0xf2, slv_ep_in: int=0x81, slv_interface: int=0) -> None:
    '''Open a new CCIF handle by given both master and slave USB device ID.
      Will automatically do USB reset and start background workers after both 
      devices are opened successfully. Return CCIF handle instance on success, 
      otherwise return NULL. Always remember to call 'close' to destory handle
      after all operations are finished.
    '''
    self.handle = _mx_ccif.mx_ccif_open(mst_vendor_id, mst_product_id, mst_ep_out, mst_interface, slv_vendor_id, slv_product_id, slv_ep_in, slv_interface)
    if not self.handle:
      raise Exception(-1)
    pass

  def __del__(self) -> None:
    '''Close CCIF handle by a given timeout to wait on-going transfers to be 
      finished. Will destory given handle after this function is called. To use 
      CCIF again, user should re-open a new handle.
    '''
    err = _mx_ccif.mx_ccif_close(self.handle, 1000)
    self.handle = None
    if err:
      raise Exception(err)
    pass

  def flush(self) -> None:
    '''Add bytes of paddings from last device to slave in order to squeeze
      out data from slave USB device to host. Flush always generates 512 BYPASS
      commands, which equals to one CCIF packet size = 2048(B).
    '''
    err = _mx_ccif.mx_ccif_flush(self.handle)
    if err:
      raise Exception(err)
    pass

  def wait(self, timeout: int=1000) -> None:
    '''Wait until currently on-going transfers to be finished or timeout.
      Only waits for master to device transfers, since in asynchronous transfers,
      background workers do not know the exact number of transfers. Also, if 
      timeout happens, this function will NOT stop any on-going transfer.
    '''
    err = _mx_ccif.mx_ccif_wait_ready(self.handle, timeout)
    if err:
      raise Exception(err)
    pass

  def init(self, chip_id: int=1, timeout: int=1000) -> None:
    '''Control channel write INIT command. This function will setup CHIP-ID
      for all cascade devices incrementally starting from the given CHIP-ID. The
      INIT command will be automatically transferred at the beginning when using
      'open' for setup purpose. However, user can still use this function to
      assign arbitrary CHIP-ID (not 0 which is reserved as host) to devices any
      time.
    '''
    err = _mx_ccif.mx_ccif_ctrl_init(self.handle, chip_id, timeout)
    if err:
      raise Exception(err)
    pass

  def c_write(self, addr: int, data: int, chip_id: int=1, timeout: int=200) -> None:
    '''Control channel write 4 bytes data to bus address given. Should be
      used only in configuration phase in normal operation before data flow start.
    '''
    err = _mx_ccif.mx_ccif_ctrl_write(self.handle, chip_id, addr, data, timeout)
    if err:
      raise Exception(err)
    pass

  def c_write_burst(self, addr: int, data: bytearray, length: int, chip_id: int=1, increment: int=1, timeout: int=1000) -> None:
    '''Control channel write multiple bytes data to address given. Should
      only be used in configuration phase in normal operation before data flow
      starts.
    '''
    c_data = (c_uint8 * length).from_buffer(data[:length])
    err = _mx_ccif.mx_ccif_ctrl_write_burst(self.handle, chip_id, addr, c_data, length, increment, timeout)
    if err:
      raise Exception(err)
    pass

  def c_read(self, addr: int, chip_id: int=1, timeout: int=200) -> int:
    '''Control channel read 4 bytes data from address given. Should only be
      used in configuration phase in normal operation before data flow starts.
    '''
    c_data = c_uint32(0)
    err = _mx_ccif.mx_ccif_ctrl_read(self.handle, chip_id, addr, byref(c_data), timeout)
    if err:
      raise Exception(err)
    return int(c_data.value)

  def c_read_burst(self, addr: int, length: int, chip_id: int=1, increment: int=1, timeout: int=1000) -> bytearray:
    '''Control channel read 4 bytes data from address given. Should only be
      used in configuration phase in normal operation before data flow starts.
    '''
    c_data = (c_uint8 * length)()
    err = _mx_ccif.mx_ccif_ctrl_read_burst(self.handle, chip_id, addr, c_data, length, increment, timeout)
    if err:
      raise Exception(err)
    return bytearray(c_data)

  def d_write(self, flow_id: int, data: bytearray, length: int, chip_id: int=1, timeout: int=1000) -> None:
    '''Data channel write multiple bytes data to specific data flow. Should
      only be used in data phase in normal operation after model configuration is
      ready.
    '''
    c_data = (c_uint8 * length).from_buffer(data[:length])
    err = _mx_ccif.mx_ccif_data_write(self.handle, chip_id, flow_id, byref(c_data), length, timeout)
    if err:
      raise Exception(err)
    pass

  def d_read(self, flow_id: int, length: int, chip_id: int=1, timeout: int=1000) -> bytearray:
    '''Data channel read multiple bytes data from specific data flow. Should
      only be used in data phase in normal operation after model configuration is
      ready. Since there is no guarantee that in what time and how many data will
      be pushed out from device, user should always check length transferred to
      make sure all data acquired is received.
    '''
    c_data = (c_uint8 * length)(); c_xfer = c_int(0)
    err = _mx_ccif.mx_ccif_data_read(self.handle, chip_id, flow_id, c_data, length, byref(c_xfer), timeout)
    if err:
      raise Exception(err)
    if int(c_xfer.value) != length:
      raise Exception(-1)
    return bytearray(c_data)

  def _show_rb(self, length: int=16384) -> None:
    '''Read out shared ring buffer data for debug purpose. Regardless of
      pointers' position, always starts from offset = 0. To be noticed, if the
      background workers are still running, data might be unstable during the 
      dump process.
    '''
    c_data = (c_uint8 * length)()
    err = _mx_ccif.mx_ccif_diag_dump_shared_ring_buffer(self.handle, c_data, length)
    if err:
      raise Exception(err)
    data = bytearray(c_data)
    print('------------------------------------')
    for i in range(length):
      if not data:
        print(' --', end='')
      else:
        print(' {:02x}'.format(data[i]), end='')
      if i&0xf == 0xf:
        print('')
    print('------------------------------------')
    pass

