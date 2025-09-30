#include <cuda_runtime.h>
#include <cstdio>

// -------------------
// Transform kernel
// -------------------
__global__ void transform_kernel_matrix(
    float* in_x, float* in_y, float* in_z,
    const float* T,  // 16-element array: row-major 4x4 matrix
    int num_points)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < num_points) {
        float x = in_x[idx];
        float y = in_y[idx];
        float z = in_z[idx];

        float out_x = T[0] * x + T[1] * y + T[2] * z + T[3];
        float out_y = T[4] * x + T[5] * y + T[6] * z + T[7];
        float out_z = T[8] * x + T[9] * y + T[10] * z + T[11];

        // No atomic needed since each thread writes its own index
        in_x[idx] = out_x;
        in_y[idx] = out_y;
        in_z[idx] = out_z;
    }
}

// -------------------
// PointCloud → LaserScan kernel
// -------------------
__global__
void pointcloud_to_laserscan_kernel(
    const float* x, const float* y, const float* z, size_t N,
    float* ranges,
    float min_height, float max_height,
    int num_steps,
    float min_range, float max_range,
    float min_angle, float max_angle,
    float angle_increment)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < N)
    {
        float xi = x[idx];
        float yi = y[idx];
        float zi = z[idx];

        if (zi < min_height || zi > max_height)
            return;

        const float range = hypotf(xi, yi);

        if (range > 2.0f && zi < 0.1f)
            return;

        if (xi < min_range || xi > max_range)
            return;

        const float angle = atan2f(yi, xi);
        if (angle < min_angle || angle > max_angle)
            return;

        int index = (int)((angle - min_angle) / angle_increment);
        if (index < 0 || index >= num_steps)
            return;

        float old_range = ranges[index];
        if (isnan(old_range) || range < old_range)
        {
            atomicExch(ranges + index, range);
        }
    }
}

// -------------------
// Host wrappers with error checking
// -------------------

static inline void checkCudaError(const char* msg)
{
    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        fprintf(stderr, "CUDA error after %s: %s\n",
                msg, cudaGetErrorString(err));
    }
}

extern "C"
void launch_transform_kernel_matrix(float* x, float* y, float* z, size_t N, const float* matrix)
{
    int blockSize = 256;
    int numBlocks = (N + blockSize - 1) / blockSize;

    transform_kernel_matrix<<<numBlocks, blockSize>>>(x, y, z, matrix, N);
    checkCudaError("transform_kernel_matrix launch");
    cudaDeviceSynchronize();
}

extern "C"
void launch_pointcloud_to_laserscan_kernel(
    const float* x, const float* y, const float* z, size_t N,
    float* ranges,
    float min_height, float max_height,   // <-- FIXED (was min_height, height_increment)
    int num_steps,
    float min_range, float max_range,
    float min_angle, float max_angle,
    float angle_increment)
{
    int blockSize = 256;
    int numBlocks = (N + blockSize - 1) / blockSize;

    pointcloud_to_laserscan_kernel<<<numBlocks, blockSize>>>(
        x, y, z, N,
        ranges,
        min_height, max_height,   // <-- FIXED
        num_steps,
        min_range, max_range,
        min_angle, max_angle,
        angle_increment);

    checkCudaError("pointcloud_to_laserscan_kernel launch");
    cudaDeviceSynchronize();
}