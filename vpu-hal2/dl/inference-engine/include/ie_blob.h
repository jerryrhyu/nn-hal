// Copyright (c) 2018 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/**
 * @brief A header file for Blob and generic TBlob<>
 * @file ie_blob.h
 */
#pragma once

#include <memory>
#include <vector>
#include <string>
#include <numeric>
#include <cstring>
#include "ie_common.h"
#include "details/ie_exception.hpp"
#include "details/ie_blob_iterator.hpp"
#include "ie_allocator.hpp"
#include "ie_locked_memory.hpp"
#include "ie_precision.hpp"
#include "ie_layouts.h"
#include "details/ie_pre_allocator.hpp"
#include <utility>
#include <functional>
#include <map>

namespace InferenceEngine {
/**
 * @class Blob
 * @brief This class implements a container object that represents a tensor in memory (host and remote/accelerated)
 */
class Blob {
public:
    /**
     * @typedef Ptr
     * @brief A smart pointer to the current Blob object
     */
    typedef std::shared_ptr<Blob> Ptr;

    /**
     * @deprecated Please use TensorDesc for getting precision
     * @brief Returns the tensor precision of the current Blob object
     */
    Precision type() const noexcept {
        return tensorDesc.getPrecision();
    }

    /**
     * @deprecated Please use TensorDesc for getting precision
     * @brief Returns the tensor precision of the current Blob object
     */
    Precision precision() const noexcept {
        return tensorDesc.getPrecision();
    }

    /**
     * @deprecated Please use TensorDesc for getting current layout
     * @brief Returns the tensor layout of the current Blob object
     */
    Layout layout() const noexcept {
        return tensorDesc.getLayout();
    }

    /**
     * @brief Creates a TBlob<> object from a Data node
     * @param Data reference to a smart pointer of the Data node
     * @return Smart pointer to TBlob<> with the relevant C type to the precision of the data node
     */
    static Ptr CreateFromData(const DataPtr &data);

    /**
     * @brief Blob virtual destructor
     */
    virtual ~Blob()  = default;

    /**
     * @brief Constructor. Creates an empty Blob object with the specified precision.
     * @param tensorDesc Defines the layout and dims of the blob
     */
    explicit Blob(TensorDesc tensorDesc): tensorDesc(tensorDesc) {}

    /**
     * @deprecated Please use TensorDesc for Blob initialization
     * @brief Constructor. Creates an empty Blob object with the specified precision.
     * @param p Precision type
     */
    explicit Blob(Precision p) : Blob(p, NCHW) {}

    /**
     * @deprecated Please use TensorDesc for Blob initialization
     * @brief Constructor. Creates an empty Blob object with the specified precision and layout.
     * @param p Precision type
     * @param l Layout
     */
    Blob(Precision p, Layout l) : tensorDesc(p, l) {}

    /**
     * @deprecated Please use TensorDesc for Blob initialization
     * @brief Constructor. Creates an empty Blob object with the specified precision and dimensions.
     * @param p Tensor precision type
     * @param dims Tensor dimensions vector
     */
    Blob(Precision p, const SizeVector &dims)
            : Blob(p, TensorDesc::getLayoutByDims(dims), dims) {}

    /**
     * @deprecated Please use TensorDesc for Blob initialization
     * @brief Constructor. Creates an empty Blob object with the specified precision, layout and dimensions.
     * @param p tensor precision type
     * @param l tensor layout
     * @param dims Tensor dimensions vector with reversed order
     */
    Blob(Precision p, Layout l, const SizeVector &dims)
            : tensorDesc(p, SizeVector(dims.rbegin(), dims.rend()), l) {}

    /**
     * @depricated It works with reversed dimensions, please create a new blob if you want to change a size
     * @brief Changes Tensor size to the specified dimensions. If it was allocated, the previous data is deallocated and lost.
     * @param dims New dimensions to set
     * @param layout New layout to set
     * @return Total number of elements (a product of all the dimensions)
     */
    size_t Resize(const SizeVector &dims, Layout layout = Layout::ANY) {
        bool bret = deallocate();

        if (layout != Layout::ANY) {
            tensorDesc = TensorDesc(tensorDesc.getPrecision(), SizeVector(dims.rbegin(), dims.rend()), layout);
        } else {
            tensorDesc.setDims(SizeVector(dims.rbegin(), dims.rend()));
        }
        if (!bret) {
            allocate();
        }
        return product(tensorDesc.getDims());
    }

    /**
     * @deprecated It works with reversed dimensions, please use TensorDescriptor.reshape()
     * @brief Changes tensor size to the specified dimensions without changing memory: the total size remains unchanged as well as memory layout.
     * @param dims New dimensions to set
     * @param layout New layout to set
     * @return The total number of elements (a product of all the dims)
     */
    size_t Reshape(const SizeVector &dims, Layout layout = Layout::ANY) {
        if (product(tensorDesc.getDims()) != product(dims)) {
            THROW_IE_EXCEPTION << "cannot reshape when total size changes";
        }

        if (layout != Layout::ANY) {
            tensorDesc = TensorDesc(tensorDesc.getPrecision(), SizeVector(dims.rbegin(), dims.rend()), layout);
        } else {
            tensorDesc.setDims(SizeVector(dims.rbegin(), dims.rend()));
        }
        return product(tensorDesc.getDims());
    }

    /**
     * @deprecated Please use TensorDesc for working with dimensions
     * @brief Returns the tensor dimensions vector with reversed order
     */
    const SizeVector dims() const {
        return SizeVector(tensorDesc.getDims().rbegin(), tensorDesc.getDims().rend());
    }

#ifdef AKS
    /**
     * @brief Returns the tensor desctiption
     */
    TensorDesc &getTensorDesc() {   // how else would I do reshape() ??
        return tensorDesc;
    }
#endif
    /**
     * @brief Returns the tensor description
     */
    const TensorDesc &getTensorDesc() const {
        return tensorDesc;
    }

    /**
     * @brief Returns the total number of elements (a product of all the dims)
     */
    size_t size() const {
        return product(tensorDesc.getDims());
    }

    /**
     * @brief Returns size of the current Blob in bytes
     */
    size_t byteSize() const {
        return product(tensorDesc.getDims()) * element_size();
    }

    /**
     * @brief Returns number of bytes per element. The overall Blob capacity is size() * element_size().
     * Abstract method.
     */
    virtual size_t element_size() const noexcept = 0;

    /**
     * @brief Allocates memory to store the data.
     * Abstract method.
     */
    virtual void allocate() = 0;

    /**
     * @brief Releases previously allocated data.
     * Abstract method.
     */
    virtual bool deallocate() = 0;

    /**
     * @brief Gets access to the allocated memory.
     * Abstract method.
     * @return A LockedMemory object
     */
    virtual LockedMemory<void> buffer() = 0;

    /**
     * @brief Gets read-only access to the allocated memory.
     * Abstract method.
     * @return A LockedMemory object
     */
    virtual LockedMemory<const void> cbuffer() const = 0;

protected:
    /**
     * @brief Precision type of the given blob
     */
    TensorDesc tensorDesc;

    /**
     * @brief Multiplies dimension vector values
     * @param dims Reference to a vector with dimension values of type size_t
     * @return Result of multiplication
     */
    static size_t product(const SizeVector &dims) {
        if (dims.empty())
            return 0;
        return std::accumulate(std::begin(dims), std::end(dims), (size_t) 1, std::multiplies<size_t>());
    }

    /**
    * @brief Gets the allocator for allocator-based blobs
    * @return The allocator for allocator-based blobs, and nullptr otherwise
    */
    virtual std::shared_ptr<IAllocator> &getAllocator() const noexcept  = 0;

    /**
    * @brief Gets the handle to allocated memory
    * @return The handle to allocated memory for allocator-based blobs, and nullptr otherwise
    */
    virtual void *getHandle() const noexcept  = 0;

    template<typename> friend
    class TBlobProxy;
};

/**
 * @brief This is a convenient type for working with a map containing pairs: (string, pointer to a Blob instance)
 */
typedef std::map<std::string, Blob::Ptr> BlobMap;

/**
 * @class TBlob
 * @brief Represents real host memory allocated of a Tensor/Blob per C type.
 */
template<typename T,
        typename = std::enable_if<std::is_pod<T>::value>>
class TBlob : public Blob {
    template<typename, typename> friend
    class TBlob;


public:
    /**
     * @brief Smart Pointer to this TBlob object
     */
    typedef std::shared_ptr<TBlob<T>> Ptr;

    /**
     * @brief Creates a TBlob object with the specified dimensions and layout.
     * Does not allocate the memory. To allocate memory, use the allocate() method.
     * @param tensorDesc Tensor description
     */
    explicit TBlob(const TensorDesc& tensorDesc): Blob(tensorDesc) {}

    /**
     * @brief Constructor. Creates a TBlob object with the specified dimensions and layout on the pre-allocated memory, allocate() call is not required.
     * @param tensorDesc Tensor description
     * @param ptr Pointer to the pre-allocated memory
     * @param data_size Length of the pre-allocated array. If not set, size is assumed equal to dot product of dims.
     */
    TBlob(const TensorDesc& tensorDesc, T* ptr, size_t data_sze = 0): Blob(tensorDesc) {
        if (size() != 0 && ptr == nullptr) {
            THROW_IE_EXCEPTION << "Using Blob on external nullptr memory";
        }
        _allocator = details::make_pre_allocator(ptr, size());
        // blob on attached memory is always allocated, so we are not forcing user to call allocate
        allocate();
    }

    /**
     * @deprecated Please use TensorDesc for Blob initialization
     * @brief Default empty constructor
     */
    explicit TBlob(Precision p, Layout l) : Blob(p, l) {}

    /**
     * @deprecated Please use TensorDesc for Blob initialization
     * @brief Creates a TBlob object with the specified dimensions.
     * Does not allocate the memory. To allocate memory, use the allocate() method.
     * @param p Precision
     * @param l Layout
     * @param dims Tensor dimensions
     */
    TBlob(Precision p, Layout l, const SizeVector& dims)
       : Blob(p, l, dims) {
    }

    /**
     * @deprecated Please use TensorDesc for Blob initialization
     * @brief Constructor. Creates a TBlob object with the specified dimensions on the pre-allocated memory, allocate() call is not required.
     * @details TBlob does not own memory pointed to by ptr, thus it does not free it after the TBlob object is destroyed.
     * Also certain operations might fail:
     *  After blob is constructed with this pointer, its size is stored in the TBlob instance.
     *  If the resize() operation happens, that requires more memory, then call to allocate() fails.
     * @param p Precision
     * @param dims Tensor dimensions
     * @param ptr Pointer to the pre-allocated memory
     * @param data_size Length of the pre-allocated array. If not set, size is assumed equal to dot product of dims.
     */
    TBlob(Precision p, Layout l, const SizeVector& dims, T* ptr, size_t data_sze = 0) : Blob(p, l, dims) {
        if (size() != 0 && ptr == nullptr) {
            THROW_IE_EXCEPTION << "Using Blob on external nullptr memory";
        }
        _allocator = details::make_pre_allocator(ptr, size());
        // blob on attached memory is always allocated, so we are not forcing user to call allocate
        allocate();
    }

    /**
     * @deprecated Please use TensorDesc for Blob initialization
     * @brief Constructor. Creates a TBlob object with the specified precision, layout, dimensions and custom memory allocator.
     * @param p Precision
     * @param l Layout
     * @param dims Tensor dimensions
     * @param alloc Allocator to be used
     */
    TBlob(Precision p, Layout l, const SizeVector &dims, std::shared_ptr<IAllocator> alloc)
            : Blob(p, l, dims), _allocator(alloc) {
    }

    /**
     * @brief Copy constructor, data is reallocated and copied from the source to the target blob
     * @param blob Source blob
     */
    TBlob(const TBlob<T> &blob) : Blob(blob.getTensorDesc()) {
        copyFrom(blob);
    }

    /**
     * A move constructor
     * @param blob rvalue to make a move from
     */
    TBlob(TBlob<T> &&blob) : Blob(blob.getTensorDesc()) {
        moveFrom(blob);
    }

    /**
     * Copy operator for the TBlob object
     * @param blob Object reference to copy from
     * @return Newly copied object
     */
    TBlob &operator=(const TBlob &blob) {
        copyFrom(blob);
        return *this;
    }

    /**
     * Virtual destructor.
     */
    virtual ~TBlob() {
        free();
    }

    /**
     * @brief Gets the size of the given type
     * @return Size of the type
     */
    size_t element_size() const noexcept override {
        return sizeof(T);
    }

    /**
     * @brief Creates an new empty rvalue LockedMemory object
     * @return rvalue for the empty locked object of type T
     */
    virtual LockedMemory<T> data() {
        return std::move(lockme<T>());
    }

    /**
     * @brief Creates an new empty rvalue read-only LockedMemory object.
     * @return rvalue for the empty locked const object of type T.
     */
    virtual LockedMemory<const T> readOnly() const {
        return std::move(lockme<const T>());
    }

    /**
      * @deprecated Deprecated to avoid memcpy() calls
      * @brief Copies data from the given vector to the blob
      * @param that Vector of values to copy to the blob
      */
    void set(const std::vector<T> &that) {
        if (tensorDesc.getDims().size() != 0 && that.size() != product(tensorDesc.getDims()))
            THROW_IE_EXCEPTION << "Size mismatch between dims and vector";
        if (tensorDesc.getDims().size() == 0) {
            tensorDesc.setDims({static_cast<unsigned int>(that.size())});
        }
        allocate();
        auto memptr = data();
        memcpy(memptr, that.data(), product(tensorDesc.getDims()) * sizeof(T));
    }

    /**
     * @brief Allocates or reallocates memory
     */
    void allocate() override {
        if (_handle != nullptr) {
            getAllocator()->free(_handle);
        }
        _handle = getAllocator()->alloc(TBlob<T>::product(tensorDesc.getDims()) * sizeof(T));
    }

    /**
     * @brief Frees all allocated data
     */
    bool deallocate() override {
        return free();
    }

    /**
     * @brief Creates an new empty rvalue LockedMemory instance of type void
     * @return rvalue for the empty locked object of type void
     */
    LockedMemory<void> buffer() override {
        return std::move(lockme<void>());
    }

    /**
     * @brief Creates an new empty rvalue LockedMemory instance of type const void
     * @return rvalue for the empty locked object of type const void
     */
    LockedMemory<const void> cbuffer() const override {
        return std::move(lockme<const void>());
    }

    /**
     * @brief Gets BlobIterator for the data.
     * Enables a ranged loop support for this TBlob object.
     * @return BlobIterator object of type T
     */
    details::BlobIterator<T> begin() {
        return details::BlobIterator<T>(data());
    }

    /**
     * @brief Gets BlobIterator for the end of data.
     * Enables a ranged loop support for this TBlob object.
     * @return BlobIterator object of type T
     */
    details::BlobIterator<T> end() {
        return details::BlobIterator<T>(data(), size());
    }

    /**
     * @brief Gets a const BlobIterator for the read-only data.
     * Enables a ranged loop support for this TBlob object.
     * @return BlobIterator object of type const T
     */
    details::BlobIterator<const T> begin() const {
        return details::BlobIterator<const T>(readOnly());
    }

    /**
    * @brief Gets a const BlobIterator for the end of read-only data.
    * Enables a ranged loop support for this TBlob object.
    * @return BlobIterator object of type const T
    */
    details::BlobIterator<const T> end() const {
        return details::BlobIterator<const T>(readOnly(), size());
    }


protected:
    /**
     * @brief Local instance of IAllocator to manipulate memory
     */
    mutable std::shared_ptr<IAllocator> _allocator;

    /**
     * @brief A handle for the stored memory
     */
    void *_handle = nullptr;

    /**
     * @brief Copies dimensions and data from the TBlob object
     * @param blob Object reference to copy from
     */
    void copyFrom(const TBlob<T> &blob) {
        tensorDesc = blob.tensorDesc;
        this->allocate();
        auto memptr = data();
        memcpy(memptr, blob.readOnly(), product(tensorDesc.getDims()) * sizeof(T));
    }

    /**
     * @brief Swaps memory handlers between the current blob and the given one
     * @tparam U Type of the blob to move from
     * @param blob TBlob instance to move from
     */
    template<class U>
    void moveFrom(TBlob<U> &blob) {
        tensorDesc = blob.tensorDesc;
        this->_allocator = std::move(blob._allocator);
        std::swap(this->_handle, blob._handle);
    }

    /**
     * @brief Frees handler and cleans the stored data up
     */
    virtual bool free() {
        bool bCanRelease = true;
        if (_handle == nullptr) return bCanRelease;

        bCanRelease = getAllocator()->free(_handle);
        _handle = nullptr;
        return bCanRelease;
    }

    /**
     * @brief Creates a LockedMemory instance
     * @tparam S Type of the LockedMemory to be created
     * @return A created instance of LockedMemory
     */
    template<class S>
    LockedMemory<S> lockme() const {
        return LockedMemory<S>(_allocator.get(), _handle, 0);
    }

    /**
     * @brief Gets an allocator or create default one
     * @return IAllocator instance
     */
    std::shared_ptr<IAllocator> &getAllocator() const noexcept override {
        // in case when constructor without allocator was used
        if (!_allocator) {
            _allocator = shared_from_irelease(CreateDefaultAllocator());
        }

        return _allocator;
    }

    /**
     * @brief Returns handle to the stored data
     */
    void *getHandle() const noexcept override {
        return _handle;
    }
};

/**
 * @deprecated Use TensorDesc to create Blob::Ptr
 * @brief Creates a blob with given precision and dimensions
 * @tparam Type Type of the shared pointer to be created
 * @param p Given precision
 * @param dims Given dimensions
 * @return A shared pointer to the created blob
 */
template<class Type>
inline typename TBlob<Type>::Ptr make_shared_blob(Precision p, Layout l, const SizeVector &dims) {
    return std::make_shared<TBlob<Type>>(p, l, dims);
}

/**
 * @deprecated Use TensorDesc to create Blob::Ptr
 * @brief Creates a blob with NCHW layout given precision and dimensions
 * @tparam Type Type of the shared pointer to be created
 * @param p Given precision
 * @param dims Given dimensions
 * @return A shared pointer to the created blob
 */
template<class Type>
inline typename TBlob<Type>::Ptr make_shared_blob(Precision p, const SizeVector &dims) {
    return make_shared_blob<Type>(p, TensorDesc::getLayoutByDims(dims), dims);
}

/**
 * @deprecated Use TensorDesc to create Blob::Ptr
 * @brief Creates a blob with the given precision
 * @tparam Type Type of the shared pointer to be created
 * @param p Given precision
 * @param arg Shared pointer to IAllocator to use in the blob
 * @return A shared pointer to the blob created
 */
template<typename Type, class TArg>
inline typename InferenceEngine::TBlob<Type>::Ptr make_shared_blob(Precision p, Layout l, const TArg &arg) {
    return std::make_shared<InferenceEngine::TBlob<Type>>(p, l, arg);
}

/**
 * @deprecated Use TensorDesc in order to create Blob::Ptr
 * @brief Creates a blob with the NCHW layout and given tensor precision
 * @tparam Type Type of the shared pointer to be created
 * @param p Given precision
 * @param arg Shared pointer to IAllocator to use in the blob
 * @return A shared pointer to the blob created
 */
template<typename Type, class TArg>
inline typename InferenceEngine::TBlob<Type>::Ptr make_shared_blob(Precision p, const TArg &arg) {
    return make_shared_blob<Type, TArg>(p, TensorDesc::getLayoutByDims(arg), arg);
}

/**
 * @brief Creates a blob with the given tensor descriptor
 * @tparam Type Type of the shared pointer to be created
 * @param tensorDesc Tensor descripttor for Blob creation
 * @return A shared pointer to the newly created blob of the given type
 */
template<typename Type>
inline typename InferenceEngine::TBlob<Type>::Ptr make_shared_blob(const TensorDesc& tensorDesc) {
    return std::make_shared<InferenceEngine::TBlob<Type>>(tensorDesc);
}

/**
 * @brief Creates a blob with the given tensor descriptor from the pointer to the pre-allocated memory
 * @tparam Type Type of the shared pointer to be created
 * @param tensorDesc TensorDesc for Blob creation
 * @param ptr Pointer to the pre-allocated memory
 * @param size Length of the pre-allocated array
 * @return A shared pointer to the newly created blob of the given type
 */
template<typename Type>
inline typename InferenceEngine::TBlob<Type>::Ptr make_shared_blob(const TensorDesc& tensorDesc, Type * ptr, size_t size = 0) {
    return std::make_shared<InferenceEngine::TBlob<Type>>(tensorDesc, ptr, size);
}

/**
 * @deprecated Use TensorDesc in order to create Blob::Ptr
 * @brief Gets a shared pointer for the new TBlob instance.
 * Created instance is based on move semantics from the given TBlob instance.
 * @tparam TypeTo Type of the shared pointer to be created
 * @param arg rvalue for the blob to move from
 * @return A shared pointer to the newly created blob of the given type
 */
template<typename TypeTo>
inline typename InferenceEngine::TBlob<TypeTo>::Ptr make_shared_blob(TBlob<TypeTo> &&arg) {
    return std::make_shared<InferenceEngine::TBlob<TypeTo>>(std::move(arg));
}

/**
 * @deprecated Use TensorDesc in order to create Blob::Ptr
 * @brief Creates a blob with the given precision
 * @tparam Type Type of the shared pointer to be created
 * @param p Given precision
 * @return A shared pointer to the blob created
 */
template<typename Type>
inline typename InferenceEngine::TBlob<Type>::Ptr make_shared_blob(Precision p, Layout l = NCHW) {
    return std::make_shared<TBlob<Type>>(p, l);
}

/**
 * @deprecated Use TensorDesc in order to create Blob::Ptr
 * @brief Creates a blob with the given precision from the vector of values
 * @tparam TypeTo Type of the shared pointer to be created
 * @param p Given precision
 * @param l Layout
 * @param arg Vector of values
 * @return A shared pointer to the blob created
 */
template<typename TypeTo>
inline typename TBlob<TypeTo>::Ptr make_shared_blob(Precision p, Layout l, const std::vector<TypeTo> &arg) {
    auto blob = std::make_shared<TBlob<TypeTo>>(p, l);
    blob->set(arg);
    return blob;
}

/**
 * @deprecated Use TensorDesc in order to create Blob::Ptr
 * @brief Creates a blob with NCHW layout and given precision from the vector of values
 * @tparam TypeTo Type of the shared pointer to be created
 * @param p Given precision
 * @param arg Vector of values
 * @return A shared pointer to the blob created
 */
template<typename TypeTo>
inline typename TBlob<TypeTo>::Ptr make_shared_blob(Precision p, const std::vector<TypeTo> &arg) {
    return make_shared_blob<TypeTo>(p, TensorDesc::getLayoutByDims(arg), arg);
}

/**
 * @deprecated Use TensorDesc in order to create Blob::Ptr
 * @brief Creates a blob with the given precision from the pointer to the pre-allocated memory
 * @param p Given precision
 * @param l Layout
 * @param dims Given dimensions
 * @param ptr Pointer to the pre-allocated memory
 * @param size Length of the pre-allocated array
 * @return A shared pointer to the blob created
 */
template <typename TypeTo>
inline typename TBlob<TypeTo>::Ptr make_shared_blob(Precision p, Layout l, const SizeVector &dims, TypeTo * ptr, size_t size = 0) {
    auto blob = std::make_shared<TBlob<TypeTo>>(p, l, dims, ptr, size);
    return blob;
}

/**
 * @deprecated Use TensorDesc in order to create Blob::Ptr
 * @brief Creates a blob with NCHW layout and given precision from the pointer to the pre-allocated memory
 * @param p Given precision
 * @param dims Given dimensions
 * @param ptr Pointer to the pre-allocated memory
 * @param size Length of the pre-allocated array
 * @return A shared pointer to the blob created
 */
template <typename TypeTo>
inline typename TBlob<TypeTo>::Ptr make_shared_blob(Precision p, const SizeVector &dims, TypeTo * ptr, size_t size = 0) {
    return make_shared_blob<TypeTo>(p, TensorDesc::getLayoutByDims(dims), dims, ptr, size);
}

}  // namespace InferenceEngine
