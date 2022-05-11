#pragma once

#include <cassert>
#include <cstdlib>
#include <new>
#include <memory>
#include <utility>
#include <algorithm>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
            : buffer_(Allocate(capacity))
            , capacity_(capacity) {
    }
    
    RawMemory(RawMemory&& other) noexcept {
        this->Swap(other);
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
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
    
    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }
    
    RawMemory& operator=(const RawMemory& rhs) = delete;

    RawMemory& operator=(RawMemory&& rhs) noexcept
    {
        this->Swap(rhs);
        return *this;
    }
    
    T* operator+(size_t offset) noexcept {
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }
    
private:
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }
    
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }
    
    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};


template <typename T>
class Vector
{
public:
    using iterator = T*;
    using const_iterator = const T*;

    Vector() = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size) {
            std::uninitialized_value_construct_n(data_.GetAddress(), size);
        }

    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_) {
            std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
        }

    Vector(Vector&& other) noexcept {
        this->Swap(other);
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }

        RawMemory<T> new_data(new_capacity);

        MoveOrCopy(data_.GetAddress(), size_, new_data.GetAddress());

        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
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
    
    Vector& operator=(const Vector& other) {
        if (other.size_ > data_.Capacity()) {
            Vector temp(other);
            Swap(temp);
            
        } else {
            std::copy_n(other.data_.GetAddress(), std::min(size_, other.Size()), data_.GetAddress());
            if (size_ > other.Size()) {
                std::destroy_n(data_ + other.Size(), size_ - other.Size());
            } else if (size_ < other.Size()) {
                std::uninitialized_copy_n(other.data_.GetAddress() + size_, other.Size() - size_, data_.GetAddress() + size_);
            }
        }

        size_ = other.Size();

        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept {
        Swap(rhs);
        return *this;
    }
    
    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }
    
    void Resize(size_t new_size) {
        if (new_size > size_) {
            if (new_size > data_.Capacity()) {
                Reserve(new_size);
            }
            std::uninitialized_default_construct_n(data_ + size_, new_size - size_);
            size_ = new_size;
        }
        else {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
            size_ = new_size;
        }
    }

    void PushBack(const T& value) {
        EmplaceBack(value);
    }
       
    void PushBack(T&& value) {
        EmplaceBack(std::move(value));
    }
    
    void PopBack() noexcept {
        assert(size_ > 0);
        --size_;
        std::destroy_at(data_ + size_);
    }
    
    template <typename... Types>
    T& EmplaceBack(Types&&... args) {
        if (size_ == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new (new_data + size_) T(std::forward<Types>(args)...);
            MoveOrCopy(data_.GetAddress(), size_, new_data.GetAddress());
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
            size_++;
            return data_[size_ - 1];
        }
        new (data_ + size_) T(std::forward<Types>(args)...);
        size_++;
        return data_[size_ - 1];
    }

    
    iterator begin() noexcept {
        return data_.GetAddress(); 
    }

    iterator end() noexcept { 
        return data_.GetAddress() + size_; 
    }

    const_iterator begin() const noexcept {
        return data_.GetAddress(); 
    }

    const_iterator end() const noexcept {
        return data_.GetAddress() + size_; 
    }

    const_iterator cbegin() const noexcept { 
        return data_.GetAddress(); 
    }

    const_iterator cend() const noexcept {
        return data_.GetAddress() + size_; 
    }
    
    template <typename... Args>
	iterator Emplace(const_iterator pos, Args&&... args) {
		const size_t dist = pos - begin();
        RawMemory<T> new_data((size_ == 0) ? 1 : (size_ * 2));
        
		if (size_ == data_.Capacity()) {
			new(new_data + dist) T(std::forward<Args>(args)...);
            MoveOrCopy(begin(), dist, new_data.GetAddress());
            MoveOrCopy(begin() + dist, size_ - dist, new_data.GetAddress() + dist + 1);
			std::destroy_n(data_.GetAddress(), size_);
			data_.Swap(new_data);
			++size_;
        } else {
            iterator it = &data_[dist];

            if (pos == nullptr || pos == end()) {
                new (end()) T(std::forward<Args>(args)...);
            } else {
                new (end()) T(std::forward<T>(*(end() - 1)));
                std::move_backward(it, end() - 1, end());
                *it = T(std::forward<Args>(args)...);
            }

            ++size_;

            return it;
        }
        return begin() + dist;
	}
    
    iterator Erase(const_iterator p) {
        iterator pos = const_cast<iterator>(p);
        
        if (begin() == end()) {
            return pos;
        }
        
        std::move(pos + 1, end(), pos);
        PopBack();
        return pos;
    }
    
    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }
    
    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }
    
private:

    RawMemory<T> data_;
    size_t size_ = 0;
    
    void MoveOrCopy(T* data, size_t size, T* new_data) {
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data, size, new_data);
        } else {
            std::uninitialized_copy_n(data, size, new_data);
        }
    }
};