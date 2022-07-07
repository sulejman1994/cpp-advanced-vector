#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <iostream>
#include <algorithm>
#include <type_traits>

template <typename T>
class RawMemory {
public:
    
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }
    
    RawMemory(const RawMemory&) = delete;
    RawMemory& operator = (const RawMemory&) = delete;
    
    RawMemory(RawMemory&& other) {
        buffer_ = std::move(other.buffer_);
        capacity_ = other.capacity_;
    }
    
    RawMemory& operator = (RawMemory&& other) {
        if (this == &other) {
            return *this;
        }
        buffer_ = std::move(other.buffer_);
        capacity_ = other.capacity_;
        return *this;
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        if (buf) {
            operator delete(buf);
        }
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;
    
    Vector() = default;
    
    explicit Vector(size_t size) : data_(size), size_(size) {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }
    
    Vector(const Vector& other)
        : data_(other.size_), size_(other.size_) {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }
    
    Vector(Vector&& other) noexcept {
        if (this == &other) {
            return;
        }
        Swap(other);
    }
    
    Vector& operator = (const Vector& other) {
        if (this == &other) {
            return *this;
        }
        if (other.size_ < size_) {
            std::copy(other.data_.GetAddress(), other.data_.GetAddress() + other.size_, data_.GetAddress());
            std::destroy_n(data_.GetAddress() + other.size_, size_ - other.size_);
            size_ = other.size_;
            return *this;
        }
        if (other.size_ <= Capacity()) {
            std::copy(other.data_.GetAddress(), other.data_.GetAddress() + size_, data_.GetAddress());
            std::uninitialized_copy_n(other.data_.GetAddress() + size_, other.size_ - size_, data_.GetAddress() + size_);
            size_ = other.size_;
            return *this;
        }
        Vector tmp(other);
        Swap(tmp);
        return *this;
    }
    
    Vector& operator = (Vector&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        Swap(other);
        return *this;
    }
    
    ~Vector() noexcept {
        std::destroy_n(data_.GetAddress(), size_);
    }
    
    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !(std::is_copy_constructible_v<T>)) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        } else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }
    
    void PushBack(const T& value) {
        EmplaceBack(value);
    }
    
    void PushBack(T&& value) {
        EmplaceBack(std::move(value));
    }
    
    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        if (size_ < Capacity()) {
            T* ptr_new = new (data_ + size_) T(std::forward<Args> (args)...);
            ++size_;
            return *ptr_new;
        }
        RawMemory<T> tmp(size_ == 0 ? 1 : 2 * size_);
        T* ptr_new = new (tmp + size_) T(std::forward<Args> (args)...);
        try {
            if constexpr (std::is_nothrow_move_constructible_v<T> || !(std::is_copy_constructible_v<T>)) {
                std::uninitialized_move_n(data_.GetAddress(), size_, tmp.GetAddress());
            } else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, tmp.GetAddress());
            }
        } catch (...) {
                std::destroy_at(ptr_new);
        }
        data_.Swap(tmp);
        ++size_;
        std::destroy_n(tmp.GetAddress(), size_ - 1);
        return *ptr_new;
    }
    
    
    void PopBack() {
        assert(size_ != 0);
        (*this)[size_ - 1].~T();
        --size_;
    }
    
   /* iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }
    
    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }*/
    
    template <typename Type>
    iterator Insert(const_iterator pos, Type&& value) {
        return Emplace(pos, std::forward<Type> (value));
    }
    
    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        
        size_t index = std::distance(cbegin(), pos);
        
        if (index == size_) {
            EmplaceBack(std::forward<Args> (args)...);
            return end() - 1;
        }
        
        if (size_ < Capacity()) {
            T value = T(std::forward<Args> (args)...);
            new (end()) T(std::forward<T> (*(end() - 1)));
            std::move_backward(begin() + index, end() - 1, end());
            data_[index] = std::move(value);
            ++size_;
            return begin() + index;
        }
        
        RawMemory<T> tmp(size_ == 0 ? 1 : 2 * size_);
        T* ptr_new = new (tmp + index) T(std::forward<Args> (args)...);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !(std::is_copy_constructible_v<T>)) {
            try {
                std::uninitialized_move(begin(), begin() + index, tmp.GetAddress());
            } catch (...) {
                std::destroy_at(ptr_new);
                throw;
            }
            try {
                std::uninitialized_move(begin() + index, end(), tmp.GetAddress() + index + 1);
            } catch (...) {
                std::destroy_n(tmp.GetAddress(), index + 1);
                throw;
            }
            
        } else {
            try {
                std::uninitialized_copy(begin(), begin() + index, tmp.GetAddress());
            } catch (...) {
                std::destroy_at(ptr_new);
                throw;
            }
            try {
                std::uninitialized_copy(begin() + index, end(), tmp.GetAddress() + index + 1);
            } catch (...) {
                std::destroy_n(tmp.GetAddress(), index + 1);
                throw;
            }
        }
        ++size_;
        data_.Swap(tmp);
        std::destroy_n(tmp.GetAddress(), size_ - 1);
        return begin() + index;
    }
    
    iterator Erase(const_iterator pos) {
        size_t index = std::distance(cbegin(), pos);
        pos->~T();
        if constexpr (std::is_nothrow_move_constructible_v<T> || !(std::is_copy_constructible_v<T>)) {
            std::move(begin() + index + 1, end(), begin() + index);
        } else {
            std::copy(begin() + index + 1, end(), begin() + index);
        }
        --size_;
        return begin() + index;
    }
    
    void Resize(size_t new_size) {
        if (new_size <= size_) {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
            size_ = new_size;
            return;
        }
        if (new_size <= Capacity()) {
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
            size_ = new_size;
            return;
        }
        Reserve(new_size);
        std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        size_ = new_size;
    }
    
    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }
    
    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }
    
    iterator begin() noexcept {
        return data_.GetAddress();
    }
    iterator end() noexcept {
        return data_ + size_;
    }
    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator end() const noexcept {
        return data_ + size_;
    }
    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator cend() const noexcept {
        return data_ + size_;
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;
};
