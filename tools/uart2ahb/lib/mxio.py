from typing import Union, List

def any_to_bytes(data: Union[int,List[int],bytes,bytearray], bylen: int=4) -> bytearray:
  '''Convert integer, list of integer, bytes or bytearray to bytearray. Return
    byte array after convertion.
  '''
  if isinstance(data, int):
    data &= (1<<(bylen<<3)) - 1
    return bytearray(data.to_bytes(bylen, byteorder='little'))
  if isinstance(data, list):
    out = bytearray()
    for word in data:
      word &= 0xffffffff
      out += bytearray(word.to_bytes(4, byteorder='little'))
    return out
  if isinstance(data, bytes):
    return bytearray(data)
  if isinstance(data, bytearray):
    return data
  raise Exception('MxIo:any_to_bytes, data type not supported = {}'.format(type(data)))

def bytes_to_int(bybuf: bytearray) -> int:
  '''Convert byte array to integer.
  '''
  return int.from_bytes(bybuf, byteorder='little')

def bytes_to_list(bybuf: bytearray) -> List[int]:
  '''Convert byte arry to list of integers.
  '''
  out = []; bylen = (len(bybuf)+3)&~0x3
  for byofs in range(0, bylen, 4):
    out.append(bybuf[byofs:byofs+4])
  return out

def assert_equal(a: Union[int,bytearray], b: Union[int,bytearray], bylen: int=4) -> None:
  a, b = any_to_bytes(a, bylen), any_to_bytes(b, bylen)
  if a != b:
    print(len(a), len(b))
    a, b = bytes_to_int(a), bytes_to_int(b)
    print('')
    print('a={:x}'.format(a))
    print('b={:x}'.format(b))
    assert 0, '{:x} != {:x}'.format(a, b)
  pass

class MxIo:
  '''Memryx device basic IO interface. Read/write method should be implemented
    by handler who inherits this class.
  '''

  def write(self, addr: int, data: int, chip_id: int=1) -> None:
    '''Write 4 bytes data to specific 4 bytes address.
    '''
    raise NotImplementedError

  def write_burst(self, addr: int, bylen: int, bybuf: bytearray, chip_id: int=1, inc: int=1) -> None:
    '''Write multiple bytes data with address incremental.
    '''
    raise NotImplementedError

  def read(self, addr: int, chip_id: int=1) -> int:
    '''Read 4 bytes data from specific 4 bytes address.
    '''
    raise NotImplementedError

  def read_burst(self, addr: int, bylen: int, chip_id: int=1, inc: int=1) -> bytearray:
    '''Read multiple bytes data with address incremental.
    '''
    raise NotImplementedError

