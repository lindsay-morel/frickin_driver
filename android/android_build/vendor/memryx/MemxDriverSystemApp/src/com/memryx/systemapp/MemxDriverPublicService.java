package com.memryx.systemapp;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.Log;
import java.util.Arrays; // Needed for logging arrays

// Import your AIDL interface
import com.memryx.memxdriver.IMemxDriverService; // Assuming this is the correct AIDL generated class

public class MemxDriverPublicService extends Service {
    // Use a consistent TAG for logging
    private static final String TAG = "MemxSysAppPublicSvc"; // Changed tag for clarity

    @Override
    public void onCreate() {
        super.onCreate();
        Log.i(TAG, "onCreate: Service creating."); // Log service creation

        // Attempt to initialize the native HAL bridge
        boolean success = MemxDriverNativeBridge.initHal();
        // Log the result of HAL initialization
        Log.i(TAG, "onCreate: Native HAL bridge initialization result: " + success);
        if (!success) {
            // Log error if HAL initialization failed
            Log.e(TAG, "onCreate: Failed to initialize HAL, native MemxDriverService might not be available!");
        }
    }

    // Implementation of the AIDL interface methods
    private final IMemxDriverService.Stub mPublicStub = new IMemxDriverService.Stub() {

        // Helper to log entry with parameters
        private void logEntry(String methodName, Object... params) {
            StringBuilder sb = new StringBuilder();
            sb.append("AIDL Call Received: ").append(methodName).append("(");
            for (int i = 0; i < params.length; i++) {
                // Handle byte arrays specifically for better logging
                if (params[i] instanceof byte[]) {
                    sb.append("byte[");
                    byte[] arr = (byte[]) params[i];
                    sb.append(arr == null ? "null" : arr.length);
                    sb.append("]");
                } else {
                    sb.append(params[i]);
                }
                if (i < params.length - 1) {
                    sb.append(", ");
                }
            }
            sb.append(")");
            Log.d(TAG, sb.toString()); // Use Debug level for entry/exit
        }

        // Helper to log exit with result
        private void logExit(String methodName, Object result) {
             // Handle array results specifically
            String resultStr;
            if (result instanceof int[]) {
                resultStr = Arrays.toString((int[]) result);
            } else if (result instanceof byte[]) {
                 byte[] arr = (byte[]) result;
                 resultStr = "byte[" + (arr == null ? "null" : arr.length) + "]";
            } else {
                 resultStr = String.valueOf(result);
            }
            Log.d(TAG, "AIDL Call Finished: " + methodName + " returned: " + resultStr);
        }

        // --- Implement AIDL Methods with Logging ---

        @Override public int memxLock(int group_id) throws RemoteException {
            logEntry("memxLock", group_id);
            int result = MemxDriverNativeBridge.memxLock(group_id);
            logExit("memxLock", result);
            return result;
        }
        @Override public int memxTrylock(int group_id) throws RemoteException {
            logEntry("memxTrylock", group_id);
            int result = MemxDriverNativeBridge.memxTrylock(group_id);
            logExit("memxTrylock", result);
            return result;
        }
        @Override public int memxUnlock(int group_id) throws RemoteException {
            logEntry("memxUnlock", group_id);
            int result = MemxDriverNativeBridge.memxUnlock(group_id);
            logExit("memxUnlock", result);
            return result;
        }
        @Override public int memxOpen(int model_id, int group_id, float chip_gen) throws RemoteException {
            logEntry("memxOpen", model_id, group_id, chip_gen);
            int result = MemxDriverNativeBridge.memxOpen(model_id, group_id, chip_gen);
            logExit("memxOpen", result);
            return result;
        }
        @Override public int memxClose(int model_id) throws RemoteException {
            logEntry("memxClose", model_id);
            int result = MemxDriverNativeBridge.memxClose(model_id);
            logExit("memxClose", result);
            return result;
        }
        @Override public int memxConfigMpuGroup(int group_id, int config) throws RemoteException {
            logEntry("memxConfigMpuGroup", group_id, config);
            int result = MemxDriverNativeBridge.memxConfigMpuGroup(group_id, config);
            logExit("memxConfigMpuGroup", result);
            return result;
        }
        @Override public byte[] memxGetChipGen(int model_id) throws RemoteException {
            logEntry("memxGetChipGen", model_id);
            byte[] result = MemxDriverNativeBridge.memxGetChipGen(model_id);
            logExit("memxGetChipGen", result); // logExit handles byte[] display
            return result;
        }
        @Override public int memxDownloadModel(int model_id, String path, int idx, int type) throws RemoteException {
            logEntry("memxDownloadModel", model_id, path, idx, type);
            int result = MemxDriverNativeBridge.memxDownloadModel(model_id, path, idx, type);
            logExit("memxDownloadModel", result);
            return result;
        }
        @Override public int memxSetStreamEnable(int model_id, int wait) throws RemoteException {
            logEntry("memxSetStreamEnable", model_id, wait);
            int result = MemxDriverNativeBridge.memxSetStreamEnable(model_id, wait);
            logExit("memxSetStreamEnable", result);
            return result;
        }
        @Override public int memxSetStreamDisable(int model_id, int wait) throws RemoteException {
            logEntry("memxSetStreamDisable", model_id, wait);
            int result = MemxDriverNativeBridge.memxSetStreamDisable(model_id, wait);
            logExit("memxSetStreamDisable", result);
            return result;
        }
        @Override public int[] memxGetIfmapSize(int model_id, int flow_id) throws RemoteException {
            logEntry("memxGetIfmapSize", model_id, flow_id);
            int[] result = MemxDriverNativeBridge.memxGetIfmapSize(model_id, flow_id);
            logExit("memxGetIfmapSize", result); // logExit handles int[] display
            return result;
        }
        @Override public int[] memxGetOfmapSize(int model_id, int flow_id) throws RemoteException {
            logEntry("memxGetOfmapSize", model_id, flow_id);
            int[] result = MemxDriverNativeBridge.memxGetOfmapSize(model_id, flow_id);
            logExit("memxGetOfmapSize", result); // logExit handles int[] display
            return result;
        }
        @Override public int memxStreamIfmap(int model_id, int flow_id, byte[] data, int timeout) throws RemoteException {
            // Log entry, but avoid logging potentially large byte array content directly
            logEntry("memxStreamIfmap", model_id, flow_id, data /* logEntry handles byte[] length */, timeout);
            int result = MemxDriverNativeBridge.memxStreamIfmap(model_id, flow_id, data, timeout);
            logExit("memxStreamIfmap", result);
            return result;
        }
        @Override public int memxStreamOfmap(int model_id, int flow_id, byte[] data, int timeout) throws RemoteException {
             // Log entry, but avoid logging potentially large byte array content directly
            logEntry("memxStreamOfmap", model_id, flow_id, data /* logEntry handles byte[] length */, timeout);
            int result = MemxDriverNativeBridge.memxStreamOfmap(model_id, flow_id, data, timeout);
             // Note: data byte[] is filled by the native call here, logExit only shows the return status
            logExit("memxStreamOfmap", result);
            return result;
        }

        // --- Standard AIDL versioning methods ---
        @Override public int getInterfaceVersion() {
            // Logging these isn't usually necessary unless debugging version issues
            return IMemxDriverService.VERSION;
        }
        @Override public String getInterfaceHash() {
            // Logging these isn't usually necessary
            return IMemxDriverService.HASH;
        }
    };

    @Override
    public IBinder onBind(Intent intent) {
        // Log when a client attempts to bind
        Log.i(TAG, "onBind: Client binding from intent: " + intent.getAction()); // Log action if available
        return mPublicStub; // Return the Binder stub implementation
    }

    @Override
    public void onDestroy() {
        Log.i(TAG, "onDestroy: Service destroying."); // Log service destruction
        // Optional: Add cleanup logic here if needed, e.g., informing native layer
        super.onDestroy();
    }
}
