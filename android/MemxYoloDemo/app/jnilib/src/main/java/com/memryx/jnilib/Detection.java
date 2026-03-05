package com.memryx.jnilib;

import android.graphics.RectF;

/**
 * A single detected box with score and label.
 */
public class Detection {
    public final RectF box;
    public final float score;
    public final int label;

    public Detection(RectF box, float score, int label) {
        this.box = box;
        this.score = score;
        this.label = label;
    }
}