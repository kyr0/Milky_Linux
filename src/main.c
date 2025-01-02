#include "main.h"

int main(int argc, char *argv[]) {

    // Seed the random number generator with the current time
    srand((unsigned int)time(NULL));
    
    omp_set_dynamic(0);
    omp_set_num_threads(8); 
    omp_set_nested(8);

    run();
}