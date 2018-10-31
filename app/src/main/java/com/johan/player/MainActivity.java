package com.johan.player;

import android.os.Environment;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.widget.TextView;

public class MainActivity extends AppCompatActivity {

    private String videoPath;
    private Player player;

    private SurfaceView surfaceView;
    private SurfaceHolder surfaceHolder;
    private TextView currentTimeView, totalTimeView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        videoPath = Environment.getExternalStorageDirectory() + "/mv.mp4";
        player = new Player();
        surfaceView = (SurfaceView) findViewById(R.id.surface_view);
        surfaceHolder = surfaceView.getHolder();
        currentTimeView = (TextView) findViewById(R.id.current_time_view);
        totalTimeView = (TextView) findViewById(R.id.total_time_view);
    }

    public void playVideo(View view) {
        player.playVideo(videoPath, surfaceHolder.getSurface());
    }

    public void playAudio(View view) {
        player.playAudio(videoPath);
    }

    public void play(View view) {
        player.play(videoPath, surfaceHolder.getSurface(), new Player.PlayerCallback() {
            @Override
            public void onStart() {
                System.err.println("播放开始了 -------------------");
            }
            @Override
            public void onProgress(final int total, final int current) {
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        currentTimeView.setText(formatTime(current));
                        totalTimeView.setText(formatTime(total));
                    }
                });
            }
            @Override
            public void onEnd() {
                System.err.println("播放结束了 -------------------");
            }
        });
    }

    private String formatTime(int time) {
        int minute = time / 60;
        int second = time % 60;
        return (minute < 10 ? ("0" + minute) : minute) + ":" + (second < 10 ? ("0" + second) : second);
    }

}
