#ifndef KERNEL_HPP
#define KERNEL_HPP

// OpenCL
#include <CL/cl.hpp>

// SeamCL
#include "image.hpp"
#include "setup.hpp"
#include "math.hpp"
#include "mem.hpp"

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
    cl::Image2D blur(cl::Context &ctx,
                     cl::CommandQueue &cmdQueue,
                     cl::Image2D &inputImage,
                     cl::Sampler &sampler,
                     int height,
                     int width) {

        // Create output buffer
        cl::Image2D outputImage = image::make(ctx, height, width);

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

        return outputImage;
    }

    /**
     * Computes the gradient of an image using openCL.
     * @param ctx An openCL context object.
     * @param cmdQueue An openCL command queue.
     * @param sampler An openCL image sampler object.
     * @param height The height of the input image.
     * @param width The width of the input image.
     * @return A buffer containing the gradient interpreted as a matrix of size height * width.
     */
    cl::Buffer gradient(cl::Context &ctx,
                        cl::CommandQueue &cmdQueue,
                        cl::Image2D &inputImage,
                        cl::Sampler &sampler,
                        int height,
                        int width) {

        // Create output buffer
        cl::Buffer resultMatrix = mem::buffer(ctx, cmdQueue, height * width * sizeof(float));
        // Setup kernel
        cl::Kernel kernel = setup::kernel(ctx, std::string("GradientKernel.cl"), std::string("image_gradient"));

        cl_int errNum;

        // Set kernel arguments
        errNum = kernel.setArg(0, inputImage);
        errNum |= kernel.setArg(1, resultMatrix);
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

        errNum = cmdQueue.enqueueCopyBufferToImage(resultMatrix,
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

        if (!image::save(cmdQueue, gradientImage, std::string("gradient_output.tif"), height, width)) {
            std::cerr << "Error writing gradient output image." << std::endl;
        }
        /** END DEBUGGING */


        if (errNum != CL_SUCCESS) {
            std::cerr << "Error enqueuing gradient kernel for execution." << std::endl;
            exit(-1);
        }

        return resultMatrix;
    }

} // namespace kernel {

#endif