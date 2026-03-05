package com.memryx.memxdriver;

import android.app.ActivityManager;
import android.content.pm.ConfigurationInfo;
import android.content.ComponentName;
import android.content.Context;
import android.content.ContentValues;
import android.content.Intent;
import android.content.ServiceConnection;
import android.net.Uri;
import android.graphics.Bitmap;
import android.graphics.RectF;
import android.graphics.SurfaceTexture;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.SystemClock;
import android.provider.MediaStore.Images.Media;
import android.util.Log;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ListView;
import android.widget.Toast;
import android.widget.TableLayout;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.camera.core.CameraSelector;
import androidx.camera.core.ImageAnalysis;
import androidx.camera.core.ImageProxy;
import androidx.camera.core.Preview;
import androidx.camera.lifecycle.ProcessCameraProvider;
import androidx.camera.view.PreviewView;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.params.StreamConfigurationMap;

import com.google.common.util.concurrent.ListenableFuture;
import com.memryx.memxdriver.IMemxDriverService;
import com.memryx.jnilib.MemxHeader;
import com.memryx.jnilib.NativeLib;
import com.memryx.jnilib.Detection;
import io.github.crow_misia.libyuv.*;
import io.github.crow_misia.libyuv.ext.*;

import org.opencv.android.OpenCVLoader;
import org.opencv.core.CvType;
import org.opencv.core.Mat;
import org.opencv.core.Scalar;
import org.opencv.core.Size;
import org.opencv.imgproc.Imgproc;

import java.io.File;
import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.Arrays;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeUnit;

public class MainActivity extends AppCompatActivity implements View.OnClickListener {
    private static final String TAG = "MainActivity";

    // defines
    private static final int BLOCKING_QUEUE_SIZE = 3;
    private static final int FPS_INTERVAL_SEC = 2;
    private static final long PROCESS_INTERVAL_MS = 0;
    private static final int QUEUE_TIMEOUT_MS = 500;
    private static final int TOPS_COUNT = 10;
    private static final int MAX_TOPS_COUNT = 200;
    private static final int INPUT_SIZE = 640;
    private static final int NUM_CLASSES = 80;    // COCO classes
    private static final float CONF_THRESHOLD = 0.25f;
    private static final float IOU_THRESHOLD = 0.45f;

    // UI elements
    private EditText editTextDirPath;
    private EditText editTextFrames;
    private Button buttonStartBenchmark;
    private Button buttonStopBenchmark;
    private Button buttonLoadDfp;
    private Button buttonStartDetect;
    private Button buttonStopDetect;
    private ListView listViewResults;
    private PreviewView previewView;
    private TableLayout resultsTable;
    private TextView fpsTextView;

    // Results adapter
    private ArrayList<String> results;
    private ArrayAdapter<String> adapter;
    private long fpsIntervalStartMs;
    private int framesSinceInterval;
    private float preprocessTimeMs = 0.0f;
    private float inferenceTimeMs = 0.0f;
    private float postprocTimeMs = 0.0f;
    private float uiTimeMs = 0.0f;
    private int preprocessCount = 0;
    private int inferenceCount = 0;
    private int postprocCount = 0;
    private int uiCount = 0;

    // JNI and DFP info
    private NativeLib nativeLib;
    private NativeLib.DfpInfo currentDfpInfo;
    private byte currentModelId = 0;
    private NativeLib.PortInfo inputPort;
    private List<NativeLib.PortInfo> outputPorts;
    int numOutputs = 0;

    // --- Service Connection ---
    private IMemxDriverService memxService;
    private boolean isBound = false;

    // Benchmark state
    private volatile boolean runBenchmarkFlag = false;
    private ExecutorService executorService = Executors.newSingleThreadExecutor();
    private Future<?> benchmarkTaskFuture = null;
    private Handler mainThreadHandler = new Handler(Looper.getMainLooper());

    // Detection pipeline
    private volatile boolean runDetectFlag = false;
    private BlockingQueue<ImageProxy> imageQueue = new LinkedBlockingQueue<>(BLOCKING_QUEUE_SIZE);
    private BlockingQueue<byte[]> inferenceQueue = new LinkedBlockingQueue<>(BLOCKING_QUEUE_SIZE);
    private BlockingQueue<byte[][]> postprocQueue = new LinkedBlockingQueue<>(BLOCKING_QUEUE_SIZE);
    private BlockingQueue<InferenceResult> resultQueue = new LinkedBlockingQueue<>(BLOCKING_QUEUE_SIZE);
    private Thread preprocThread, inferenceThread, uiThread;
    private ExecutorService detectExecutor;

    // CameraX
    private ExecutorService cameraExecutor;
    private static final String[] REQUIRED_PERMISSIONS = { android.Manifest.permission.CAMERA };
    private static final int REQUEST_CODE_PERMISSIONS = 10;
    private long lastProcessTime = 0;
    private int cameraWidth = 0;
    private int cameraHeight = 0;
    private Mat yuvMat, rgbMat, resizedMat, paddedMat, floatMat;
    private float[] normalizeFloats;
    private float[][] outputFloatsArr;
    private Nv21Buffer nv21Buf;
    private byte[] nv21Bytes;
    private byte[] rawInputBytes;
    private byte[][] rawOutputBytesArr;
    private ByteBuffer inBuf;
    private ByteBuffer[] outBufs;
    private int inputByteSize, outputByteSize;
    private double inputScale = 1.0;
    private int newInputWidth = 0;
    private int newInputHeight = 0;
    private boolean cameraReady = false;
    private boolean imageSaved = false;

    private static final String[] COCO_NAMES = {
        "person","bicycle","car","motorbike","aeroplane","bus","train","truck","boat","traffic light",
        "fire hydrant","stop sign","parking meter","bench","bird","cat","dog","horse","sheep","cow",
        "elephant","bear","zebra","giraffe","backpack","umbrella","handbag","tie","suitcase","frisbee",
        "skis","snowboard","sports ball","kite","baseball bat","baseball glove","skateboard","surfboard","tennis racket","bottle",
        "wine glass","cup","fork","knife","spoon","bowl","banana","apple","sandwich","orange",
        "broccoli","carrot","hot dog","pizza","donut","cake","chair","sofa","pottedplant","bed",
        "diningtable","toilet","tvmonitor","laptop","mouse","remote","keyboard","cell phone","microwave","oven",
        "toaster","sink","refrigerator","book","clock","vase","scissors","teddy bear","hair drier","toothbrush"
    };

    // Overlay view for results
    private OverlayView overlayView;
    private final List<RectF> resultBoxes = new ArrayList<>();
    private final List<String> resultLabels = new ArrayList<>();

    // Holds inference result for UI
    public static class InferenceResult {
        public final List<Detection> detections;

        public InferenceResult(List<Detection> dets) {
            this.detections = dets;
        }
    }

    // --- ServiceConnection Implementation ---
    private ServiceConnection memxConnection = new ServiceConnection() {
        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            memxService = IMemxDriverService.Stub.asInterface(service);
            isBound = true;
            Log.d(TAG, "Service Connected to: " + name.getClassName());
            runOnUiThread(() -> {
                buttonStartBenchmark.setEnabled(true);
                Toast.makeText(MainActivity.this, "MemryX Service Connected", Toast.LENGTH_SHORT).show();
            });
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
            memxService = null;
            isBound = false;
            Log.w(TAG, "Service Disconnected from: " + name.getClassName());
            runOnUiThread(() -> {
                buttonStartBenchmark.setEnabled(false);
                buttonStopBenchmark.setEnabled(false);
                Toast.makeText(MainActivity.this, "MemryX Service Disconnected", Toast.LENGTH_SHORT).show();
            });
        }
    };

    @Override
    protected void onStart() {
        super.onStart();
        bindMemxService(); // Bind service on start
    }

    @Override
    protected void onStop() {
        super.onStop();
        unbindMemxService(); // Unbind service on stop
    }

    // Encapsulated bind logic (remains the same)
    private void bindMemxService() {
        if (!isBound && memxService == null) {
            Intent intent = new Intent();
            String targetPackage = "com.memryx.systemapp";
            String targetClass = "com.memryx.systemapp.MemxDriverPublicService";
            intent.setComponent(new ComponentName(targetPackage, targetClass));

            Log.d(TAG, "Attempting to bind service: " + targetClass);
            try {
                boolean success = bindService(intent, memxConnection, Context.BIND_AUTO_CREATE);
                if (!success) {
                    Log.e(TAG, "bindService() failed immediately. Check configuration.");
                    Toast.makeText(this, "Failed to initiate binding", Toast.LENGTH_LONG).show();
                } else {
                    Log.d(TAG, "bindService() initiation successful.");
                }
            } catch (SecurityException e) {
                 Log.e(TAG, "bindService() failed: SecurityException. Check permissions.", e);
                 Toast.makeText(this, "Permission denied to bind service", Toast.LENGTH_LONG).show();
            } catch (Exception e) {
                 Log.e(TAG, "bindService() failed.", e);
                 Toast.makeText(this, "Error binding service", Toast.LENGTH_LONG).show();
            }
        }
    }

    // Encapsulated unbind logic (remains the same)
    private void unbindMemxService() {
        if (isBound) {
            Log.d(TAG, "Unbinding service...");
            stopBenchmark();
            unbindService(memxConnection);
            isBound = false;
            memxService = null;
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // initialize UI
        editTextDirPath = findViewById(R.id.editText_dirPath);
        editTextFrames = findViewById(R.id.editText_frames);
        buttonStartBenchmark = findViewById(R.id.button_start_benchmark);
        buttonStopBenchmark = findViewById(R.id.button_stop_benchmark);
        buttonLoadDfp = findViewById(R.id.button_load_dfp);
        buttonStartDetect = findViewById(R.id.button_start_detect);
        buttonStopDetect = findViewById(R.id.button_stop_detect);
        listViewResults = findViewById(R.id.listView_results);
        previewView = findViewById(R.id.previewView);
        overlayView = findViewById(R.id.overlayView);
        resultsTable = findViewById(R.id.results_table);
        fpsTextView  = findViewById(R.id.fps_textview);

        // adapter setup
        results = new ArrayList<>();
        adapter = new ArrayAdapter<>(this, android.R.layout.simple_list_item_1, results);
        listViewResults.setAdapter(adapter);

        // listeners
        buttonStartBenchmark.setOnClickListener(this);
        buttonStopBenchmark.setOnClickListener(this);
        buttonLoadDfp.setOnClickListener(this);
        buttonStartDetect.setOnClickListener(this);
        buttonStopDetect.setOnClickListener(this);

        // initial states
        editTextFrames.setText("1");
        buttonStopBenchmark.setEnabled(false);
        buttonLoadDfp.setEnabled(false);
        buttonStopDetect.setEnabled(false);
        buttonStartDetect.setEnabled(false);
        fpsIntervalStartMs = System.currentTimeMillis();
        framesSinceInterval = 0;

        // setup path
        String defaultPath = "/data/dfp";
        editTextDirPath.setText(defaultPath);

        // load JNI library
        try { Class.forName(NativeLib.class.getName()); } catch (ClassNotFoundException e) { Log.e(TAG, "NativeLib not found", e); }

        // start camera
        cameraExecutor = Executors.newSingleThreadExecutor();
        if (allPermissionsGranted()) {
            startCamera();
        } else {
            ActivityCompat.requestPermissions(this, REQUIRED_PERMISSIONS, REQUEST_CODE_PERMISSIONS);
        }

        // init OpenCV
        if (!OpenCVLoader.initDebug()) {
            Log.e(TAG, "OpenCV init failed");
        }
    }

    public boolean isEs3Supported(Context context) {
        ActivityManager am = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        ConfigurationInfo info = am.getDeviceConfigurationInfo();
        return info.reqGlEsVersion >= 0x30000;
    }

    @Override
    public void onClick(View v) {
        int id = v.getId();
        if (id == R.id.button_start_benchmark) {
            startBenchmark();
        } else if (id == R.id.button_stop_benchmark) {
            stopBenchmark();
        } else if (id == R.id.button_load_dfp) {
            loadDfp();
        } else if (id == R.id.button_start_detect) {
            startDetect();
        } else if (id == R.id.button_stop_detect) {
            stopDetect();
        }
    }

    private void loadDfp() {
        String dir = editTextDirPath.getText().toString().trim();
        addResultOnUiThread("Searching DFP files in: " + dir);
        nativeLib = new NativeLib();
        try {
            List<String> files = findDfpFiles(dir);
            Log.d(TAG, "findDfpFiles returned " + files.size() + " files");
            if (files.isEmpty()) {
                throw new RuntimeException("No DFP files found");
            }

            // load first model
            String path = files.get(0);
            addResultOnUiThread("Loading DFP from: " + path);
            currentDfpInfo = nativeLib.loadDfpInfo(path);
            if (currentDfpInfo == null || !currentDfpInfo.valid) {
                throw new RuntimeException("Invalid DFP info");
            }
            addResultOnUiThread("DFP parsed: inputs=" + currentDfpInfo.inputPorts.size()
                            + ", outputs=" + currentDfpInfo.outputPorts.size()
                            + ", chips=" + currentDfpInfo.num_chips);

            // open device
            int s = memxService.memxOpen(currentModelId, (byte)0, MemxHeader.MEMX_DEVICE_CASCADE_PLUS);
            if (s != MemxHeader.MEMX_STATUS_OK) throw new RuntimeException("memx_open failed: " + s);

            // config MPU
            byte cfg = mapNumChipsToConfig(currentDfpInfo.num_chips);
            s = memxService.memxConfigMpuGroup((byte)0, cfg);
            if (s != MemxHeader.MEMX_STATUS_OK) throw new RuntimeException("memx_config failed: " + s);

            // download model
            s = memxService.memxDownloadModel(currentModelId, path, (byte)0, MemxHeader.MEMX_DOWNLOAD_TYPE_WTMEM_AND_MODEL);
            if (s != MemxHeader.MEMX_STATUS_OK) throw new RuntimeException("download_model failed: " + s);

            // enable stream
            s = memxService.memxSetStreamEnable(currentModelId, (byte)0);
            if (s != MemxHeader.MEMX_STATUS_OK) throw new RuntimeException("set_stream_enable failed: " + s);
            addResultOnUiThread("DFP loaded successfully");

            inputPort  = currentDfpInfo.inputPorts.get(0);
            inputByteSize = nativeLib.calculateFormattedSize(
                inputPort.total_size, inputPort.format,
                inputPort.dim_h, inputPort.dim_w, inputPort.dim_z, inputPort.dim_c);
            rawInputBytes = new byte[inputByteSize];
            Log.d(TAG, "DFP input0 format=" + inputPort.format + " HWZC(" + inputPort.dim_h
                        + ", " + inputPort.dim_w + ", " + inputPort.dim_z
                        + ", " + inputPort.dim_c + ") Size=" + inputByteSize);
            inBuf = ByteBuffer.allocateDirect(inputByteSize)
                    .order(ByteOrder.nativeOrder());

            outputPorts = currentDfpInfo.outputPorts;
            numOutputs = outputPorts.size();
            outBufs = new ByteBuffer[numOutputs];
            rawOutputBytesArr = new byte[numOutputs][];

            for (int i = 0; i < numOutputs; i++) {
                NativeLib.PortInfo p = outputPorts.get(i);
                int byteSize = nativeLib.calculateFormattedSize(
                    p.total_size, p.format,
                    p.dim_h, p.dim_w, p.dim_z, p.dim_c);
                Log.d(TAG, "DFP output" + i + " format=" + p.format + " HWZC(" + p.dim_h
                        + ", " + p.dim_w + ", " + p.dim_z + ", " + p.dim_c + "), Size=" + byteSize);
                rawOutputBytesArr[i] = new byte[byteSize];
                outBufs[i] = ByteBuffer
                    .allocateDirect(byteSize)
                    .order(ByteOrder.nativeOrder());
            }

            inputScale = Math.min(
                inputPort.dim_h / (double) cameraHeight,
                inputPort.dim_w / (double) cameraWidth);
            newInputHeight = (int) (cameraHeight * inputScale);
            newInputWidth = (int) (cameraWidth * inputScale);
            addResultOnUiThread("Camera: " + cameraWidth + "x" + cameraHeight +
                " -> " + newInputWidth + "x" + newInputHeight);

            yuvMat     = new Mat(cameraHeight*3/2, cameraWidth, CvType.CV_8UC1);
            rgbMat     = new Mat(cameraHeight, cameraWidth, CvType.CV_8UC3);
            resizedMat = new Mat(inputPort.dim_h, inputPort.dim_w, CvType.CV_8UC3);
            paddedMat  = new Mat(inputPort.dim_h, inputPort.dim_w, CvType.CV_8UC3);
            floatMat   = new Mat(inputPort.dim_h, inputPort.dim_w, CvType.CV_32FC3);
            nv21Buf = Nv21Buffer.Factory.allocate(cameraWidth, cameraHeight);
            nv21Bytes = new byte[cameraWidth * cameraHeight * 3 / 2];
            normalizeFloats = new float[inputPort.dim_h * inputPort.dim_w * inputPort.dim_c];
            outputFloatsArr = new float[numOutputs][];
            for (int i = 0; i < numOutputs; i++) {
                NativeLib.PortInfo p = outputPorts.get(i);
                outputFloatsArr[i] = new float[p.total_size];
            }

            runOnUiThread(() -> {
                overlayView.setInputSize(newInputWidth, newInputHeight);
                buttonStartDetect.setEnabled(true);
                buttonLoadDfp.setEnabled(false);
            });

        } catch (Exception e) {
            Log.e(TAG, "loadDfp error", e);
            addResultOnUiThread("loadDfp fail: " + e.getMessage());
        }
    }

    // Preprocessing thread
    private void preprocLoop() {
        while (runDetectFlag) {
            ImageProxy img = null;
            try {
                img = imageQueue.poll(QUEUE_TIMEOUT_MS, TimeUnit.MILLISECONDS);
                if (img == null) continue;

                long t0 = System.nanoTime();

				// Convert YUV to NV21
                I420Buffer i420Buf = ImageProxyExt.toI420Buffer(img);
                i420Buf.convertTo(nv21Buf);
                ByteBuffer nv21BB = nv21Buf.asBuffer();
                nv21BB.position(0);
                nv21BB.get(nv21Bytes);
                i420Buf.close();
                yuvMat.put(0, 0, nv21Bytes);

                // Convert NV21 to RGB
                Imgproc.cvtColor(yuvMat, rgbMat, Imgproc.COLOR_YUV2RGB_NV21);

                // Resize and pad to inputPort dimensions
                if (resizedMat.empty() || paddedMat.empty() || floatMat.empty()) {
                    Log.e(TAG, "PreprocThread: Mat is empty, cannot proceed");
                    continue;
                }
                if (newInputWidth <= 0 || newInputHeight <= 0) {
                    Log.e(TAG, "PreprocThread: Invalid input dimensions, cannot proceed");
                    continue;
                }
                Imgproc.resize(rgbMat, resizedMat, new Size(newInputWidth, newInputHeight), 0, 0, Imgproc.INTER_NEAREST);

                // Padding to grey
                paddedMat.setTo(new Scalar(114, 114, 114));
                resizedMat.copyTo(paddedMat.submat(0, newInputHeight, 0, newInputWidth));

                // Convert to float and normalize
                paddedMat.convertTo(floatMat, CvType.CV_32FC3, 1.0 / 255.0);
                floatMat.get(0, 0, normalizeFloats);

                // Convert floats to formatted bytes
                boolean convOk = nativeLib.convertFloatToFormattedBytes(
                    normalizeFloats,
                    rawInputBytes,
                    inputPort.dim_h, inputPort.dim_w,
                    inputPort.dim_z, inputPort.dim_c,
                    inputPort.format
                );
                if (!convOk) {
                    throw new RuntimeException("convert failed");
                }

                // inBuf.clear();
                // inBuf.put(rawInputBytes);
                // inBuf.flip();
                long t1 = System.nanoTime();
                preprocessTimeMs += (t1 - t0) / 1_000_000.0f;
                preprocessCount++;

                boolean offered = inferenceQueue.offer(rawInputBytes, QUEUE_TIMEOUT_MS, TimeUnit.MILLISECONDS);
                img.close();
            } catch (InterruptedException ex) {
                Log.i(TAG, "PreprocThread: interrupted, exiting");
                break;
            } catch (Exception ex) {
                Log.e(TAG, "PreprocThread: exception in preprocessing", ex);
            }
        }
    }

    // Inference thread
    private void inferenceLoop() {
        while (runDetectFlag) {
            try {
                byte[] inBuf = inferenceQueue.poll(QUEUE_TIMEOUT_MS, TimeUnit.MILLISECONDS);
                // ByteBuffer inBuf = inferenceQueue.poll(QUEUE_TIMEOUT_MS, TimeUnit.MILLISECONDS);
                if (inBuf == null) { continue; }

                long t0 = System.nanoTime();
                int ifs = memxService.memxStreamIfmap(currentModelId, (byte)inputPort.port, inBuf, 0);

                for (int i = 0; i < numOutputs; i++) {
                    NativeLib.PortInfo p = outputPorts.get(i);
                    // ByteBuffer ob = outBufs[i];
                    byte[] rawBytes = rawOutputBytesArr[i];

                    if (!p.active) continue;

                    // ob.clear();
                    int ofs = memxService.memxStreamOfmap(currentModelId, (byte)i, rawBytes, 0);

                //     ob.rewind();
                //     ob.get(rawBytes);
                }

                long t1 = System.nanoTime();
                inferenceTimeMs += (t1 - t0) / 1_000_000.0f;
                inferenceCount++;

                postprocQueue.offer(rawOutputBytesArr, QUEUE_TIMEOUT_MS, TimeUnit.MILLISECONDS);

            } catch (InterruptedException ex) {
                Log.i(TAG, "InferenceThread: interrupted, exiting");
                break;
            } catch (Exception ex) {
                Log.e(TAG, "InferenceThread: exception in inference", ex);
                }
        }
    }

    private void postprocLoop() {
        while (runDetectFlag) {
            try {
                byte[][] rawOutputs = postprocQueue.poll(QUEUE_TIMEOUT_MS, TimeUnit.MILLISECONDS);
                if (rawOutputs == null) { continue; }

                long t0 = System.nanoTime();

                // Unconvert formatted bytes
                for (int i = 0; i < numOutputs; i++) {
                    NativeLib.PortInfo p = outputPorts.get(i);
                    if (!p.active) continue;

                    boolean unconvOk = nativeLib.unconvertFormattedBytesToFloat(
                        rawOutputs[i],
                        outputFloatsArr[i],
                        p.dim_h, p.dim_w,
                        p.dim_z, p.dim_c,
                        p.format,
                        p.hpoc_en,
                        p.hpoc_en ? (p.hpoc_dim_c - p.dim_c) : 0,
                        p.hpoc_dummy_channels,
                        (p.format == MemxHeader.MEMX_FMAP_FORMAT_GBF80_ROW_PAD)
                    );
                    if (!unconvOk) {
                        throw new RuntimeException("unconvert failed on port " + i);
                    }
                }

                long t2 = System.nanoTime();

                int numFlows = outputFloatsArr.length;
                int[] dims = new int[numFlows*3];
                int totalLen = 0;
                for (int i = 0; i < numFlows; i++) {
                    dims[i*3  ] = outputPorts.get(i).dim_h;
                    dims[i*3+1] = outputPorts.get(i).dim_w;
                    dims[i*3+2] = outputPorts.get(i).dim_c;
                    totalLen += outputPorts.get(i).total_size;
                }
                Detection[] dets = nativeLib.postProcessNative(
                    outputFloatsArr, dims, numFlows,
                    CONF_THRESHOLD, IOU_THRESHOLD, TOPS_COUNT,
                    inputPort.dim_w, inputPort.dim_h, MAX_TOPS_COUNT
                );

                long t1 = System.nanoTime();
                postprocTimeMs += (t1 - t0) / 1_000_000.0f;
                postprocCount++;
                resultQueue.offer(new InferenceResult(Arrays.asList(dets)), QUEUE_TIMEOUT_MS, TimeUnit.MILLISECONDS);
            } catch (InterruptedException e) { break; }
        }
    }

    private void uiLoop() {
        while (runDetectFlag) {
            try {
                InferenceResult res = resultQueue.poll(QUEUE_TIMEOUT_MS, TimeUnit.MILLISECONDS);
                if (res == null) continue;
                if (res.detections.isEmpty()) continue;

                long t0 = System.nanoTime();
                runOnUiThread(() -> {
                    framesSinceInterval++;
                    long now = System.currentTimeMillis();
                    long elapsed = now - fpsIntervalStartMs;
                    float avgFps = 0;
                    if (elapsed >= FPS_INTERVAL_SEC * 1000) {
                        avgFps = framesSinceInterval * 1000f / elapsed;
                        fpsTextView.setText(String.format("FPS: %.2f", avgFps));
                        fpsIntervalStartMs = now;
                        framesSinceInterval = 0;

                        Log.d(TAG, String.format("Average Preproc: %.2f ms, Inference: %.2f ms, Postproc: %.2f ms, UI: %.2f ms",
                            preprocessTimeMs/preprocessCount, inferenceTimeMs/inferenceCount, postprocTimeMs/postprocCount, uiTimeMs/uiCount));
                        preprocessTimeMs = 0.0f;
                        inferenceTimeMs = 0.0f;
                        postprocTimeMs = 0.0f;
                        uiTimeMs = 0.0f;
                        preprocessCount = 0;
                        inferenceCount = 0;
                        postprocCount = 0;
                        uiCount = 0;
                    }

                    // resultsTable.removeAllViews();
                    // TableRow header = new TableRow(this);
                    // header.addView(makeCell("index"));
                    // header.addView(makeCell("label"));
                    // header.addView(makeCell("score"));
                    // header.addView(makeCell("box"));
                    // resultsTable.addView(header);

                    // Log.d(TAG, "UIThread: setDetections called with " + res.detections.size() + " detections");
                    List<Detection> tops = res.detections.size() <= TOPS_COUNT
                        ? res.detections
                        : res.detections.subList(0, TOPS_COUNT);

                    overlayView.setDetections(tops);
                    overlayView.invalidate();

                    // for (int i = 0; i < tops.size(); i++) {
                    //     Detection det = tops.get(i);

                    //     float x1 = det.box.left;
                    //     float y1 = det.box.top;
                    //     float x2 = det.box.right;
                    //     float y2 = det.box.bottom;
                    //     String boxText = String.format("(%.2f, %.2f, %.2f, %.2f)", x1, y1, x2, y2);

                    //     Log.d(TAG, "Detection " + i + ": label=" + det.label + " (" + COCO_NAMES[det.label] + ")" +
                    //             ", score=" + det.score + ", " + boxText);

                    //     TableRow row = new TableRow(this);
                    //     row.addView(makeCell(String.valueOf(i + 1)));
                    //     row.addView(makeCell(COCO_NAMES[det.label]));
                    //     row.addView(makeCell(String.format(Locale.US, "%.2f", det.score)));
                    //     row.addView(makeCell(boxText));
                    //     resultsTable.addView(row);
                    // }
                });
                long t1 = System.nanoTime();
                uiTimeMs += (t1 - t0) / 1_000_000.0f;
                uiCount++;

            } catch (InterruptedException ex) {
                Log.i(TAG, "UIThread: interrupted, exiting");
                Thread.currentThread().interrupt();
                break;
            }
        }
    }

    private void startDetect() {
        addResultOnUiThread("startDetect invoked");

        if (currentDfpInfo == null) {
            Toast.makeText(this, "Load DFP first", Toast.LENGTH_SHORT).show();
            Log.w(TAG, "startDetect: currentDfpInfo == null, abort");
            return;
        }

        runDetectFlag = true;
        buttonStartDetect.setEnabled(false);
        buttonStopDetect.setEnabled(true);

        detectExecutor = Executors.newFixedThreadPool(3);
        detectExecutor.execute(this::preprocLoop);
        detectExecutor.execute(this::inferenceLoop);
        detectExecutor.execute(this::postprocLoop);
        uiThread        = new Thread(this::uiLoop,        "UIThread");
        uiThread.start();
    }

    private void stopDetect() {
        runDetectFlag = false;
        detectExecutor.shutdownNow();
        if (uiThread != null) uiThread.interrupt();
        addResultOnUiThread("Detection stopped");
        buttonLoadDfp.setEnabled(true);
        buttonStartDetect.setEnabled(true);
        buttonStopDetect.setEnabled(false);
    }

    private TextView makeCell(String text) {
        TextView tv = new TextView(this);
        tv.setText(text);
        tv.setPadding(8, 4, 8, 4);
        return tv;
    }

    private void saveBitmapToGallery(Bitmap bmp) {
        ContentValues values = new ContentValues();
        values.put(Media.DISPLAY_NAME,
                   "memx_capture_" + System.currentTimeMillis() + ".jpg");
        values.put(Media.MIME_TYPE, "image/jpeg");
        values.put(Media.RELATIVE_PATH,
                   Environment.DIRECTORY_DCIM + "/Camera");

        Uri uri = null;
        try {
            uri = getContentResolver().insert(
                    Media.EXTERNAL_CONTENT_URI, values);
            if (uri == null) {
                Log.e(TAG, "Failed to create new MediaStore record.");
                return;
            }
            try (OutputStream out = getContentResolver().openOutputStream(uri)) {
                if (out == null) {
                    Log.e(TAG, "Failed to get output stream.");
                    return;
                }
                boolean success = bmp.compress(Bitmap.CompressFormat.JPEG, 90, out);
                if (!success) {
                    Log.e(TAG, "Bitmap compress failed.");
                }
            }
            Log.d(TAG, "Image saved to gallery: " + uri.toString());
        } catch (Exception e) {
            Log.e(TAG, "Error saving image", e);
            if (uri != null) {
                getContentResolver().delete(uri, null, null);
            }
        }
    }

    private int logCameraAndPick() {
        CameraManager cm = (CameraManager) getSystemService(CAMERA_SERVICE);
        int selectedFacing = CameraCharacteristics.LENS_FACING_BACK;

        try {
            for (String cameraId : cm.getCameraIdList()) {

                CameraCharacteristics chars = cm.getCameraCharacteristics(cameraId);

                // 1. Print Lens Facing
                Integer facing = chars.get(CameraCharacteristics.LENS_FACING);
                String facingStr;
                if (facing == null) {
                    facingStr = "UNKNOWN";
                } else if (facing == CameraCharacteristics.LENS_FACING_BACK) {
                    facingStr = "BACK";
                } else if (facing == CameraCharacteristics.LENS_FACING_FRONT) {
                    facingStr = "FRONT";
                } else if (facing == CameraCharacteristics.LENS_FACING_EXTERNAL) {
                    facingStr = "EXTERNAL";
                } else {
                    facingStr = "OTHER(" + facing + ")";
                }
                Log.d(TAG, "Camera ID: " + cameraId + " Lens facing: " + facingStr);

                // 2. Print resolution and format
                StreamConfigurationMap map = chars.get(
                        CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
                if (map != null) {
                    for (android.util.Size s : map.getOutputSizes(SurfaceTexture.class)) {
                        Log.d(TAG, "  Preview size: " + s.getWidth() + "x" + s.getHeight());
                    }
                    for (int fmt : map.getOutputFormats()) {
                        Log.d(TAG, "  Supported format: " + fmt);
                    }
                } else {
                    Log.d(TAG, "  No StreamConfigurationMap available");
                }

                // 3. Try to select by priority
                if (facing != null) {
                    if (facing == CameraCharacteristics.LENS_FACING_BACK) {
                        // Selected back camera if found
                        return CameraCharacteristics.LENS_FACING_BACK;
                    } else if (selectedFacing != CameraCharacteristics.LENS_FACING_FRONT
                            && facing == CameraCharacteristics.LENS_FACING_FRONT) {
                        // Selected FRONT if BACK not found
                        selectedFacing = CameraCharacteristics.LENS_FACING_FRONT;
                    } else if (selectedFacing == CameraCharacteristics.LENS_FACING_BACK
                            && facing == CameraCharacteristics.LENS_FACING_EXTERNAL) {
                        // Select EXTERNAL if BACK or FRONT not found
                        selectedFacing = CameraCharacteristics.LENS_FACING_EXTERNAL;
                    }
                }
            }
        } catch (CameraAccessException e) {
            Log.e(TAG, "Failed to list camera resolutions", e);
        }

        // If no BACK found, return FRONT or EXTERNAL
        switch (selectedFacing) {
            case CameraCharacteristics.LENS_FACING_FRONT:
                Log.d(TAG, "Picked FRONT camera (no BACK found)");
                addResultOnUiThread("Picked FRONT camera (no BACK found)");
                break;
            case CameraCharacteristics.LENS_FACING_EXTERNAL:
                Log.d(TAG, "Picked EXTERNAL camera (no BACK/FRONT found)");
                addResultOnUiThread("Picked EXTERNAL camera (no BACK/FRONT found)");
                break;
            default:
                Log.d(TAG, "Falling back to BACK camera (default)");
                addResultOnUiThread("Falling back to BACK camera (default)");
                break;
        }
        return selectedFacing;
    }

    private void startCamera() {
        addResultOnUiThread("Starting camera...");
        int lensFacing =logCameraAndPick();
        CameraSelector selector = new CameraSelector.Builder()
            .requireLensFacing(lensFacing)
            .build();
        ListenableFuture<ProcessCameraProvider> future = ProcessCameraProvider.getInstance(this);
        future.addListener(() -> {
            try {
                ProcessCameraProvider provider = future.get();
                provider.unbindAll();
                Preview preview = new Preview.Builder().build();
                preview.setSurfaceProvider(previewView.getSurfaceProvider());
                ImageAnalysis analysis = new ImageAnalysis.Builder()
                        // .setTargetResolution(new android.util.Size(INPUT_SIZE, INPUT_SIZE))
                        .setBackpressureStrategy(ImageAnalysis.STRATEGY_KEEP_ONLY_LATEST)
                        .setImageQueueDepth(1)
                        .build();
                analysis.setAnalyzer(cameraExecutor, image -> {
                    if (!cameraReady) {
                        cameraReady = true;
                        runOnUiThread(() -> {
                            cameraWidth = image.getWidth();
                            cameraHeight = image.getHeight();
                            buttonLoadDfp.setEnabled(true);
                        });
                    }
                    long now = SystemClock.elapsedRealtime();
                    if (runDetectFlag && now - lastProcessTime > PROCESS_INTERVAL_MS) {
                        lastProcessTime = now;
                        imageQueue.offer(image);
                    } else {
                        image.close();
                    }
                });
                provider.unbindAll();
                provider.bindToLifecycle(this, selector, preview, analysis);
            } catch (ExecutionException | InterruptedException e) {
                Log.e(TAG, "Camera error", e);
            }
        }, ContextCompat.getMainExecutor(this));
    }

    private List<String> findDfpFiles(String root) {
        List<String> list = new ArrayList<>();
        File dir = new File(root);
        if (!dir.isDirectory()) return list;
        for (File f: dir.listFiles(File::isDirectory)) {
            File dfp = new File(f, "model.dfp");
            if (dfp.exists()) list.add(dfp.getAbsolutePath());
        }
        return list;
    }

    private boolean allPermissionsGranted() {
        for (String p: REQUIRED_PERMISSIONS) {
            if (ContextCompat.checkSelfPermission(this, p) !=
                android.content.pm.PackageManager.PERMISSION_GRANTED) return false;
        }
        return true;
    }

    @Override
    public void onRequestPermissionsResult(int code, @NonNull String[] p, @NonNull int[] r) {
        if (code==REQUEST_CODE_PERMISSIONS && allPermissionsGranted()) startCamera();
        else super.onRequestPermissionsResult(code,p,r);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        stopDetect();
    }

    // --- Benchmark Logic ---
    private void startBenchmark() {
        if (runBenchmarkFlag || (benchmarkTaskFuture != null && !benchmarkTaskFuture.isDone())) {
            Log.w(TAG, "Benchmark already running.");
            return;
        }

        // Get parameters from UI
        final String dirPath;
        final byte groupId = 0;
        final int frames;
        final boolean burning = false;
        final int hours = 0;
        nativeLib = new NativeLib();
        try {
            dirPath = editTextDirPath.getText().toString().trim();
            frames = Integer.parseInt(editTextFrames.getText().toString());
            if (dirPath.isEmpty() || frames <= 0 || hours < 0) {
                 Toast.makeText(this, "Invalid parameters", Toast.LENGTH_SHORT).show();
                 return;
            }
        } catch (Exception e) {
            Log.e(TAG, "Invalid input parameters", e);
            Toast.makeText(this, "Invalid parameters", Toast.LENGTH_SHORT).show();
            return;
        }

        // Prepare UI
        results.clear();
        adapter.notifyDataSetChanged();
        addResultOnUiThread("Benchmark starting (Group ID: " + groupId + ")...");
        buttonStartBenchmark.setEnabled(false);
        buttonStopBenchmark.setEnabled(true);
        runBenchmarkFlag = true;

        // Run in background
        benchmarkTaskFuture = executorService.submit(() -> {
            long benchmarkStartTime = System.currentTimeMillis();
            int round = 0;
            int totalFps = 0;
            int modelsProcessed = 0;
            boolean errorOccurred = false;

            try {
                // 1. Find DFP files
                List<String> dfpFiles = findDfpFiles(dirPath);
                if (dfpFiles.isEmpty()) {
                    addResultOnUiThread("Error: No DFP files found in subdirs of " + dirPath);
                    errorOccurred = true;
                    return; // Exit background task
                }
                addResultOnUiThread("Found " + dfpFiles.size() + " DFP files.");

                // 2. Benchmark Loop
                MAIN_LOOP:
                do {
                    round++;
                    addResultOnUiThread("=== Round " + round + " ===");

                    for (String dfpPath : dfpFiles) {
                        if (!runBenchmarkFlag) break MAIN_LOOP;

                        String modelName = new File(dfpPath).getParentFile().getName();
                        addResultOnUiThread("Processing: " + modelName);
                        byte currentModelId = 0;
                        int fps = -1;
                        NativeLib.DfpInfo currentDfpInfo = null; // Use NativeLib class

                        try {
                            // *** Load DFP Info using JNI ***
                            currentDfpInfo = nativeLib.loadDfpInfo(dfpPath); // Call JNI
                            if (currentDfpInfo == null || !currentDfpInfo.valid) {
                                throw new RuntimeException("Failed to load or parse DFP via JNI: " + dfpPath);
                            }
                            // Log some DFP info
                            addResultOnUiThread("DFP Info: Inputs=" + currentDfpInfo.inputPorts.size()
                                                + ", Outputs=" + currentDfpInfo.outputPorts.size()
                                                + ", Chips=" + currentDfpInfo.num_chips);
                            if (!currentDfpInfo.inputPorts.isEmpty()) {
                                Log.d(TAG, "Input Port 0 Info: " + currentDfpInfo.inputPorts.get(0));
                            }
                             if (!currentDfpInfo.outputPorts.isEmpty()) {
                                Log.d(TAG, "Output Port 0 Info: " + currentDfpInfo.outputPorts.get(0));
                            }


                            // 3. Initialize Device via AIDL
                            int status;
                            status = memxService.memxOpen(currentModelId, groupId, MemxHeader.MEMX_DEVICE_CASCADE_PLUS);
                            if (status != MemxHeader.MEMX_STATUS_OK) throw new RuntimeException("memxOpen failed: " + status);

                            // Configure MPU Group based on DFP info
                            byte mpuConfig = mapNumChipsToConfig(currentDfpInfo.num_chips);
                            if (mpuConfig >= 0) {
                                status = memxService.memxConfigMpuGroup(groupId, mpuConfig);
                                if (status != MemxHeader.MEMX_STATUS_OK) throw new RuntimeException("memxConfigMpuGroup failed: " + status);
                                Log.d(TAG, "memxConfigMpuGroup called with config: " + mpuConfig);
                            } else {
                                Log.w(TAG, "Unsupported chip count for MPU config: " + currentDfpInfo.num_chips);
                            }

                            // Download model (using path)
                            byte modelIndex = 0; // Assuming single model for now
                            status = memxService.memxDownloadModel(currentModelId, dfpPath, modelIndex, MemxHeader.MEMX_DOWNLOAD_TYPE_WTMEM_AND_MODEL);
                            if (status != MemxHeader.MEMX_STATUS_OK) throw new RuntimeException("memxDownloadModel failed for " + dfpPath + ": " + status);
                            status = memxService.memxSetStreamEnable(currentModelId, 0);
                            if (status != MemxHeader.MEMX_STATUS_OK) throw new RuntimeException("memxSetStreamEnable failed: " + status);


                            // 4. Prepare Buffers using DFP Info from JNI
                            int numInputs = currentDfpInfo.inputPorts.size();
                            int numOutputs = currentDfpInfo.outputPorts.size();
                            float[][] inputData = new float[numInputs][];
                            byte[][] formattedInputData = new byte[numInputs][];
                            float[][] outputData = new float[numOutputs][];
                            byte[][] formattedOutputData = new byte[numOutputs][];
                            NativeLib.PortInfo[] inputPorts = currentDfpInfo.inputPorts.toArray(new NativeLib.PortInfo[0]);
                            NativeLib.PortInfo[] outputPorts = currentDfpInfo.outputPorts.toArray(new NativeLib.PortInfo[0]);

                            // Allocate input buffers
                            for (int i = 0; i < numInputs; i++) {
                                NativeLib.PortInfo port = inputPorts[i];
                                if (!port.active) continue;
                                if (port.total_size <= 0) throw new RuntimeException("Invalid input float count for port " + i);
                                inputData[i] = new float[port.total_size]; // Allocate float buffer

                                // Calculate formatted size using JNI
                                int byteSize = nativeLib.calculateFormattedSize(
                                    port.total_size, port.format, port.dim_h, port.dim_w, port.dim_z, port.dim_c);
                                if (byteSize <= 0) throw new RuntimeException("Failed to calculate input byte size via JNI for port " + i + ", format=" + port.format);
                                formattedInputData[i] = new byte[byteSize]; // Allocate byte buffer

                                // Fill inputData[i] with dummy or real data
                                java.util.Arrays.fill(inputData[i], 0.5f);
                                Log.d(TAG, "Input Port " + i + ": float[" + inputData[i].length + "], byte[" + formattedInputData[i].length + "], format=" + port.format);
                            }

                            // Allocate output buffers
                            for (int i = 0; i < numOutputs; i++) {
                                NativeLib.PortInfo port = outputPorts[i];
                                 if (!port.active) continue;
                                if (port.total_size <= 0) throw new RuntimeException("Invalid output float count for port " + i);
                                outputData[i] = new float[port.total_size]; // Allocate float buffer

                                // Calculate formatted size using JNI
                                int byteSize = nativeLib.calculateFormattedSize(
                                    port.total_size, port.format, port.dim_h, port.dim_w, port.dim_z, port.dim_c);
                                if (byteSize <= 0) throw new RuntimeException("Failed to calculate output byte size via JNI for port " + i + ", format=" + port.format);
                                formattedOutputData[i] = new byte[byteSize]; // Allocate byte buffer for receiving data

                                Log.d(TAG, "Output Port " + i + ": float[" + outputData[i].length + "], byte[" + formattedOutputData[i].length + "], format=" + port.format);
                            }


                            // // 5. Run Inference Loop
                            Log.d(TAG, "Starting inference loop for " + frames + " frames...");
                            long loopStartTime = System.nanoTime();

                            for (int f = 0; f < frames; f++) {
                                if (!runBenchmarkFlag) break;

                                // Convert input float[] to byte[] using JNI
                                for (int i = 0; i < numInputs; i++) {
                                     if (!inputPorts[i].active) continue;
                                    NativeLib.PortInfo port = inputPorts[i];
                                    boolean convOk = nativeLib.convertFloatToFormattedBytes(
                                        inputData[i], formattedInputData[i],
                                        port.dim_h, port.dim_w, port.dim_z, port.dim_c, port.format);
                                    if (!convOk) throw new RuntimeException("JNI Input conversion failed for port " + i);
                                }

                                // Send input
                                for (int i = 0; i < numInputs; i++) {
                                    if (!inputPorts[i].active) continue;

                                    // Allocate a direct ByteBuffer and copy the byte array into it
                                    // This is important for performance and JNI compatibility
                                    byte[] currentInputData = formattedInputData[i];
                                    // ByteBuffer inputBuffer = ByteBuffer.allocateDirect(currentInputData.length);
                                    // inputBuffer.put(currentInputData);
                                    // inputBuffer.flip();
                                    int ifmapStatus = memxService.memxStreamIfmap(currentModelId, (byte)i, currentInputData, 0);
                                    if (ifmapStatus != MemxHeader.MEMX_STATUS_OK) throw new RuntimeException("memxStreamIfmap failed: " + ifmapStatus);
                                }

                                // Receive output
                                for (int i = 0; i < numOutputs; i++) {
                                    if (!outputPorts[i].active) continue;
                                    // Allocate a direct ByteBuffer
                                    byte[] currentOutputData = formattedOutputData[i];
                                    // ByteBuffer outputBuffer = ByteBuffer.allocateDirect(currentOutputData.length);

                                     // Use port index 'i' as flow_id assuming 1-to-1 mapping for now
                                    int ofmapStatus = memxService.memxStreamOfmap(currentModelId, (byte)i, currentOutputData, 0);
                                    if (ofmapStatus != MemxHeader.MEMX_STATUS_OK) throw new RuntimeException("memxStreamOfmap failed: " + ofmapStatus);

                                    // Copy data from the direct ByteBuffer back into the byte[]
                                    // outputBuffer.rewind(); // Reset buffer position to the beginning before reading
                                    // outputBuffer.get(formattedOutputData[i]); // Read data from buffer into the byte array

                                    // Log.d(TAG, "Flow"+i+" formattedOutputData[0..9] = " +
                                    //     Arrays.toString(Arrays.copyOf(formattedOutputData[i], 20)));

                                    // Unconvert output byte[] to float[] using JNI
                                    NativeLib.PortInfo port = outputPorts[i];
                                    boolean rowPad = (port.format == MemxHeader.MEMX_FMAP_FORMAT_GBF80_ROW_PAD); // Check if format requires row padding info
                                    int hpocSize = port.hpoc_en ? (port.hpoc_dim_c - port.dim_c) : 0;
                                    boolean unconvOk = nativeLib.unconvertFormattedBytesToFloat(
                                        formattedOutputData[i], outputData[i],
                                        port.dim_h, port.dim_w, port.dim_z, port.dim_c, port.format,
                                        port.hpoc_en, hpocSize, port.hpoc_dummy_channels,
                                        rowPad
                                    );
                                    if (!unconvOk) throw new RuntimeException("JNI Output unconversion failed for port " + i);

                                    // Process outputData[i] if needed
                                    // Log.v(TAG, "Output data sample for port " + i + ": " + outputData[i][0]);
                                }
                            } // End frame loop

                            long loopEndTime = System.nanoTime();

                            // Calculate and display FPS if run completed
                            if (runBenchmarkFlag) {
                                fps = calculateFps(loopStartTime, loopEndTime, frames);
                                if (fps > 0) {
                                    totalFps += fps;
                                    modelsProcessed++;
                                    addResultOnUiThread(modelName + ", PASS, FPS: " + fps);
                                } else {
                                     addResultOnUiThread(modelName + ", PASS, Time too short");
                                }
                                Log.i(TAG, "Inference done for " + dfpPath + ". FPS: " + fps);
                            } else {
                                addResultOnUiThread(modelName + ", STOPPED");
                                Log.i(TAG, "Inference stopped for " + dfpPath);
                            }

                        } catch (Exception e) {
                            Log.e(TAG, "Exception during benchmark for " + modelName, e);
                            addResultOnUiThread(modelName + ", FAIL (" + e.getClass().getSimpleName() + ")");
                            errorOccurred = true;
                        } finally {
                            // 6. Close Device
                            try {
                                if (nativeLib != null) {
                                    memxService.memxClose(currentModelId);
                                    Log.d(TAG, "memxClose called for model " + currentModelId);
                                }
                            } catch (Exception e) {
                                Log.e(TAG, "Exception during memxClose", e);
                            }
                        }

                        if (!runBenchmarkFlag || errorOccurred) break; // Exit model loop if stopped or error

                    } // End DFP file loop

                    if (!runBenchmarkFlag || errorOccurred) break; // Exit round loop if stopped or error

                    // Check time limit
                    if (hours > 0) {
                         long currentTime = System.currentTimeMillis();
                         long elapsedTimeHours = (currentTime - benchmarkStartTime) / (1000 * 60 * 60);
                         if (elapsedTimeHours >= hours) {
                             Log.i(TAG,"Benchmark time limit reached.");
                             addResultOnUiThread("Time limit reached (" + hours + " hours).");
                             runBenchmarkFlag = false; // Stop the loop
                         }
                    }

                } while (burning && runBenchmarkFlag); // End burning loop

                // --- Benchmark finished ---
                 if (modelsProcessed > 0 && !errorOccurred) {
                    addResultOnUiThread("=== Benchmark Finished ===");
                    addResultOnUiThread("Average FPS: " + (totalFps / modelsProcessed));
                } else if (runBenchmarkFlag && !errorOccurred) {
                     addResultOnUiThread("=== Benchmark Finished (No models fully processed) ===");
                } else if (!runBenchmarkFlag) {
                     addResultOnUiThread("=== Benchmark Stopped ===");
                } else { // errorOccurred
                     addResultOnUiThread("=== Benchmark Finished with Errors ===");
                }

            } catch (Exception e) { // Catch errors during setup (e.g., file finding, initial AIDL calls)
                Log.e(TAG, "Exception during benchmark setup or main loop", e);
                addResultOnUiThread("Error: " + e.getMessage());
            } finally {
                // --- Update UI after finishing or error ---
                stopBenchmarkInternal(); // Ensure UI is updated correctly on finish/error
            }
        }); // End executorService.submit

    } // End startBenchmark()

    // --- Helper method for FPS calculation ---
    private int calculateFps(long startTimeNs, long endTimeNs, int frameCount) {
         long durationNs = endTimeNs - startTimeNs;
         if (durationNs <= 0 || frameCount <= 0) return -1;
         double durationMs = durationNs / 1_000_000.0;
         return (int) ((double) frameCount * 1000.0 / durationMs);
    }

    // --- Helper method to map chip count to config constant ---
    // *** TODO: Define the actual MEMX_MPU_GROUP_CONFIG_* constants in Java ***
    private byte mapNumChipsToConfig(int numChips) {
        // Map num_chips from DFP to the corresponding MEMX_MPU_GROUP_CONFIG_* constant
        switch (numChips) {
            // These values are examples, replace with actual constants from memx.h
            case 1: return MemxHeader.MEMX_MPU_GROUP_CONFIG_ONE_GROUP_ONE_MPU;
            case 2: return MemxHeader.MEMX_MPU_GROUP_CONFIG_ONE_GROUP_TWO_MPUS;
            case 3: return MemxHeader.MEMX_MPU_GROUP_CONFIG_ONE_GROUP_THREE_MPUS;
            case 4: return MemxHeader.MEMX_MPU_GROUP_CONFIG_ONE_GROUP_FOUR_MPUS;
            case 8: return MemxHeader.MEMX_MPU_GROUP_CONFIG_ONE_GROUP_EIGHT_MPUS;
            case 12: return MemxHeader.MEMX_MPU_GROUP_CONFIG_ONE_GROUP_TWELVE_MPUS;
            case 16: return MemxHeader.MEMX_MPU_GROUP_CONFIG_ONE_GROUP_SIXTEEN_MPUS;
            default:
                Log.e(TAG, "Unsupported chip count for MPU config: " + numChips);
                return -1; // Indicate invalid config
        }
    }

    // Method to request benchmark stop
    private void stopBenchmark() {
         if (runBenchmarkFlag) {
             Log.i(TAG, "Stop benchmark requested.");
             runBenchmarkFlag = false;
             if (benchmarkTaskFuture != null) {
                 // benchmarkTaskFuture.cancel(true); // Optional: Interrupt if needed
             }
             runOnUiThread(() -> {
                 buttonStartBenchmark.setEnabled(true);
                 buttonStopBenchmark.setEnabled(false);
                 addResultOnUiThread("Stopping benchmark...");
            });
        }
    }

    // Helper method to update UI when benchmark stops
    private void stopBenchmarkInternal() {
         runBenchmarkFlag = false;
         runOnUiThread(() -> {
             buttonStartBenchmark.setEnabled(true);
             buttonStopBenchmark.setEnabled(false);
         });
    }


    // Helper method to post results to the UI thread safely
    private void addResultOnUiThread(String result) {
        Log.d(TAG, "UI Result: " + result);
        mainThreadHandler.post(() -> {
            results.add(result);
            adapter.notifyDataSetChanged();
            listViewResults.smoothScrollToPosition(adapter.getCount() - 1);
        });
    }
}
