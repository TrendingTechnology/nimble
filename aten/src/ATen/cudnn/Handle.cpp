#include <ATen/cudnn/Handle.h>
#include <ATen/cuda/detail/DeviceThreadHandles.h>

namespace at { namespace native {
namespace {

void createCuDNNHandle(cudnnHandle_t *handle) {
  AT_CUDNN_CHECK(cudnnCreate(handle));
}

void destroyCuDNNHandle(cudnnHandle_t handle) {
// this is because of something dumb in the ordering of
// destruction. Sometimes atexit, the cuda context (or something)
// would already be destroyed by the time this gets destroyed. It
// happens in fbcode setting. @colesbury and I decided to not destroy
// the handle as a workaround.
//   - @soumith
#ifdef NO_CUDNN_DESTROY_HANDLE
#else
    TORCH_WARN("We temporarily disable destroying cudnnHandle_t at process exit due to unidentified segfault issue in cudnn v8.");
    // cudnnDestroy(handle);
#endif
}

at::cuda::DeviceThreadHandlePool<cudnnHandle_t, createCuDNNHandle, destroyCuDNNHandle> pool;

// Thread local PoolWindows are wrapped by unique_ptrs and lazily-initialized
// to avoid initialization issues that caused hangs on Windows.
// See: https://github.com/pytorch/pytorch/pull/22405
// This thread local unique_ptrs will be destroyed when the thread terminates,
// releasing its reserved handles back to the pool.
thread_local std::unique_ptr<decltype(pool)::PoolWindow> myPoolWindow;

} // namespace

cudnnHandle_t getCudnnHandle()
{
  int device;
  AT_CUDA_CHECK(cudaGetDevice(&device));

  if (!myPoolWindow)
    myPoolWindow.reset(pool.newPoolWindow());

  return myPoolWindow->reserve(device);
}

}} // namespace at::native
