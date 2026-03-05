#include "postprocess.h"
#include <arm_neon.h>
#include <vector>
#include <queue>
#include <algorithm>
#include <cmath>

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "NativeLib"
#if defined(__ANDROID__)
#include <android/log.h>
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#else
#define ALOGE(...) fprintf(stderr, "E/%s: ", LOG_TAG); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n")
    #if defined(DEBUG)
    #define ALOGD(...) fprintf(stdout, "D/%s: ", LOG_TAG); fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n")
    #define ALOGW(...) fprintf(stdout, "W/%s: ", LOG_TAG); fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n")
    #else
    #define ALOGD(...) do {} while (0)
    #define ALOGW(...) do {} while (0)
    #endif
#endif

// Helper: sigmoid activation
static inline float sigmoid(float x) {
    return 1.0f / (1.0f + std::exp(-x));
}

//----------------------------------------------------------------------
// Decode one cell's raw coordinate output into a Detection
// using softmax + weighted sum over 16 bins per direction.
// Parameters:
//   coordPtr      - pointer to this cell's 4×16 coordinate logits
//   layerW,layerH - width/height of the feature map
//   row,col       - this cell's indices
//   score,label   - the class score & label already computed
//   inputW,inputH - original network input dimensions (e.g. 640×640)
static Detection decodeCell(const float* coordPtr,
                            int layerW, int layerH,
                            int row, int col,
                            float score, int label,
                            int inputW, int inputH) {
    float fv[4];
    // For each of the 4 directions
    for (int d = 0; d < 4; ++d) {
        const float* ptr = coordPtr + d * 16;
        // 1) find max logit for numeric stability
        float m = ptr[0];
        for (int j = 1; j < 16; ++j) {
            if (ptr[j] > m) m = ptr[j];
        }
        // 2) compute weighted sum exp
        float sumExp = 0.f;
        float weighted = 0.f;
        for (int j = 0; j < 16; ++j) {
            float e = std::exp(ptr[j] - m);
            sumExp += e;
            weighted += j * e;
        }
        fv[d] = weighted / sumExp;
    }
    // 3) convert to box coordinates
    float ratioX = float(inputW) / float(layerW);
    float ratioY = float(inputH) / float(layerH);
    float cx = (fv[2] - fv[0] + 2*(0.5f + col)) * 0.5f * ratioX;
    float cy = (fv[3] - fv[1] + 2*(0.5f + row)) * 0.5f * ratioY;
    float w  = (fv[2] + fv[0]) * ratioX;
    float h  = (fv[3] + fv[1]) * ratioY;
    Detection det;
    det.left   = cx - 0.5f * w;
    det.top    = cy - 0.5f * h;
    det.right  = cx + 0.5f * w;
    det.bottom = cy + 0.5f * h;
    det.score  = score;
    det.label  = label;
    return det;
}

//----------------------------------------------------------------------
// Compute Intersection-over-Union between two boxes
static float computeIoU(const Detection &a, const Detection &b) {
    float left   = std::max(a.left,   b.left);
    float top    = std::max(a.top,    b.top);
    float right  = std::min(a.right,  b.right);
    float bottom = std::min(a.bottom, b.bottom);
    float w = std::max(0.f, right - left);
    float h = std::max(0.f, bottom - top);
    float inter = w * h;
    float areaA = (a.right - a.left) * (a.bottom - a.top);
    float areaB = (b.right - b.left) * (b.bottom - b.top);
    return inter / (areaA + areaB - inter);
}

//----------------------------------------------------------------------
// Non-Maximum Suppression on a small set of candidates
//   candidates - vector of Detection to filter
//   iouThr     - IoU threshold
//   topK       - maximum boxes to keep
//   output     - filtered result
static void doNMS(const std::vector<Detection> &candidates,
                  float iouThr, int topK,
                  std::vector<Detection> &output) {
    // sort by score descending
    std::vector<Detection> dets = candidates;
    std::sort(dets.begin(), dets.end(),
              [](auto &x, auto &y){ return x.score > y.score; });
    std::vector<bool> removed(dets.size(), false);
    for (size_t i = 0; i < dets.size(); ++i) {
        if (removed[i]) continue;
        output.push_back(dets[i]);
        if ((int)output.size() >= topK) break;
        for (size_t j = i+1; j < dets.size(); ++j) {
            if (!removed[j] &&
                computeIoU(dets[i], dets[j]) > iouThr) {
                removed[j] = true;
            }
        }
    }
}

//----------------------------------------------------------------------
// The main NEON-optimized post-processing function.
//   bufs     - array of pointers to each flow's float data
//   dims     - pointer to an int array of length numFlows*3: [h,w,c,...]
//   numFlows - total number of flows (2×#featureLevels)
//   confThr  - score threshold
//   iouThr   - NMS IoU threshold
//   topK     - number of boxes to keep
//   inputW,inputH - original input dimensions (e.g. 640×480)
//   preMax  - maximum number of boxes to consider before NMS
//   detections - output vector
void neonPostProcess(float **bufs, const int *dims, int numFlows,
                        float confThr, float iouThr, int topK,
                        int inputW, int inputH, int preMax,
                        std::vector<Detection> &detections) {
    // half of flows are coordinate maps, half confidence maps
    int flows2 = numFlows / 2;

    // Min‐heap to keep topK by score
    auto cmp = [](const Detection &a, const Detection &b){
        return a.score > b.score;  // min‐heap
    };
    std::priority_queue<Detection, std::vector<Detection>, decltype(cmp)> pq(cmp);

    // Iterate through each feature level
    for (int layer = 0; layer < flows2; ++layer) {
        float* coordBuf = bufs[layer*2];
        float* confBuf  = bufs[layer*2+1];
        int h   = dims[(layer*2)*3 + 0];
        int w = dims[(layer*2)*3+1];
        int c = dims[(layer*2)*3+2];
        int cls = dims[(layer*2+1)*3+2];

        int strideCoord = w * c;
        int strideConf  = w * cls;

        // For each cell in the grid
        for (int r = 0; r < h; ++r) {
            const float* rowCoord = coordBuf + r * strideCoord;
            const float* rowConf  = confBuf  + r * strideConf;
            for (int col = 0; col < w; ++col) {
                int baseConf = col * cls;

                // 1. find best class logit via NEON, then argmax scalar ---
                int full = cls / 4;
                float32x4_t maxVec = vdupq_n_f32(-1e9f);
                for (int i = 0; i < full; ++i) {
                    float32x4_t v = vld1q_f32(rowConf + baseConf + i*4);
                    maxVec = vmaxq_f32(maxVec, v);
                }
                float tmp[4];
                vst1q_f32(tmp, maxVec);
                float bestLogit = tmp[0];
                if (tmp[1] > bestLogit) bestLogit = tmp[1];
                if (tmp[2] > bestLogit) bestLogit = tmp[2];
                if (tmp[3] > bestLogit) bestLogit = tmp[3];
                // leftover
                for (int k = full*4; k < cls; ++k) {
                    float v = rowConf[baseConf + k];
                    if (v > bestLogit) bestLogit = v;
                }
                // now find the index of bestLogit
                int bestLabel = 0;
                for (int k = 0; k < cls; ++k) {
                    if (rowConf[baseConf + k] == bestLogit) {
                        bestLabel = k;
                        break;
                    }
                }
                // final class probability
                float clsProb = sigmoid(bestLogit);
                if (clsProb < confThr) continue;  // threshold on class score

                // 2. decode box from coordBuf ---
                const float* coordPtr = rowCoord + col * c;
                Detection det = decodeCell(
                    coordPtr, w, h, r, col,
                    clsProb, bestLabel,
                    inputW, inputH
                );

                // 3. push into min-heap ---
                if ((int)pq.size() < preMax) {
                    pq.push(det);
                } else if (det.score > pq.top().score) {
                    pq.pop();
                    pq.push(det);
                }
            }
        }
    }

    // collect, NMS, trim to topK
    std::vector<Detection> preDets;
    while (!pq.empty()) { preDets.push_back(pq.top()); pq.pop(); }
    std::reverse(preDets.begin(), preDets.end());

    std::vector<Detection> nmsDets;
    doNMS(preDets, iouThr, (int)preDets.size(), nmsDets);
    if ((int)nmsDets.size() > topK) nmsDets.resize(topK);
    detections = std::move(nmsDets);
}
