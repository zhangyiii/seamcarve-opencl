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
                      int inset,
                      int width,
                      int height,
                      int pitch) {


        std::cerr << "in verify::computeSeams" << std::endl;
        float *hostResult = new float[width * height];
        memcpy(hostResult, originalEnergyMatrix, width * height * sizeof(float));

        for (int y = inset; y < height; ++y) {
            for (int x = inset; x < (width - inset); ++x) {
                rM(hostResult, x, y) = rM(originalEnergyMatrix, x, y) + std::min(rM(hostResult, x, y-1),
                                                                                 std::min(rM(hostResult, x-1, y-1),
                                                                                          rM(hostResult, x+1, y-1)));
            }
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
        for (int x = inset; x < width - inset; ++x) {
            for (int y = inset; y < height; ++y) {
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