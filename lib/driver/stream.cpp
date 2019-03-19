/* Copyright 2015-2017 Philippe Tillet
* 
* Permission is hereby granted, free of charge, to any person obtaining 
* a copy of this software and associated documentation files 
* (the "Software"), to deal in the Software without restriction, 
* including without limitation the rights to use, copy, modify, merge, 
* publish, distribute, sublicense, and/or sell copies of the Software, 
* and to permit persons to whom the Software is furnished to do so, 
* subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be 
* included in all copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF 
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <iostream>
#include <cassert>
#include <array>

#include "triton/driver/backend.h"
#include "triton/driver/stream.h"
#include "triton/driver/context.h"
#include "triton/driver/device.h"
#include "triton/driver/event.h"
#include "triton/driver/kernel.h"
#include "triton/driver/buffer.h"

namespace triton
{

namespace driver
{

/* ------------------------ */
//         Base             //
/* ------------------------ */

stream::stream(driver::context *ctx, CUstream cu, bool has_ownership)
  : polymorphic_resource(cu, has_ownership), ctx_(ctx) {

}

stream::stream(driver::context *ctx, cl_command_queue cl, bool has_ownership)
  : polymorphic_resource(cl, has_ownership), ctx_(ctx) {

}

driver::context* stream::context() const {
  return ctx_;
}


/* ------------------------ */
//         OpenCL           //
/* ------------------------ */


void cl_stream::synchronize() {
  dispatch::clFinish(*cl_);
}


/* ------------------------ */
//         CUDA             //
/* ------------------------ */

inline CUcontext get_context() {
  CUcontext result;
  dispatch::cuCtxGetCurrent(&result);
  return result;
}

cu_stream::cu_stream(CUstream str, bool take_ownership):
  stream(backend::contexts::import(get_context()), str, take_ownership) {
}

cu_stream::cu_stream(driver::context *context): stream((driver::cu_context*)context, CUstream(), true) {
  cu_context::context_switcher ctx_switch(*ctx_);
  dispatch::cuStreamCreate(&*cu_, 0);
}

void cu_stream::synchronize() {
  cu_context::context_switcher ctx_switch(*ctx_);
  dispatch::cuStreamSynchronize(*cu_);
}

void cu_stream::enqueue(driver::cu_kernel const & kernel, std::array<size_t, 3> grid, std::array<size_t, 3> block, std::vector<Event> const *, Event* event) {
  cu_context::context_switcher ctx_switch(*ctx_);
  if(event)
    dispatch::cuEventRecord(event->cu()->first, *cu_);
  dispatch::cuLaunchKernel(*kernel.cu(), grid[0], grid[1], grid[2], block[0], block[1], block[2], 0, *cu_,(void**)kernel.cu_params(), NULL);
  if(event)
    dispatch::cuEventRecord(event->cu()->second, *cu_);
}

void cu_stream::write(driver::cu_buffer const & buffer, bool blocking, std::size_t offset, std::size_t size, void const* ptr) {
  cu_context::context_switcher ctx_switch(*ctx_);
  if(blocking)
    dispatch::cuMemcpyHtoD(*buffer.cu() + offset, ptr, size);
  else
    dispatch::cuMemcpyHtoDAsync(*buffer.cu() + offset, ptr, size, *cu_);
}

void cu_stream::read(driver::cu_buffer const & buffer, bool blocking, std::size_t offset, std::size_t size, void* ptr) {
  cu_context::context_switcher ctx_switch(*ctx_);
  if(blocking)
    dispatch::cuMemcpyDtoH(ptr, *buffer.cu() + offset, size);
  else
    dispatch::cuMemcpyDtoHAsync(ptr, *buffer.cu() + offset, size, *cu_);
}


}

}
