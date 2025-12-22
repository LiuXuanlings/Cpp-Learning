#ifndef ARRAY_HPP_D2DS
#define ARRAY_HPP_D2DS

#include <initializer_list>

namespace d2ds {
// show your code
    template<typename T, unsigned int N>
    class Array{
    public:
        Array(std::initializer_list<T> list){
            for(auto it = list.begin(); it != list.end() && it - list.begin() < list.size(); it++){
                a[it - list.begin()] = *it;
            }
        }

        Array()=default;
        ~Array()=default;
        Array(const Array& other){
            for(int i=0;i<N;i++){
                a[i]=other.a[i];
            }
        }

        Array& operator =(const Array& other){
            if(this==&other)  return *this;
            for(int i=0;i<N;i++){
                a[i]=other.a[i];
            }
            return *this;
        }

        Array(Array && other){
            for(int i=0;i<N;i++){
                a[i]=std::move(other.a[i]);
            }
        }

        Array &operator=(Array && other){
            if(this==&other)  return *this;
            for(int i=0;i<N;i++){
                a[i]=std::move(other.a[i]);
            }
            return *this;
        }

        T & operator[](int index){
            if(index<0)
                index=index+N;
            d2ds_assert(index>=0 && index<N);
            return a[index];
        }

        unsigned int size() const{
            return N;
        }

        const T& back() const{
            return a[N==0?0:N-1];
        }

        T* begin(){
            return a;
        }

        T* end(){
            return a+N;
        }
    
    private:
        T a[N == 0?1:N];
    };

}

#endif