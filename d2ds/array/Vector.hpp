#ifndef VECTOR_HPP_D2DS
#define VECTOR_HPP_D2DS

#include <initializer_list>

#include "common/common.hpp"

namespace d2ds {
// show your code

template<typename T, typename Allocator =DefaultAllocator>
class Vector{
public:
    Vector(): _size(0),_capacity(0),_data(nullptr){}
    Vector(unsigned int n) : _size(n),_capacity(n){
        _data = static_cast<T*> (Allocator::allocate(sizeof(T)*_capacity));
        for(int i=0;i<_size;i++)
            new (_data+i) T();
    }
    Vector(std::initializer_list<T> list){
        _size=list.end()-list.begin();
        _capacity=_size;
        _data = static_cast<T*>(Allocator::allocate(sizeof(T)*_capacity));
        for(auto it =list.begin();it!=list.end();it++){
            new (_data+(it - list.begin())) T(*it);
        }
    }
    ~Vector(){
        for(int i=0;i<_size;i++){
            (_data+i)->~T();
        }
        if(_data)
            Allocator::deallocate(_data,sizeof(T)*_capacity);
    }

    Vector(const Vector & other){
        _size=other._size;
        _capacity=other._capacity;
        _data=static_cast<T*>(Allocator::allocate(sizeof(T)*_capacity));
        for(int i=0;i<_size;i++){
            new(_data+i)T(other._data[i]);
        }
    }

    Vector& operator=(const Vector & dsObj){
        D2DS_SELF_ASSIGNMENT_CHECKER;

        this->~Vector();
        _size = dsObj._size;
        _capacity=dsObj._capacity;
        _data = static_cast<T*>(Allocator::allocate(sizeof(T) * _capacity));

        for (unsigned i = 0; i < _size; ++i)
            new(_data+i)T(dsObj._data[i]);

        return *this;
    }

    Vector(Vector&&other){
        _size=other._size;
        _data=other._data;
        _capacity=other._capacity;
        other._size = 0;
        other._capacity = 0;
        other._data = nullptr;
    }

    Vector &operator=(Vector&&other){
        if(&other==this) return *this;
        this->~Vector();
        _size=other._size;
        _data=other._data;
        _capacity=other._capacity;
        other._size = 0;
        other._data = nullptr;
        other._capacity = 0;
        return *this;
    }
    unsigned int size() const{
        return _size;
    }

    bool empty(){
        return _size==0;
    }

    T& operator[](unsigned int i){
        return _data[i];
    }

    const T& operator[](unsigned int i)const{
        return _data[i];
    }

    void push_back(const T& a){
        if(_size+1>_capacity){
            resize(_capacity==0?2:2*_capacity);
        }
        new(_data+_size) T(a);
        _size+=1;
    }

    void pop_back(){
        _size-=1;
        (_data+_size)->~T();
        if(_size<=_capacity/3){
            resize(_capacity/2);
        }
    }

    void resize(unsigned int n){//_size<=n
        T* new_mem=(n==0?nullptr:static_cast<T*>(Allocator::allocate(sizeof(T)*n)));
        for(int i=0;i<_size;i++){
            new (new_mem+i) T(_data[i]);
            (_data+i)->~T();
        }
        if(_data)
            Allocator::deallocate(_data,sizeof(T)*_capacity);

        _data=new_mem;
        _capacity=n;
    }

    unsigned int capacity(){
        return _capacity;
    }

    T* begin(){
        return _data;
    }

    T* end(){
        return _data+_size;
    }

    const T* begin() const{
        return _data;
    }

    const T* end() const{
        return _data+_size;
    }

    template<typename U>
    friend bool operator==(const Vector<U>& v1, const Vector<U>&v2);

    template<typename U>
    friend Vector<U> operator+(const Vector<U>& v1, const Vector<U>&v2);

    template<typename U>
    friend Vector<U> operator-(const Vector<U>& v1, const Vector<U>&v);
private:
    unsigned int _size,_capacity;
    T * _data;
};

template<typename T> 
bool operator==(const Vector<T>& v1, const Vector<T>&v2){
    bool equal = v1._size == v2._size;
    if(equal){
        for(int i=0;i<v1._size;i++){
            if(v1._data[i]!=v2._data[i]){
                equal=false;
                break;
            }
        }
    }
    return equal;
}
template<typename T>
Vector<T> operator+(const Vector<T>& v1, const Vector<T>&v2){
    Vector<T> v(v1._size);
    for(int i=0;i<v1._size;i++){
        v._data[i]=v1._data[i]+v2._data[i];   
    }
    return v;
}
template<typename T>
Vector<T> operator-(const Vector<T>& v1, const Vector<T>&v2){
    Vector<T> v(v1._size);
    for(int i=0;i<v1._size;i++){
        v._data[i]=v1._data[i]-v2._data[i];     
    }
    return v;
}
} // namespace d2ds

#endif