#ifndef KERNEL_HPP
#define KERNEL_HPP

// OpenCL
#include <CL/cl.hpp>

// SeamCL
#include "image.hpp"
#include "setup.hpp"
#include "math.hpp"
#include "mem.hpp"
#include "verify.hpp"

// Wrapper functions around calling kernels
namespace kernel {

    /**
     * Applies a gaussian blur filter to an image using openCL.
     * @param ctx An openCL context object.
     * @param cmdQueue An openCL command queue.
     * @param sampler An openCL image sampler object.
     * @param height The height of the input image.
     * @param width The width of the input image.
     * @return An Image2D object containing the resulting image data.
     */
    void blur(cl::Context &ctx,
              cl::CommandQueue &cmdQueue,
              cl::Image2D &inputImage,
              cl::Image2D &outputImage,
              cl::Sampler &sampler,
              int height,
              int width) {

        // Create kernel
        cl::Kernel kernel = setup::kernel(ctx, std::string("GaussianKernel.cl"), std::string("gaussian_filter"));

        // Set kernel arguments
        cl_int errNum;

        errNum = kernel.setArg(0, inputImage);
        errNum |= kernel.setArg(1, outputImage);
        errNum |= kernel.setArg(2, sampler);
        errNum |= kernel.setArg(3, width);
        errNum |= kernel.setArg(4, height);

        if (errNum != CL_SUCCESS) {
            std::cerr << "Error setting kernel arguments." << std::endl;
            exit(-1);
        }

        // Determine local and global work size
        cl::NDRange offset = cl::NDRange(0, 0);
        cl::NDRange localWorkSize = cl::NDRange(16, 16);
        cl::NDRange globalWorkSize = cl::NDRange(math::roundUp(localWorkSize[0], width),
                                                 math::roundUp(localWorkSize[1], height));
        // Run kernel
        errNum = cmdQueue.enqueueNDRangeKernel(kernel,
                                               offset,
                                               globalWorkSize,
                                               localWorkSize);
        if (errNum != CL_SUCCESS) {
            std::cerr << "Error enqueuing blur kernel for execution." << std::endl;
            exit(-1);
        }

    }

    /**
     * Computes the gradient of an image using openCL.
     * @param ctx An openCL context object.
     * @param cmdQueue An openCL command queue.
     * @param inputImage The image to blur
     * @param energyMatrix An openCL buffer to store the output in.
     * @param sampler An openCL image sampler object.
     * @param height The height of the input image.
     * @param width The width of the input image.
     * @return A buffer containing the gradient interpreted as a matrix of size height * width.
     */
    void gradient(cl::Context &ctx,
                  cl::CommandQueue &cmdQueue,
                  cl::Image2D &inputImage,
                  cl::Buffer &energyMatrix,
                  cl::Sampler &sampler,
                  int height,
                  int width) {


        // Setup kernel
        cl::Kernel kernel = setup::kernel(ctx, std::string("GradientKernel.cl"), std::string("image_gradient"));

        cl_int errNum;

        // Set kernel arguments
        errNum = kernel.setArg(0, inputImage);
        errNum |= kernel.setArg(1, energyMatrix);
        errNum |= kernel.setArg(2, sampler);
        errNum |= kernel.setArg(3, width);
        errNum |= kernel.setArg(4, height);

        if (errNum != CL_SUCCESS) {
            std::cerr << "Error setting gradient kernel arguments." << std::endl;
            exit(-1);
        }

        cl::NDRange offset = cl::NDRange(0, 0);
        cl::NDRange localWorkSize = cl::NDRange(16, 16);
        cl::NDRange globalWorkSize = cl::NDRange(math::roundUp(localWorkSize[0], width),
                                                 math::roundUp(localWorkSize[1], height));

        errNum = cmdQueue.enqueueNDRangeKernel(kernel,
                                               offset,
                                               globalWorkSize,
                                               localWorkSize);

        if (errNum != CL_SUCCESS) {
            std::cerr << "Error enqueuing gradient kernel for execution." << std::endl;
            exit(-1);
        }

        // TODO(amidvidy): make this debugging code
        // Read data into an image object
        cl::Image2D gradientImage = cl::Image2D(ctx,
                                                (cl_mem_flags) CL_MEM_READ_WRITE,
                                                cl::ImageFormat(CL_LUMINANCE, CL_FLOAT),
                                                width,
                                                height,
                                                0,
                                                NULL,
                                                &errNum);
        if (errNum != CL_SUCCESS) {
            std::cerr << "Error creating gradient output image" << std::endl;
            exit(-1);
        }

        cl::size_t<3> origin;
        origin.push_back(0);
        origin.push_back(0);
        origin.push_back(0);
        cl::size_t<3> region;
        region.push_back(width);
        region.push_back(height);
        region.push_back(1);

        errNum = cmdQueue.enqueueCopyBufferToImage(energyMatrix,
                                                   gradientImage,
                                                   0,
                                                   origin,
                                                   region,
                                                   NULL,
                                                   NULL);

        if (errNum != CL_SUCCESS) {
            std::cerr << "Error copying gradient image from buffer" << std::endl;
            std::cerr << "ERROR_CODE = " << errNum << std::endl;
        }

        image::save(cmdQueue, gradientImage, std::string("gradient_output.tif"), height, width);
        /** END DEBUGGING */

    }

    void computeSeams(cl::Context &ctx,
                      cl::CommandQueue &cmdQueue,
                      cl::Buffer &energyMatrix,
                      int width,
                      int height,
                      int pitch) {

        // Setup kernel
        cl::Kernel kernel = setup::kernel(ctx, std::string("computeSeams.cl"), std::string("computeSeams"));

        cl_int errNum;

        // Set kernel arguments
        errNum = kernel.setArg(0, energyMatrix);
        errNum |= kernel.setArg(1, width);
        errNum |= kernel.setArg(2, height);
        errNum |= kernel.setArg(3, pitch);

        if (errNum != CL_SUCCESS) {
            std::cerr << "Error setting computeSeam kernel arguments." << std::endl;
            exit(-1);
        }

        cl::NDRange offset = cl::NDRange(0);
        cl::NDRange localWorkSize = cl::NDRange(256);
        cl::NDRange globalWorkSize = cl::NDRange(256);

        //TODO(amidvidy): this should be configurable with a flag

        /** DEBUGGING */
        float *originalEnergyMatrix = new float[width * height];
        size_t copyOffset = 0;
        size_t size = width * height * sizeof(float);
        // read in original data
        errNum = cmdQueue.enqueueReadBuffer(energyMatrix,
                                            CL_TRUE,
                                            copyOffset,
                                            size,
                                            (void *) originalEnergyMatrix,
                                            NULL,
                                            NULL);

        if (errNum != CL_SUCCESS) {
            std::cerr << "Error reading original energyMatrix" << std::endl;
            delete [] originalEnergyMatrix;
            exit(-1);
        }

        /** END DEBUGGING **/

        errNum = cmdQueue.enqueueNDRangeKernel(kernel,
                                               offset,
                                               globalWorkSize,
                                               localWorkSize);

        if (errNum != CL_SUCCESS) {
            std::cerr << "Error enqueuing computeSeams kernel for execution." << std::endl;
            delete [] originalEnergyMatrix;
            exit(-1);
        }

        /** DEBUGGING **/
        float *deviceResult = new float[width * height];
        errNum = cmdQueue.enqueueReadBuffer(energyMatrix,
                                            CL_TRUE,
                                            copyOffset,
                                            size,
                                            (void *) deviceResult,
                                            NULL,
                                            NULL);

        if(!verify::computeSeams(deviceResult, originalEnergyMatrix, width, height, pitch)) {
            std::cerr << "Incorrect results from kernel::computeSeams" << std::endl;
            delete [] originalEnergyMatrix;
            delete [] deviceResult;
            exit(-1);
        }

        delete [] originalEnergyMatrix;
        delete [] deviceResult;
        /** END DEBUGGING **/
    }

    void backtrack(cl::Context &ctx,
                   cl::CommandQueue &cmdQueue,
                   cl::Buffer &energyMatrix,
                   cl::Buffer &carveArray,
                   int width,
                   int height,
                   int pitch) {

        // Setup
        cl::Kernel kernel = setup::kernel(ctx, std::string("Backtrack1.cl"), std::string("Backtrack"));

        cl_int errNum;

        // Set kernel arguments
        errNum = kernel.setArg(0, energyMatrix);
        errNum |= kernel.setArg(1, carveArray);
        errNum |= kernel.setArg(2, width);
        errNum |= kernel.setArg(3, height);
        errNum |= kernel.setArg(4, pitch);

        if (errNum != CL_SUCCESS) {
            std::cerr << "Error setting backtrack kernel arguments." << std::endl;
        }

        cl::NDRange offset = cl::NDRange(0);
        cl::NDRange localWorkSize = cl::NDRange(1);
        cl::NDRange globalWorkSize = cl::NDRange(256);

        errNum = cmdQueue.enqueueNDRangeKernel(kernel,
                                               offset,
                                               globalWorkSize,
                                               localWorkSize);


    }

    /**
     * Computes the laplacian of the gaussian convolution
     *  with an image (using openCL).
     * @param ctx An openCL context object.
     * @param cmdQueue An openCL command queue.
     * @param inputImage The image to blur
     * @param energyMatrix An openCL buffer to store the output in.
     * @param sampler An openCL image sampler object.
     * @param height The height of the input image.
     * @param width The width of the input image.
     * @return A buffer containing the gradient interpreted as a matrix of size height * width.
     */
    void laplacian(cl::Context &ctx,
                  cl::CommandQueue &cmdQueue,
                  cl::Image2D &inputImage,
                  cl::Buffer &energyMatrix,
                  cl::Sampler &sampler,
                  int height,
                  int width) {


        // Setup kernel
        cl::Kernel kernel = setup::kernel(ctx, std::string("LaplacianGaussianKernel.cl"), std::string("gaussian_laplacian"));

        cl_int errNum;

        // Set kernel arguments
        errNum = kernel.setArg(0, inputImage);
        errNum |= kernel.setArg(1, energyMatrix);
        errNum |= kernel.setArg(2, sampler);
        errNum |= kernel.setArg(3, width);
        errNum |= kernel.setArg(4, height);

        if (errNum != CL_SUCCESS) {
            std::cerr << "Error setting laplacian kernel arguments." << std::endl;
            exit(-1);
        }

        cl::NDRange offset = cl::NDRange(0, 0);
        cl::NDRange localWorkSize = cl::NDRange(16, 16);
        cl::NDRange globalWorkSize = cl::NDRange(math::roundUp(localWorkSize[0], width),
                                                 math::roundUp(localWorkSize[1], height));

        errNum = cmdQueue.enqueueNDRangeKernel(kernel,
                                               offset,
                                               globalWorkSize,
                                               localWorkSize);

        if (errNum != CL_SUCCESS) {
            std::cerr << "Error enqueuing laplacian kernel for execution." << std::endl;
            exit(-1);
        }

        // TODO(amidvidy): make this debugging code
        // Read data into an image object
        cl::Image2D gradientImage = cl::Image2D(ctx,
                                                (cl_mem_flags) CL_MEM_READ_WRITE,
                                                cl::ImageFormat(CL_LUMINANCE, CL_FLOAT),
                                                width,
                                                height,
                                                0,
                                                NULL,
                                                &errNum);
        if (errNum != CL_SUCCESS) {
            std::cerr << "Error creating laplacian output image" << std::endl;
            exit(-1);
        }

        cl::size_t<3> origin;
        origin.push_back(0);
        origin.push_back(0);
        origin.push_back(0);
        cl::size_t<3> region;
        region.push_back(width);
        region.push_back(height);
        region.push_back(1);

        errNum = cmdQueue.enqueueCopyBufferToImage(energyMatrix,
                                                   gradientImage,
                                                   0,
                                                   origin,
                                                   region,
                                                   NULL,
                                                   NULL);

        if (errNum != CL_SUCCESS) {
            std::cerr << "Error copying laplacian image from buffer" << std::endl;
            std::cerr << "ERROR_CODE = " << errNum << std::endl;
        }

        image::save(cmdQueue, gradientImage, std::string("gradient_output.tif"), height, width);
        /** END DEBUGGING */

    } // End of laplacian method.

} // namespace kernel {

#endif
