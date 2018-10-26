package com.johan.player;

import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.view.Surface;

/**
 * Created by johan on 2018/10/16.
 */

public class Player {

    private AudioTrack audioTrack;

    static {
        System.loadLibrary("player");
    }

    /**
     * 播放视频
     * @param path
     * @param surface
     */
    public native void playVideo(String path, Surface surface);

    /**
     * 播放音频
     * @param path
     */
    public native void playAudio(String path);

    /**
     * 创建 AudioTrack
     * 由 C 反射调用
     * @param sampleRate  采样率
     * @param channels     通道数
     */
    public void createAudioTrack(int sampleRate, int channels) {
        int channelConfig;
        if (channels == 1) {
            channelConfig = AudioFormat.CHANNEL_OUT_MONO;
        } else if (channels == 2) {
            channelConfig = AudioFormat.CHANNEL_OUT_STEREO;
        }else {
            channelConfig = AudioFormat.CHANNEL_OUT_MONO;
        }
        int bufferSize = AudioTrack.getMinBufferSize(sampleRate, channelConfig, AudioFormat.ENCODING_PCM_16BIT);
        audioTrack = new AudioTrack(AudioManager.STREAM_MUSIC, sampleRate, channelConfig,
                AudioFormat.ENCODING_PCM_16BIT, bufferSize, AudioTrack.MODE_STREAM);
        audioTrack.play();
    }

    /**
     * 播放 AudioTrack
     * 由 C 反射调用
     * @param data
     * @param length
     */
    public void playAudioTrack(byte[] data, int length) {
        if (audioTrack != null && audioTrack.getPlayState() == AudioTrack.PLAYSTATE_PLAYING) {
            audioTrack.write(data, 0, length);
        }
    }

    /**
     * 释放 AudioTrack
     * 由 C 反射调用
     */
    public void releaseAudioTrack() {
        if (audioTrack != null) {
            if (audioTrack.getPlayState() == AudioTrack.PLAYSTATE_PLAYING) {
                audioTrack.stop();
            }
            audioTrack.release();
            audioTrack = null;
        }
    }

    /**
     * 同步播放音视频
     * @param path
     * @param surface
     */
    public native void play(String path, Surface surface);

    /**
     * 测试C多线程
     */
    /**
    public native void testCThread();
     */

}
