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

#include "Utils.h"

using namespace lbcrypto;
using namespace std;
using namespace std::chrono;

using namespace utils;

using Ptxt = Plaintext;
using Ctxt = Ciphertext<DCRTPoly>;

class FHEController {
    CryptoContext<DCRTPoly> context;

public:
    usint circuit_depth;
    usint num_slots;

    FHEController() {}

    /*
     * Context generating/loading stuff
     */
    void generate_context(bool serialize = false);
    void load_context();
    /*
     * Generating bootstrapping and rotation keys stuff
     */
    void generate_bootstrapping_keys(usint bootstrap_slots);
    void generate_rotation_keys(vector<int> rotations, bool serialize = false, string filename = "");
    void generate_bootstrapping_and_rotation_keys(vector<int> rotations,
                                                  usint bootstrap_slots,
                                                  bool serialize,
                                                  const string& filename);

    void load_bootstrapping_and_rotation_keys(const string& filename, usint bootstrap_slots);
    void load_rotation_keys(const string& filename);
    void clear_bootstrapping_and_rotation_keys(usint bootstrap_num_slots);
    void clear_rotation_keys();


    /*
     * CKKS Encoding/Decoding/Encryption/Decryption
     */
    Ptxt encode(const vector<double>& vec, usint level, usint plaintext_num_slots);
    Ctxt encrypt(const vector<double>& vec, usint level = 0, usint plaintext_num_slots = 0);


    /*
     * Homomorphic operations
     */
    Ctxt add(const Ctxt& c1, const Ctxt& c2);
    Ctxt mult(const Ctxt& c, double d);
    Ctxt mult(const Ctxt& c, const Ptxt& p);
    Ctxt bootstrap(const Ctxt& c, bool timing = false);
    Ctxt relu(const Ctxt& c, double scale, bool timing = false);

    /*
     * I/O
     */
    Ctxt read_input(const string& filename, double scale = 1);
    void print(const Ctxt& c, usint slots = 0, string prefix = "");

    /*
     * Convolutional Neural Network functions
     */
    Ctxt convbn(const Ctxt &in, int layer, int n, double scale = 0.5, bool timing = false);
    Ctxt convbn2(const Ctxt &in, int layer, int n, double scale = 0.5, bool timing = false);
    vector<Ctxt> convbn1632sx(const Ctxt &in, int layer, int n, double scale = 0.5, bool timing = false);
    vector<Ctxt> convbn1632dx(const Ctxt &in, int layer, int n, double scale = 0.5, bool timing = false);
    vector<Ctxt> convbn3264sx(const Ctxt &in, int layer, int n, double scale = 0.5, bool timing = false);
    vector<Ctxt> convbn3264dx(const Ctxt &in, int layer, int n, double scale = 0.5, bool timing = false);

    Ctxt downsample1024to256(const Ctxt& c1, const Ctxt& c2);
    Ctxt downsample256to64(const Ctxt &c1, const Ctxt &c2);

    //TODO: studia sta roba
    Ctxt convbnV2(const Ctxt &in, int layer, int n, double scale = 0.5, bool timing = false);
    Ctxt convbn1632sxV2(const Ctxt &in, int layer, int n, double scale = 0.5, bool timing = false);
    Ctxt convbn1632dxV2(const Ctxt &in, int layer, int n, double scale = 0.5, bool timing = false);

    /*
     * Masking things
     */
    Ptxt gen_mask(int n, usint level);
    Ptxt mask_first_n(int n, usint level);
    Ptxt mask_second_n(int n, usint level);
    Ptxt mask_first_n_mod(int n, int padding, int pos, usint level);
    Ptxt mask_channel(int n, usint level);
    Ptxt mask_channel_2(int n, usint level);

private:
    KeyPair<DCRTPoly> key_pair;
    vector<uint32_t> level_budget = {5, 5};
};


#endif //PAPERRESNET_FHECONTROLLER_H
