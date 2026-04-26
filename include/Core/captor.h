//
// Created by 66 on 2026/4/26.
//

#ifndef OBSIM_CAPTOR_H
#define OBSIM_CAPTOR_H

#include <iostream>

extern "C" {
#include "libavutil/avutil.h"
}

using std::cout;

class Captor {
public:
    explicit Captor();
    ~Captor();

private:
    bool init_ctx();
};


#endif //OBSIM_CAPTOR_H
