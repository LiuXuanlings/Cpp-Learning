#ifndef EMBEDDED_LIST_HPP_D2DS
#define EMBEDDED_LIST_HPP_D2DS

namespace d2ds {
// show your code

    struct SinglyLink{
        struct SinglyLink * next;

        SinglyLink():next{this}{}

        static void insert(struct SinglyLink* prev, struct SinglyLink* target){
            target->next=prev->next;
            prev->next=target;
        }

        static void remove(struct SinglyLink* prev, struct SinglyLink* target){
            prev->next=target->next;
            target->next=target;
        }
    };

}

#endif