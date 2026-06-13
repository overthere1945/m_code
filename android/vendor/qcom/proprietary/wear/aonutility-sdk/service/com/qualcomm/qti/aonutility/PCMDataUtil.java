/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

/** @file PCMDataUtil.java
 * @addtogroup pcmdatautil
 * @{ */

package com.qualcomm.qti.aonutility;

import android.os.Parcel;
import android.os.Parcelable;
import android.util.Log;
import android.content.Context;
import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;
import org.xmlpull.v1.XmlPullParserFactory;

import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;

import java.util.List;
import java.util.ArrayList;
import java.util.HashMap;

import com.qualcomm.qti.aonutility.AONUtilityManager;
import com.qualcomm.qti.aonutility.AONPCMData;
import com.qualcomm.qti.aonutility.AONPCMType;
import com.qualcomm.qti.aonutility.AONAudioInfo;
import com.qualcomm.qti.aonutility.AONHapticInfo;
import com.qualcomm.qti.aonutility.XMLParser;
import vendor.qti.hardware.aonutility.*;


/**
 * A utility class used to download and offload default PCM audio and haptic data files from the SDK
 * to the HAL based on XML configuration.
 *
 * It provides methods to:
 * - Parse XML files names to create PCM data definitions which internally calls to SDK.
 * - Load and register audio and haptic data buffers.
 * - Interface with AONUtilityService to add PCM data to the system.
 *
 */
public class PCMDataUtil {
    private static final String TAG = "PCMDataUtil";
    private AONUtilityService aonUtilityService;

    public PCMDataUtil(AONUtilityService aonUtilityService){
        this.aonUtilityService = aonUtilityService;
    }

    /**
     * Loads and registers default PCM data from the given XML file.
     *
     * @param context      the aonutility service context
     * @param xmlFileName  the XML file name containing PCM data
     * @return 0 on success, -1 on failure
     */
    public int loadDefaultPCMFiles(Context context, String xmlFileName){
        Log.d(TAG, "loadDefaultPCMFiles called");
        List<AONPCMData> allPcmData = parseXML(context, xmlFileName);
        if (allPcmData == null) {
                Log.e(TAG, "PCM data list is null");
                return -1;
            }
        if (allPcmData.size() == 0) {
            Log.e(TAG, "PCM data size is 0");
            return -1;
        }
        Log.d(TAG, "Data Parsed from default XML=" + xmlFileName + " with size=" + allPcmData.size());
        for (AONPCMData pcmData : allPcmData){
            Log.d(TAG, pcmData.toString());
        }
        for (AONPCMData pcmData : allPcmData){
            if (pcmData == null) {
                Log.e(TAG, "pcmData is null");
                return -1;
            }
            int resultID = -1;
            try {
                Log.d(TAG, "Downloading default PCM Data with info = " + pcmData);
                resultID = aonUtilityService.getServiceImpl().addPCMData(pcmData);
            } catch (Exception e) {
                Log.e(TAG, "addPCMData failed with expception" + e.getMessage());
                e.printStackTrace();
            }
            if (resultID == -1) {
                Log.e(TAG, "loadDefaultPCMFiles failed");
                return -1;
            }
            else{
                Log.d(TAG, "PCM data downloaded with ID = " + resultID);
            }
        }
        return 0;
    }


    /**
     * Parses the given XML file and loads PCM data with audio and haptic buffers.
     *
     * @param context   the aonutility service context
     * @param fileName  the XML file name to parse
     * @return list of parsed {@code AONPCMData} objects
     */
    public List<AONPCMData> parseXML(Context context, String fileName) {
        Log.d(TAG, "called parseXML for " + fileName);
        List<AONPCMData> pcmDataList = new ArrayList<>();

        try {
            int resId = context.getResources().getIdentifier(fileName, "xml", context.getPackageName());
            if (resId == 0) {
                Log.d(TAG, "Resource not found for file: " + fileName);
                throw new Exception("Resource not found for file: " + fileName);
            }
            Log.d(TAG, "Resource found for file name '" + fileName + "' with ID: " + resId);

            XmlPullParser parser = context.getResources().getXml(resId);
            HashMap<String, String> fileNameToFilePathMap = XMLParser.parseXmlPullParser(parser, pcmDataList);

            if (pcmDataList.size() == 0){
                Log.d(TAG, "No data to send");
                throw new Exception("No data to send");
            }
            if (fileNameToFilePathMap.size() == 0){
                Log.d(TAG, "No file path found");
                throw new Exception("No file path found");
            }
            HashMap<String, byte[]> pcmNameToBufferMap = new HashMap<>();
            for (String pcmFileName : fileNameToFilePathMap.keySet()) {
                String filePath = fileNameToFilePathMap.get(pcmFileName);
                if (filePath != null){
                    byte[] buffer = readRawFile(context, filePath);
                    pcmNameToBufferMap.put(pcmFileName, buffer);
                }
            }
            XMLParser.loadPCMDataBuffers(pcmDataList, pcmNameToBufferMap);
        }
        catch (XmlPullParserException | IOException e) {
            Log.d(TAG, "Error parsing XML: " + e.getMessage());
            e.printStackTrace();
        }
        catch (Exception e){
            Log.d(TAG, "Faced Error: " + e.getMessage());
            e.printStackTrace();
        }
        return pcmDataList;
    }

    /**
     * Reads a .raw/.pcm file from the raw resource directory.
     *
     * @param context   the service context
     * @param fileName  the name of the raw file (with or without .pcm extension)
     * @return byte array of file content, or null if not found or error occurs
     */
    private byte[] readRawFile(Context context, String fileName) {
        try {
            int resId = context.getResources().getIdentifier(fileName, "raw", context.getPackageName());
            if (resId == 0) {
                Log.d(TAG, "Raw resource not found: " + fileName);
                return null;
            }
            Log.d(TAG, "Raw resource found for " + fileName + " with id = " + resId);
            InputStream inputStream = context.getResources().openRawResource(resId);
            return inputStream.readAllBytes();
        } catch (IOException e) {
            Log.d(TAG, "Error reading WAV file from raw: " + e.getMessage());
            e.printStackTrace();
            return null;
        }
    }

    /**
     * Prepares and populates the {@link vendor.qti.hardware.aonutility.PCMMetaData} object
     * with audio information from the provided {@link AONAudioInfo} instance.
     *
     * @param pcmMetaData the metadata object to be populated with audio data
     * @param audioInfo the source of audio information to be converted
     */
    public void prepareAudioInfo(vendor.qti.hardware.aonutility.PCMMetaData pcmMetaData, AONAudioInfo audioInfo){
        Log.d(TAG, "Converting Audio Data = " + audioInfo + " to HAL readable format");
        pcmMetaData.pcmDataType = PCMDataType.AUDIO;
        pcmMetaData.audioData.name = audioInfo.getName();
        pcmMetaData.audioData.playDurationMs = audioInfo.getPlayDurationMs();
        pcmMetaData.audioData.patternDataSize = audioInfo.getPatternDataSize();
        pcmMetaData.audioData.bitWidth = audioInfo.getBitWidth();
        pcmMetaData.audioData.fmt = convertToneFormat(audioInfo.getFmt());
        pcmMetaData.audioData.sampleRate = audioInfo.getSampleRate();
        pcmMetaData.audioData.numChannels = audioInfo.getNumChannels();
        pcmMetaData.audioData.audioBuf = audioInfo.getAudioBuf();
        Log.d(TAG, "PCM Audio Metadata: name=" + pcmMetaData.audioData.name +
                    ", playDurationMs=" + pcmMetaData.audioData.playDurationMs +
                    ", patternDataSize=" + pcmMetaData.audioData.patternDataSize +
                    ", bitWidth=" + pcmMetaData.audioData.bitWidth +
                    ", fmt=" + pcmMetaData.audioData.fmt +
                    ", sampleRate=" + pcmMetaData.audioData.sampleRate +
                    ", numChannels=" + pcmMetaData.audioData.numChannels);
    }

    /**
    * Populates the {@link vendor.qti.hardware.aonutility.PCMMetaData} object with haptic metadata
    * derived from the provided {@link AONHapticInfo} instance.
    *
    * @param pcmMetaData the metadata object to be populated with haptic data
    * @param hapticInfo the source of haptic information to be converted
    * @param pcmDataParentId the parent ID to use for custom pattern types
    */
    public void prepareHapticInfo(vendor.qti.hardware.aonutility.PCMMetaData pcmMetaData, AONHapticInfo hapticInfo,
            int pcmDataParentId){
        Log.d(TAG, "Converting Haptic Data = " + hapticInfo + " to HAL readable format");
        pcmMetaData.pcmDataType = PCMDataType.HAPTIC;
        if (hapticInfo.getHapticType() == HapticType.PREDEFINED_PATTERN){
            pcmMetaData.hapticData.id = hapticInfo.getHapticEffectID();
        }
        else if (hapticInfo.getHapticType() == HapticType.CUSTOM_PATTERN) {
            pcmMetaData.hapticData.id = pcmDataParentId;
        }
        pcmMetaData.hapticData.name = hapticInfo.getName();
        pcmMetaData.hapticData.playDurationMs = hapticInfo.getPlayDurationMs();
        pcmMetaData.hapticData.patternDataSize = hapticInfo.getPatternDataSize();
        pcmMetaData.hapticData.strength = convertEffectStrength(hapticInfo.getStrength());
        pcmMetaData.hapticData.hapticBuf = hapticInfo.getHapticBuf();
        pcmMetaData.hapticData.loopCount = hapticInfo.getLoopCount();
        Log.d(TAG, "PCM Haptic Metadata: id=" + pcmMetaData.hapticData.id +
                    ", name=" + pcmMetaData.hapticData.name +
                    ", playDurationMs=" + pcmMetaData.hapticData.playDurationMs +
                    ", patternDataSize=" + pcmMetaData.hapticData.patternDataSize +
                    ", strength=" + pcmMetaData.hapticData.strength +
                    ", loopCount=" + pcmMetaData.hapticData.loopCount);
    }

    /**
     * Converts a given PCM type from {@link AONPCMType} to its corresponding value in {@link vendor.qti.hardware.aonutility.PCMDataType}.
     *
     * @param pcmType the input PCM type from {@link AONPCMType}
     * @return the corresponding PCM data type from {@link PCMDataType}
     * @throws IllegalArgumentException if the input type is unknown
     */
    public int convertPCMType(int pcmType) throws Exception{
        if (pcmType == AONPCMType.AUDIO){
            return PCMDataType.AUDIO;
        } else if (pcmType == AONPCMType.HAPTIC){
            return PCMDataType.HAPTIC;
        } else if (pcmType == AONPCMType.AUDIO_SYNC_HAPTIC){
            return PCMDataType.AUDIO_SYNC_HAPTIC;
        } else{
            Log.e(TAG, "flushAllPCMData failed! Unknown type = " + pcmType + " found for flush All");
            throw new IllegalArgumentException("Unknown type = " + pcmType + " found for flush All");
        }
    }

    /**
     * Converts a given haptic tone format constant from {@link HapticToneFormat}
     * to its corresponding value in {@link vendor.qti.hardware.aonutility.ToneFormat}.
     *
     * @param toneFormat the input tone format constant from {@link HapticToneFormat}
     * @return the corresponding tone format constant from {@link ToneFormat}
     */
    private int convertToneFormat(int toneFormat){
        switch (toneFormat) {
            case HapticToneFormat.AP_OFFLOAD_AUDIO_FMT_PCM_S8_LE:
                return ToneFormat.AP_OFFLOAD_AUDIO_FMT_PCM_S8_LE;
            case HapticToneFormat.AP_OFFLOAD_AUDIO_FMT_DEFAULT_PCM:             // same case for AP_OFFLOAD_AUDIO_FMT_PCM_S16_LE
                return ToneFormat.AP_OFFLOAD_AUDIO_FMT_DEFAULT_PCM;
            case HapticToneFormat.AP_OFFLOAD_AUDIO_FMT_PCM_S24_LE:
                return ToneFormat.AP_OFFLOAD_AUDIO_FMT_PCM_S24_LE;
            case HapticToneFormat.AP_OFFLOAD_AUDIO_FMT_PCM_S24_3LE:
                return ToneFormat.AP_OFFLOAD_AUDIO_FMT_PCM_S24_3LE;
            case HapticToneFormat.AP_OFFLOAD_AUDIO_FMT_PCM_S32_LE:
                return ToneFormat.AP_OFFLOAD_AUDIO_FMT_PCM_S32_LE;
            case HapticToneFormat.AP_OFFLOAD_AUDIO_FMT_DEFAULT_COMPRESSED:      // same case for AP_OFFLOAD_AUDIO_FMT_MP3
                return ToneFormat.AP_OFFLOAD_AUDIO_FMT_DEFAULT_COMPRESSED;
            case HapticToneFormat.AP_OFFLOAD_AUDIO_FMT_AAC_ADTS:
                return ToneFormat.AP_OFFLOAD_AUDIO_FMT_AAC_ADTS;
            case HapticToneFormat.AP_OFFLOAD_AUDIO_FMT_NON_SUPPORTABLE:
            default:
                return ToneFormat.AP_OFFLOAD_AUDIO_FMT_NON_SUPPORTABLE;
        }
    }

    /**
     * Converts a haptic effect strength value from {@link HapticEffectStrength}
     * to its corresponding value in {@link vendor.qti.hardware.aonutility.EffectStrength}.
     *
     * @param strength the input strength value from {@link HapticEffectStrength}
     * @return the corresponding strength value from {@link EffectStrength}
     */
    private int convertEffectStrength(int strength){
        switch (strength){
            case HapticEffectStrength.LIGHT:
                return EffectStrength.LIGHT;
            case HapticEffectStrength.MEDIUM:
                return EffectStrength.MEDIUM;
            case HapticEffectStrength.STRONG:
                return EffectStrength.STRONG;
            default:
                return EffectStrength.MEDIUM;
        }
    }
}
