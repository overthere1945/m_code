/**
 * Copyright (c) 2021, 2023 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

package com.qualcomm.qti.aonutility;

import android.app.ActivityManager;
import android.app.ActivityManager.RunningServiceInfo;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.SystemProperties;
import android.os.UserHandle;
import android.util.Log;

public class AONUtilityAutoBootReceiver extends BroadcastReceiver {
    private static final String TAG = "AONUtility";
    private final static String mClassAONUtility = AONUtilityService.class.getName();
    private final static String mClassAONLoader = AONLoaderService.class.getName();
    @Override
    public void onReceive(Context context, Intent intent) {
        String intentAction = intent.getAction();
        if (Intent.ACTION_BOOT_COMPLETED.equals(intentAction) ||
            Intent.ACTION_LOCKED_BOOT_COMPLETED.equals(intentAction)) {
            if (!isServiceRunning(context, mClassAONLoader)) {
                Log.d(TAG, "Starting " + mClassAONLoader + " : " + intentAction + " received. ");
                startService(context, mClassAONLoader);
            } else {
                Log.d(TAG, mClassAONLoader + " is already running. " +
                           intentAction + " ignored. ");
            }

            if (!isServiceRunning(context, mClassAONUtility)) {
                Log.d(TAG, "Starting " + mClassAONUtility + " : " + intentAction + " received. ");
                startService(context, mClassAONUtility);
            } else {
                Log.d(TAG, mClassAONUtility + " is already running. " +
                           intentAction + " ignored. ");
            }

        } else {
            Log.e(TAG, "Received Intent: " + intent.toString());
        }
    }
    private void startService(Context context, String mClassName) {
        ComponentName comp = new ComponentName(context.getPackageName(), mClassName);
        ComponentName service = context.startService(new Intent().setComponent(comp));
        if (service == null) {
            Log.e(TAG, "Could Not Start Service " + comp.toString());
        } else {
            Log.d(TAG, mClassName + " service Started Successfully");
        }
    }

    private boolean isServiceRunning(Context context, String mClassName) {
        ActivityManager manager =
              (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        for (RunningServiceInfo service : manager.getRunningServices(Integer.MAX_VALUE)) {
            if (mClassName.equals(service.service.getClassName())) {
                return true;
            }
        }
        return false;
    }
}
