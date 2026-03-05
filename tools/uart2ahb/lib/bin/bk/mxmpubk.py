import os, re
from typing import Generator, Tuple, List
from app.lib.mxiobk import MxIo, any_to_bytes

# little help, use function instead of lambda due to typing hint only
def FIELD(x: int, msb: int, lsb: int) -> int:
  '''Pack data to specific bit location'''
  return (x & ((1<<(msb-lsb+1))-1)) << lsb

#------------------------------------------------------------------------------
# ip portfolio
#------------------------------------------------------------------------------
class IngDCore:
  '''MPU ingress DCore configuration'''
  def __init__(self) -> None:
    self.c_ing_format_type = 0
    self.c_ing_yuv422_signed_uv = 0
    self.c_ing_frame_packed_gbf80 = 0
    self.c_ing_ifmap_w_m1 = 0
    self.c_ing_ifmap_h_m1 = 0
    self.c_ing_ifmap_z_multof8_m8 = 0
    self.c_ing_sram_base_bus_addr = 0
    self.c_ing_sram_max_bus_addr = 0

class EgrDCore:
  '''MPU egress DCore configuration'''
  def __init__(self) -> None:
    self.c_egr_ifmap_w_m1 = 0
    self.c_egr_ifmap_h_m1 = 0
    self.c_egr_ifmap_z_multof8_m8 = 0
    self.c_egr_frame_packed_gbf80 = 0
    self.c_egr_sram_base_bus_addr = 0
    self.c_egr_sram_max_bus_addr = 0

class DCoreTop:
  '''MPU ingress and egress DCore common configuration'''
  def __init__(self, ing_dcore_num: int, egr_dcore_num: int) -> None:
    self.ing_dcores = [IngDCore() for _ in range(ing_dcore_num)]
    self.egr_dcores = [EgrDCore() for _ in range(egr_dcore_num)]
    self.c_ing_arb_always_move_forward = 0
    self.c_ing_en_flow_num = 0
    self.c_egr_skip_token_chk = 0
    self.c_egr_en_flow_num = 0
    self.cfg_ing_start = 0
    self.cfg_egr_start = 0

class MCore:
  '''MPU MCore configuration'''
  def __init__(self) -> None:
    self.c_op_type = 0
    self.c_fm_read_from = 0
    self.c_fm_write_to = 0
    self.c_special_dense = 0
    self.c_pool_type = 0
    self.c_act_type = 0
    self.c_up_type = 0
    self.c_wt_sharing = 0
    self.c_wt_read_from = 0
    self.c_wt_size = 0
    self.c_ifmap_w_m1 = 0
    self.c_ifmap_h_m1 = 0
    self.c_ofmap_w_m1 = 0
    self.c_ofmap_h_m1 = 0
    self.c_k_offset_x_min = 0
    self.c_k_offset_x_max = 0
    self.c_k_offset_y_min = 0
    self.c_k_offset_y_max = 0
    self.c_start_ch = 0
    self.c_ich_cnt_m1 = 0
    self.c_och_cnt_m1 = 0
    self.c_vch_cnt_m1 = 0
    self.c_kc_x_min = 0
    self.c_kc_x_max = 0
    self.c_kc_y_min = 0
    self.c_kc_y_max = 0
    self.c_kc_x_step = 0
    self.c_kc_x_stop_share = 0
    self.c_kc_y_step = 0
    self.c_kc_y_stop_share = 0
    self.c_ofmap_x_min = 0
    self.c_ofmap_x_step = 0
    self.c_ofmap_y_min = 0
    self.c_ofmap_y_step = 0
    self.c_pool_x_cnt_m1 = 0
    self.c_pool_y_cnt_m1 = 0
    self.c_dilation_x = 0
    self.c_dilation_y = 0
    self.c_up_x_cnt_m1 = 0
    self.c_up_y_cnt_m1 = 0
    self.c_relu_max = 0
    self.c_pad_value = 0
    self.c_wt_base_addr = 0
    self.c_fm_rd_base_addr = 0
    self.c_fm_rd_max_offset = 0
    self.c_fm_wb_base_addr = 0
    self.c_fm_wb_max_offset = 0
    self.c_fm_rd_y_buf_row_num = 0
    self.c_fm_wb_y_buf_row_num = 0
    self.c_dense_dec_step_m1 = 0
    self.c_wt_1vch_h_m1 = 0

class ACore:
  '''MPU ACore configuration'''
  def __init__(self) -> None:
    self.c_acore_en = 0
    self.c_arg_a_l0r1 = 0
    self.c_arg_b_l0r1 = 0
    self.c_output_l0r1 = 0
    self.c_do_clip_min = 0
    self.c_do_clip_max = 0
    self.c_main_op = 0
    self.c_constant = 0
    self.c_fmap_a_height_m1 = 0
    self.c_fmap_a_width_m1 = 0
    self.c_fmap_b_height_m1 = 0
    self.c_fmap_b_width_m1 = 0
    self.c_fmap_out_height_m1 = 0
    self.c_fmap_out_width_m1 = 0
    self.c_assigned_a_ch_d8_m1 = 0
    self.c_assigned_b_ch_d8_m1 = 0
    self.c_fmap_a_ch_d8_m1 = 0
    self.c_fmap_b_ch_d8_m1 = 0
    self.c_fmap_out_ch_d8_m1 = 0
    self.c_assigned_out_ch_d8_m1 = 0
    self.c_fetch_a_start_ch_d8 = 0
    self.c_fetch_b_start_ch_d8 = 0
    self.c_wb_start_ch_d8 = 0
    self.c_final_ch_chunk_num_run_steps = 0
    self.c_final_ch_chunk_valid_mask = 0
    self.c_arg_a_start_addr = 0
    self.c_arg_a_region_size = 0
    self.c_arg_b_start_addr = 0
    self.c_arg_b_region_size = 0
    self.c_wb_start_addr = 0
    self.c_wb_region_size = 0
    self.c_clip_min = 0
    self.c_clip_max = 0

class CoreGroup:
  '''MPU Core Group configuration'''
  def __init__(self, mcore_num: int, acore_num: int) -> None:
    self.mcores = [MCore() for _ in range(mcore_num)]
    self.acores = [ACore() for _ in range(acore_num)]
    self.cfg_mcore_clk_dis = 0 # width = mcore_num
    self.cfg_acore_clk_dis = 0 # width = acore_num
    self.cfg_mcore_start = 0 # width = mcore_num
    self.cfg_acore_start = 0 # width = acore_num
    self.wt_mem:List[int] = []

class ILC:
  '''MPU ILC configuration'''

  class IngDCore:
    '''MPU ILC ingress DCore layer configuration'''
    def __init__(self) -> None:
      self.c_layer_index_ing_dcore = 0

  class EgrDCore:
    '''MPU ILC egress DCore layer configuration'''
    def __init__(self) -> None:
      self.c_layer_index_egr_dcore = 0    

  class DCoreTop:
    '''MPU ILC ingress and egress DCore layer configuation'''
    def __init__(self, ing_dcore_num: int, egr_dcore_num: int) -> None:
      self.ing_dcores = [ILC.IngDCore() for _ in range(ing_dcore_num)]
      self.egr_dcores = [ILC.EgrDCore() for _ in range(egr_dcore_num)]

  class MCore:
    '''MPU ILC MCore layer configuration'''
    def __init__(self) -> None:
      self.c_layer_index_mcore_inc = 0
      self.c_layer_index_mcore_dec = 0

  class ACore:
    '''MPU ILC ACore layer configuration'''
    def __init__(self) -> None:
      self.c_layer_index_acore_inc = 0
      self.c_layer_index_acore_dec_ch0 = 0
      self.c_layer_index_acore_dec_ch1 = 0
  
  class CoreGroup:
    '''MPU ILC Core Group layer configuration'''
    def __init__(self, mcore_num: int, acore_num: int) -> None:
      self.mcores = [ILC.MCore() for _ in range(mcore_num)]
      self.acores = [ILC.ACore() for _ in range(acore_num)]

  class Layer:
    '''MPU ILC Feature Map Tower X Layer configuration'''
    def __init__(self) -> None:
      self.c_tx_init = 1
      self.c_tx_min = 1
      self.c_tx_max = 1000
      self.c_tx_inc_cnt_m1 = 0
      self.c_tx_dec_cnt_m1 = 0
      self.c_tx_inc_step = 1
      self.c_tx_dec_step = 1

  def __init__(self, ing_dcore_num: int, egr_dcore_num: int, core_group_num: int, core_group_mcore_num: int, core_group_acore_num: int, ilc_tower_num: int, ilc_layer_num: int) -> None:
    self.dcore_top = ILC.DCoreTop(ing_dcore_num, egr_dcore_num)
    self.core_groups = [ILC.CoreGroup(core_group_mcore_num, core_group_acore_num) for _ in range(core_group_num)]
    self.layers = [[ILC.Layer() for _ in range(ilc_layer_num)] for _tower in range(ilc_tower_num)]
    self.cfg_start = 0

#------------------------------------------------------------------------------
# mpu io wrapper
#------------------------------------------------------------------------------
class MpuIoWrapper:
  '''MPU IO wrapper for logging purpose'''

  @staticmethod
  def mpu_write(mpu_port: MxIo, addr: int, data: List[int], bylen: int=0, inc: int=1, show: int=0) -> None:
    '''Write data to MPU'''
    if not data:
      return # skip no data to write
    bybuf = any_to_bytes(data)
    bylen = len(bybuf) if not bylen else bylen
    if show:
      print(' + 0x{:08x} < {}'.format(addr, bylen), end='')
      print(' = 0x{:x}'.format(data) if show>=2 else '')
    mpu_port.write_burst(addr, bybuf, bylen, inc)

  @staticmethod
  def mpu_read(mpu_port: MxIo, addr: int, bylen: int=4, inc: int=1, show: int=0) -> int:
    '''Read data from MPU'''
    data = mpu_port.read_burst(addr, bylen, inc)
    if show:
      print(' + 0x{:08x} > {}'.format(addr, bylen), end='')
      print(' = 0x{:x}'.format(data) if show>=2 else '')
    return data

#------------------------------------------------------------------------------
# soc
#------------------------------------------------------------------------------
class Cascade:
  '''Cascade MPU configuration'''

  ING_DCORE_NUM = 15
  '''Cascade MPU ingress DCore number'''
  EGR_DCORE_NUM = 15
  '''Cascade MPU egress DCore number'''
  CORE_GROUP_NUM = 6
  '''Cascade MPU CoreGroup number'''
  CORE_GROUP_MCORE_NUM = 32
  '''Cascade MPU MCore number per CoreGroup'''
  CORE_GROUP_ACORE_NUM = 16
  '''Cascade MPU ACore number per CoreGroup'''
  ILC_TOWER_NUM = 3
  '''Cascade MPU ILC Tower number'''
  ILC_LAYER_NUM = 64
  '''Cascade MPU Feature Map Layer number per Tower'''

  ING_DCORE_FIFO_ADDR = 0x5000_0000
  '''Cascade MPU ingress DCore FIFO base address'''
  ING_DCORE_ADDR = 0x5010_0000
  '''Cascade MPU ingress DCore base address'''
  ING_DCORE_PORT_OFST = 0x0000_2000
  '''Cascade MPU ingress DCore port control offset'''
  ING_DCORE_PORT_SIZE = 0x0000_0100
  '''Cascade MPU ingress DCore port control size'''
  ING_DCORE_START_OFST = 0x0000_0000
  '''Cascade MPU ingress DCore start control offset'''
  EGR_DCORE_ADDR = 0x5020_0000
  '''Cascade MPU egress DCore base address'''
  EGR_DCORE_PORT_OFST = 0x0000_2000
  '''Cascade MPU egress DCore port control offset'''
  EGR_DCORE_PORT_SIZE = 0x0000_0100
  '''Cascade MPU eress DCore port control size'''
  EGR_DCORE_START_OFST = 0x0000_0000
  '''Cascade MPU egress DCore start control offset'''

  ILC_TOWER_ADDR = 0x5030_0000
  '''Cascade MPU ILC Tower bank address'''
  ILC_TOWER_SIZE = 0x0000_1000
  '''Cascade MPU ILC Tower bank size'''
  ILC_ING_DCORE_ADDR = 0x5030_3000
  '''Cascade MPU ILC ingress DCore bank address'''
  ILC_EGR_DCORE_ADDR = 0x5030_3040
  '''Cascade MPU ILC egress DCore bank address'''
  ILC_CORE_GROUP_MCORE_ADDR = 0x5030_4000
  '''Cascade MPU ILC CoreGroup MCore bank address'''
  ILC_CORE_GROUP_MCORE_SIZE = 0x0000_0100
  '''Cascade MPU ILC CoreGroup MCore bank size'''
  ILC_CORE_GROUP_ACORE_ADDR = 0x5030_5000
  '''Cascade MPU ILC CoreGroup ACore bank address'''
  ILC_CORE_GROUP_ACORE_SIZE = 0x0000_0100
  '''Cascade MPU ILC CoreGroup ACore bank size'''
  ILC_START_ADDR = 0x5030_6000
  '''Cascade MPU ILC start control address'''

  CORE_GROUP_ADDR = 0x5100_0000
  '''Cascade MPU CoreGroup base address'''
  CORE_GROUP_SIZE = 0x0100_0000
  '''Cascade MPU CoreGroup bank size'''
  CORE_GROUP_TOP_MCORE_START_OFST = 0x0080_0008
  '''Cascade MPU CoreGroup Top MCore start control offset'''
  CORE_GROUP_TOP_ACORE_START_OFST = 0x0080_000c
  '''Cascade MPU CoreGroup Top ACore start control offset'''
  CORE_GROUP_MCORE_OFST = 0x0090_0000
  '''Cascade MPU CoreGroup MCore bank offset'''
  CORE_GROUP_MCORE_SIZE = 0x0001_0000
  '''Cascade MPU CoreGroup MCore bank size'''
  CORE_GROUP_ACORE_OFST = 0x00b0_0000
  '''Cascade MPU CoreGroup ACore bank offset'''
  CORE_GROUP_ACORE_SIZE = 0x0001_0000
  '''Cascade MPU CoreGroup ACore bank size'''

  EGR_DCORE_REDIRECT_SRAM_ADDR = 0x2000_0000
  '''Cascade MPU egress DCore output redirect to SRAM base address'''
  EGR_DCORE_REDIRECT_SRAM_SIZE = 0x0000_2000
  '''Cascade MPU egress DCore output redirect to SRAM size per channel'''

  CORE_GROUP_FMAP_SRAM_ADDR = 0x5880_0000
  '''Cascade MPU intermediate feature map SRAM base address'''
  CORE_GROUP_FMAP_SRAM_OFST = 0x0100_0000
  '''Cascade MPU intermediate feature map SRAM offset per group'''

  def __init__(self) -> None:
    self.dcore_top = DCoreTop(self.ING_DCORE_NUM, self.EGR_DCORE_NUM)
    self.core_groups = [CoreGroup(self.CORE_GROUP_MCORE_NUM, self.CORE_GROUP_ACORE_NUM) for _ in range(self.CORE_GROUP_NUM)]
    self.ilc = ILC(self.ING_DCORE_NUM, self.EGR_DCORE_NUM, self.CORE_GROUP_NUM, self.CORE_GROUP_MCORE_NUM, self.CORE_GROUP_ACORE_NUM, self.ILC_TOWER_NUM, self.ILC_LAYER_NUM)

  def get_config_core_group_weights(self) -> Generator[Tuple[int,List[int]],None,None]:
    '''Return continuous words for specific Core Group weight memory. Use
      generator to pop out all Core Group's weight memory.
    '''
    words:List[int] = []
    for core_group_idx, core_group in enumerate(self.core_groups):
      core_group_weight_addr = self.CORE_GROUP_ADDR + self.CORE_GROUP_SIZE*core_group_idx
      # convert entry to words (1 entry = 512 bits) in order to align to other
      # configuration output format
      words.clear()
      for wt_mem in core_group.wt_mem:
        for lsb in range(0, 512, 32):
          words.append((wt_mem >> lsb)& 0xffffffff)
      yield core_group_weight_addr, words
    pass

  def get_config_ing_dcores(self) -> Generator[Tuple[int,List[int]],None,None]:
    '''Return continuous words for all ingress DCore configuration. There are
      two parts exist, cross-core configuration and by-core configuration.
    '''
    words:List[int] = []
    # cross-core config.
    for _ in range(1):
      ing_dcore_cmn_addr = self.ING_DCORE_ADDR
      words.clear()
      words.append(0 # 0x000: words[0]
        | 0) # cfg_ing_start, do not trigger here before core config. is ready
      words.append(0 # 0x004: words[1]
        | FIELD(self.dcore_top.c_ing_en_flow_num, 3, 0)
        | FIELD(self.dcore_top.c_ing_arb_always_move_forward, 4, 4))
      yield ing_dcore_cmn_addr, words
    # by-core config.
    for dcore_idx, dcore in enumerate(self.dcore_top.ing_dcores):
      ing_dcore_addr = self.ING_DCORE_ADDR + self.ING_DCORE_PORT_OFST + self.ING_DCORE_PORT_SIZE*dcore_idx
      words.clear()
      words.append(0 # 0x000: words[0]
        | FIELD(dcore.c_ing_format_type, 1, 0)
        | FIELD(dcore.c_ing_yuv422_signed_uv, 4, 4)
        | FIELD(dcore.c_ing_frame_packed_gbf80, 5, 5))
      words.append(0 # 0x004: words[1]
        | FIELD(dcore.c_ing_ifmap_w_m1, 15, 0)
        | FIELD(dcore.c_ing_ifmap_h_m1, 31, 16))
      words.append(0 # 0x008: words[2]
        | FIELD(dcore.c_ing_ifmap_z_multof8_m8, 15, 0))
      words.append(0 # 0x00c: words[3]
        | FIELD(dcore.c_ing_sram_base_bus_addr, 27, 0))
      words.append(0 # 0x010: words[4]
        | FIELD(dcore.c_ing_sram_max_bus_addr, 27, 0))
      yield ing_dcore_addr, words
    pass

  def get_config_egr_dcores(self) -> Generator[Tuple[int,List[int]],None,None]:
    '''Return continuous words for all egress DCore configuration. There are
      two parts exist, cross-core configuration and by-core configuration.
    '''
    words:List[int] = []
    # cross-core config.
    for _ in range(1):
      egr_dcore_cmn_addr = self.EGR_DCORE_ADDR
      words.clear()
      words.append(0 # 0x000: words[0]
        | 0) # cfg_egr_start, do not trigger here before core config. is ready
      words.append(0 # 0x004: words[1]
        | FIELD(self.dcore_top.c_egr_en_flow_num, 3, 0)
        | FIELD(self.dcore_top.c_egr_skip_token_chk, 5, 5))
      yield egr_dcore_cmn_addr, words
    # by-core config.
    for dcore_idx, dcore in enumerate(self.dcore_top.egr_dcores):
      egr_dcore_addr = self.EGR_DCORE_ADDR + self.EGR_DCORE_PORT_OFST + self.EGR_DCORE_PORT_SIZE*dcore_idx
      words.clear()
      words.append(0 # 0x000: words[0]
        | FIELD(dcore.c_egr_ifmap_w_m1, 15, 0)
        | FIELD(dcore.c_egr_ifmap_h_m1, 31, 16))
      words.append(0 # 0x004: words[1]
        | FIELD(dcore.c_egr_ifmap_z_multof8_m8, 15, 0)
        | FIELD(dcore.c_egr_frame_packed_gbf80, 16, 16))
      words.append(0 # 0x008: words[2]
        | FIELD(dcore.c_egr_sram_base_bus_addr, 27, 0))
      words.append(0 # 0x00c: words[3]
        | FIELD(dcore.c_egr_sram_max_bus_addr, 27, 0))
      yield egr_dcore_addr, words
    pass

  def get_config_core_group_mcores(self) -> Generator[Tuple[int,List[int]],None,None]:
    '''Return continuous words for specific MCore configuration. Use generator
      to pop out all MCores' configuration.
    '''
    words:List[int] = []
    # calc. configuration base address for specified mcore
    for core_group_idx, core_group in enumerate(self.core_groups):
      core_group_addr = self.CORE_GROUP_ADDR + self.CORE_GROUP_SIZE*core_group_idx
      for mcore_idx, mcore in enumerate(core_group.mcores):
        core_group_mcore_ofst = self.CORE_GROUP_MCORE_OFST + self.CORE_GROUP_MCORE_SIZE*mcore_idx
        core_group_mcore_addr = core_group_addr + core_group_mcore_ofst
        # pack configuration of specified mcore into words
        words.clear()
        words.append(0 # 0x000: words[0]
          | FIELD(mcore.c_pool_type, 27, 24)
          | FIELD(mcore.c_up_type, 23, 20)
          | FIELD(mcore.c_act_type, 19, 16)
          | FIELD(mcore.c_op_type, 11, 8)
          | FIELD(mcore.c_fm_write_to, 7, 7)
          | FIELD(mcore.c_fm_read_from, 6, 6)
          | FIELD(mcore.c_wt_sharing, 5, 5)
          | FIELD(mcore.c_wt_read_from, 4, 4)
          | FIELD(mcore.c_special_dense, 3, 3)
          | FIELD(mcore.c_wt_size, 1, 1))
        words.append(0 # 0x004: words[1]
          | FIELD(mcore.c_ifmap_w_m1, 27, 16)
          | FIELD(mcore.c_ifmap_h_m1, 11, 0))
        words.append(0 # 0x008: words[2]
          | FIELD(mcore.c_ofmap_w_m1, 27, 16)
          | FIELD(mcore.c_ofmap_h_m1, 11, 0))
        words.append(0 # 0x00c: words[3]
          | FIELD(mcore.c_k_offset_x_max, 24, 16)
          | FIELD(mcore.c_k_offset_x_min, 8, 0))
        words.append(0 # 0x010: words[4]
          | FIELD(mcore.c_k_offset_y_max, 24, 16)
          | FIELD(mcore.c_k_offset_y_min, 8, 0))
        words.append(0 # 0x014: words[5]
          | FIELD(mcore.c_start_ch, 17, 0))
        words.append(0 # 0x018: words[6]
          | FIELD(mcore.c_ich_cnt_m1, 17, 0))
        words.append(0 # 0x01c: words[7]
          | FIELD(mcore.c_och_cnt_m1, 17, 0))
        words.append(0 # 0x020: words[8]
          | FIELD(mcore.c_vch_cnt_m1, 17, 0))
        words.append(0 # 0x024: words[9]
          | FIELD(mcore.c_kc_x_max, 28, 16)
          | FIELD(mcore.c_kc_x_min, 12, 0))
        words.append(0 # 0x028: words[10]
          | FIELD(mcore.c_kc_y_max, 28, 16)
          | FIELD(mcore.c_kc_y_min, 12, 0))
        words.append(0 # 0x02c: words[11]
          | FIELD(mcore.c_kc_x_stop_share, 28, 16)
          | FIELD(mcore.c_kc_x_step, 9, 0))
        words.append(0 # 0x030: words[12]
          | FIELD(mcore.c_kc_y_stop_share, 28, 16)
          | FIELD(mcore.c_kc_y_step, 9, 0))
        words.append(0 # 0x034: words[13]
          | FIELD(mcore.c_ofmap_x_step, 25, 16)
          | FIELD(mcore.c_ofmap_x_min, 12, 0))
        words.append(0 # 0x038: words[14]
          | FIELD(mcore.c_ofmap_y_step, 25, 16)
          | FIELD(mcore.c_ofmap_y_min, 12, 0))
        words.append(0 # 0x03c: words[15]
          | FIELD(mcore.c_pool_y_cnt_m1, 24, 16)
          | FIELD(mcore.c_pool_x_cnt_m1, 8, 0))
        words.append(0 # 0x040: words[16]
          | FIELD(mcore.c_up_y_cnt_m1, 27, 24)
          | FIELD(mcore.c_up_x_cnt_m1, 19, 16)
          | FIELD(mcore.c_dilation_y, 11, 8)
          | FIELD(mcore.c_dilation_x, 3, 0))
        words.append(0 # 0x044: words[17]
          | FIELD(mcore.c_pad_value, 31, 16)
          | FIELD(mcore.c_relu_max, 15, 0))
        words.append(0 # 0x048: words[18]
          | FIELD(mcore.c_wt_base_addr, 14, 0))
        words.append(0 # 0x04c: words[19]
          | FIELD(mcore.c_fm_rd_base_addr, 15, 0))
        words.append(0 # 0x050: words[20]
          | FIELD(mcore.c_fm_rd_max_offset, 15, 0))
        words.append(0 # 0x054: words[21]
          | FIELD(mcore.c_fm_wb_base_addr, 15, 0))
        words.append(0 # 0x058: words[22]
          | FIELD(mcore.c_fm_wb_max_offset, 15, 0))
        words.append(0 # 0x05c: words[23]
          | FIELD(mcore.c_fm_rd_y_buf_row_num, 27, 16)
          | FIELD(mcore.c_fm_wb_y_buf_row_num, 11, 0))
        words.append(0 # 0x060: words[24]
          | FIELD(mcore.c_dense_dec_step_m1, 15, 0))
        words.append(0 # 0x064: words[25]
          | FIELD(mcore.c_wt_1vch_h_m1, 14, 0))
        yield core_group_mcore_addr, words
    pass

  def get_config_core_group_acores(self) -> Generator[Tuple[int,List[int]],None,None]:
    '''Return continuous words for specific ACore configuration. Use generator
      to pop out all ACores' configuration.
    '''
    words:List[int] = []
    # calc. configuration base address for specified acore
    for core_group_idx, core_group in enumerate(self.core_groups):
      core_group_addr = self.CORE_GROUP_ADDR + self.CORE_GROUP_SIZE*core_group_idx
      for acore_idx, acore in enumerate(core_group.acores):
        core_group_acore_ofst = self.CORE_GROUP_ACORE_OFST + self.CORE_GROUP_ACORE_SIZE*acore_idx
        core_group_acore_addr = core_group_addr + core_group_acore_ofst
        # pack configuration of specified acore into words
        words.clear()
        words.append(0 # 0x000: words[0]
          | FIELD(acore.c_constant, 31, 16)
          | FIELD(acore.c_do_clip_max, 10, 10)
          | FIELD(acore.c_do_clip_min, 9, 9)
          | FIELD(acore.c_main_op, 8, 5)
          | FIELD(acore.c_output_l0r1, 3, 3)
          | FIELD(acore.c_arg_b_l0r1, 2, 2)
          | FIELD(acore.c_arg_a_l0r1, 1, 1)
          | FIELD(acore.c_acore_en, 0, 0))
        words.append(0 # 0x004: words[1]
          | FIELD(acore.c_fmap_a_height_m1, 27, 16)
          | FIELD(acore.c_fmap_a_width_m1, 11, 0))
        words.append(0 # 0x008: words[2]
          | FIELD(acore.c_fmap_b_height_m1, 27, 16)
          | FIELD(acore.c_fmap_b_width_m1, 11, 0))
        words.append(0 # 0x00c: words[3]
          | FIELD(acore.c_fmap_out_height_m1, 27, 16)
          | FIELD(acore.c_fmap_out_width_m1, 11, 0))
        words.append(0 # 0x010: words[4]
          | FIELD(acore.c_assigned_b_ch_d8_m1, 29, 15)
          | FIELD(acore.c_assigned_a_ch_d8_m1, 14, 0))
        words.append(0 # 0x014: words[5]
          | FIELD(acore.c_fmap_b_ch_d8_m1, 29, 15)
          | FIELD(acore.c_fmap_a_ch_d8_m1, 14, 0))
        words.append(0 # 0x018: words[6]
          | FIELD(acore.c_assigned_out_ch_d8_m1, 29, 15)
          | FIELD(acore.c_fmap_out_ch_d8_m1, 14, 0))
        words.append(0 # 0x01c: words[7]
          | FIELD(acore.c_fetch_b_start_ch_d8, 29, 15)
          | FIELD(acore.c_fetch_a_start_ch_d8, 14, 0))
        words.append(0 # 0x020: words[8]
          | FIELD(acore.c_wb_start_ch_d8, 14, 0))
        words.append(0 # 0x024: words[9]
          | FIELD(acore.c_final_ch_chunk_valid_mask, 11, 4)
          | FIELD(acore.c_final_ch_chunk_num_run_steps, 2, 0))
        words.append(0 # 0x028: words[10]
          | FIELD(acore.c_arg_a_start_addr, 16, 0))
        words.append(0 # 0x02c: words[11]
          | FIELD(acore.c_arg_a_region_size, 16, 0))
        words.append(0 # 0x030: words[12]
          | FIELD(acore.c_arg_b_start_addr, 16, 0))
        words.append(0 # 0x034: words[13]
          | FIELD(acore.c_arg_b_region_size, 16, 0))
        words.append(0 # 0x038: words[14]
          | FIELD(acore.c_wb_start_addr, 16, 0))
        words.append(0 # 0x03c: words[15]
          | FIELD(acore.c_wb_region_size, 16, 0))
        words.append(0 # 0x040: words[16]
          | FIELD(acore.c_clip_max, 31, 16)
          | FIELD(acore.c_clip_min, 15, 0))
        yield core_group_acore_addr, words
    pass

  def get_config_ilcs(self) -> Generator[Tuple[int,List[int]],None,None]:
    '''Return continuous words for all ILC configurations. Since there are
      multiple parts setting including Feature Map Layers, DCores, MCores and
      ACores, here we use generator to pop out configuration words.
    '''
    words:List[int] = []
    # ILC Layer
    for tower_idx, tower in enumerate(self.ilc.layers):
      words.clear()
      ilc_tower_addr = self.ILC_TOWER_ADDR + self.ILC_TOWER_SIZE*tower_idx
      for layer in tower:
        words.append(0 # 0x000: words[0]
          | FIELD(layer.c_tx_init, 15, 0))
        words.append(0 # 0x004: words[1]
          | FIELD(layer.c_tx_min, 15, 0)
          | FIELD(layer.c_tx_max, 31, 16))
        words.append(0 # 0x008: words[2]
          | FIELD(layer.c_tx_inc_cnt_m1, 15, 0)
          | FIELD(layer.c_tx_dec_cnt_m1, 31, 16))
        words.append(0 # 0x00c: words[3]
          | FIELD(layer.c_tx_inc_step, 15, 0)
          | FIELD(layer.c_tx_dec_step, 31, 16))
      yield ilc_tower_addr, words
    # ILC ingress DCore
    for _ in range(1): # just to align to the same indent
      words.clear()
      ilc_ing_dcore_addr = self.ILC_ING_DCORE_ADDR
      for dcore in self.ilc.dcore_top.ing_dcores:
        words.append(0 # 0x000: words[0]
          | FIELD(dcore.c_layer_index_ing_dcore, 5, 0))
      yield ilc_ing_dcore_addr, words
    # ILC egress DCore
    for _ in range(1):
      words.clear()
      ilc_egr_dcore_addr = self.ILC_EGR_DCORE_ADDR
      for dcore in self.ilc.dcore_top.egr_dcores:
        words.append(0 # 0x000: words[0]
          | FIELD(dcore.c_layer_index_egr_dcore, 5, 0))
      yield ilc_egr_dcore_addr, words
    # ILC CoreGroup MCore
    for core_group_idx, core_group in enumerate(self.ilc.core_groups):
      words.clear()
      ilc_core_group_mcore_addr = self.ILC_CORE_GROUP_MCORE_ADDR + self.ILC_CORE_GROUP_MCORE_SIZE*core_group_idx
      for mcore in core_group.mcores:
        words.append(0 # 0x000: words[0]
          | FIELD(mcore.c_layer_index_mcore_inc, 5, 0)
          | FIELD(mcore.c_layer_index_mcore_dec, 11, 6))
      yield ilc_core_group_mcore_addr, words
    # ILC CoreGroup ACore
    for core_group_idx, core_group in enumerate(self.ilc.core_groups):
      words.clear()
      ilc_core_group_acore_addr = self.ILC_CORE_GROUP_ACORE_ADDR + self.ILC_CORE_GROUP_ACORE_SIZE*core_group_idx
      for acore in core_group.acores:
        words.append(0 # 0x000: words[0]
          | FIELD(acore.c_layer_index_acore_inc, 5, 0)
          | FIELD(acore.c_layer_index_acore_dec_ch0, 11, 6)
          | FIELD(acore.c_layer_index_acore_dec_ch1, 17, 12))
      yield ilc_core_group_acore_addr, words
    pass

  def get_config_stops(self) -> Generator[Tuple[int,List[int]],None,None]:
    '''Special one to generate configuration to disable all Cores and ILC.
      Module disable order needs to be specific.
    '''
    words:List[int] = []
    # CoreGroup MCore
    for core_group_idx, core_group in enumerate(self.core_groups):
      core_group_addr = self.CORE_GROUP_ADDR + self.CORE_GROUP_SIZE*core_group_idx
      core_group_top_mcore_start_addr = core_group_addr + self.CORE_GROUP_TOP_MCORE_START_OFST
      words.clear()
      words.append(0 # 0x000: words[0]
        | 0) # bitmap
      yield core_group_top_mcore_start_addr, words
    # CoreGroup ACore
    for core_group_idx, core_group in enumerate(self.core_groups):
      core_group_addr = self.CORE_GROUP_ADDR + self.CORE_GROUP_SIZE*core_group_idx
      core_group_top_acore_start_addr = core_group_addr + self.CORE_GROUP_TOP_ACORE_START_OFST
      words.clear()
      words.append(0 # 0x000: words[0]
        | 0) # bitmap
      yield core_group_top_acore_start_addr, words
    # ingress DCore
    for _ in range(1):
      dcore_top_ing_dcore_start_addr = self.ING_DCORE_ADDR + self.ING_DCORE_START_OFST
      words.clear()
      words.append(0 # 0x000: words[0]
        | 0) # bitmap
      yield dcore_top_ing_dcore_start_addr, words
    # egress DCore
    for _ in range(1):
      dcore_top_egr_dcore_start_addr = self.EGR_DCORE_ADDR + self.EGR_DCORE_START_OFST
      words.clear()
      words.append(0 # 0x000: words[0]
        | 0) # bitmap
      yield dcore_top_egr_dcore_start_addr, words
    # ILC
    for _ in range(1):
      ilc_start_addr = self.ILC_START_ADDR
      words.clear()
      words.append(0 # 0x000: words[0]
        | 0) # bitmap
      yield ilc_start_addr, words
    pass

  def get_config_starts(self) -> Generator[Tuple[int,List[int]],None,None]:
    '''Based on current Cores enabled to generate correspond Cores and ILC start
      configuration. Module enable order needs to be specific.
    '''
    words:List[int] = []
    # ILC - enable at the first for flow control
    for _ in range(1):
      ilc_start_addr = self.ILC_START_ADDR
      words.clear()
      words.append(0 # 0x000: words[0]
        | FIELD(self.ilc.cfg_start, 0, 0))
      yield ilc_start_addr, words
    # CoreGroup MCore
    for core_group_idx, core_group in enumerate(self.core_groups):
      core_group_addr = self.CORE_GROUP_ADDR + self.CORE_GROUP_SIZE*core_group_idx
      core_group_top_mcore_start_addr = core_group_addr + self.CORE_GROUP_TOP_MCORE_START_OFST
      words.clear()
      words.append(0 # 0x000: words[0]
        | FIELD(core_group.cfg_mcore_start, self.CORE_GROUP_MCORE_NUM-1, 0))
      yield core_group_top_mcore_start_addr, words
    # CoreGroup ACore
    for core_group_idx, core_group in enumerate(self.core_groups):
      core_group_addr = self.CORE_GROUP_ADDR + self.CORE_GROUP_SIZE*core_group_idx
      core_group_top_acore_start_addr = core_group_addr + self.CORE_GROUP_TOP_ACORE_START_OFST
      words.clear()
      words.append(0 # 0x000: words[0]
        | FIELD(core_group.cfg_acore_start, self.CORE_GROUP_ACORE_NUM-1, 0))
      yield core_group_top_acore_start_addr, words
    # ingress DCore
    for _ in range(1):
      dcore_top_ing_dcore_start_addr = self.ING_DCORE_ADDR + self.ING_DCORE_START_OFST
      words.clear()
      words.append(0 # 0x000: words[0]
        | FIELD(self.dcore_top.cfg_ing_start, self.ING_DCORE_NUM-1, 0))
      yield dcore_top_ing_dcore_start_addr, words
    # egress DCore
    for _ in range(1):
      dcore_top_egr_dcore_start_addr = self.EGR_DCORE_ADDR + self.EGR_DCORE_START_OFST
      words.clear()
      words.append(0 # 0x000: words[0]
        | FIELD(self.dcore_top.cfg_egr_start, self.EGR_DCORE_NUM-1, 0))
      yield dcore_top_egr_dcore_start_addr, words
    pass

  def get_config_miscs(self) ->Generator[Tuple[int,List[int]],None,None]:
    '''For debug only some test configuration.
    '''
    words:List[int] = []
    # overwrite egress DCore output target to SRAM
    '''
    for _ in range(1):
      egr_dcore_out_addr = self.EGR_DCORE_ADDR + 0x170 # 92*4
      for dcore_idx, dcore in enumerate(self.egr_dcores):
        words += [ self.EGR_DCORE_REDIRECT_SRAM_ADDR + self.EGR_DCORE_REDIRECT_SRAM_SIZE*dcore_idx ]
      words += [ 1 ]
      yield egr_dcore_out_addr, words
    '''
    pass

  def write_config_to_device(self, mpu_port: MxIo, show: int=1) -> None:
    '''Write MPU configuration stored within this object to device through given
      IO access port.
    '''
    # stop
    if show: print('- mpu-stop')
    for addr, words in self.get_config_stops():
      MpuIoWrapper.mpu_write(mpu_port, addr, words, show=show)

    # core group weight memory
    if show: print('- weigth')
    for addr, words in self.get_config_core_group_weights():
      MpuIoWrapper.mpu_write(mpu_port, addr, words, show=show)

    # ingress dcore
    if show: print('- ing-dcore')
    for addr, words in self.get_config_ing_dcores():
      MpuIoWrapper.mpu_write(mpu_port, addr, words, show=show)

    # egress dcore
    if show: print('- egr-dcore')
    for addr, words in self.get_config_egr_dcores():
      MpuIoWrapper.mpu_write(mpu_port, addr, words, show=show)

    # core group mcore
    if show: print('- cg-mcore')
    for addr, words in self.get_config_core_group_mcores():
      MpuIoWrapper.mpu_write(mpu_port, addr, words, show=show)

    # core group acore
    if show: print('- cg-acore')
    for addr, words in self.get_config_core_group_acores():
      MpuIoWrapper.mpu_write(mpu_port, addr, words, show=show)

    # ilc
    if show: print('- ilc')
    for addr, words in self.get_config_ilcs():
      MpuIoWrapper.mpu_write(mpu_port, addr, words, show=show)

    # misc
    if show: print('- misc')
    for addr, words in self.get_config_miscs():
      MpuIoWrapper.mpu_write(mpu_port, addr, words, show=show)

    # start
    if show: print('- mpu-start')
    for addr, words in self.get_config_starts():
      MpuIoWrapper.mpu_write(mpu_port, addr, words, show=show)

  def write_fmap_from_file_to_device(self, mpu_port: MxIo, input_channel: int, ifmap_file: str, show: int=1) -> None:
    '''Read feature map from file and concat all bytes into a long long byte
      array. Finally align to 4 bytes as a complete frame.
    '''
    data = 0; bylen = 0
    with open(ifmap_file, 'r') as fp:
      lines = fp.readlines()
      for i, line in enumerate(lines):
        data |= int(line, 16) << (80*i) # each entry is 80 bits
        bylen += 10
      bylen = (bylen+3) & ~0x3 # ceiling to 4 bytes finally
    MpuIoWrapper.mpu_write(mpu_port, self.ING_DCORE_FIFO_ADDR+(input_channel<<2), data, bylen, inc=0, show=show)

  def read_fmap_from_device_to_file(self, mpu_port: MxIo, output_channel: int, ofmap_file: str, show: int=1) -> Tuple[int,int]:
    '''Read output result from device SRAM to do comparison golden data. Will
      also output data read from data to given path for debug purpose.
    '''
    dcore = self.egr_dcores[output_channel] # z: 1~8=0, 2~16=1, ...
    bylen = (dcore.c_egr_ifmap_h_m1+1) * (dcore.c_egr_ifmap_w_m1+1) * (dcore.c_egr_ifmap_z_multof8_m8//8+1) * 10
    temp = data = MpuIoWrapper.mpu_read(mpu_port, self.EGR_DCORE_REDIRECT_SRAM_ADDR + self.EGR_DCORE_REDIRECT_SRAM_SIZE*output_channel, bylen, show=show)
    with open(ofmap_file, 'w') as fp:
      for bycnt in range(0, bylen, 10): # each entry is 80 bits
        fp.write('{:020X}'.format(temp&0xffff_ffffffff_ffffffff)+'\n')
        temp >>= 80
    return bylen, data

  def read_fmap_from_golden(self, ofmap_file: str) -> Tuple[int,int]:
    '''Read output golden data from given file to do comparison with read data.
    '''
    golden = 0; bylen = 0
    with open(ofmap_file, 'r') as fp:
      lines = fp.readlines()
      for i, line in enumerate(lines):
        golden |= int(line, 16) << (80*i)
        bylen += 10
    return bylen, golden

  def read_fmap_sram_from_device_to_file(self, mpu_port: MxIo, fmap_idx: int, fmap_file: str, entry_num: int=0x100) -> None:
    '''Dump data from inter-mediate SRAM for debug purpose. Currently the total
      size of SRAM is 2^16=64k entries with each entry equals 10 bytes. To write
      single SRAM entry, overall 80 bits must be written in order starting from
      address 0x0, 0x4 and finally 0x8 which will trigger HW to write 10 bytes
      data buffered into single entry. To read from single SRAM entry, also
      starting from address 0x0, 0x4 and 0x8 for the first 0x0 will trigger HW
      to latch 10 bytes data into buffer.
    '''
    bylen = entry_num << 4 # align to 4 bytes address
    core_group_fmap_addr = self.CORE_GROUP_FMAP_SRAM_ADDR + self.CORE_GROUP_FMAP_SRAM_OFST*fmap_idx
    with open(fmap_file, 'w') as fp: 
      # since mix compiler tends to use SRAM starting from 0x0000 and 0xe000
      # we will dump these two address with given length to save some time
      data = mpu_port.read_burst(core_group_fmap_addr+0x0000, bylen)
      for byofs in range(0x00, bylen, 0x10):
        fp.write('{:>5X} - {:>3} : {:20X}'.format(byofs+0x0000, byofs>>4, data&0xffff_ffffffff_ffffffff)+'\n')
        data >>= 128
      fp.write('='*35+'\n')
      fp.write(' ...'+'\n')
      fp.write('='*35+'\n')
      data = mpu_port.read_burst(core_group_fmap_addr+0xe000, bylen)
      for byofs in range(0x00, bylen, 0x10):
        fp.write('{:>5X} - {:>3} : {:20X}'.format(byofs+0xe000, byofs>>4, data&0xffff_ffffffff_ffffffff)+'\n')
        data >>= 128
    pass

#------------------------------------------------------------------------------
# fpga
#------------------------------------------------------------------------------
class CascadeVu9p(Cascade):
  '''Cascade on FPGA configuration. Due to FPGA bitstream size limitation, here
    we decrease MCore and ACore number within Core Group. All MIX configuration
    should be gernerated based on the decreased core number.
  '''
  CORE_GROUP_MCORE_NUM = 4
  '''Cascade MPU MCore number per Core Group on FPGA'''
  CORE_GROUP_ACORE_NUM = 2
  '''Cascade MPU ACore number per Core Group on FPGA'''

class CascadeVu19p(Cascade):
  '''Cascade on FPGA configuration. Due to FPGA bitstream size limitation, here
    we decrease MCore and ACore number within Core Group. All MIX configuration
    should be gernerated based on the decreased core number.
  '''
  CORE_GROUP_MCORE_NUM = 16
  '''Cascade MPU MCore number per Core Group on FPGA'''
  CORE_GROUP_ACORE_NUM = 8
  '''Cascade MPU ACore number per Core Group on FPGA'''

