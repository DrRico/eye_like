#ifndef FINDEYECENTER_H
#define FINDEYECENTER_H

#include <opencv2/opencv.hpp>
#include <iostream>
#include <stdio.h>
#include <queue>

using namespace std;
using namespace cv;

// Prepeocessing
const float kSmoothFaceFactor = 0.005;

// Postprocessing
const bool kEnablePostProcess = true;
const float kPostProcessThreshold = 0.97;

// Algorithm Parameters
const int kWeightBlurSize = 5;
const bool kEnableWeight = true;
const float kWeightDivisor = 1.0;
const double kGradientThreshold = 50.0;

Point findEyeCenter(Mat eye, int radii);

#endif
