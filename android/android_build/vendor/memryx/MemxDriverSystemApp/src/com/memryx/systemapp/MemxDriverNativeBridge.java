package com.memryx.systemapp;

public class MemxDriverNativeBridge {
    static {
        System.loadLibrary("memxdriver_jni");
    }

    public static native boolean initHal();

    public static native int memxLock(int groupId);
    public static native int memxTrylock(int groupId);
    public static native int memxUnlock(int groupId);
    public static native int memxOpen(int modelId, int groupId, float chipGen);
    public static native int memxClose(int modelId);
    public static native int memxConfigMpuGroup(int groupId, int config);
    public static native byte[] memxGetChipGen(int modelId);
    public static native int memxDownloadModel(int modelId, String path, int idx, int type);
    public static native int memxSetStreamEnable(int modelId, int wait);
    public static native int memxSetStreamDisable(int modelId, int wait);
    public static native int[] memxGetIfmapSize(int modelId, int flowId);
    public static native int[] memxGetOfmapSize(int modelId, int flowId);
    public static native int memxStreamIfmap(int modelId, int flowId, byte[] data, int timeout);
    public static native int memxStreamOfmap(int modelId, int flowId, byte[] data, int timeout);
}
