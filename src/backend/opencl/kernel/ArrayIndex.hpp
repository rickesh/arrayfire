/*******************************************************
 * Copyright (c) 2014, ArrayFire
 * All rights reserved.
 *
 * This file is distributed under 3-clause BSD license.
 * The complete license agreement can be obtained at:
 * http://arrayfire.com/licenses/BSD-3-Clause
 ********************************************************/

#pragma once
#include <kernel_headers/ArrayIndex.hpp>
#include <program.hpp>
#include <traits.hpp>
#include <string>
#include <mutex>
#include <map>
#include <dispatch.hpp>
#include <Param.hpp>
#include <debug_opencl.hpp>

using cl::Buffer;
using cl::Program;
using cl::Kernel;
using cl::make_kernel;
using cl::EnqueueArgs;
using cl::NDRange;
using std::string;

namespace opencl
{

namespace kernel
{

static const dim_type THREADS_X = 32;
static const dim_type THREADS_Y = 8;

template<typename in_t, typename idx_t, unsigned dim>
void arrayIndex(Param out, const Param in, const Param indices, dim_type nDims)
{
    try {
        static std::once_flag compileFlags[DeviceManager::MAX_DEVICES];
        static std::map<int, Program*>  aiProgs;
        static std::map<int, Kernel*> aiKernels;

        int device = getActiveDeviceId();

        std::call_once( compileFlags[device], [device] () {
                    std::ostringstream options;
                    options << " -D in_t=" << dtype_traits<in_t>::getName()
                            << " -D idx_t=" << dtype_traits<idx_t>::getName()
                            << " -D DIM=" <<dim;

                    if (std::is_same<in_t, double>::value || 
                        std::is_same<in_t, cdouble>::value || 
                        std::is_same<idx_t, double>::value) {
                        options << " -D USE_DOUBLE";
                    }

                    Program prog;
                    buildProgram(prog, ArrayIndex_cl, ArrayIndex_cl_len, options.str());
                    aiProgs[device]   = new Program(prog);
                    aiKernels[device] = new Kernel(*aiProgs[device], "arrayIndexND");
                });

        NDRange local(THREADS_X, THREADS_Y);

        dim_type blk_x = divup(out.info.dims[0], THREADS_X);
        dim_type blk_y = divup(out.info.dims[1], THREADS_Y);

        NDRange global(blk_x * out.info.dims[2] * THREADS_X, 
                       blk_y * out.info.dims[3] * THREADS_Y);

        auto arrIdxOp = make_kernel<Buffer, KParam,
                                    Buffer, KParam,
                                    Buffer, KParam,
                                    dim_type, dim_type>(*aiKernels[device]);

        arrIdxOp(EnqueueArgs(getQueue(), global, local),
                *out.data, out.info, *in.data, in.info, *indices.data, indices.info, blk_x, blk_y);

        CL_DEBUG_FINISH(getQueue());
    } catch (cl::Error err) {
        CL_TO_AF_ERROR(err);
        throw;
    }
}

}

}
