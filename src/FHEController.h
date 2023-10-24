//
// Created by Lorenzo on 24/10/23.
//

#ifndef PAPERRESNET_FHECONTROLLER_H
#define PAPERRESNET_FHECONTROLLER_H

#include "openfhe.h"
#include "ciphertext-ser.h"
#include "scheme/ckksrns/ckksrns-ser.h"
#include "ciphertext-ser.h"
#include "cryptocontext-ser.h"
#include "key/key-ser.h"
#include <thread>

using namespace lbcrypto;
using namespace std;
using namespace std::chrono;

using Ptxt = Plaintext;
using Ctxt = Ciphertext<DCRTPoly>;

class FHEController {

};


#endif //PAPERRESNET_FHECONTROLLER_H
