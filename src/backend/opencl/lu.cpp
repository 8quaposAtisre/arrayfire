/*******************************************************
 * Copyright (c) 2014, ArrayFire
 * All rights reserved.
 *
 * This file is distributed under 3-clause BSD license.
 * The complete license agreement can be obtained at:
 * http://arrayfire.com/licenses/BSD-3-Clause
 ********************************************************/

#include <lu.hpp>
#include <err_common.hpp>

#if defined(WITH_OPENCL_LINEAR_ALGEBRA)
#include <magma/magma.h>
#include <kernel/lu_split.hpp>
#include <copy.hpp>

namespace opencl
{

Array<int> convertPivot(int *ipiv, int in_sz, int out_sz)
{

    int *out = new int[out_sz];

    for (int i = 0; i < out_sz; i++) {
        out[i] = i;
    }

    for(int j = 0; j < in_sz; j++) {
        // 1 indexed in pivot
        std::swap(out[j], out[ipiv[j] - 1]);
    }

    Array<int> res = createHostDataArray(dim4(out_sz), out);

    delete[] out;
    return res;
}

template<typename T>
void lu(Array<T> &lower, Array<T> &upper, Array<int> &pivot, const Array<T> &in)
{

    try {
        dim4 iDims = in.dims();
        int M = iDims[0];
        int N = iDims[1];
        int MN = std::min(M, N);
        int *ipiv = new int[MN];

        Array<T> in_copy = copyArray<T>(in);

        cl::Buffer *in_buf = in_copy.get();
        int info = 0;
        magma_getrf_gpu<T>(M, N, (*in_buf)(), in.getOffset(), in.strides()[1],
                           ipiv, getQueue()(), &info);

        // SPLIT into lower and upper
        dim4 ldims(M, MN);
        dim4 udims(MN, N);
        lower = createEmptyArray<T>(ldims);
        upper = createEmptyArray<T>(udims);

        kernel::lu_split<T>(lower, upper, in_copy);

        pivot = convertPivot(ipiv, MN, M);
        delete[] ipiv;
    } catch (cl::Error &err) {
        CL_TO_AF_ERROR(err);
    }
}

template<typename T>
Array<int> lu_inplace(Array<T> &in, const bool convert_pivot)
{
    try {
        dim4 iDims = in.dims();
        int M = iDims[0];
        int N = iDims[1];
        int MN = std::min(M, N);
        int *ipiv = new int[MN];

        cl::Buffer *in_buf = in.get();
        int info = 0;
        magma_getrf_gpu<T>(M, N, (*in_buf)(), in.getOffset(), in.strides()[1],
                           ipiv, getQueue()(), &info);

        Array<int> pivot = convertPivot(ipiv, MN, M);
        delete[] ipiv;
        return pivot;
    } catch(cl::Error &err) {
        CL_TO_AF_ERROR(err);
    }
}

#define INSTANTIATE_LU(T)                                                                           \
    template Array<int> lu_inplace<T>(Array<T> &in, const bool convert_pivot);                      \
    template void lu<T>(Array<T> &lower, Array<T> &upper, Array<int> &pivot, const Array<T> &in);

INSTANTIATE_LU(float)
INSTANTIATE_LU(cfloat)
INSTANTIATE_LU(double)
INSTANTIATE_LU(cdouble)

}

#else

namespace opencl
{

template<typename T>
void lu(Array<T> &lower, Array<T> &upper, Array<int> &pivot, const Array<T> &in)
{
    AF_ERROR("Linear Algebra is disabled on OpenCL", AF_ERR_NOT_CONFIGURED);
}

template<typename T>
Array<int> lu_inplace(Array<T> &in, const bool convert_pivot)
{
    AF_ERROR("Linear Algebra is disabled on OpenCL", AF_ERR_NOT_CONFIGURED);
}

#define INSTANTIATE_LU(T)                                                                           \
    template Array<int> lu_inplace<T>(Array<T> &in, const bool convert_pivot);                      \
    template void lu<T>(Array<T> &lower, Array<T> &upper, Array<int> &pivot, const Array<T> &in);

INSTANTIATE_LU(float)
INSTANTIATE_LU(cfloat)
INSTANTIATE_LU(double)
INSTANTIATE_LU(cdouble)

}

#endif
