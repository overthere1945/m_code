/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

package com.qualcomm.qti.aonutility;

import android.content.Context;
import android.os.HandlerThread;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.PowerManager;
import android.os.RemoteException;
import android.os.RemoteCallbackList;
import android.os.ServiceManager;
import android.util.Log;
import java.util.List;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Arrays;
import vendor.qti.hardware.aonfwloader.*;
import vendor.qti.hardware.aonutility.*;
import android.hidl.manager.V1_0.IServiceManager;
import android.hidl.manager.V1_0.IServiceNotification;
import android.os.HwBinder;
import android.app.Service;
import android.content.Intent;
import android.os.Binder;
import android.os.IBinder;
import android.os.IBinder.DeathRecipient;
import android.util.SparseArray;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map.Entry;
import java.util.concurrent.atomic.AtomicLong;

public class AONLoaderService extends Service {
    private static final String TAG = "AONLoader";
    private static final boolean VERBOSE = Log.isLoggable(TAG, Log.VERBOSE);
    private static final boolean DEBUG = Log.isLoggable(TAG, Log.DEBUG);
    private IAONFwLoader mAONLoaderHal = null;
    private IAONFwLoaderCallback mAonLoaderResponse;
    private IServiceManager mServiceManager;
    private AONLoaderDeathRecipient mDeathRecipient;
    private ServiceNotificationCallback mServiceNotification;
    private static final String AONFW_SERVICE_NAME = "vendor.qti.hardware.aonfwloader.IAONFwLoader/default";
    private final AtomicLong mAONLoaderCookie = new AtomicLong(0);

    private final HandlerThread mHandlerThread;


    public AONLoaderService() {
        mDeathRecipient = new AONLoaderDeathRecipient();
        mHandlerThread = new HandlerThread("AONLoaderService");
        mHandlerThread.start();
        Handler handler = new CallBackHandler(mHandlerThread.getLooper());
        mServiceNotification = new ServiceNotificationCallback();
        mAonLoaderResponse = new AonFwLoaderResponse(handler);
        try {
            mAONLoaderHal = vendor.qti.hardware.aonfwloader.IAONFwLoader.Stub.asInterface(ServiceManager.getService(AONFW_SERVICE_NAME)); //AIDL
            if(DEBUG) {
                Log.d(TAG, "Got a IAONFwLoader: " + mAONLoaderHal);
            }
        } catch (Exception e) {
            Log.e(TAG,"Exception on getService: " + e.toString());
        }
    }

    @Override
    public void onCreate() {
        if(VERBOSE) {
            Log.v(TAG, "onCreate()");
        }
    }

    @Override
    public IBinder onBind(Intent intent) {
        if(VERBOSE) {
            Log.v(TAG, "onBind()");
        }
        IBinder res = null;
        return res;
    }

    @Override
    public boolean onUnbind(Intent intent) {
        if (VERBOSE) {
            Log.v(TAG, "onUnbind");
        }
        return super.onUnbind(intent);
    }

    @Override
    public void onDestroy() {
        if (VERBOSE) {
            Log.v(TAG, "onDestroy()");
        }
        unRegisterCallback(mAONLoaderHal);
        resetHal();
        super.onDestroy();
    }

    /* Callback for AonFwLoader response */
    class AonFwLoaderResponse extends IAONFwLoaderCallback.Stub {
        private Handler mHandler;
        AonFwLoaderResponse(Handler handler) {
            mHandler = handler;
        }
        public void onAONFWLoaderStatusChange(vendor.qti.hardware.aonfwloader.AONFwLoaderStatus mode) {
            if (DEBUG) {
                Log.d(TAG, "got onAONFWLoaderStatusChange " + mode);
                Log.v(TAG, "bootMode: " + mode.bootMode + " fwUpdateStatus:" + mode.fwUpdateStatus);
            }
            if(mode.bootMode == AONBootMode.AON_BOOT_FLASH && mode.fwUpdateStatus == AONFwUpdateStatus.AON_UPDATE_DONE){
                rebootDevice();
            }
        }
        @Override
        public String getInterfaceHash() {
            // We do not require the interface hash as the client.
            throw new UnsupportedOperationException(
                    "WeaverHidlAdapter does not support getInterfaceHash");
        }
        @Override
        public final int getInterfaceVersion() {
            return 0;
        }
    }

    private class CallBackHandler extends Handler
    {
        private static final int MSG_METRIC_EVENT = 1;
        private static final int MSG_METRIC_STATUS = 2;
        public CallBackHandler(Looper looper) {
            super(looper);
        }

        @Override
        public void handleMessage(Message msg) {
            Log.d(TAG,"notifyEvents ");
        }
    }

    /*
     * Death recipient callback that is notified when IAONFwLoader HIDL
     * service dies
     */
    final class AONLoaderDeathRecipient implements IBinder.DeathRecipient {
        /**
         * Callback that gets called when the service has died
         */
        @Override
        public void binderDied() {

            Log.e(TAG, "IAONFwLoader service died");
            resetHal();
        }
    }

    /* Callback that registers with ServiceManager to be notified of
     * service availability
     */
    private final class ServiceNotificationCallback extends IServiceNotification.Stub {
        @Override
        public void onRegistration(String fqName, String name, boolean preexisting) {

            if(DEBUG) {
                Log.d(TAG, "got mServiceNotificationCallback notification for: "
                            + fqName + ", " + name + " preexisting=" + preexisting);
            }
            initHal();
        }
    }

    private synchronized void initHal() {
        if(VERBOSE) {
            Log.v(TAG, "initHal");
        }

        try {
            mAONLoaderHal = vendor.qti.hardware.aonfwloader.IAONFwLoader.Stub.asInterface(ServiceManager.getService(AONFW_SERVICE_NAME)); //AIDL
            if (mAONLoaderHal == null) {
                Log.e(TAG, "initHal: mAONLoaderHal == null");
                return;
            }
            mAONLoaderHal.asBinder().linkToDeath(mDeathRecipient, 0);
            registerCallback(mAONLoaderHal);
            AONFwLoaderStatus mode = getAonFwLoaderStatus(mAONLoaderHal);
            if(mode.bootMode == AONBootMode.AON_BOOT_FLASH && mode.fwUpdateStatus == AONFwUpdateStatus.AON_UPDATE_DONE){
                rebootDevice();
            }
        } catch (Exception e) {
                Log.e(TAG, "initHal: Exception: " + e);
        }
    }

    private synchronized void resetHal() {
        if(VERBOSE) {
            Log.v(TAG, "resetHal");
        }
        try {
            if (mAONLoaderHal != null) {
                mAONLoaderHal.asBinder().unlinkToDeath(mDeathRecipient, 0);
                mAONLoaderHal = null;
            }
        } catch(Exception e) {
            Log.e(TAG, "resetHal: Exception=" + e );
        }
    }

    private void registerCallback(IAONFwLoader hal)
    {
        if(VERBOSE) {
            Log.v(TAG, "registerAONFwLoaderCallback called: ");
        }
        if (hal == null) {
            return;
        }
        try {
            hal.registerAONFwLoaderCallback(mAonLoaderResponse);
        } catch (RemoteException e) {
            Log.e(TAG,"registerCallback: RemoteException" + e.toString());
        }
    }

    public vendor.qti.hardware.aonfwloader.AONFwLoaderStatus getAonFwLoaderStatus(IAONFwLoader hal)
    {
        vendor.qti.hardware.aonfwloader.AONFwLoaderStatus bootstatus = new vendor.qti.hardware.aonfwloader.AONFwLoaderStatus();
        if(VERBOSE) {
            Log.v(TAG, "getAonFwLoaderStatus called: ");
        }
        if (hal == null) {
            return bootstatus;
        }
        try {
            bootstatus = hal.getAonFwLoaderStatus();
            Log.v(TAG, "bootMode: " + bootstatus.bootMode + " fwUpdateStatus:" + bootstatus.fwUpdateStatus);
        } catch (RemoteException e) {
            Log.e(TAG,"registerCallback: RemoteException" + e.toString());
        }
        return bootstatus;
    }

    private void unRegisterCallback(IAONFwLoader hal)
    {
        if(VERBOSE) {
            Log.v(TAG, "registerAONFwLoaderCallback called: NULL");
        }
        if (hal == null) {
            return;
        }
        try {
            hal.registerAONFwLoaderCallback(null);
        } catch (RemoteException e) {
            Log.e(TAG,"unRegisterCallback: RemoteException" + e.toString());
        }
    }

    private void rebootDevice() {

        PowerManager powerManager = (PowerManager)getSystemService(Context.POWER_SERVICE);
        if (powerManager != null) {
            Log.e(TAG,"AON software updated, mandatory reboot required. Rebooting device...");
            powerManager.reboot("aon_fw_update");
        }
    }
}
