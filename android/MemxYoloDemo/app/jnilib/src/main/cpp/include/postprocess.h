#ifndef POSTPROCESS_H
#define POSTPROCESS_H

#include <vector>

#ifdef __cplusplus
extern "C" {
#endif

struct Detection {
    float left, top, right, bottom;
    float score;
    int   label;
};

void neonPostProcess(float **bufs, const int *dims, int numFlows,
                        float confThr, float iouThr, int topK,
                        int inputW, int inputH, int preMax,
                        std::vector<Detection> &detections);


#ifdef __cplusplus
}
#endif

#endif //POSTPROCESS_H
