#include "FindEyeCenter.h"

using namespace std;
using namespace cv;

// 计算x方向梯度
Mat computeMatXGradient(const Mat &mat)
{
    Mat out(mat.rows, mat.cols, CV_64F);

    for (int y = 0; y < mat.rows; ++y)
    {
        const uchar *Mr = mat.ptr<uchar>(y);
        double *Or = out.ptr<double>(y);

        Or[0] = Mr[1] - Mr[0];
        for (int x = 1; x < mat.cols - 1; ++x)
        {
            Or[x] = (Mr[x + 1] - Mr[x - 1]) / 2.0;
        }
        Or[mat.cols - 1] = Mr[mat.cols - 1] - Mr[mat.cols - 2];
    }
    return out;
}

// 计算梯度幅值
Mat matrixMagnitude(const Mat &matX, const Mat &matY)
{
    Mat mags(matX.rows, matX.cols, CV_64F);
    for (int y = 0; y < matX.rows; ++y)
    {
        const double *Xr = matX.ptr<double>(y), *Yr = matY.ptr<double>(y);
        double *Mr = mags.ptr<double>(y);
        for (int x = 0; x < matX.cols; ++x)
        {
            double gX = Xr[x], gY = Yr[x];
            double magnitude = sqrt((gX * gX) + (gY * gY));
            Mr[x] = magnitude;
        }
    }
    return mags;
}

// 动态阈值
double computeDynamicThreshold(const Mat &mat, double stdDevFactor)
{
    Scalar stdMagnGrad, meanMagnGrad;
    meanStdDev(mat, meanMagnGrad, stdMagnGrad);
    double stdDev = stdMagnGrad[0] / sqrt(mat.rows * mat.cols);
    return stdDevFactor * stdDev + meanMagnGrad[0];
}

// 对每一个可能的中心点求点积
void testPossibleCentersFormula(int x, int y, const Mat &weight, double gx, double gy, Mat &out)
{
    // for all possible centers
    for (int cy = 0; cy < out.rows; ++cy)
    {
        double *Or = out.ptr<double>(cy);
        const unsigned char *Wr = weight.ptr<unsigned char>(cy);

        for (int cx = 0; cx < out.cols; ++cx)
        {
            if (x == cx && y == cy)
            {
                continue;
            }
            // create a vector from the possible center to the gradient origin
            double dx = x - cx;
            double dy = y - cy;
            // normalize d
            double magnitude = sqrt((dx * dx) + (dy * dy));
            dx = dx / magnitude;
            dy = dy / magnitude;
            double dotProduct = dx * gx + dy * gy;
            dotProduct = max(0.0, dotProduct);
            // square and multiply by the weight
            if (kEnableWeight)
            {
                Or[cx] += dotProduct * dotProduct * (Wr[cx] / kWeightDivisor);
            }
            else
            {
                Or[cx] += dotProduct * dotProduct;
            }
        }
    }
}

bool inMat(Point p, int rows, int cols)
{
    return p.x >= 0 && p.x < cols && p.y >= 0 && p.y < rows;
}

bool floodShouldPushPoint(const Point &np, const Mat &mat)
{
    return inMat(np, mat.rows, mat.cols);
}

// Kill edges, returns a mask
Mat floodKillEdges(Mat &mat)
{
    rectangle(mat, Rect(0, 0, mat.cols, mat.rows), 255);

    Mat mask(mat.rows, mat.cols, CV_8U, 255);
    queue<Point> toDo;
    toDo.push(Point(0, 0));
    while (!toDo.empty())
    {
        Point p = toDo.front();
        toDo.pop();
        if (mat.at<float>(p) == 0.0f)
        {
            continue;
        }
        // add in every direction
        Point np(p.x + 1, p.y); // right
        if (floodShouldPushPoint(np, mat))
            toDo.push(np);
        np.x = p.x - 1;
        np.y = p.y; // left
        if (floodShouldPushPoint(np, mat))
            toDo.push(np);
        np.x = p.x;
        np.y = p.y + 1; // down
        if (floodShouldPushPoint(np, mat))
            toDo.push(np);
        np.x = p.x;
        np.y = p.y - 1; // up
        if (floodShouldPushPoint(np, mat))
            toDo.push(np);
        // kill it
        mat.at<float>(p) = 0.0f;
        mask.at<uchar>(p) = 0;
    }
    return mask;
}

Point findEyeCenter(Mat eye, int radii)
{
    //-- Find the gradient
    Mat gradientX = computeMatXGradient(eye);
    Mat gradientY = computeMatXGradient(eye.t()).t();

    //-- Normalize and threshold the gradient
    // compute all the magnitudes
    Mat mags = matrixMagnitude(gradientX, gradientY);

    //compute the threshold
    double gradientThresh = computeDynamicThreshold(mags, kGradientThreshold);
    //normalize
    for (int y = 0; y < eye.rows; ++y)
    {
        double *Xr = gradientX.ptr<double>(y), *Yr = gradientY.ptr<double>(y);
        double *Mr = mags.ptr<double>(y);
        for (int x = 0; x < eye.cols; ++x)
        {
            double gX = Xr[x], gY = Yr[x];
            double magnitude = Mr[x];
            if (magnitude > gradientThresh)
            {
                // 快速径向对称变换正好也需要normalize
                Xr[x] = gX / magnitude;
                Yr[x] = gY / magnitude;
            }
            else
            {
                Xr[x] = 0.0;
                Yr[x] = 0.0;
            }
        }
    }
    //-- Create a blurred and inverted image for weighting
    Mat weight;
    GaussianBlur(eye, weight, Size(kWeightBlurSize, kWeightBlurSize), 0, 0);
    for (int y = 0; y < weight.rows; ++y)
    {
        unsigned char *row = weight.ptr<unsigned char>(y);
        for (int x = 0; x < weight.cols; ++x)
        {
            row[x] = (255 - row[x]);
        }
    }
    // 快速径向对称变换 幅度映射图
    Mat M_n = Mat::zeros(eye.rows, eye.cols, CV_64F);

    //-- Run the algorithm
    Mat outSum = Mat::zeros(eye.rows, eye.cols, CV_64F);

    // for each possible gradient location
    // Note: these loops are reversed from the way the paper does them
    // it evaluates every possible center for each gradient location instead of
    // every possible gradient location for every center.

    for (int y = 0; y < weight.rows; ++y)
    {
        const double *Xr = gradientX.ptr<double>(y), *Yr = gradientY.ptr<double>(y);
        const double *Mr = mags.ptr<double>(y);
        for (int x = 0; x < weight.cols; ++x)
        {
            double gX = Xr[x], gY = Yr[x];
            if (gX == 0.0 && gY == 0.0)
            {
                continue;
            }
            testPossibleCentersFormula(x, y, weight, gX, gY, outSum);

            // 快速径向变换的部分,定义一个p点和2d g,其中放x和y在点p的梯度
            // 2019/3/25 暂未完成,可能需要取反后再次计算
            double magnitude = Mr[x];
            Point p(y, x);
            Vec2d g = Vec2d(Xr[x], Yr[x]);
            Vec2i gp;
            gp.val[0] = (int)round((g.val[0]) * radii);
            gp.val[1] = (int)round((g.val[1]) * radii);
            Point ppve(p.x + gp.val[0], p.y + gp.val[1]);

            if (ppve.x > weight.rows - 1 || ppve.y > weight.cols - 1 || ppve.x < 0 || ppve.y < 0)
            {
                continue;
            }
            else
            {
                M_n.at<double>(ppve.x, ppve.y) += magnitude;
            }
        }
    }
    // scale all the values down, basically averaging them
    double numGradients = (weight.rows * weight.cols);
    Mat out;
    cout << "Numbers: " << numGradients << endl;
    cout << "rows and cols: " << outSum.rows << outSum.cols << endl;
    outSum.convertTo(out, CV_32F, 1.0 / numGradients);
    //-- Find the maximum point
    Point maxP;
    double maxVal;
    minMaxLoc(out, NULL, &maxVal, NULL, &maxP);
    // 找快速径向对称变换的maxVal
    Point maxPFrst;
    double maxValFrst;
    minMaxLoc(M_n, NULL, &maxValFrst, NULL, &maxPFrst);
    //-- Flood fill the edges
    if (kEnablePostProcess)
    {
        Mat floodClone;
        //double floodThresh = computeDynamicThreshold(out, 1.5);
        double floodThresh = maxVal * kPostProcessThreshold;
        threshold(out, floodClone, floodThresh, 0.0f, THRESH_TOZERO);
        Mat mask = floodKillEdges(floodClone);
        // redo max
        minMaxLoc(out, NULL, &maxVal, NULL, &maxP, mask);
        /*
        cout << "Dot Value:" << maxVal << endl;
        cout << "Dot Location:" << maxP << endl;
        cout << "Frst Value:" << maxValFrst << endl;
        cout << "Frst Location:" << maxPFrst << endl;
        */
    }
    return Point(maxP);
}
