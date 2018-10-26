package com.johan.player;

import android.os.Environment;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;

public class MainActivity extends AppCompatActivity {

    private String videoPath;
    private Player player;

    private SurfaceView surfaceView;
    private SurfaceHolder surfaceHolder;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        videoPath = Environment.getExternalStorageDirectory() + "/mv.mp4";
        player = new Player();
        surfaceView = (SurfaceView) findViewById(R.id.surface_view);
        surfaceHolder = surfaceView.getHolder();
    }

    public void playVideo(View view) {
        player.playVideo(videoPath, surfaceHolder.getSurface());
    }

    public void playAudio(View view) {
        player.playAudio(videoPath);
    }

    public void play(View view) {
        player.play(videoPath, surfaceHolder.getSurface());
    }

}
