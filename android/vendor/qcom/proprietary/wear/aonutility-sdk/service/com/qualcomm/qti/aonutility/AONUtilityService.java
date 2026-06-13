/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

package com.qualcomm.qti.aonutility;

import android.content.Context;
import android.os.RemoteException;
import android.os.RemoteCallbackList;
import android.os.ServiceManager;
import android.os.PowerManager;
import android.os.SystemProperties;
import android.os.SystemClock;
import android.util.Log;
import android.hardware.common.Ashmem;
import java.util.List;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Arrays;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.FileChannel;
import java.io.FileInputStream;
import android.os.ParcelFileDescriptor;
import vendor.qti.hardware.aonutility.*;
import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Binder;
import android.os.IBinder;
import android.os.IBinder.DeathRecipient;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.SparseArray;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map.Entry;
import java.util.concurrent.atomic.AtomicLong;
import java.util.Calendar;
import java.util.concurrent.TimeUnit;
import java.util.NoSuchElementException;
import android.content.Intent;
import android.os.UserHandle;
import android.os.Handler;
import android.os.Looper;
import android.content.ComponentName;

public class AONUtilityService extends Service{
    private static final String TAG = "AONUtility-Service";
    private static final boolean VERBOSE = Log.isLoggable(TAG, Log.VERBOSE);
    private static final boolean DEBUG = Log.isLoggable(TAG, Log.DEBUG);
    private vendor.qti.hardware.aonutility.IAONUtility mAONUtilityHal = null;
    private IAONUtilityCallback mAONUtilityResponse;
    private static Context context ;
    private static PowerManager mPowerManager;
    private static final String AONU_SERVICE_NAME = "vendor.qti.hardware.aonutility.IAONUtility/default";
    private final HandlerThread mHalHandlerThread = new HandlerThread("AONUtility-Hal");
    private Handler mHalHandler;
    private final AtomicLong mAONUtilityCookie = new AtomicLong(0);
    private static int MAXBITS = 8;
    private static int MAX_ALARMS = 10;
    private static int MAX_ASSETS = 10;
    private static long MAX_TIME_IN_MILLI_SEC = 24*60*60*1000;
    public static int SNOOZE_PERIOD = 1*60*1000;
    private PCMDataUtil pcmDownloaderUtil;
    private SoundModelUtil soundModelUtil;

    // Central death recipient for the HAL binder.
    private final IBinder.DeathRecipient mDeathRecipient = () -> {
        Log.e(TAG, "IAONUtility binder died");
        // Run reconnection on our handler thread.
        mHalHandler.post(this::handleHalDeathAndReconnect);
        notifyHalStatus(AONUtilityHALState.HAL_DOWN);
    };



    /**
     * To keep record of AONUtility HAL Client
     */
    public final SparseArray<ClientRecord> mCallbacks =
            new SparseArray<ClientRecord>();
    /**
     * To keep record of Alarms
     */
    private final HashMap<Integer, AONAlarm> mAlarmList =
            new HashMap<>();
    /**
     * To keep record of PCM Data
     */
    private final HashMap<Integer, AONPCMData> mPCMDataMap =
            new HashMap<>();
    private final List<AONPCMData> mPCMDataList =
            new ArrayList<>();

    private IAONUtilityServiceImpl serviceImpl;
    public AONUtilityService() {
        pcmDownloaderUtil = new PCMDataUtil(this);
        soundModelUtil = new SoundModelUtil(this);
        try {
            mAONUtilityHal =  vendor.qti.hardware.aonutility.IAONUtility.Stub.asInterface(ServiceManager.getService(AONU_SERVICE_NAME)); //AIDL
            if(DEBUG) {
                Log.d(TAG, "Got a AONUtility: " + mAONUtilityHal);
            }
        } catch (Exception e) {
            Log.e(TAG,"Exception on getService: " + e.toString());
        }
        mAONUtilityResponse = new AONUtilityResponse();
    }

    @Override
    public IBinder onBind(Intent intent) {
        if(VERBOSE) {
            Log.v(TAG, "onBind()");
        }
        return serviceImpl;
    }

    @Override
    public void onCreate() {
        super.onCreate();
        if(VERBOSE) {
            Log.v(TAG, "onCreate()");
        }
        mHalHandlerThread.start();
        mHalHandler = new Handler(mHalHandlerThread.getLooper());
        mHalHandler.post(this::connectHalInitial);

        registerCallback(mAONUtilityHal);
        serviceImpl = new IAONUtilityServiceImpl();
        context = this.getApplicationContext();
        mPowerManager = context.getSystemService(PowerManager.class);

        IntentFilter skuIntentFilter = new IntentFilter();
        skuIntentFilter.setPriority(IntentFilter.SYSTEM_HIGH_PRIORITY);
        skuIntentFilter.addAction(Intent.ACTION_SHUTDOWN);
        skuIntentFilter.addAction(Intent.ACTION_TIME_CHANGED);
        skuIntentFilter.addAction(Intent.ACTION_DATE_CHANGED);
        skuIntentFilter.addAction(Intent.ACTION_TIMEZONE_CHANGED);
        this.registerReceiver(mBroadcastReceiver, skuIntentFilter);
    }


    /**
     * Performs the initial connection to the IAONUtility HAL on a background thread.
     * This method waits for the HAL service to appear, links a death recipient,
     * registers the AIDL callback, and restores all required state (time, alarms,
     * battery thresholds, and PCM defaults) to ensure the service is fully functional.
     */
    private void connectHalInitial() {
        try {
            // Blocks until the service is available.
            IBinder binder = ServiceManager.waitForService(AONU_SERVICE_NAME);
            if (binder == null) {
                Log.e(TAG, "waitForService returned null for " + AONU_SERVICE_NAME);
                scheduleReconnectWithBackoff();
                return;
            }
            mAONUtilityHal = vendor.qti.hardware.aonutility.IAONUtility.Stub.asInterface(binder);
            binder.linkToDeath(mDeathRecipient, 0);

            // Register AIDL callback with HAL.
            registerCallback(mAONUtilityHal);
            AONTimeUpdate();
            sendBatteryThresholdValue();
            reConfigureAlarms();

            // load default pcm files post boot
            if (mAONUtilityHal != null) {
                loadDefaultPCMFiles("pcm_data_precanned");
                loadDefaultPCMFiles("pcm_data_predefined");
                for (int i = 0; i < mPCMDataList.size(); i++) {
                    Log.d(TAG, "Default pcm data = " + mPCMDataList.get(i));
                }
            }

            Log.i(TAG, "Connected to IAONUtility and re-initialized state");
            notifyHalStatus(AONUtilityHALState.HAL_UP);
        } catch (RemoteException e) {
            Log.e(TAG, "connectHalInitial RemoteException", e);
            scheduleReconnectWithBackoff();
        } catch (Exception e) {
            Log.e(TAG, "connectHalInitial failed", e);
            scheduleReconnectWithBackoff();
        }
    }


    /**
     * Handles the HAL binder‑death event triggered by the DeathRecipient.
     * This method clears the current HAL handle, unlinks callbacks, and
     * initiates the reconnection sequence to restore HAL functionality.
     */
    private void handleHalDeathAndReconnect() {
        if (VERBOSE) Log.v(TAG, "handleHalDeathAndReconnect()");
        // resetHAL for unregister callbacks, unlinks death, nulls mAONUtilityHal
        resetHal();
        scheduleReconnectWithBackoff();
    }


    /**
     * Attempts to reconnect to the IAONUtility HAL after a binder death event.
     * It retries multiple times with increasing delays (backoff),
     * using ServiceManager.checkService() to detect when the HAL becomes available again.
     *
     * Once the HAL is found, it restores the binder linkToDeath(), re-registers
     * the HAL callback, and replays essential state (time, alarms, battery info).
     * If all attempts fail, the method reschedules itself to try again later.
     */
    private void scheduleReconnectWithBackoff() {
        // Try several times using non-blocking checkService; if still not present, reschedule later.
        final int[] delaysMs = {500, 1000, 2000, 4000, 8000};
        for (int delay : delaysMs) {
            try {
                Thread.sleep(delay);
                IBinder binder = ServiceManager.checkService(AONU_SERVICE_NAME);
                if (binder != null) {
                    mAONUtilityHal = vendor.qti.hardware.aonutility.IAONUtility.Stub.asInterface(binder);
                    binder.linkToDeath(mDeathRecipient, 0);
                    registerCallback(mAONUtilityHal);
                    Log.i(TAG, "Reconnected to IAONUtility after HAL restart");

                    AONTimeUpdate();
                    sendBatteryThresholdValue();
                    reConfigureAlarms();

                    List<AONPCMData> pcmFiles = new ArrayList<>(mPCMDataList);
                    mPCMDataList.clear();
                    mPCMDataMap.clear();
                    int resultID = -1;
                    for (int i = 0; i < pcmFiles.size(); i++) {
                        try {
                            resultID = this.getServiceImpl().addPCMData(pcmFiles.get(i));
                        } catch (Exception e) {
                            Log.e(TAG, "addPCMData failed with expception" + e.getMessage());
                            e.printStackTrace();
                        }
                        if (resultID == -1) {
                            Log.e(TAG, "Downloading PCM failed after Reconnection with HAL");
                        }
                        else{
                            Log.d(TAG, "PCM data downloaded with ID = " + resultID);
                        }
                    }
                    notifyHalStatus(AONUtilityHALState.HAL_UP);
                    return;
                }
            } catch (InterruptedException ignored) {
            } catch (RemoteException e) {
                Log.e(TAG, "linkToDeath during reconnect failed", e);
            }
        }
        Log.w(TAG, "HAL not available yet; will retry");
        mHalHandler.postDelayed(this::scheduleReconnectWithBackoff, 10_000);
    }

    public boolean sendBatteryThresholdValue() {
        Context mContext;
        mContext = this;

        int critLevel = mContext.getResources().getInteger(
                com.android.internal.R.integer.config_criticalBatteryWarningLevel);
        int warnLevel = mContext.getResources().getInteger(
                com.android.internal.R.integer.config_lowBatteryWarningLevel);
        Log.d(TAG, "critLevel: " + critLevel + " warnLevel: " + warnLevel);
        vendor.qti.hardware.aonutility.BatteryThesholdInfo btcriticalInfo = new vendor.qti.hardware.aonutility.BatteryThesholdInfo();
        btcriticalInfo.key = vendor.qti.hardware.aonutility.BatteryLevel.CRITICAL_BATTERY_WARNING_LEVEL;
        btcriticalInfo.value = critLevel;
        vendor.qti.hardware.aonutility.BatteryThesholdInfo btwarningInfo = new vendor.qti.hardware.aonutility.BatteryThesholdInfo();
        btwarningInfo.key = vendor.qti.hardware.aonutility.BatteryLevel.LOW_BATTERY_WARNING_LEVEL;
        btwarningInfo.value = warnLevel;
        List<vendor.qti.hardware.aonutility.BatteryThesholdInfo> bthresholdInfo = new ArrayList<>();
        bthresholdInfo.add(btcriticalInfo);
        bthresholdInfo.add(btwarningInfo);
        vendor.qti.hardware.aonutility.BatteryInfo bInfo = new  vendor.qti.hardware.aonutility.BatteryInfo();
        bInfo.lowBatteryThresholds = bthresholdInfo;
        if (mAONUtilityHal != null) {
            try {
                int status = mAONUtilityHal.sendBatteryInfo(bInfo);
                if (status == vendor.qti.hardware.aonutility.Status.OK){
                    return true;
                } else
                    return false;
            } catch (RemoteException e) {
                Log.e(TAG,"RemoteException" + e.toString());
                return false;
            }
        }
        return false;
    }

    /**
     * Private getter for Service Implementation
     * @return serviceImpl
     */
    IAONUtilityServiceImpl getServiceImpl(){
        return serviceImpl;
    }

    /**
     * Load Default PCM Audio files on startup
     */
    private int loadDefaultPCMFiles(String xmlFileName) {
        if(VERBOSE) {
            Log.v(TAG, "AONUtilityService loadDefaultPCMFiles called: ");
        }
        try {
            int status = pcmDownloaderUtil.loadDefaultPCMFiles(this, xmlFileName);
            if(status == -1) {
                Log.e(TAG, "Failed to save default audio file");
            }
            else{
                Log.d(TAG, "Default audio file saved sucessfully." );
            }
            return status;
        } catch (Exception e) {
            Log.e(TAG,"Exception" + e.toString());
            return -1;
        }
    }

    BroadcastReceiver mBroadcastReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            switch (action) {
                case Intent.ACTION_SHUTDOWN:
                    String shutdownReason = SystemProperties.get("sys.shutdown.requested");
                    if (shutdownReason.equals("0" + PowerManager.SHUTDOWN_LOW_BATTERY)) {
                        enterTwm();
                    }
                break;
                case Intent.ACTION_TIME_CHANGED:
                case Intent.ACTION_DATE_CHANGED:
                case Intent.ACTION_TIMEZONE_CHANGED:
                    reConfigureAlarms();
                    AONTimeUpdate();
                break;
            }
        }
    };

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
        try {
            this.unregisterReceiver(mBroadcastReceiver);
        } catch (IllegalArgumentException e) {
            // This can happen if the receiver was never registered,or already unregistered (e.g., if onCreate was never fully completed)
            Log.w(TAG, "mBroadcastReceiver was not registered or already unregistered: " + e.getMessage());
        }
        resetHal();
        if (mHalHandlerThread != null) {
            mHalHandlerThread.quitSafely();
            try { mHalHandlerThread.join(); } catch (InterruptedException ignored) {}
        }
        serviceImpl = null ;
        super.onDestroy();
    }

    private synchronized void initHal() {
        if(VERBOSE) {
            Log.v(TAG, "initHal");
        }

        try {
            mAONUtilityHal = vendor.qti.hardware.aonutility.IAONUtility.Stub.asInterface(ServiceManager.getService(AONU_SERVICE_NAME)); //AIDL
            if (mAONUtilityHal == null) {
                Log.e(TAG, "initHal: mAONUtilityHal == null");
                return;
            }
            mAONUtilityHal.asBinder().linkToDeath(mDeathRecipient, 0);
            reConfigureAlarms();
        } catch (Exception e) {
                Log.e(TAG, "initHal: Exception: " + e);
        }
    }

    private synchronized void resetHal() {
        if(VERBOSE) {
            Log.v(TAG, "resetHal");
        }
        if (mAONUtilityHal == null) {
            Log.e(TAG, "resetHal: mAONUtilityHal is null");
            return;
        }
        try {
            final IBinder binder = mAONUtilityHal.asBinder();
            // Only try to unregister if the server is still alive
            if (binder != null && binder.isBinderAlive()) {
                unRegisterCallback(mAONUtilityHal);
            } else {
                Log.i(TAG, "resetHal: binder not alive; skip unregisterCallback");
            }
            // Always try to unlink death recipient locally
            if (binder != null) {
                try {
                    binder.unlinkToDeath(mDeathRecipient, 0);
                } catch (NoSuchElementException e) {
                    Log.w(TAG, "resetHal: death recipient already unlinked: " + e.getMessage());
                }
            } else {
                Log.e(TAG, "resetHal: binder is null");
            }
        } catch (Exception e) {
            Log.e(TAG, "resetHal: Unknown Exception: " + e);
        } finally {
            mAONUtilityHal = null;
        }

    }

    private void AONTimeUpdate() {
           if(DEBUG) {
               Log.d(TAG, "AONTimeUpdate called: ");
           }
           if (mAONUtilityHal == null) {
               return;
           }
           try {
               mAONUtilityHal.updateAONTime();
           } catch (RemoteException e) {
               Log.e(TAG,"RemoteException" + e.toString());
           }
    }

    private synchronized void notifyHalStatus(AONUtilityHALState status) {
        for(int i = 0; i < mCallbacks.size(); i++) {
            int key = mCallbacks.keyAt(i);
            ClientRecord record = mCallbacks.get(key);
            try {
                record.mCallback.notifyAONUtilityHALStatus(status);
            } catch (android.os.RemoteException e) {
                Log.e(TAG,"RemoteException" + e.toString());
            }
        }
    }
    public final class IAONUtilityServiceImpl extends IAONUtilityService.Stub {

        @Override
        public boolean setAlarm(AONAlarm aonAlarm)
        {
            if(DEBUG) {
                Log.d(TAG, "setAlarm called: " + aonAlarm.getAlarmID());
            }
            if (mAONUtilityHal == null) {
                Log.w(TAG, "No AONUtility service.");
                return false;
            }
            if(mAlarmList.size() >= MAX_ALARMS){
                Log.e(TAG, "Only 10 ALARMS allowed: ");
                return false;
            }
            try {
                final int callingPid = Binder.getCallingPid();
                vendor.qti.hardware.aonutility.AlarmInfo alarmInfo = new vendor.qti.hardware.aonutility.AlarmInfo();
                alarmInfo.id = aonAlarm.getAlarmID();
                alarmInfo.alarmType = aonAlarm.alarmType;
                alarmInfo.alarmTimeInEpoch = TimeUnit.MILLISECONDS.toSeconds(aonAlarm.msecTime);
                alarmInfo.recurenceDays = getRecurranceForMonth(aonAlarm);
                alarmInfo.ringId = aonAlarm.ringToneID;
                alarmInfo.vibratorId = aonAlarm.vibratorID;
                int resSize = aonAlarm.resourceIDs.size();
                if (resSize > MAX_ASSETS) {
                    Log.w(TAG, "Only 10 resources allowed.");
                    resSize = MAX_ASSETS;
                }

                int status = mAONUtilityHal.setAlarm(alarmInfo);
                if (status == vendor.qti.hardware.aonutility.Status.OK){
                    mAlarmList.put(aonAlarm.getAlarmID(), aonAlarm);
                    return true;
                } else
                    return false;
            } catch (RemoteException e) {
                Log.e(TAG,"RemoteException" + e.toString());
                return false;
            }
        }
        @Override
        public boolean updateAlarm(AONAlarm aonAlarm)
        {
            if(DEBUG) {
                Log.d(TAG, "updateAlarm called: " + aonAlarm.getAlarmID());
            }
            if (mAONUtilityHal == null) {
                Log.w(TAG, "No AONUtility service.");
                return false;
            }
            if(mAlarmList.size() >= MAX_ALARMS){
                Log.e(TAG, "Only 10 ALARMS allowed: ");
                return false;
            }
            try {
                final int callingPid = Binder.getCallingPid();
                vendor.qti.hardware.aonutility.AlarmInfo alarmInfo = new vendor.qti.hardware.aonutility.AlarmInfo();
                alarmInfo.id = aonAlarm.getAlarmID();
                alarmInfo.alarmType = aonAlarm.alarmType;
                alarmInfo.alarmTimeInEpoch = TimeUnit.MILLISECONDS.toSeconds(aonAlarm.msecTime);
                alarmInfo.recurenceDays = getRecurranceForMonth(aonAlarm);
                alarmInfo.ringId = aonAlarm.ringToneID;
                alarmInfo.vibratorId = aonAlarm.vibratorID;
                int resSize = aonAlarm.resourceIDs.size();
                if (resSize > MAX_ASSETS) {
                    Log.w(TAG, "Only 10 resources allowed.");
                    resSize = MAX_ASSETS;
                }

                int status = mAONUtilityHal.updateAlarm(alarmInfo);
                if (status == vendor.qti.hardware.aonutility.Status.OK){
                    mAlarmList.put(aonAlarm.getAlarmID(), aonAlarm);
                    return true;
                } else
                    return false;
            } catch (RemoteException e) {
                Log.e(TAG,"RemoteException" + e.toString());
                return false;
            }
        }
        @Override
        public boolean deleteAlarm(AONAlarm aonAlarm) {

            if(VERBOSE) {
                Log.v(TAG, "deleteAlarm called: ");
            }
            if (mAONUtilityHal == null) {
                Log.w(TAG, "No AONUtility service.");
                return false;
            }

            try {
                vendor.qti.hardware.aonutility.AlarmInfo alarmInfo = new vendor.qti.hardware.aonutility.AlarmInfo();
                alarmInfo.id = aonAlarm.getAlarmID();
                alarmInfo.alarmType = aonAlarm.alarmType;
                alarmInfo.alarmTimeInEpoch = TimeUnit.MILLISECONDS.toSeconds(aonAlarm.msecTime);
                alarmInfo.recurenceDays = getRecurranceForMonth(aonAlarm);
                alarmInfo.ringId = aonAlarm.ringToneID;
                alarmInfo.vibratorId = aonAlarm.vibratorID;
                int status = mAONUtilityHal.deleteAlarm(alarmInfo);
                if (status == vendor.qti.hardware.aonutility.Status.OK) {
                    mAlarmList.remove(aonAlarm.getAlarmID());
                    return true;
                } else
                    return false;
            } catch (RemoteException e) {
                Log.e(TAG,"RemoteException" + e.toString());
                return false;
            }
        }
        @Override
        public boolean configureRingTone(AONRingTone ringTone) {

            if(VERBOSE) {
                Log.v(TAG, "configureRingTone called: ");
            }
            if (mAONUtilityHal == null) {
                Log.w(TAG, "No AONUtility service.");
                return false;
            }

            try {
                vendor.qti.hardware.aonutility.RingToneInfo ringToneInfo = new vendor.qti.hardware.aonutility.RingToneInfo();
                ringToneInfo.id = ringTone.getRingToneID();
                ringToneInfo.ringToneBuf = new byte[ringTone.ringToneBuff.size()];
                int i = 0;
                for (Byte value : ringTone.ringToneBuff ){
                    ringToneInfo.ringToneBuf[i]  = (byte)value;
                    i++;
                }
                int status = mAONUtilityHal.configureRingTone(ringToneInfo);
                if (status == vendor.qti.hardware.aonutility.Status.OK) {
                    return true;
                } else
                    return false;
            } catch (RemoteException e) {
                Log.e(TAG,"RemoteException" + e.toString());
                return false;
            }
        }
        @Override
        public boolean configureAlarmSettings(AONAlarmSettings alarmSettings) {

            if(VERBOSE) {
                Log.v(TAG, "configureAlarmSettings called: ");
            }
            if (mAONUtilityHal == null) {
                Log.w(TAG, "No AONUtility service.");
                return false;
            }

            try {
                vendor.qti.hardware.aonutility.AlarmSettings alarmSettingsInfo = new vendor.qti.hardware.aonutility.AlarmSettings();
                alarmSettingsInfo.msRingPeriod = alarmSettings.msRingPeriod;
                alarmSettingsInfo.msSnoozePeriod = alarmSettings.msSnoozePeriod;
                alarmSettingsInfo.maxAllowedSnoozes = alarmSettings.maxAllowedSnoozes;
                alarmSettingsInfo.ringVolume = alarmSettings.ringVolume;
                alarmSettingsInfo.buttonSnooze = alarmSettings.snoozeButtonID;
                alarmSettingsInfo.buttonDismiss = alarmSettings.dismissButtonID;
                int resSize = alarmSettings.resourceIDs.size();
                if (resSize > MAX_ASSETS) {
                    Log.w(TAG, "Only 10 resources allowed.");
                    resSize = MAX_ASSETS;
                }
                for (int i = 0; i < resSize; i++){
                    alarmSettingsInfo.dispId[i] = (int)alarmSettings.resourceIDs.get(i);
                }
                int status = mAONUtilityHal.configureAlarmSettings(alarmSettingsInfo);
                if (status == vendor.qti.hardware.aonutility.Status.OK) {
                    return true;
                } else
                    return false;
            } catch (RemoteException e) {
                Log.e(TAG,"RemoteException" + e.toString());
                return false;
            }
        }
        @Override
        public boolean deleteRingTone(AONRingTone ringTone) {

            if(VERBOSE) {
                Log.v(TAG, "deleteRingTone called: ");
            }
            if (mAONUtilityHal == null) {
                Log.w(TAG, "No AONUtility service.");
                return false;
            }

            try {
                vendor.qti.hardware.aonutility.RingToneInfo ringToneInfo = new vendor.qti.hardware.aonutility.RingToneInfo();
                ringToneInfo.id = ringTone.getRingToneID();
                ringToneInfo.ringToneBuf = new byte[ringTone.ringToneBuff.size()];
                int i = 0;
                for (Byte value : ringTone.ringToneBuff ){
                    ringToneInfo.ringToneBuf[i]  = (byte)value;
                    i++;
                }
                int status = mAONUtilityHal.deleteRingTone(ringToneInfo);
                if (status == vendor.qti.hardware.aonutility.Status.OK) {
                    return true;
                } else
                    return false;
            } catch (RemoteException e) {
                Log.e(TAG,"RemoteException" + e.toString());
                return false;
            }
        }
        @Override
        public boolean sendGenericCommand(GenericCmd cmd, byte[] buffer) {
            if(VERBOSE) {
                Log.v(TAG, "sendGenericCommand called: ");
            }
            if (mAONUtilityHal == null) {
                Log.w(TAG, "No AONUtility service.");
                return false;
            }
            try {
                int aidlCmd;
                if (cmd.getValue() == GenericCmd.SOUNDMODEL_QPAQUE_DATA.getValue()){
                    aidlCmd = vendor.qti.hardware.aonutility.GenericCmd.CUSTOM_1;       // CUSTOM_1 used for SOUNDMODEL_QPAQUE_DATA
                }
                else {
                    Log.e(TAG, "Generic Command ID Type is not supported");
                    return false;
                }
                int status = mAONUtilityHal.sendGenericCommand(aidlCmd, buffer);
                if (status == vendor.qti.hardware.aonutility.Status.OK) {
                    return true;
                } else
                    return false;
            } catch (RemoteException e) {
                Log.e(TAG,"RemoteException" + e.toString());
                return false;
            }
        }
        @Override
        public int addPCMData(AONPCMData mAONPCMData)
        {
            if(DEBUG) {
                Log.d(TAG, "addPCMData called");
            }
            if (mAONUtilityHal == null) {
                Log.w(TAG, "No AONUtility service.");
                return -1;
            }
            try {
                final int callingPid = Binder.getCallingPid();
                Log.d(TAG, "PCM Data to add is " + mAONPCMData);
                vendor.qti.hardware.aonutility.PCMMetaData pcmMetaData = new vendor.qti.hardware.aonutility.PCMMetaData();
                pcmMetaData.id = mAONPCMData.getId();

                if (mAONPCMData.hasAudio()){
                    pcmMetaData.audioData = new vendor.qti.hardware.aonutility.AudioInfo();
                    pcmDownloaderUtil.prepareAudioInfo(pcmMetaData, mAONPCMData.getAudioInfo());
                }
                if (mAONPCMData.hasHaptic()){
                    pcmMetaData.hapticData = new vendor.qti.hardware.aonutility.HapticsInfo();
                    pcmDownloaderUtil.prepareHapticInfo(pcmMetaData, mAONPCMData.getHapticInfo(), mAONPCMData.getId());
                }
                if (mAONPCMData.hasAudio() && mAONPCMData.hasHaptic()){
                    pcmMetaData.pcmDataType = PCMDataType.AUDIO_SYNC_HAPTIC;
                }
                Log.d(TAG, "PCM Metadata: id=" + pcmMetaData.id + ", pcmDataType=" + pcmMetaData.pcmDataType);
                int status = mAONUtilityHal.addPCMData(pcmMetaData);
                if (status == vendor.qti.hardware.aonutility.Status.OK){
                    mPCMDataMap.put(mAONPCMData.getId(), mAONPCMData);
                    mPCMDataList.add(mAONPCMData);
                    return mAONPCMData.getId();
                }
                else
                    return -1;
            } catch (RemoteException e) {
                Log.e(TAG,"RemoteException" + e.toString());
                return -1;
            }
            catch (Exception e) {
                Log.e(TAG, "addPCMData failed with exception" + e.getMessage());
                e.printStackTrace();
                return -1;
            }
        }
        @Override
        public boolean deletePCMData(int id) {
            if(VERBOSE) {
                Log.v(TAG, "deletePCMData called: ");
            }
            if (mAONUtilityHal == null) {
                Log.w(TAG, "No AONUtility service.");
                return false;
            }
            try {
                int status = mAONUtilityHal.deletePCMData(id);
                if (status == vendor.qti.hardware.aonutility.Status.OK) {
                    Log.d(TAG, "delete: Before deleting Total Data size is " + mPCMDataList.size());
                    AONPCMData toDelete = mPCMDataMap.get(id);
                    Log.d(TAG, "Deleting data = " + toDelete);
                    if (!mPCMDataMap.containsKey(id)){
                        Log.e(TAG, "deletePCMData: No data found for id = " + id);
                        return false;
                    }
                    mPCMDataMap.remove(id);
                    mPCMDataList.remove(toDelete);
                    Log.d(TAG, "delete: After deleting Total Data size is " + mPCMDataList.size());
                    return true;
                } else
                return false;
            } catch (RemoteException e) {
                Log.e(TAG,"RemoteException" + e.toString());
                return false;
            } catch (Exception e) {
                Log.e(TAG, "deletePCMData failed with exception" + e.getMessage());
                e.printStackTrace();
                return false;
            }
        }
        @Override
        public boolean flushAllPCMData(int pcmType) {
            if(VERBOSE) {
                Log.v(TAG, "flushAllPCMData called: ");
            }
            if (mAONUtilityHal == null) {
                Log.w(TAG, "No AONUtility service.");
                return false;
            }
            try {
                int halPCMType = pcmDownloaderUtil.convertPCMType(pcmType);
                int status = mAONUtilityHal.flushAllPCMData(halPCMType);
                if (status == vendor.qti.hardware.aonutility.Status.OK) {
                    Log.d(TAG, "flushAll: Before deleting total Data size is " + mPCMDataList.size());
                    ArrayList<AONPCMData> removeList = new ArrayList<>();
                    for (int i = 0; i < mPCMDataList.size(); i++) {
                        if (mPCMDataList.get(i).getType() == pcmType) {
                            removeList.add(mPCMDataList.get(i));
                            Log.d(TAG, "Deleting pcm data = " + mPCMDataList.get(i));
                        }
                    }
                    for (int i = 0; i < removeList.size(); i++) {
                        mPCMDataMap.remove(removeList.get(i).getId());
                    }
                    mPCMDataList.removeAll(removeList);
                    Log.d(TAG, "flushAll: deleted total " + removeList.size() + " files. Now Data size is " + mPCMDataList.size());
                    return true;
                }
                else return false;
            } catch (RemoteException e) {
                Log.e(TAG,"RemoteException" + e.toString());
                return false;
            } catch (Exception e) {
                Log.e(TAG, "flushAllPCMData failed with expception" + e.getMessage());
                e.printStackTrace();
                return false;
            }
        }
        @Override
        public AONPCMData getPCMFile(int id){
            if(VERBOSE) {
                Log.v(TAG, "getPCMFile called: ");
            }
            if (!mPCMDataMap.containsKey(id)){
                Log.e(TAG, "getPCMFile failed! No PCM data found for id = " + id);
                return null;
            }
            Log.d(TAG, "PCMData fetched in getPCMFile: " + mPCMDataMap.get(id));
            return mPCMDataMap.get(id);
        }
        @Override
        public List<AONPCMData> getPCMDataList(){
            if(VERBOSE) {
                Log.v(TAG, "getPCMDataList called: ");
            }
            return new ArrayList<AONPCMData>(mPCMDataList);
        }
        @Override
        public boolean downloadSoundModel(AONSVAData mAONSVAData) {
            if(VERBOSE) {
                Log.v(TAG, "downloadSoundModel called: ");
            }
            if (mAONUtilityHal == null) {
                Log.w(TAG, "No AONUtility service.");
                return false;
            }
            try {
                vendor.qti.hardware.aonutility.SVAData svaData = soundModelUtil.toAidlSVAData(mAONSVAData);
                int status = mAONUtilityHal.downloadSoundModel(svaData);
                if (status == vendor.qti.hardware.aonutility.Status.OK) {
                    Log.d(TAG, "Sound Model downloaded successfully!");
                    return true;
                }
                else return false;
            } catch (RemoteException e) {
                Log.e(TAG,"RemoteException" + e.toString());
                return false;
            } catch (Exception e) {
                Log.e(TAG, "downloadSoundModel failed with expception" + e.getMessage());
                e.printStackTrace();
                return false;
            }
        }
        @Override
        public boolean removeSoundModel() {
            if(VERBOSE) {
                Log.v(TAG, "removeSoundModel called: ");
            }
            if (mAONUtilityHal == null) {
                Log.w(TAG, "No AONUtility service.");
                return false;
            }
            try {
                int status = mAONUtilityHal.removeSoundModel();
                if (status == vendor.qti.hardware.aonutility.Status.OK) {
                    Log.d(TAG, "Sound Model deleted successfully!");
                    return true;
                }
                else return false;
            } catch (RemoteException e) {
                Log.e(TAG,"RemoteException" + e.toString());
                return false;
            } catch (Exception e) {
                Log.e(TAG, "removeSoundModel failed with expception" + e.getMessage());
                e.printStackTrace();
                return false;
            }
        }
        @Override
        public boolean registerCallback(IAONUtilityServiceCallback callback)
        {
            if(VERBOSE) {
                Log.v(TAG, "register_callbacks called: ");
            }
            if (mAONUtilityHal == null) {
                Log.w(TAG, "No AONUtility service.");
                return false;
            }
            final int callingPid = Binder.getCallingPid();
            if(callback == null){
                unRegisterCallbackInternal(callingPid);
                return true;
            } else {
                registerCallbackInternal(callingPid, callback);
                return true;
            }
        }

        @Override
        public boolean reset() {
            if(DEBUG) {
                Log.d(TAG, "AONUtility reset called ");
            }
            if (mAONUtilityHal == null) {
                Log.w(TAG, "No AONUtility service.");
                return false;
            }

            try {
                mAlarmList.clear();
                int status = mAONUtilityHal.reset();
                if (status == vendor.qti.hardware.aonutility.Status.OK)
                    return true;
                else
                    return false;
            } catch (RemoteException e) {
                Log.e(TAG,"RemoteException" + e.toString());
                return false;
            }
        }
    }

    private final class ClientRecord implements DeathRecipient {
        public final int mPid;
        public final IAONUtilityServiceCallback mCallback;
        private IBinder mBinder;

        public ClientRecord(int pid, IAONUtilityServiceCallback callback) {
            if(DEBUG) {
                Log.d(TAG, "ClientRecord pid " + pid);
            }
            mPid = pid;
            mCallback = callback;
            mBinder = callback.asBinder();
        }

        @Override
        public void binderDied() {
            if(DEBUG) {
                Log.d(TAG, "ClientRecord pid " + mPid + " died.");
            }
            onClientDied(this);
            unlinkToDeath();
        }

        public void unlinkToDeath() {
            if (mBinder != null) {
                mBinder.unlinkToDeath(this, 0);
            }
        }

    }
 

    private void open4KCaptureFromPuiCapture() {
        try {
            // 1. Intent 생성 및 명시적 컴포넌트 설정
            final Intent intent = new Intent("com.lge.wearcamera.ACTION_ALWAYS_ON_CAPTURE");
            intent.setComponent(new ComponentName(
                "com.lge.wearcamera",
                "com.lge.wearcamera.CameraCaptureService"
            ));

            Log.d(TAG, "HYL - LPAI: com.lge.wearcamera.ACTION_ALWAYS_ON_CAPTURE");

            // 4. mHandler 변수 누락 에러 해결: 메인 스레드 루퍼로 즉석 핸들러 생성 후 비동기 실행
            new Handler(Looper.getMainLooper()).post(new Runnable() {
                @Override
                public void run() {
                    try {
                            // ⭐ mContext 누락 에러 해결: 서비스 자신을 의미하는 'AONUtilityService.this' 사용
                            AONUtilityService.this.startForegroundService(intent);
                            Log.d(TAG, "HYL - LPAI: sstartForegroundService executed successfully");
                        } catch (Exception e) {
                            Log.e(TAG, "Failed to startForegroundService for wear home listening", e);
                    }
                }
            });

        } catch (Exception e) {
            Log.e(TAG, "Exception in openListeningFromPuiCapture", e);
        }
    }

    class AONUtilityResponse extends IAONUtilityCallback.Stub {
        @Override
        public void onBatteryThresholdReached( vendor.qti.hardware.aonutility.BatteryThesholdInfo info) {
            Log.d(TAG," Received onBatteryThresholdReached: ");
            if (mPowerManager != null) {
                mPowerManager.wakeUp(SystemClock.uptimeMillis(), PowerManager.WAKE_REASON_APPLICATION, "Critical Battery Warning Level");
            }
        }
        @Override
        public void onCallbackReceived(int[] callbackData) {
            Log.d(TAG, "Received onCallbackReceived");
            try {
                Log.d(TAG, "Callback data size = " + callbackData.length);
                int opcode = callbackData[0];
                int payloadSize = callbackData[1];
                int[] payload = new int[payloadSize];
                for (int i = 0; i < payloadSize; i++) {
                    payload[i] = callbackData[2 + i];
                }
                Log.d(TAG, "Opcode: " + opcode + ", Payload size: " + payloadSize + ", Payload data: " + Arrays.toString(payload));
                AONUtilityCallbackCommand command = AONUtilityCallbackCommand.fromValue(opcode);
                switch (command) {
                    case CMD_POWER_BUTTON_PRESS:
                        Log.d(TAG, "Power button press callback detected!");
                        if (mPowerManager != null) {
                            mPowerManager.wakeUp(SystemClock.uptimeMillis(), PowerManager.WAKE_REASON_APPLICATION,
                                "Wakeup from Ambient Mode triggered by Power Button Press");
                        }
                        Log.d(TAG, "HYL! LPAI BUTTON PRESS");
                        if (payload.length > 0 && payload[0] == 2)
                        {
                            open4KCaptureFromPuiCapture();
                        }
                        break;
                    case CMD_DOWNLOAD_PCM_TONES:
                        Log.d(TAG, "Going to download the saved pcm tones!");
                        List<AONPCMData> pcmFiles = new ArrayList<>(mPCMDataList);
                        mPCMDataList.clear();
                        mPCMDataMap.clear();
                        int resultID = -1;
                        for (int i = 0; i < pcmFiles.size(); i++) {
                            try {
                                resultID = AONUtilityService.this.getServiceImpl().addPCMData(pcmFiles.get(i));
                            } catch (Exception e) {
                                Log.e(TAG, "addPCMData failed with expception" + e.getMessage());
                                e.printStackTrace();
                            }
                            if (resultID == -1) {
                                Log.e(TAG, "Downloading PCM failed after Reconnection with HAL");
                            }
                            else{
                                Log.d(TAG, "PCM data downloaded with ID = " + resultID);
                            }
                        }
                        for (int i = 0; i < mPCMDataList.size(); i++) {
                            Log.d(TAG, "Default pcm data = " + mPCMDataList.get(i));
                        }
                        break;
                    default:
                        Log.d(TAG, "Unknown callback opcode " + opcode + " received via callback");
                }
            } catch (Exception e) {
                Log.e(TAG, "Error reading Ashmem!" + e);
            }
        }
        @Override
        public final int getInterfaceVersion() {
            return IAONUtilityCallback.VERSION;
        }

        @Override
        public final String getInterfaceHash() {
            return IAONUtilityCallback.HASH;
        }
    }
    private void registerCallback(IAONUtility hal)
    {

        if (hal == null) {
            return;
        }
        try {
            hal.registerCallback(mAONUtilityResponse);
            Log.d(TAG,"Registered to AONUtility client: ");
        } catch (RemoteException e) {
            Log.e(TAG,"RemoteException" + e.toString());
        }
    }

    private void unRegisterCallback(IAONUtility hal)
    {

        if (hal == null) {
            return;
        }
        try {
            hal.unregisterCallback(mAONUtilityResponse);
            Log.d(TAG,"UnRegistered to AONUtility client: " );
        } catch (android.os.DeadObjectException dead) {
            // Expected when server has already died and ignore
            Log.w(TAG, "unRegisterCallback: HAL is dead; ignoring");
        } catch (RemoteException e) {
            Log.e(TAG, "unRegisterCallback RemoteException" + e.toString());
        }
    }

    private void registerCallbackInternal(int callingPid, IAONUtilityServiceCallback callback) {

        if (mCallbacks.get(callingPid) != null) {
            throw new SecurityException("The calling process has already "
                    + "registered an IAONUtilityServiceCallback.");
        }

        ClientRecord record = new ClientRecord(callingPid, callback);
        try {
            IBinder binder = callback.asBinder();
            binder.linkToDeath(record, 0);
        } catch (RemoteException e) {
            Log.e(TAG,"RemoteException" + e.toString());
        }
        mCallbacks.put(callingPid, record);

    }

    private void unRegisterCallbackInternal(int callingPid) {

        ClientRecord record = mCallbacks.get(callingPid);
        if (record == null) {
            throw new SecurityException("The calling process has not "
                    + "registered an IAONUtilityServiceCallback.");
        }
        record.unlinkToDeath();
        mCallbacks.remove(callingPid);

    }

    private void onClientDied(ClientRecord record) {
        mCallbacks.remove(record.mPid);
        if(DEBUG) {
            Log.d(TAG, "onClientDied pid " + record.mPid );
        }
    }

    private void reConfigureAlarms() {

        Iterator<Entry<Integer, AONAlarm>> entryIt = mAlarmList.entrySet().iterator();
        Log.d(TAG, "reConfigureAlarms called: size " + mAlarmList.size());
        while (entryIt.hasNext()) {
            Entry<Integer, AONAlarm> entry = entryIt.next();
            AONAlarm aonAlarm = entry.getValue();
            vendor.qti.hardware.aonutility.AlarmInfo alarmInfo = new vendor.qti.hardware.aonutility.AlarmInfo();
            alarmInfo.id = aonAlarm.getAlarmID();
            alarmInfo.alarmType = aonAlarm.alarmType;
            alarmInfo.alarmTimeInEpoch = TimeUnit.MILLISECONDS.toSeconds(aonAlarm.msecTime);
            alarmInfo.recurenceDays = getRecurranceForMonth(aonAlarm);
            alarmInfo.ringId = aonAlarm.ringToneID;
            alarmInfo.vibratorId = aonAlarm.vibratorID;
            int resSize = aonAlarm.resourceIDs.size();
            if (resSize > MAX_ASSETS) {
                Log.w(TAG, "Only 10 resources allowed.");
                resSize = MAX_ASSETS;
            }

            Log.d(TAG, "setAlarm called: " + alarmInfo.id);
            setAlarm(alarmInfo);
        }
    }

    private void setAlarm(vendor.qti.hardware.aonutility.AlarmInfo alarmInfo)
    {
        if(DEBUG) {
            Log.d(TAG, "setAlarm called: " + alarmInfo.id);
        }
        if (mAONUtilityHal == null) {
            return;
        }
        try {
            mAONUtilityHal.setAlarm(alarmInfo);
        } catch (RemoteException e) {
            Log.e(TAG,"RemoteException" + e.toString());
        }
    }

    private void enterTwm()
    {
        if(DEBUG) {
            Log.d(TAG, "enterTwm called: ");
        }
        if (mAONUtilityHal == null) {
            return;
        }
        try {
            int status = mAONUtilityHal.enterTwm();
            if (status == vendor.qti.hardware.aonutility.Status.OK) {
                Log.d(TAG, "enterTwm success: ");
            } else
                Log.d(TAG, "enterTwm failed: ");
        } catch (RemoteException e) {
            Log.e(TAG,"RemoteException" + e.toString());
        }
    }

    private byte getRecurranceForMonth(AONAlarm aonAlarm)
    {
        byte result = 0x0;
        if(aonAlarm.alarmType == AONAlarmType.ALARM){
            result = (byte)getAlarmRecurranceForMonth(aonAlarm.recurrenceDays);
        } else if(aonAlarm.alarmType == AONAlarmType.REMINDER) {
            result = getReminderRecurranceForMonth(aonAlarm.msecTime);
        }
        return result;
    }
    private byte getReminderRecurranceForMonth(long msecTime)
    {
        int result = 0x00;
        return (byte)result;
    }
    private long getAlarmRecurranceForMonth(int recurenceDays)
    {
        long result = 0x0;
        if(recurenceDays == AONAlarm.NONE) {
            result = 0x00;
        } else {
             result = result | recurenceDays ;
        }
        return result;
    }

}
