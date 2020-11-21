#include "ValOrErr.hpp"

int main(int argc, char* argv[]) {
    utility::ValOrErr<void, int>{-1};

    return 0;
}
