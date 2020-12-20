// ----------------------------------------------------------------------------
// -                        Open3D: www.open3d.org                            -
// ----------------------------------------------------------------------------
// The MIT License (MIT)
//
// Copyright (c) 2018 www.open3d.org
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
// ----------------------------------------------------------------------------

#include "open3d/core/Tensor.h"

#include <vector>

#include "open3d/core/Blob.h"
#include "open3d/core/CUDAUtils.h"
#include "open3d/core/Device.h"
#include "open3d/core/Dispatch.h"
#include "open3d/core/Dtype.h"
#include "open3d/core/SizeVector.h"
#include "open3d/core/TensorKey.h"
#include "open3d/utility/Optional.h"
#include "pybind/core/core.h"
#include "pybind/core/tensor_converter.h"
#include "pybind/docstring.h"
#include "pybind/open3d_pybind.h"
#include "pybind/pybind_utils.h"

#define CONST_ARG const
#define NON_CONST_ARG

#define BIND_BINARY_OP_ALL_DTYPES(py_name, cpp_name, self_const)            \
    tensor.def(#py_name, [](self_const Tensor& self, const Tensor& other) { \
        return self.cpp_name(other);                                        \
    });                                                                     \
    tensor.def(#py_name, &Tensor::cpp_name<float>);                         \
    tensor.def(#py_name, &Tensor::cpp_name<double>);                        \
    tensor.def(#py_name, &Tensor::cpp_name<int32_t>);                       \
    tensor.def(#py_name, &Tensor::cpp_name<int64_t>);                       \
    tensor.def(#py_name, &Tensor::cpp_name<uint8_t>);                       \
    tensor.def(#py_name, &Tensor::cpp_name<bool>);

#define BIND_BINARY_R_OP_ALL_DTYPES(py_name, cpp_name)                    \
    tensor.def(#py_name, [](const Tensor& self, float value) {            \
        return Tensor::Full({}, value, self.GetDtype(), self.GetDevice()) \
                .cpp_name(self);                                          \
    });                                                                   \
    tensor.def(#py_name, [](const Tensor& self, double value) {           \
        return Tensor::Full({}, value, self.GetDtype(), self.GetDevice()) \
                .cpp_name(self);                                          \
    });                                                                   \
    tensor.def(#py_name, [](const Tensor& self, int32_t value) {          \
        return Tensor::Full({}, value, self.GetDtype(), self.GetDevice()) \
                .cpp_name(self);                                          \
    });                                                                   \
    tensor.def(#py_name, [](const Tensor& self, int64_t value) {          \
        return Tensor::Full({}, value, self.GetDtype(), self.GetDevice()) \
                .cpp_name(self);                                          \
    });                                                                   \
    tensor.def(#py_name, [](const Tensor& self, uint8_t value) {          \
        return Tensor::Full({}, value, self.GetDtype(), self.GetDevice()) \
                .cpp_name(self);                                          \
    });                                                                   \
    tensor.def(#py_name, [](const Tensor& self, bool value) {             \
        return Tensor::Full({}, value, self.GetDtype(), self.GetDevice()) \
                .cpp_name(self);                                          \
    });

namespace open3d {
namespace core {

template <typename T>
static std::vector<T> ToFlatVector(
        py::array_t<T, py::array::c_style | py::array::forcecast> np_array) {
    py::buffer_info info = np_array.request();
    T* start = static_cast<T*>(info.ptr);
    return std::vector<T>(start, start + info.size);
}

template <typename func_t>
static void BindTensorCreation(py::class_<Tensor>& tensor,
                               const std::string& py_name,
                               func_t cpp_func) {
    tensor.def_static(
            py_name.c_str(),
            [cpp_func](const SizeVector& shape, utility::optional<Dtype> dtype,
                       utility::optional<Device> device) {
                return cpp_func(
                        shape,
                        dtype.has_value() ? dtype.value() : Dtype::Float32,
                        device.has_value() ? device.value() : Device("CPU:0"));
            },
            "shape"_a, "dtype"_a = py::none(), "device"_a = py::none());
    tensor.def_static(
            py_name.c_str(),
            [cpp_func](const py::tuple& shape, utility::optional<Dtype> dtype,
                       utility::optional<Device> device) {
                return cpp_func(
                        PyTupleToSizeVector(shape),
                        dtype.has_value() ? dtype.value() : Dtype::Float32,
                        device.has_value() ? device.value() : Device("CPU:0"));
            },
            "shape"_a, "dtype"_a = py::none(), "device"_a = py::none());
    tensor.def_static(
            py_name.c_str(),
            [cpp_func](const py::list& shape, utility::optional<Dtype> dtype,
                       utility::optional<Device> device) {
                return cpp_func(
                        PyListToSizeVector(shape),
                        dtype.has_value() ? dtype.value() : Dtype::Float32,
                        device.has_value() ? device.value() : Device("CPU:0"));
            },
            "shape"_a, "dtype"_a = py::none(), "device"_a = py::none());
}

template <typename T>
static void BindTensorFullCreation(py::class_<Tensor>& tensor) {
    tensor.def_static(
            "full",
            [](const SizeVector& shape, T fill_value,
               utility::optional<Dtype> dtype,
               utility::optional<Device> device) {
                return Tensor::Full<T>(
                        shape, fill_value,
                        dtype.has_value() ? dtype.value() : Dtype::Float32,
                        device.has_value() ? device.value() : Device("CPU:0"));
            },
            "shape"_a, "fill_value"_a, "dtype"_a = py::none(),
            "device"_a = py::none());
    tensor.def_static(
            "full",
            [](const py::tuple& shape, T fill_value,
               utility::optional<Dtype> dtype,
               utility::optional<Device> device) {
                return Tensor::Full<T>(
                        PyTupleToSizeVector(shape), fill_value,
                        dtype.has_value() ? dtype.value() : Dtype::Float32,
                        device.has_value() ? device.value() : Device("CPU:0"));
            },
            "shape"_a, "fill_value"_a, "dtype"_a = py::none(),
            "device"_a = py::none());
    tensor.def_static(
            "full",
            [](const py::list& shape, T fill_value,
               utility::optional<Dtype> dtype,
               utility::optional<Device> device) {
                return Tensor::Full<T>(
                        PyListToSizeVector(shape), fill_value,
                        dtype.has_value() ? dtype.value() : Dtype::Float32,
                        device.has_value() ? device.value() : Device("CPU:0"));
            },
            "shape"_a, "fill_value"_a, "dtype"_a = py::none(),
            "device"_a = py::none());
}

void pybind_core_tensor(py::module& m) {
    py::class_<Tensor> tensor(
            m, "Tensor",
            "A Tensor is a view of a data Blob with shape, stride, data_ptr.");

    // o3c.Tensor(np.array([[0, 1, 2], [3, 4, 5]]), dtype=None, device=None).
    tensor.def(py::init([](const py::array& np_array,
                           utility::optional<Dtype> dtype,
                           utility::optional<Device> device) {
                   Tensor t = PyArrayToTensor(np_array, /*inplace=*/false);
                   if (dtype.has_value()) {
                       t = t.To(dtype.value(), /*copy=*/false);
                   }
                   if (device.has_value() &&
                       device.value() != core::Device("CPU:0")) {
                       t = t.Copy(device.value());
                   }
                   return t;
               }),
               "np_array"_a, "dtype"_a = py::none(), "device"_a = py::none());

    // o3c.Tensor(1, dtype=None, device=None).
    // Default to Int64, CPU:0.
    tensor.def(py::init([](int64_t scalar_value, utility::optional<Dtype> dtype,
                           utility::optional<Device> device) {
                   return IntToTensor(scalar_value, dtype, device);
               }),
               "scalar_value"_a, "dtype"_a = py::none(),
               "device"_a = py::none());

    // o3c.Tensor(3.14, dtype=None, device=None).
    // Default to Float64, CPU:0.
    tensor.def(py::init([](double scalar_value, utility::optional<Dtype> dtype,
                           utility::optional<Device> device) {
                   return DoubleToTensor(scalar_value, dtype, device);
               }),
               "scalar_value"_a, "dtype"_a = py::none(),
               "device"_a = py::none());

    // o3c.Tensor([[0, 1, 2], [3, 4, 5]], dtype=None, device=None).
    tensor.def(
            py::init([](const py::list& shape, utility::optional<Dtype> dtype,
                        utility::optional<Device> device) {
                return PyListToTensor(shape, dtype, device);
            }),
            "shape"_a, "dtype"_a = py::none(), "device"_a = py::none());

    // o3c.Tensor(((0, 1, 2), (3, 4, 5)), dtype=None, device=None).
    tensor.def(
            py::init([](const py::tuple& shape, utility::optional<Dtype> dtype,
                        utility::optional<Device> device) {
                return PyTupleToTensor(shape, dtype, device);
            }),
            "shape"_a, "dtype"_a = py::none(), "device"_a = py::none());

    pybind_core_tensor_accessor(tensor);

    // Tensor creation API
    BindTensorCreation(tensor, "empty", Tensor::Empty);
    BindTensorCreation(tensor, "zeros", Tensor::Zeros);
    BindTensorCreation(tensor, "ones", Tensor::Ones);
    BindTensorFullCreation<float>(tensor);
    BindTensorFullCreation<double>(tensor);
    BindTensorFullCreation<int32_t>(tensor);
    BindTensorFullCreation<int64_t>(tensor);
    BindTensorFullCreation<uint8_t>(tensor);
    BindTensorFullCreation<bool>(tensor);
    tensor.def_static(
            "eye",
            [](int64_t n, utility::optional<Dtype> dtype,
               utility::optional<Device> device) {
                return Tensor::Eye(
                        n, dtype.has_value() ? dtype.value() : Dtype::Float32,
                        device.has_value() ? device.value() : Device("CPU:0"));
            },
            "n"_a, "dtype"_a = py::none(), "device"_a = py::none());
    tensor.def_static("diag", &Tensor::Diag);

    // Tensor copy
    tensor.def("shallow_copy_from", &Tensor::ShallowCopyFrom);

    // Device transfer
    tensor.def("cuda", [](const Tensor& tensor, int device_id = 0) {
              if (!cuda::IsAvailable()) {
                  utility::LogError(
                          "CUDA is not available, cannot copy Tensor.");
              }
              if (device_id < 0 || device_id >= cuda::DeviceCount()) {
                  utility::LogError(
                          "Invalid device_id {}, must satisfy 0 <= "
                          "device_id < {}",
                          device_id, cuda::DeviceCount());
              }
              return tensor.Copy(Device(Device::DeviceType::CUDA, device_id));
          }).def("cpu", [](const Tensor& tensor) {
        return tensor.Copy(Device(Device::DeviceType::CPU, 0));
    });

    // Buffer I/O for Numpy and DLPack(PyTorch)
    tensor.def("numpy", &core::TensorToPyArray);

    tensor.def_static("from_numpy", [](py::array np_array) {
        return core::PyArrayToTensor(np_array, true);
    });

    tensor.def("to_dlpack", [](const Tensor& tensor) {
        DLManagedTensor* dl_managed_tensor = tensor.ToDLPack();
        // See PyTorch's torch/csrc/Module.cpp
        auto capsule_destructor = [](PyObject* data) {
            DLManagedTensor* dl_managed_tensor =
                    (DLManagedTensor*)PyCapsule_GetPointer(data, "dltensor");
            if (dl_managed_tensor) {
                // the dl_managed_tensor has not been consumed,
                // call deleter ourselves
                dl_managed_tensor->deleter(
                        const_cast<DLManagedTensor*>(dl_managed_tensor));
            } else {
                // The dl_managed_tensor has been consumed
                // PyCapsule_GetPointer has set an error indicator
                PyErr_Clear();
            }
        };
        return py::capsule(dl_managed_tensor, "dltensor", capsule_destructor);
    });

    tensor.def_static("from_dlpack", [](py::capsule data) {
        DLManagedTensor* dl_managed_tensor =
                static_cast<DLManagedTensor*>(data);
        if (!dl_managed_tensor) {
            utility::LogError(
                    "from_dlpack must receive "
                    "DLManagedTensor PyCapsule.");
        }
        // Make sure that the PyCapsule is not used again.
        // See:
        // torch/csrc/Module.cpp, and
        // https://github.com/cupy/cupy/pull/1445/files#diff-ddf01ff512087ef616db57ecab88c6ae
        Tensor t = Tensor::FromDLPack(dl_managed_tensor);
        PyCapsule_SetName(data.ptr(), "used_dltensor");
        return t;
    });

    /// Linalg operations.
    tensor.def("matmul", &Tensor::Matmul);
    tensor.def("lstsq", &Tensor::LeastSquares);
    tensor.def("solve", &Tensor::Solve);
    tensor.def("inv", &Tensor::Inverse);
    tensor.def("svd", &Tensor::SVD);

    // Casting.
    tensor.def("to", &Tensor::To);
    tensor.def("T", &Tensor::T);
    tensor.def("contiguous", &Tensor::Contiguous);

    // Binary element-wise ops.
    BIND_BINARY_OP_ALL_DTYPES(add, Add, CONST_ARG);
    BIND_BINARY_OP_ALL_DTYPES(add_, Add_, NON_CONST_ARG);
    BIND_BINARY_OP_ALL_DTYPES(__add__, Add, CONST_ARG);
    BIND_BINARY_OP_ALL_DTYPES(__iadd__, Add_, NON_CONST_ARG);
    BIND_BINARY_R_OP_ALL_DTYPES(__radd__, Add);

    BIND_BINARY_OP_ALL_DTYPES(sub, Sub, CONST_ARG);
    BIND_BINARY_OP_ALL_DTYPES(sub_, Sub_, NON_CONST_ARG);
    BIND_BINARY_OP_ALL_DTYPES(__sub__, Sub, CONST_ARG);
    BIND_BINARY_OP_ALL_DTYPES(__isub__, Sub_, NON_CONST_ARG);
    BIND_BINARY_R_OP_ALL_DTYPES(__rsub__, Sub);

    BIND_BINARY_OP_ALL_DTYPES(mul, Mul, CONST_ARG);
    BIND_BINARY_OP_ALL_DTYPES(mul_, Mul_, NON_CONST_ARG);
    BIND_BINARY_OP_ALL_DTYPES(__mul__, Mul, CONST_ARG);
    BIND_BINARY_OP_ALL_DTYPES(__imul__, Mul_, NON_CONST_ARG);
    BIND_BINARY_R_OP_ALL_DTYPES(__rmul__, Mul);

    BIND_BINARY_OP_ALL_DTYPES(div, Div, CONST_ARG);
    BIND_BINARY_OP_ALL_DTYPES(div_, Div_, NON_CONST_ARG);
    BIND_BINARY_OP_ALL_DTYPES(__div__, Div, CONST_ARG);
    BIND_BINARY_OP_ALL_DTYPES(__idiv__, Div_, NON_CONST_ARG);
    BIND_BINARY_R_OP_ALL_DTYPES(__rdiv__, Div);
    BIND_BINARY_OP_ALL_DTYPES(__truediv__, Div, CONST_ARG);
    BIND_BINARY_OP_ALL_DTYPES(__itruediv__, Div_, NON_CONST_ARG);
    BIND_BINARY_R_OP_ALL_DTYPES(__rtruediv__, Div);
    BIND_BINARY_OP_ALL_DTYPES(__floordiv__, Div, CONST_ARG);  // truediv only.
    BIND_BINARY_OP_ALL_DTYPES(__rfloordiv__, Div_, NON_CONST_ARG);
    BIND_BINARY_R_OP_ALL_DTYPES(__ifloordiv__, Div);

    // Binary boolean element-wise ops.
    BIND_BINARY_OP_ALL_DTYPES(logical_and, LogicalAnd, CONST_ARG);
    BIND_BINARY_OP_ALL_DTYPES(logical_and_, LogicalAnd_, NON_CONST_ARG);
    BIND_BINARY_OP_ALL_DTYPES(logical_or, LogicalOr, CONST_ARG);
    BIND_BINARY_OP_ALL_DTYPES(logical_or_, LogicalOr_, NON_CONST_ARG);
    BIND_BINARY_OP_ALL_DTYPES(logical_xor, LogicalXor, CONST_ARG);
    BIND_BINARY_OP_ALL_DTYPES(logical_xor_, LogicalXor_, NON_CONST_ARG);
    BIND_BINARY_OP_ALL_DTYPES(gt, Gt, CONST_ARG);
    BIND_BINARY_OP_ALL_DTYPES(gt_, Gt_, NON_CONST_ARG);
    BIND_BINARY_OP_ALL_DTYPES(lt, Lt, CONST_ARG);
    BIND_BINARY_OP_ALL_DTYPES(lt_, Lt_, NON_CONST_ARG);
    BIND_BINARY_OP_ALL_DTYPES(ge, Ge, CONST_ARG);
    BIND_BINARY_OP_ALL_DTYPES(ge_, Ge_, NON_CONST_ARG);
    BIND_BINARY_OP_ALL_DTYPES(le, Le, CONST_ARG);
    BIND_BINARY_OP_ALL_DTYPES(le_, Le_, NON_CONST_ARG);
    BIND_BINARY_OP_ALL_DTYPES(eq, Eq, CONST_ARG);
    BIND_BINARY_OP_ALL_DTYPES(eq_, Eq_, NON_CONST_ARG);
    BIND_BINARY_OP_ALL_DTYPES(ne, Ne, CONST_ARG);
    BIND_BINARY_OP_ALL_DTYPES(ne_, Ne_, NON_CONST_ARG);

    // Getters and setters as properties.
    tensor.def_property_readonly(
            "shape", [](const Tensor& tensor) { return tensor.GetShape(); });
    tensor.def_property_readonly("strides", [](const Tensor& tensor) {
        return tensor.GetStrides();
    });
    tensor.def_property_readonly("dtype", &Tensor::GetDtype);
    tensor.def_property_readonly("device", &Tensor::GetDevice);
    tensor.def_property_readonly("blob", &Tensor::GetBlob);
    tensor.def_property_readonly("ndim", &Tensor::NumDims);
    tensor.def("num_elements", &Tensor::NumElements);
    tensor.def("__len__", &Tensor::GetLength);
    tensor.def("__bool__", &Tensor::IsNonZero);  // Python 3.X.

    // Unary element-wise ops.
    tensor.def("sqrt", &Tensor::Sqrt);
    tensor.def("sqrt_", &Tensor::Sqrt_);
    tensor.def("sin", &Tensor::Sin);
    tensor.def("sin_", &Tensor::Sin_);
    tensor.def("cos", &Tensor::Cos);
    tensor.def("cos_", &Tensor::Cos_);
    tensor.def("neg", &Tensor::Neg);
    tensor.def("neg_", &Tensor::Neg_);
    tensor.def("exp", &Tensor::Exp);
    tensor.def("exp_", &Tensor::Exp_);
    tensor.def("abs", &Tensor::Abs);
    tensor.def("abs_", &Tensor::Abs_);
    tensor.def("logical_not", &Tensor::LogicalNot);
    tensor.def("logical_not_", &Tensor::LogicalNot_);

    // Boolean.
    tensor.def("_non_zero", &Tensor::NonZero);
    tensor.def("_non_zero_numpy", &Tensor::NonZeroNumpy);
    tensor.def("all", &Tensor::All);
    tensor.def("any", &Tensor::Any);

    // Reduction ops.
    tensor.def("sum", &Tensor::Sum);
    tensor.def("mean", &Tensor::Mean);
    tensor.def("prod", &Tensor::Prod);
    tensor.def("min", &Tensor::Min);
    tensor.def("max", &Tensor::Max);
    tensor.def("argmin_", &Tensor::ArgMin);
    tensor.def("argmax_", &Tensor::ArgMax);

    // Comparison.
    tensor.def("allclose", &Tensor::AllClose, "other"_a, "rtol"_a = 1e-5,
               "atol"_a = 1e-8);
    tensor.def("isclose", &Tensor::IsClose, "other"_a, "rtol"_a = 1e-5,
               "atol"_a = 1e-8);
    tensor.def("issame", &Tensor::IsSame);

    // Print tensor.
    tensor.def("__repr__",
               [](const Tensor& tensor) { return tensor.ToString(); });
    tensor.def("__str__",
               [](const Tensor& tensor) { return tensor.ToString(); });

    // Get item from Tensor of one element.
    tensor.def("_item_float", [](const Tensor& t) { return t.Item<float>(); });
    tensor.def("_item_double",
               [](const Tensor& t) { return t.Item<double>(); });
    tensor.def("_item_int32_t",
               [](const Tensor& t) { return t.Item<int32_t>(); });
    tensor.def("_item_int64_t",
               [](const Tensor& t) { return t.Item<int64_t>(); });
    tensor.def("_item_uint8_t",
               [](const Tensor& t) { return t.Item<uint8_t>(); });
    tensor.def("_item_bool", [](const Tensor& t) { return t.Item<bool>(); });
}

}  // namespace core
}  // namespace open3d
