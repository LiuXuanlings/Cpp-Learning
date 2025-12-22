#ifndef SLINKED_LIST_HPP_D2DS
#define SLINKED_LIST_HPP_D2DS

#include <common/common.hpp>

namespace d2ds {
// show your code
    template<typename T>
    struct SLinkedListNode{
        SLinkedListNode<T>*next;
        T data;    
    };

    template<typename T>
    struct SLinkedListIterator{
        using Node=SLinkedListNode<T>;
        SLinkedListIterator():_nodeptr{nullptr}{};
        SLinkedListIterator(Node * node): _nodeptr{node}{};
        Node* _nodeptr;

        T& operator*(){
            return _nodeptr->data;
        }

        T* operator->(){
            return &_nodeptr->data;
        }


        bool operator==(const SLinkedListIterator & other){
            return _nodeptr==other._nodeptr;
        }

        bool operator!=(const SLinkedListIterator &it) const {
            return _nodeptr != it._nodeptr;
        }

        SLinkedListIterator & operator++(){
            _nodeptr = _nodeptr->next;
            return *this;
        }

        SLinkedListIterator operator++(int){
            auto old=*this;
            _nodeptr = _nodeptr->next;
            return old;
        }




    };

    template<typename T,typename Allocator=DefaultAllocator>
    class SLinkedList{
    public:
        using Node = SLinkedListNode<T>;
        using Iterator = SLinkedListIterator<T>;
        SLinkedList():_size{0},head{&head,T()},tail{&head}{}
        SLinkedList(std::initializer_list<T> list):SLinkedList(){
            for(auto t : list){
                push_back(t);
            }
        }
        SLinkedList(const SLinkedList& other):SLinkedList(){
            for(auto it=&other.head;it->next!=&other.head;it=it->next){
                push_back(it->next->data);
            }
        }

        SLinkedList& operator=(const SLinkedList &other){
            if(this==&other) return *this;
            this->~SLinkedList();
            for(auto it=&other.head;it->next!=&other.head;it=it->next){
                push_back(it->next->data);
            }
            return *this;
        }

        SLinkedList(SLinkedList&& other):SLinkedList(){
            _size=other._size;
            head.next=other.head.next;
            tail=other.tail;

            tail->next = &head;

            other._size=0;
            other.head.next=&other.head;
            other.tail=&other.head;

        }

        SLinkedList& operator=(SLinkedList&& other){
            if(this!=&other){
                this->~SLinkedList();
                _size=other._size;
                head.next=other.head.next;
                tail=other.tail;

                tail->next = &head;

                other._size=0;
                other.head.next=&other.head;
                other.tail=&other.head;
            }
            return *this;
        }

        ~SLinkedList(){
            for(auto it=&head;it->next!=&head;){
                auto nodeptr = it->next;
                it->next = nodeptr->next;
                nodeptr->data.~T();
                Allocator::deallocate(nodeptr,sizeof(Node));
            }
            _size=0;
            tail=&head;
        }

        void push_back(const T& t){
            auto nodeptr = static_cast<Node*>(Allocator::allocate(sizeof(Node)));
            new (&nodeptr->data) T(t);
            nodeptr->next = tail->next;
            tail->next = nodeptr;
            tail = nodeptr;
            _size++;
        }

        void _pop_back(){
            assert(size()>0);
            auto it= head.next;
            while(it->next!=tail) it=it->next;
            it->next=tail->next;
            tail->data.~T();
            Allocator::deallocate(tail,sizeof(Node));
            tail = it;
            _size--;
        }

        void push_front(const T&t){
            auto nodeptr = static_cast<Node*>(Allocator::allocate(sizeof(Node)));
            new (&nodeptr->data) T(t);

            nodeptr->next = head.next;
            head.next = nodeptr;
            _size++;

            if(nodeptr->next==&head)
                tail = nodeptr;
        }

        void pop_front(){
            auto nodeptr = head.next;

            head.next = nodeptr->next; 
            nodeptr->data.~T();
            Allocator::deallocate(nodeptr,sizeof(Node));
            _size--;

            if(_size==0) tail = &head;
        }

        unsigned int size() const{
            return _size;
        }

        bool empty()const{
            return head.next==&head;
        }

        T & front(){
            return head.next->data;
        }

        T& back(){
            return tail->data;
        }

        T& operator[](unsigned int index){
            auto it=head.next;
            for(int i=0;i<index && it!=&head;i++){
                it=it->next;
            }
            return it->data;
        }

        Iterator begin(){
            return head.next;
        }

        Iterator end() {
            return &head;
        }

        void erase_after(Iterator& it){
            auto nodeptr=it._nodeptr->next;
            if(nodeptr!=&head){
                it._nodeptr->next=nodeptr->next;
                _size--;
                nodeptr->data.~T();
                Allocator::deallocate(nodeptr,sizeof(Node));
            }   
            if(it._nodeptr->next==&head){
                tail=&head;
            }
        }

        void insert_after(Iterator pos, const T &data) {
            auto nodePtr = static_cast<Node *>(Allocator::allocate(sizeof(Node)));
            new (&(nodePtr->data)) T(data);
            nodePtr->next = pos._nodeptr->next;
            pos._nodeptr->next = nodePtr;
            _size++;
            if (nodePtr->next == &head) {
                tail = nodePtr;
            }
        }

    private:    
        unsigned int _size;
        Node head;
        Node* tail;

    };
}

#endif