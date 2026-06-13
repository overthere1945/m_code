/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

/** @file SoundModelUtil.java
 * @addtogroup soundmodelutil
 * @{ */

package com.qualcomm.qti.aonutility;

import android.util.Log;
import java.util.List;

/**
 * Utility class for converting SDK Sound Model objects to AIDL SVAData and related types.
 *
 * Provides methods to:
 * - Convert SDK SoundModel, PhraseSoundModel, RecognitionConfig, etc. to AIDL equivalents.
 * - Prepare SVAData for HAL download/offload.
 */
public class SoundModelUtil {
    private static final String TAG = "SoundModelUtil";
    private AONUtilityService aonUtilityService;

    public SoundModelUtil(AONUtilityService aonUtilityService){
        this.aonUtilityService = aonUtilityService;
    }

    /**
     * Converts an SDK AONSVAData object to an AIDL SVAData object.
     *
     * @param sdk the SDK AONSVAData object
     * @return the corresponding AIDL SVAData object
     */
    public vendor.qti.hardware.aonutility.SVAData toAidlSVAData(com.qualcomm.qti.aonutility.AONSVAData sdk) {
        Log.d(TAG, "Converting AONSVAData: " + sdk);
        if (sdk == null) return null;
        vendor.qti.hardware.aonutility.SVAData aidl = new vendor.qti.hardware.aonutility.SVAData();
        aidl.size = sdk.getSize();
        aidl.modelBuffer = sdk.getModelBuffer();
        aidl.smUuid = toAidlSoundModelUUID(sdk.getSmUuid());
        aidl.smConfig = toAidlPhraseSoundModel(sdk.getSmConfig());
        aidl.recogConfig = toAidlRecognitionConfig(sdk.getRecogConfig());
        return aidl;
    }

    /**
     * Converts an SDK SoundModelUUID to an AIDL SoundModelUUID.
     */
    public vendor.qti.hardware.aonutility.SoundModelUUID toAidlSoundModelUUID(com.qualcomm.qti.aonutility.SoundModelUUID sdk) {
        Log.d(TAG, "Converting SoundModelUUID: " + sdk);
        if (sdk == null) return null;
        vendor.qti.hardware.aonutility.SoundModelUUID aidl = new vendor.qti.hardware.aonutility.SoundModelUUID();
        aidl.timeLow = sdk.getTimeLow();
        aidl.timeHigh = sdk.getTimeHigh();
        aidl.timeHiAndVersion = sdk.getTimeHiAndVersion();
        aidl.clockSeq = sdk.getClockSeq();
        aidl.node = sdk.getNode();
        return aidl;
    }

    /**
     * Converts an SDK SoundModel to an AIDL SoundModel.
     */
    public vendor.qti.hardware.aonutility.SoundModel toAidlSoundModel(com.qualcomm.qti.aonutility.SoundModel sdk) {
        Log.d(TAG, "Converting SoundModel: " + sdk);
        if (sdk == null) return null;
        vendor.qti.hardware.aonutility.SoundModel aidl = new vendor.qti.hardware.aonutility.SoundModel();
        aidl.soundModelType = convertSoundModelType(sdk.getSoundModelType());
        aidl.uuid = toAidlSoundModelUUID(sdk.getUuid());
        aidl.vendorUuid = toAidlSoundModelUUID(sdk.getVendorUuid());
        aidl.dataSize = sdk.getDataSize();
        aidl.dataOffset = sdk.getDataOffset();
        return aidl;
    }

    /**
     * Converts an SDK Phrase to an AIDL Phrase.
     */
    public vendor.qti.hardware.aonutility.Phrase toAidlPhrase(com.qualcomm.qti.aonutility.Phrase sdk) {
        Log.d(TAG, "Converting Phrase: " + sdk);
        if (sdk == null) return null;
        vendor.qti.hardware.aonutility.Phrase aidl = new vendor.qti.hardware.aonutility.Phrase();
        aidl.id = sdk.getId();
        aidl.recognitionMode = sdk.getRecognitionMode();
        aidl.numUsers = sdk.getNumUsers();
        aidl.users = sdk.getUsers();
        aidl.locale = sdk.getLocale();
        aidl.text = sdk.getText();
        return aidl;
    }

    /**
     * Converts an SDK PhraseSoundModel to an AIDL PhraseSoundModel.
     */
    public vendor.qti.hardware.aonutility.PhraseSoundModel toAidlPhraseSoundModel(com.qualcomm.qti.aonutility.PhraseSoundModel sdk) {
        Log.d(TAG, "Converting PhraseSoundModel: " + sdk);
        if (sdk == null) return null;
        vendor.qti.hardware.aonutility.PhraseSoundModel aidl = new vendor.qti.hardware.aonutility.PhraseSoundModel();
        aidl.common = toAidlSoundModel(sdk.getCommon());
        aidl.numPhrases = sdk.getNumPhrases();
        List<com.qualcomm.qti.aonutility.Phrase> sdkPhrases = sdk.getPhrases();
        aidl.phrases = new vendor.qti.hardware.aonutility.Phrase[sdkPhrases.size()];
        for (int i = 0; i < sdkPhrases.size(); ++i) {
            aidl.phrases[i] = toAidlPhrase(sdkPhrases.get(i));
        }
        return aidl;
    }

    /**
     * Converts an SDK ConfidenceLevel to an AIDL ConfidenceLevel.
     */
    public vendor.qti.hardware.aonutility.ConfidenceLevel toAidlConfidenceLevel(com.qualcomm.qti.aonutility.ConfidenceLevel sdk) {
        Log.d(TAG, "Converting ConfidenceLevel: " + sdk);
        if (sdk == null) return null;
        vendor.qti.hardware.aonutility.ConfidenceLevel aidl = new vendor.qti.hardware.aonutility.ConfidenceLevel();
        aidl.userId = sdk.getUserId();
        aidl.level = sdk.getLevel();
        return aidl;
    }

    /**
     * Converts an SDK PhraseRecognition to an AIDL PhraseRecognition.
     */
    public vendor.qti.hardware.aonutility.PhraseRecognition toAidlPhraseRecognition(com.qualcomm.qti.aonutility.PhraseRecognition sdk) {
        Log.d(TAG, "Converting PhraseRecognition: " + sdk);
        if (sdk == null) return null;
        vendor.qti.hardware.aonutility.PhraseRecognition aidl = new vendor.qti.hardware.aonutility.PhraseRecognition();
        aidl.id = sdk.getId();
        aidl.recognitionModes = sdk.getRecognitionModes();
        aidl.confidenceLevel = sdk.getConfidenceLevel();
        aidl.numLevels = sdk.getNumLevels();
        List<com.qualcomm.qti.aonutility.ConfidenceLevel> sdkLevels = sdk.getLevels();
        aidl.levels = new vendor.qti.hardware.aonutility.ConfidenceLevel[sdkLevels.size()];
        for (int i = 0; i < sdkLevels.size(); ++i) {
            aidl.levels[i] = toAidlConfidenceLevel(sdkLevels.get(i));
        }
        return aidl;
    }

    /**
     * Converts an SDK RecognitionConfig to an AIDL RecognitionConfig.
     */
    public vendor.qti.hardware.aonutility.RecognitionConfig toAidlRecognitionConfig(com.qualcomm.qti.aonutility.RecognitionConfig sdk) {
        Log.d(TAG, "Converting RecognitionConfig: " + sdk);
        if (sdk == null) return null;
        vendor.qti.hardware.aonutility.RecognitionConfig aidl = new vendor.qti.hardware.aonutility.RecognitionConfig();
        aidl.captureRequested = sdk.isCaptureRequested();
        aidl.captureDuration = sdk.getCaptureDuration();
        aidl.numPhrases = sdk.getNumPhrases();
        List<com.qualcomm.qti.aonutility.PhraseRecognition> sdkPhrases = sdk.getPhrases();
        aidl.phrases = new vendor.qti.hardware.aonutility.PhraseRecognition[sdkPhrases.size()];
        for (int i = 0; i < sdkPhrases.size(); ++i) {
            aidl.phrases[i] = toAidlPhraseRecognition(sdkPhrases.get(i));
        }
        aidl.cookie = sdk.getCookie();
        aidl.dataSize = sdk.getDataSize();
        aidl.dataOffset = sdk.getDataOffset();
        return aidl;
    }

    /**
     * Converts a sound model type value from SDK {@link SoundModelsType}
     * to its corresponding value in AIDL {@link vendor.qti.hardware.aonutility.SoundModelType}.
     *
     * @param sdkType the input type value from SDK SoundModelType
     * @return the corresponding value for AIDL SoundModelType
     */
    private int convertSoundModelType(int sdkType) {
        if (sdkType == SoundModelType.ST_HAL_SOUND_MODEL_TYPE_UNKNOWN.getValue()) {
            return vendor.qti.hardware.aonutility.SoundModelType.SOUND_MODEL_TYPE_UNKNOWN;
        } else if (sdkType == SoundModelType.ST_HAL_SOUND_MODEL_TYPE_KEYPHRASE.getValue()) {
            return vendor.qti.hardware.aonutility.SoundModelType.SOUND_MODEL_TYPE_KEYPHRASE;
        } else if (sdkType == SoundModelType.ST_HAL_SOUND_MODEL_TYPE_GENERIC.getValue()) {
            return vendor.qti.hardware.aonutility.SoundModelType.SOUND_MODEL_TYPE_GENERIC;
        } else {
            Log.w(TAG, "Unknown SoundModelType value: " + sdkType + ", defaulting to UNKNOWN");
            return vendor.qti.hardware.aonutility.SoundModelType.SOUND_MODEL_TYPE_UNKNOWN;
        }
    }
}
