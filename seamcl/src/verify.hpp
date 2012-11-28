#ifndef VERIFY_HPP
#define VERIFY_HPP

// C
#include <cmath>

// STL
#include <iostream>
#include <algorithm>

// OpencL
#include <CL/cl.hpp>

#define rM(M,X,Y) (M)[((Y)*pitch+(X))]
#define min3(A,B,C) (std::min((A),std::min((B),(C))))

// Checks to ensure that kernels produce correct output
namespace verify {

    inline void printMatrix(float *matrix, int height, int width, int pitch) {

        for (int j = 0; j < height; ++j) {
            for (int i = 0; i < width; ++i) {
                std::cout << rM(matrix, i, j) << "\t";
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;
    }



    bool computeSeams(float* deviceResult,
                      float* originalEnergyMatrix,
                      int width,
                      int height,
                      int pitch) {


        std::cerr << "in verify::computeSeams" << std::endl;
        float *hostResult = new float[pitch * height];
        memcpy(hostResult, originalEnergyMatrix, pitch * height * sizeof(float));

        for (int y = 0; y < height; ++y) {
            int x = 0;
            rM(hostResult, x, y) = FLT_MAX;
            while (++x < (width-1)) {
                const float pathCost = (y < 1) ? 0.0f :
                    min3(rM(hostResult, x, y-1), rM(hostResult, x-1, y-1), rM(hostResult, x+1, y-1));
                rM(hostResult, x, y) = rM(originalEnergyMatrix, x, y) + pathCost;
            }
            rM(hostResult, x, y) = FLT_MAX;
        }

        std::cerr << "height\t" << height << std::endl;
        std::cerr << "width\t" << width << std::endl;


        // print original matrix
        //std::cout << "ENERGYMATRIX: " << std::endl;
        //printMatrix(originalEnergyMatrix, height, width, pitch);

        //std::cout << "DEVICERESULT: " << std::endl;
        //printMatrix(deviceResult, height, width, pitch);

        //std::cout << "HOSTRESULT: " << std::endl;
        //printMatrix(hostResult, height, width, pitch);

        bool correct = true;

        float epsilon = 0.00001f;
        for (int x = 0; x < width; ++x) {
            for (int y = 0; y < height; ++y) {
                if (fabsf(rM(hostResult, x, y) - rM(deviceResult, x, y)) > epsilon) {
                    //std::cerr << "Mismatch at (" << x << ", " << y << ") " << std::endl;
                    //std::cerr << "Expected:\t" << rM(hostResult, x, y) << std::endl;
                    //std::cerr << "Actual:\t" << rM(deviceResult, x, y) << std::endl;
                    correct = false;
                }
            }
        }
        delete [] hostResult;
        return correct;
    }

}

#endif
