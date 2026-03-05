// IMemxDriverService.aidl
package com.memryx.memxdriver;

// Declare any non-default types here with import statements

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