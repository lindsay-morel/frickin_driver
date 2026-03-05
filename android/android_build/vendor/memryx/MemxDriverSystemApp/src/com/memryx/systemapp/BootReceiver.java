package com.memryx.systemapp;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

public class BootReceiver extends BroadcastReceiver {
    private static final String TAG = "BootReceiver";

    @Override
    public void onReceive(Context context, Intent intent) {
        Log.i(TAG, "Boot completed received, initializing...");
        // Start necessary services here
        Intent serviceIntent = new Intent(context, MemxDriverPublicService.class);
        context.startService(serviceIntent);
    }
}
