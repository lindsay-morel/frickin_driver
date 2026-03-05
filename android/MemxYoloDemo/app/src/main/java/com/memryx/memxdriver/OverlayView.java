package com.memryx.memxdriver;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.util.AttributeSet;
import android.view.View;

import java.util.ArrayList;
import java.util.List;

import com.memryx.jnilib.Detection;

/**
 * Custom view to draw detection boxes + labels on top of camera preview.
 * Scales model-space coordinates (inputW × inputH) to full preview size.
 */
public class OverlayView extends View {
    private final List<Detection> detections = new ArrayList<>();
    private final Paint boxPaint = new Paint();
    private final Paint textPaint = new Paint();

    // Model input dimensions
    private int inputW = 1;
    private int inputH = 1;

    // COCO class names
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

    public OverlayView(Context context, AttributeSet attrs) {
        super(context, attrs);
        boxPaint.setStyle(Paint.Style.STROKE);
        boxPaint.setStrokeWidth(4);
        boxPaint.setColor(Color.RED);

        textPaint.setTextSize(36);
        textPaint.setColor(Color.YELLOW);
        textPaint.setStyle(Paint.Style.FILL);
    }

    /**
     * Set the model's input size for coordinate scaling.
     */
    public void setInputSize(int width, int height) {
        this.inputW = width;
        this.inputH = height;
    }

    /**
     * Supply new detections and trigger redraw.
     */
    public void setDetections(List<Detection> dets) {
        synchronized (detections) {
            detections.clear();
            detections.addAll(dets);
        }
        postInvalidate();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        if (inputW <= 0 || inputH <= 0) {
            return;
        }

        float viewW = getWidth();
        float viewH = getHeight();
        // Scale independently in X/Y to fill the preview
        float scaleX = viewW / inputW;
        float scaleY = viewH / inputH;

        synchronized (detections) {
            for (Detection det : detections) {
                RectF r = det.box;
                // Map to view coordinates
                float left   = r.left   * scaleX;
                float top    = r.top    * scaleY;
                float right  = r.right  * scaleX;
                float bottom = r.bottom * scaleY;

                // Draw bounding box
                canvas.drawRect(left, top, right, bottom, boxPaint);

                // Draw label
                String label = String.format("%s %.2f", COCO_NAMES[det.label], det.score);
                float textX = left;
                float textY = top > 36 ? top - 8 : top + 36;
                canvas.drawText(label, textX, textY, textPaint);
            }
        }
    }
}
