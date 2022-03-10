//cringe container should be removed probably and replaced with vector+swap and pop

#pragma once

#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <stdexcept>
#include <iterator>
#include <type_traits>
#include <limits>
#include <utility>
#include <new>

//Guarantees: contiguous storage, automatic resize
//Does not guarantee: order of elements
//On erase places last element in place of erased one
template <typename T>
class DynamicPool
{
	T* _mem;
	size_t _size;
	size_t _capacity;
	size_t _resize_len;

	T* alloc(size_t size)
	{
		size_t bytes = size * sizeof(T);
		if (size != 0 && bytes / size != sizeof(T))
		{
			throw std::overflow_error(
				"Allocation failed due to integer multiplication overflow."
			);
		}
		if(void* mem = std::malloc(bytes))
			return static_cast<T*>(mem);
		else
			throw std::bad_alloc();
	};

	T* realloc(T* block, size_t size)
	{
		size_t bytes = size * sizeof(T);
		if (size != 0 && bytes / size != sizeof(T))
		{
			throw std::overflow_error(
				"Reallocation failed due to integer multiplication overflow."
			);
		}
		if(void* mem = std::realloc(block, bytes))
			return static_cast<T*>(mem);
		else
			throw std::bad_alloc();
	};
	
public:
	DynamicPool(size_t capacity = 256, size_t resize_len = 256)
	{
		_mem = alloc(capacity);
		this->_size = 0;
		this->_capacity = capacity;
		this->_resize_len = resize_len;
	}

	DynamicPool(const DynamicPool& other)
	{
		_size = other._size;
		_capacity = other._capacity;
		_resize_len = other._resize_len;
		_mem = alloc(_capacity);
		if constexpr(std::is_trivially_copyable<T>::value)
		{
			std::memcpy(_mem, other._mem, other._size*sizeof(T));
		}
		else
		{
			iterator _mem_iter = begin();
			iterator _mem_end = end();
			iterator _mem_other_iter = other.begin();
			while(_mem_iter != _mem_end)
			{
				new (&(*_mem_iter)) T(*_mem_other_iter);
				++_mem_iter;
				++_mem_other_iter;
			}
		}
	}

	DynamicPool& operator=(const DynamicPool& other)
	{
		//prevent self-assignment
		if (this == &other) return *this;

		//check if capacity allows us to reuse existing memory
		if(this->_capacity >= other._capacity)
		{
			_size = other._size;
			_resize_len = other._resize_len;

			if constexpr(std::is_trivially_copyable<T>::value)
			{
				std::memcpy(_mem, other._mem, other._size*sizeof(T));
			}
			else
			{
				iterator _mem_iter = begin();
				iterator _mem_end = end();
				iterator _mem_other_iter = other.begin();
				while(_mem_iter != _mem_end)
				{
					new (&(*_mem_iter)) T(*_mem_other_iter);
					++_mem_iter;
					++_mem_other_iter;
				}
			}
		}
		else
		{
			if constexpr(std::is_trivially_copyable<T>::value)
			{
				_size = other._size;
				_capacity = other._capacity;
				_resize_len = other._resize_len;
				_mem = realloc(_mem, other._capacity);
			}
			else
			{
				for(size_t i = 0; i < _size; ++i)
				{
					_mem[i].~T();
				}
				std::free(_mem);
				_size = other._size;
				_capacity = other._capacity;
				_resize_len = other._resize_len;
				_mem = alloc(other._capacity);
				iterator _mem_iter = begin();
				iterator _mem_end = end();
				iterator _mem_other_iter = other.begin();
				while(_mem_iter != _mem_end)
				{
					new (&(*_mem_iter)) T(*_mem_other_iter);
					++_mem_iter;
					++_mem_other_iter;
				}
			}
		}
		return *this;
	}
	
	~DynamicPool()
	{
		if constexpr(!std::is_trivially_destructible<T>::value)
		{
			//call destructors
			for(size_t i = 0; i < _size; ++i)
			{
				_mem[i].~T();
			}
		}
		//free memory
		std::free(_mem);
	}
	
	class iterator
	{
		T* ptr;

	public:
		using iterator_category = std::random_access_iterator_tag;
		using value_type = iterator;
		using difference_type = std::ptrdiff_t;
		using pointer = iterator*;
		using reference = iterator&;

		iterator(T * ptr): ptr(ptr){}
		iterator operator++() { ++ptr; return *this; }
		iterator operator--() { --ptr; return *this; }
		iterator operator+(const std::ptrdiff_t i) { return ptr+i; }
		iterator operator-(const std::ptrdiff_t i) { return ptr-i; }
		difference_type operator-(const iterator& other) { return ptr-other.ptr; }
		bool operator!=(const iterator & other) const { return ptr != other.ptr; }
		T* operator->() { return ptr; }
		T& operator*() const { return *ptr; }
	};
	
	iterator begin() const { return iterator(&_mem[0]); }
	iterator end() const { return iterator(&_mem[_size]); }

	T& at(size_t index)
	{
		if(index >= _size)
			throw std::out_of_range("Subscript out of range.");
		return _mem[index];
	}
	
	T& operator[](size_t index)
	{
		return _mem[index];
	}

	T& front()
	{
		return _mem[0];
	}

	T& back()
	{
		return _mem[_size-1];
	}

	T* data()
	{
		return _mem;
	}
	
	size_t size()
	{
		return _size;
	}

	size_t max_size()
	{
		constexpr size_t size = std::numeric_limits<size_t>::max();
		return size;
	}

	size_t capacity()
	{
		return _capacity;
	}

	size_t empty()
	{
		return _size == 0;
	}
	
	void resize(size_t size)
	{
		if constexpr(std::is_trivially_copyable<T>::value)
		{
			_mem = realloc(_mem, size);
		}
		else
		{
			//allocate new buffer and relocate one by one
			T* old_mem = _mem;
			_mem = alloc(size);
			for(size_t i = 0; i < _size; ++i)
			{
				new (&_mem[i]) T(std::move(old_mem[i]));
				old_mem[i].~T();
			}
			std::free(old_mem);
		}
		//update capacity
		this->_capacity = size;
	}

	void clear()
	{
		if constexpr(!std::is_trivially_destructible<T>::value)
		{
			//call destructors
			for(size_t i = 0; i < _size; ++i)
			{
				_mem[i].~T();
			}
		}
		//set size to zero
		_size = 0;
	}
	
	void push_back(T& elem)
	{
		//check if size exceeds capacity
		if(_size+1 > _capacity)
		{	
			//check if automatic resize is allowed
			if(_resize_len == -1)
				return;
			
			//increase capacity
			_capacity += _resize_len;
			//resize storage
			resize(_capacity);
		}
		//copy element
		new (_mem + _size) T(elem);
		//increment size
		++_size;
	}
	
	template <typename... Ts>
	void emplace_back(Ts&&... args)
	{
		//check if size exceeds capacity
		if(_size+1 > _capacity)
		{
			//check if automatic resize is allowed
			if(_resize_len == -1)
				return;
			
			//increase capacity
			_capacity += _resize_len;
			//resize storage
			resize(_capacity);
		}
		//construct new element in-place
		new (_mem + _size) T(std::forward<Ts>(args)...);
		//increment size
		++_size;
	}

	void pop_back()
	{
		if(!_size) return;
		erase(end()-1);
	}

	template <typename I>
	void insert(iterator at, I first, I last)
	{
		//store position
		size_t at_pos = std::distance(begin(), at);
		//check position
		if(at_pos > _size)
			throw std::out_of_range("Cannot insert past end iterator.");
		size_t dist = std::distance(first, last);
		//check if size exceeds capacity
		if(_size+dist > _capacity)
		{
			//check if automatic resize is allowed
			if(_resize_len == -1)
				return;

			//increase capacity
			_capacity = ((_size+dist+_resize_len-1)/_resize_len)*_resize_len;

			//resize storage
			resize(_capacity);
			//restore iterator from position
			at = begin()+at_pos;
		}
		//move elements out of the way by placing them at the end
		iterator at_end = at + dist;
		iterator it_dest =
			(std::distance(begin(), end()) > std::distance(begin(), at_end))?
			end():at_end;
		if constexpr(std::is_trivially_copyable<T>::value)
		{
			std::memcpy(
				&(*(it_dest)),
				&(*at),
				std::min(
					std::distance(at, end()),
					std::distance(at, at_end)
				)*sizeof(T)
			);
		}
		else
		{
			iterator it_end =
				(std::distance(begin(), end()) < std::distance(begin(), at_end))?
				end():at_end;
			for(auto it = at; it != it_end; ++it, ++it_dest)
			{
				new (&(*it_dest)) T(std::move(*it));
				(*it).~T();
			}
		}
		//copy new elements
		while(at != at_end)
		{
			new (&(*at)) T(*first);
			++at, ++first;
		}
		_size += dist;
	}

	template <typename... Ts>
	void emplace(iterator at, Ts&&... args)
	{
		//store position
		size_t at_pos = std::distance(begin(), at);
		//check position
		if(at_pos > _size)
			throw std::out_of_range("Cannot emplace past end iterator.");
		//check if size exceeds capacity
		if(_size+1 > _capacity)
		{
			//check if automatic resize is allowed
			if(_resize_len == -1)
				return;

			//increase capacity
			++_capacity;

			//resize storage
			resize(_capacity);
			//restore iterator from position
			at = begin()+at_pos;
		}
		//move element at target position to the end and
		//construct new one in its place
		if constexpr(std::is_trivially_copyable<T>::value)
		{
			std::memcpy(&(*(end())), &(*at), sizeof(T));
			new (&(*at)) T(std::forward<Ts>(args)...);
		}
		else
		{
			//move "at" element at the end and construct new in place
			new (&_mem[_size]) T(std::move(*at));
			//destruct old
			(*at).~T();
			//construct new
			new (&(*at)) T(std::forward<Ts>(args)...);
		}
		++_size;
	}
	
	void erase(iterator it)
	{
		T& elem = *it;
		--_size;
		//put last element in place of erased one
		if constexpr(std::is_trivially_copyable<T>::value)
			std::memcpy(&elem, &_mem[_size], sizeof(T));
		else
		{
			//check if any elements left after erase
			if (_size > 0)
				elem = std::move(_mem[_size]);
		}
		//call destructor on last element
		if constexpr(!std::is_trivially_destructible<T>::value)
			_mem[_size].~T();
	}

	void swap(DynamicPool& other)
	{
		std::swap(_size, other._size);
		std::swap(_capacity, other._capacity);
		std::swap(_resize_len, other._resize_len);
		std::swap(_mem, other._mem);
	}
};
