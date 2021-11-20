
#include <stdint.h>
unsigned __specpriv_begin_invocation() {

}
// remove busy waiting
void __specpriv_begin_iter() {

}
void __specpriv_end_iter() {

}
// type Exit
uint32_t __specpriv_end_invocation() {

}

// imitate main process and spawned worker processes
void main(int num_workers, int num_iters) {
    // begin_invocation
    __specpriv_begin_invocation();
    for(int w = 0; w < num_workers; w++) {
        for(int iter = 0; iter < num_iters; iter++) {
            // imitate busy waiting
            if(iter == 0) {
                busy_wait();
            }
            // begin_iter
            __specpriv_begin_iter();
            // end_iter
            __specpriv_end_iter();
        }
    }
    //end_invocation
    uint32_t exit = __specpriv_end_invocation();
}