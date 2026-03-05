import os, re
from lib.mxmpu import Cascade

class CascadeInterp:
  '''Parser to translate MIX compiler output MPU configuration into pre-defined
    Python data structure for later to configure to device.
  '''
  MIX_WEIGHTS_CONFIG_FILE = os.path.join('weights','cgroup_wmem_{}.hex')
  MIX_DCORES_CONFIG_FILE = os.path.join('config','dcores.txt')
  MIX_MCORES_CONFIG_FILE = os.path.join('config','mcores.txt')
  MIX_ACORES_CONFIG_FILE = os.path.join('config','acores.txt')
  MIX_ILCS_CONFIG_FILE = os.path.join('config','ilcs.txt')
  MIX_STARTS_CONFIG_FILE = os.path.join('config','starts.txt')

  @staticmethod
  def load_config_from_file_weights(mpu: Cascade, core_group_idx: int, mix_output_weights_config: str) -> None:
    '''Walk through the given file and load all weights setting into buffer.
      Each Core Group here may or may not have a weight memory file depends on
      if any of MCore within Core Group is enabled or not.
    '''
    with open(mix_output_weights_config, 'r') as fp:
      lines = fp.readlines()
      core_group = mpu.core_groups[core_group_idx]
      core_group.wt_mem.clear()
      for _, line in enumerate(lines):
        core_group.wt_mem += [ int(line, 16) ]
    pass

  @staticmethod
  def load_config_from_file_dcores(mpu: Cascade, mix_output_dcores_config: str) -> None:
    '''Walk through the given file and update all ingress DCores' and egress
      DCores' configuration to pre-defined MPU data structure. Since ingress and
      egress DCore configurations are now placed within the same input file.
    '''
    with open(mix_output_dcores_config, 'r') as fp:
      lines = fp.readlines()
      f1 = re.compile(r'`DCORE\.(\S+)\s+(\d+)') # `DCORE.c_ing_arb_always_move_forward 1
      f2 = re.compile(r'`DCORE\.(\S+)\[(\d+)\]\s+(\d+)') # `DCORE.c_ing_yuv422_signed_uv[0] 0
      f3 = re.compile(r'`DCORE\.(\S+)\[(\d+)\+\:(\d+)\]\s+(\d+)') # `DCORE.c_ing_format_type[0+:2] 0
      for i, line in enumerate(lines):
        if not line: continue
        m1, m2, m3 = f1.match(line), f2.match(line), f3.match(line)
        if m1:
          dcore_top_fld_name = str(m1.group(1))
          dcore_top_fld_data = str(m1.group(2))
          if getattr(mpu.dcore_top, dcore_top_fld_name, None) is None:
            raise Exception('Parse DCoreTop configuration failure: line={}, field={}'.format(i+1, dcore_top_fld_name))
          setattr(mpu.dcore_top, dcore_top_fld_name, dcore_top_fld_data)
        elif m2:
          dcore_fld_name = str(m1.group(1))
          dcore_idx = int(m1.group(2))
          dcore_fld_data = int(m1.group(3))
          dcore = mpu.dcore_top.ing_dcores[dcore_idx] if 'ing' in dcore_fld_name else mpu.dcore_top.egr_dcores[dcore_idx]
          if getattr(dcore, dcore_fld_name, None) is None:
            raise Exception('Parse DCore configuration failure: line={}, field={}'.format(i+1, dcore_fld_name))
          setattr(dcore, dcore_fld_name, dcore_fld_data)
        elif m3:
          dcore_fld_name = str(m3.group(1))
          dcore_fld_lsb = int(m3.group(2))
          dcore_fld_width = int(m3.group(3))
          dcore_fld_data = int(m3.group(4))
          dcore_idx = dcore_fld_lsb // dcore_fld_width
          dcore = mpu.dcore_top.ing_dcores[dcore_idx] if 'ing' in dcore_fld_name else mpu.dcore_top.egr_dcores[dcore_idx]
          if getattr(dcore, dcore_fld_name, None) is None:
            raise Exception('Parse DCore configuration failure: line={}, field={}'.format(i+1, dcore_fld_name))
          setattr(dcore, dcore_fld_name, dcore_fld_data)
        else:
          raise Exception('Parse DCore configuration failure: line={}\n  {}'.format(i+1, line))
    pass

  @staticmethod
  def load_config_from_file_mcores(mpu: Cascade, mix_output_mcores_config: str) -> None:
    '''Walk through the given file and update all MCores' configuration to
      pre-defined MPU data structure.
    '''
    with open(mix_output_mcores_config, 'r') as fp:
      lines = fp.readlines()
      f1 = re.compile(r'`MCORE(\d+)\.(\S+)\s+(-?\d+).*') # `MCORE0.c_op_type 1		// Conv2D
      for i, line in enumerate(lines):
        if not line: continue
        m1 = f1.match(line)
        if m1:
          mcore_idx = int(m1.group(1))
          mcore_fld_name = str(m1.group(2))
          mcore_fld_data = int(m1.group(3))
          core_group_idx = mcore_idx // mpu.CORE_GROUP_MCORE_NUM
          mcore_idx = mcore_idx % mpu.CORE_GROUP_MCORE_NUM
          mcore = mpu.core_groups[core_group_idx].mcores[mcore_idx]
          if getattr(mcore, mcore_fld_name, None) is None:
            raise Exception('Parse MCore configuration failure: line={}, field={}'.format(i+1, mcore_fld_name))
          setattr(mcore, mcore_fld_name, mcore_fld_data)
        else:
          raise Exception('Parse MCore configuration failure: line={}\n  {}'.format(i+1, line))
    pass

  @staticmethod
  def load_config_from_file_acores(mpu: Cascade, mix_output_acores_config: str) -> None:
    '''Walk through the given file and update all ACores' configuration to
      pre-defined MPU data structure.
    '''
    with open(mix_output_acores_config, 'r') as fp:
      lines = fp.readlines()
      f1 = re.compile(r'`ACORE(\d+)\.(\S+)\s+(-?\d+).*') # `ACORE24.c_main_op 0 // Passthrough
      for i, line in enumerate(lines):
        if not line: continue
        m1 = f1.match(line)
        if m1:
          acore_idx = int(m1.group(1))
          acore_fld_name = str(m1.group(2))
          acore_fld_data = int(m1.group(3))
          core_group_idx = acore_idx // mpu.CORE_GROUP_ACORE_NUM
          acore_idx = acore_idx % mpu.CORE_GROUP_ACORE_NUM
          mcore = mpu.core_groups[core_group_idx].acores[acore_idx]
          if getattr(mcore, acore_fld_name, None) is None:
            raise Exception('Parse ACore configuration failure: line={}, field={}'.format(i+1, acore_fld_name))
          setattr(mcore, acore_fld_name, acore_fld_data)
        else:
          raise Exception('Parse ACore configuration failure: line={}\n  {}'.format(i+1, line))
    pass

  @staticmethod
  def load_config_from_file_ilcs(mpu: Cascade, mix_output_ilcs_config: str) -> None:
    '''Walk through the given file and update all ILC configuration to
      pre-defined MPU data structure. There are some differences between MIX
      output and library defined fields which should be handled manually.
    '''
    with open(mix_output_ilcs_config, 'r') as fp:
      lines = fp.readlines()
      f1 = re.compile(r'`ILC\.c_layer_index_mcore\[(\d+)\]\[(\d+)\]\[(\d+)\*6\+\:6\]\s+(\d+)') # `ILC.c_layer_index_mcore[5][15][1*6+:6] 0
      f2 = re.compile(r'`ILC\.c_layer_index_acore\[(\d+)\]\[(\d+)\]\[(\d+)\*6\+\:6\]\s+(\d+)') # `ILC.c_layer_index_acore[5][7][2*6+:6] 0
      f3 = re.compile(r'`ILC\.c_layer_index_ing_dcore\[(\d+)\*6\+\:6\]\s+(\d+)') # `ILC.c_layer_index_ing_dcore[14*6+:6] 0
      f4 = re.compile(r'`ILC\.c_layer_index_egr_dcore\[(\d+)\*6\+\:6\]\s+(\d+)') # `ILC.c_layer_index_egr_dcore[14*6+:6] 0
      f5 = re.compile(r'`ILC\.(\S+)\[(\d+)\]\s+(\d+)') # `ILC.c_t0_init[0] 1
      f6 = re.compile(r'(c_t)(\d+)(_\S+)')
      for i, line in enumerate(lines):
        if not line: continue
        m1, m2, m3, m4, m5 = f1.match(line), f2.match(line), f3.match(line), f4.match(line), f5.match(line)
        if m1: # ILC MCore
          core_group_idx = int(m1.group(1))
          mcore_idx = int(m1.group(2))
          ilc_mcore_fld_idx = int(m1.group(3))
          ilc_mcore_fld_data = int(m1.group(4))
          ilc_mcore = mpu.ilc.core_groups[core_group_idx].mcores[mcore_idx]
          setattr(ilc_mcore,{
            0:'c_layer_index_mcore_inc',
            1:'c_layer_index_mcore_dec'
            }[ilc_mcore_fld_idx], ilc_mcore_fld_data)
        elif m2: # ILC ACore
          core_group_idx = int(m2.group(1))
          acore_idx = int(m2.group(2))
          ilc_acore_fld_idx = int(m2.group(3))
          ilc_acore_fld_data = int(m2.group(4))
          ilc_acore = mpu.ilc.core_groups[core_group_idx].acores[acore_idx]
          setattr(ilc_acore,{
            0:'c_layer_index_acore_inc',
            1:'c_layer_index_acore_dec_ch0',
            2:'c_layer_index_acore_dec_ch1'
            }[ilc_acore_fld_idx], ilc_acore_fld_data)
        elif m3: # ILC ingress DCore
          dcore_idx = int(m3.group(1))
          dcore_fld_data = int(m3.group(2))
          ilc_dcore = mpu.ilc.dcore_top.ing_dcores[dcore_idx]
          ilc_dcore.c_layer_index_ing_dcore = dcore_fld_data
        elif m4: # ILC egress DCore
          dcore_idx = int(m4.group(1))
          dcore_fld_data = int(m4.group(2))
          ilc_dcore = mpu.ilc.dcore_top.egr_dcores[dcore_idx]
          ilc_dcore.c_layer_index_egr_dcore = dcore_fld_data
        elif m5: # ILC Layer
          layer_fld_name = str(m5.group(1))
          layer_idx = int(m5.group(2))
          layer_fld_data = int(m5.group(3))
          # fetch tower index and replace field with 'x': c_t0_init -> c_tx_init
          tower_idx = int(f6.match(layer_fld_name).group(2))
          layer_fld_name = f6.sub(lambda x: x.group(1)+'x'+x.group(3), layer_fld_name)
          layer = mpu.ilc.layers[tower_idx][layer_idx]
          if getattr(layer, layer_fld_name, None) is None:
            raise Exception('Parse ILC configuration failure: line={}, field={}'.format(i+1, layer_fld_name))
          setattr(layer, layer_fld_name, layer_fld_data)
          continue
        else: # unknown error
          raise Exception('Parse ILC configuration failure: line={}\n  {}'.format(i+1, line))
    pass

  @staticmethod
  def load_config_from_file_starts(mpu: Cascade, mix_output_starts_config: str) -> None:
    '''Walk through the given file and update all Start configuration to
      pre-defined MPU data structure.
    '''
    with open(mix_output_starts_config, 'r') as fp:
      lines = fp.readlines()
      f1 = re.compile(r'`CG(\d+)\.cfg_mcore_start\s+(\d+)') # `CG5.cfg_mcore_start 0
      f2 = re.compile(r'`CG(\d+)\.cfg_acore_start\s+(\d+)') # `CG5.cfg_acore_start 0
      f3 = re.compile(r'`DCORE\.cfg_ing_start\s+(\d+)') # `DCORE.cfg_ing_start 1
      f4 = re.compile(r'`DCORE\.cfg_egr_start\s+(\d+)') # `DCORE.cfg_egr_start 1
      f5 = re.compile(r'`ILC.cfg_start\s+(\d+)') # `ILC.cfg_start 1
      for i, line in enumerate(lines):
        if not line: continue
        m1, m2, m3, m4, m5 = f1.match(line), f2.match(line), f3.match(line), f4.match(line), f5.match(line)
        if m1: # CoreGroup MCore
          core_group_idx = int(m1.group(1))
          cfg_mcore_start = int(m1.group(2))
          mpu.core_groups[core_group_idx].cfg_mcore_start = cfg_mcore_start
        elif m2: # CoreGroup ACore
          core_group_idx = int(m2.group(1))
          cfg_acore_start = int(m2.group(2))
          mpu.core_groups[core_group_idx].cfg_acore_start = cfg_acore_start
        elif m3: # ingress DCore
          cfg_ing_start = int(m3.group(1))
          mpu.dcore_top.cfg_ing_start = cfg_ing_start
        elif m4: # egress DCore
          cfg_egr_start = int(m4.group(1))
          mpu.dcore_top.cfg_egr_start = cfg_egr_start
        elif m5: # ILC
          cfg_start = int(m5.group(1))
          mpu.ilc.cfg_start = cfg_start
        else : # unknown error
          raise Exception('Parse Start configuration failure: line={}\n  {}'.format(i+1, line))
    pass

  @staticmethod
  def load_config_from_dir(mpu: Cascade, mix_output_mpu_config_dirpath: str) -> None:
    '''Read from MIX compiler output .txt file and translate into pre-defined
      Python data structure.
    '''
    for core_group_idx in range(mpu.CORE_GROUP_NUM): # weight memory file might not exists if no mcore is enabled
      mix_output_weights_config = os.path.join(mix_output_mpu_config_dirpath, CascadeInterp.MIX_WEIGHTS_CONFIG_FILE.format(core_group_idx))
      if os.path.exists(mix_output_weights_config):
        CascadeInterp.load_config_from_file_weights(mpu, core_group_idx, mix_output_weights_config)

    mix_output_dcores_config = os.path.join(mix_output_mpu_config_dirpath, CascadeInterp.MIX_DCORES_CONFIG_FILE)
    CascadeInterp.load_config_from_file_dcores(mpu, mix_output_dcores_config)

    mix_output_mcores_config = os.path.join(mix_output_mpu_config_dirpath, CascadeInterp.MIX_MCORES_CONFIG_FILE)
    CascadeInterp.load_config_from_file_mcores(mpu, mix_output_mcores_config)

    mix_output_acores_config = os.path.join(mix_output_mpu_config_dirpath, CascadeInterp.MIX_ACORES_CONFIG_FILE)
    CascadeInterp.load_config_from_file_acores(mpu, mix_output_acores_config)

    mix_output_ilcs_config = os.path.join(mix_output_mpu_config_dirpath, CascadeInterp.MIX_ILCS_CONFIG_FILE)
    CascadeInterp.load_config_from_file_ilcs(mpu, mix_output_ilcs_config)

    mix_output_starts_config = os.path.join(mix_output_mpu_config_dirpath, CascadeInterp.MIX_STARTS_CONFIG_FILE)
    CascadeInterp.load_config_from_file_starts(mpu, mix_output_starts_config)
    pass

