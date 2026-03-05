package com.memryx.memxdriver;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.RemoteException;
import android.util.Log;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ListView;
import android.widget.Spinner;
import android.widget.Toast;
import android.widget.ToggleButton;

import androidx.appcompat.app.AppCompatActivity;

import java.io.File;
import java.io.FilenameFilter;
import java.lang.Thread;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;

// Import your AIDL interface class - *** Ensure package name is correct ***
import com.memryx.memxdriver.IMemxDriverService;
import com.memryx.memxdriver.MemxHeader;
import com.memryx.nativelib.NativeLib; // Using the provided package name


public class MainActivity extends AppCompatActivity implements View.OnClickListener {
    private static final String TAG = "MainActivity";

    // --- UI Elements ---
    private EditText editTextDirPath;
    private Spinner spinnerGroup;
    private EditText editTextFrames;
    private ToggleButton toggleButtonBurning;
    private EditText editTextHours;
    private Button buttonStartBenchmark;
    private Button buttonStopBenchmark;
    private ListView listViewResults;

    // --- Data & Adapter ---
    private ArrayList<String> results;
    private ArrayAdapter<String> adapter;

    // --- Service Connection ---
    private IMemxDriverService memxService;
    private boolean isBound = false;
    private volatile boolean runBenchmarkFlag = false; // Controls the benchmark loop

    // --- Threading ---
    private ExecutorService executorService = Executors.newSingleThreadExecutor();
    private Future<?> benchmarkTaskFuture = null; // To manage the background task
    private Handler mainThreadHandler = new Handler(Looper.getMainLooper()); // To post results to UI thread

    // --- Constants (Reference from C++, verify values) ---
    private static final int DEFAULT_MODEL = 0;

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
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main); // Ensure layout name is correct

        // --- Initialize UI Elements ---
        editTextDirPath = findViewById(R.id.editText_dirPath);
        spinnerGroup = findViewById(R.id.spinner_group);
        editTextFrames = findViewById(R.id.editText_frames);
        toggleButtonBurning = findViewById(R.id.toggleButton_burning);
        editTextHours = findViewById(R.id.editText_hours);
        buttonStartBenchmark = findViewById(R.id.button_start_benchmark);
        buttonStopBenchmark = findViewById(R.id.button_stop_benchmark);
        listViewResults = findViewById(R.id.listView_results);

        // Setup Spinner
        List<String> groupOptions = Arrays.asList("0", "1", "2", "3");
        ArrayAdapter<String> groupAdapter = new ArrayAdapter<>(this, android.R.layout.simple_spinner_item, groupOptions);
        groupAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        spinnerGroup.setAdapter(groupAdapter);
        spinnerGroup.setSelection(0); // Default to group 0

        // Setup Adapter
        results = new ArrayList<>();
        adapter = new ArrayAdapter<>(this, android.R.layout.simple_list_item_1, results);
        listViewResults.setAdapter(adapter);

        // Set Listeners
        buttonStartBenchmark.setOnClickListener(this);
        buttonStopBenchmark.setOnClickListener(this);

        // Initial button states
        buttonStartBenchmark.setEnabled(false);
        buttonStopBenchmark.setEnabled(false);

        // Set default UI values
        editTextFrames.setText("100");
        editTextHours.setText("0");

        // Set default internal storage path
        setupDefaultPath();

        // Trigger JNI library loading (optional, static block in NativeLib already does it)
        try { Class.forName(NativeLib.class.getName()); } catch (ClassNotFoundException e) { Log.e(TAG, "Failed to explicitly load NativeLib class", e); }
    }

    private void setupDefaultPath() {
        // File internalFilesBaseDir = getFilesDir();
        // File internalModelsDir = new File(internalFilesBaseDir, "dfp"); // Using "dfp" subdir
        File internalModelsDir = new File("/data/dfp");
        if (!internalModelsDir.exists()) {
            boolean success = internalModelsDir.mkdirs();
            if (!success) {
                Log.e(TAG, "Failed to create internal models directory: " + internalModelsDir.getAbsolutePath());
                Toast.makeText(this, "Failed to create internal models directory", Toast.LENGTH_LONG).show();
            } else {
                 Log.d(TAG, "Internal models directory created: " + internalModelsDir.getAbsolutePath());
            }
        } else {
            Log.d(TAG, "Internal models directory already exists: " + internalModelsDir.getAbsolutePath());
        }
        if (internalModelsDir.exists()) {
             editTextDirPath.setText(internalModelsDir.getAbsolutePath());
        } else {
             editTextDirPath.setHint("Failed to access internal models directory");
        }
    }


    @Override
    protected void onStart() {
        super.onStart();
        bindMemxService(); // Bind service on start

        // For testing: run benchmark on start
        // Handler startHandler = new Handler();
        // Runnable r = new Runnable() {
        //     public void run() {
        //         startBenchmark(); // Start benchmark if needed
        //     }
        // };
        // startHandler.postDelayed(r, 1000);
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
    public void onClick(View v) {
        int id = v.getId();
        if (id == R.id.button_start_benchmark) {
            startBenchmark();
        } else if (id == R.id.button_stop_benchmark) {
            stopBenchmark();
        }
    }

    // --- Benchmark Logic ---

    private void startBenchmark() {
        if (!isBound || memxService == null) {
            Log.w(TAG, "Service not bound.");
            Toast.makeText(this, "Service not connected", Toast.LENGTH_SHORT).show();
            return;
        }
        if (runBenchmarkFlag || (benchmarkTaskFuture != null && !benchmarkTaskFuture.isDone())) {
            Log.w(TAG, "Benchmark already running.");
            return;
        }

        // Get parameters from UI
        final String dirPath;
        final int groupId;
        final int frames;
        final boolean burning;
        final int hours;
        try {
            dirPath = editTextDirPath.getText().toString().trim();
            groupId = Integer.parseInt(spinnerGroup.getSelectedItem().toString());
            frames = Integer.parseInt(editTextFrames.getText().toString());
            burning = toggleButtonBurning.isChecked();
            hours = Integer.parseInt(editTextHours.getText().toString());
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
                        int currentModelId = DEFAULT_MODEL;
                        int fps = -1;
                        NativeLib.DfpInfo currentDfpInfo = null; // Use NativeLib class

                        try {
                            // *** Load DFP Info using JNI ***
                            currentDfpInfo = NativeLib.nativeLoadDfpInfo(dfpPath); // Call JNI
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
                            int mpuConfig = mapNumChipsToConfig(currentDfpInfo.num_chips);
                            if (mpuConfig >= 0) {
                                status = memxService.memxConfigMpuGroup(groupId, mpuConfig);
                                if (status != MemxHeader.MEMX_STATUS_OK) throw new RuntimeException("memxConfigMpuGroup failed: " + status);
                                Log.d(TAG, "memxConfigMpuGroup called with config: " + mpuConfig);
                            } else {
                                Log.w(TAG, "Unsupported chip count for MPU config: " + currentDfpInfo.num_chips);
                            }

                            // Download model (using path)
                            status = memxService.memxDownloadModel(currentModelId, dfpPath, 0, MemxHeader.MEMX_DOWNLOAD_TYPE_WTMEM_AND_MODEL);
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
                                int byteSize = NativeLib.nativeCalculateFormattedSize(
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
                                int byteSize = NativeLib.nativeCalculateFormattedSize(
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
                                    boolean convOk = NativeLib.nativeConvertFloatToFormattedBytes(
                                        inputData[i], formattedInputData[i],
                                        port.dim_h, port.dim_w, port.dim_z, port.dim_c, port.format);
                                    if (!convOk) throw new RuntimeException("JNI Input conversion failed for port " + i);
                                }

                                // Send input
                                for (int i = 0; i < numInputs; i++) {
                                     if (!inputPorts[i].active) continue;
                                    // Use port index 'i' as flow_id assuming 1-to-1 mapping for now
                                    int ifmapStatus = memxService.memxStreamIfmap(currentModelId, i, formattedInputData[i], 0);
                                    if (ifmapStatus != MemxHeader.MEMX_STATUS_OK) throw new RuntimeException("memxStreamIfmap failed: " + ifmapStatus);
                                }

                                // Receive output
                                for (int i = 0; i < numOutputs; i++) {
                                     if (!outputPorts[i].active) continue;
                                     // Use port index 'i' as flow_id assuming 1-to-1 mapping for now
                                    int ofmapStatus = memxService.memxStreamOfmap(currentModelId, i, formattedOutputData[i], 0);
                                    if (ofmapStatus != MemxHeader.MEMX_STATUS_OK) throw new RuntimeException("memxStreamOfmap failed: " + ofmapStatus);

                                    // Unconvert output byte[] to float[] using JNI
                                    NativeLib.PortInfo port = outputPorts[i];
                                    boolean rowPad = (port.format == MemxHeader.MEMX_FMAP_FORMAT_GBF80_ROW_PAD); // Check if format requires row padding info
                                    int hpocSize = port.hpoc_en ? (port.hpoc_dim_c - port.dim_c) : 0;
                                    boolean unconvOk = NativeLib.nativeUnconvertFormattedBytesToFloat(
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

                        } catch (RemoteException e) {
                            Log.e(TAG, "RemoteException during benchmark for " + modelName, e);
                            addResultOnUiThread(modelName + ", FAIL (RemoteException)");
                            errorOccurred = true;
                        } catch (Exception e) {
                            Log.e(TAG, "Exception during benchmark for " + modelName, e);
                            addResultOnUiThread(modelName + ", FAIL (" + e.getClass().getSimpleName() + ")");
                            errorOccurred = true;
                        } finally {
                            // 6. Close Device
                            try {
                                if (memxService != null && isBound) {
                                    memxService.memxClose(currentModelId);
                                    Log.d(TAG, "memxClose called for model " + currentModelId);
                                }
                            } catch (RemoteException e) {
                                Log.e(TAG, "RemoteException during memxClose", e);
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
    private int mapNumChipsToConfig(int numChips) {
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
                 buttonStartBenchmark.setEnabled(isBound);
                 buttonStopBenchmark.setEnabled(false);
                 addResultOnUiThread("Stopping benchmark...");
             });
         }
    }

    // Helper method to update UI when benchmark stops
    private void stopBenchmarkInternal() {
         runBenchmarkFlag = false;
         runOnUiThread(() -> {
             buttonStartBenchmark.setEnabled(isBound);
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

    // Helper method to find DFP files
    private List<String> findDfpFiles(String rootDirPath) {
        List<String> dfpPaths = new ArrayList<>();
        File rootDir = new File(rootDirPath);
        if (!rootDir.exists() || !rootDir.isDirectory()) {
            Log.e(TAG, "Root directory error: " + rootDirPath);
            return dfpPaths;
        }
        File[] subDirs = rootDir.listFiles(File::isDirectory);
        if (subDirs != null) {
            for (File subDir : subDirs) {
                File dfpFile = new File(subDir, "model.dfp");
                if (dfpFile.exists() && dfpFile.isFile()) {
                    dfpPaths.add(dfpFile.getAbsolutePath());
                } else {
                     Log.w(TAG, "model.dfp not found in: " + subDir.getAbsolutePath());
                }
            }
        } else {
             Log.e(TAG, "Failed to list subdirectories in: " + rootDirPath);
        }
        Log.i(TAG, "Found " + dfpPaths.size() + " DFP paths in subdirs of " + rootDirPath);
        return dfpPaths;
    }

    // --- JNI Method Declarations are now in NativeLib.java ---
    // --- Size calculation and data conversion rely on native methods ---
    // --- Ensure NativeLib.java and the JNI library are correctly set up ---

}
