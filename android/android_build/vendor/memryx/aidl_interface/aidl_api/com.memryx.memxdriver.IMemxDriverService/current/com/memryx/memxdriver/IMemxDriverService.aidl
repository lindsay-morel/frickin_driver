// IMemxDriverService.aidl
///////////////////////////////////////////////////////////////////////////////
// THIS FILE IS IMMUTABLE. DO NOT EDIT IN ANY CASE.                          //
///////////////////////////////////////////////////////////////////////////////

// This file is a snapshot of an AIDL file. Do not edit it manually. There are
// two cases:
// 1). this is a frozen version file - do not edit this in any case.
// 2). this is a 'current' file. If you make a backwards compatible change to
//     the interface (from the latest frozen version), the build system will
//     prompt you to update this file with `m <name>-update-api`.
//
// You must not make a backward incompatible change to any AIDL file built
// with the aidl_interface module type with versions property set. The module
// type is used to build AIDL files in a way that they can be used across
// independently updatable components of the system. If a device is shipped
// with such a backward incompatible change, it has a high risk of breaking
// later when a module using the interface is updated, e.g., Mainline modules.

package com.memryx.memxdriver;
@VintfStability
interface IMemxDriverService {
  int memxLock(int group_id);
  int memxTrylock(int group_id);
  int memxUnlock(int group_id);
  int memxOpen(int model_id, int group_id, float chip_gen);
  int memxClose(int model_id);
  int memxConfigMpuGroup(int group_id, int config);
  byte[] memxGetChipGen(int model_id);
  int memxDownloadModel(int model_id, String filePath, int model_idx, int type);
  int memxSetStreamEnable(int model_id, int wait);
  int memxSetStreamDisable(int model_id, int wait);
  int[] memxGetIfmapSize(int model_id, int flow_id);
  int[] memxGetOfmapSize(int model_id, int flow_id);
  int memxStreamIfmap(int model_id, int flow_id, in byte[] data, int timeout);
  int memxStreamOfmap(int model_id, int flow_id, inout byte[] data, int timeout);
}
