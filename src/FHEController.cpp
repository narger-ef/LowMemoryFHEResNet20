//
// Created by Lorenzo on 24/10/23.
//

#include "FHEController.h"

void FHEController::generate_context(bool serialize) {
    CCParams<CryptoContextCKKSRNS> parameters;

    num_slots = 1 << 14;

    parameters.SetSecretKeyDist(UNIFORM_TERNARY);
    parameters.SetSecurityLevel(lbcrypto::HEStd_NotSet);
    parameters.SetRingDim(1 << 16);
    parameters.SetBatchSize(num_slots);

    ScalingTechnique rescaleTech = FLEXIBLEAUTO;
    usint dcrtBits               = 59;
    usint firstMod               = 60;

    parameters.SetScalingModSize(dcrtBits);
    parameters.SetScalingTechnique(rescaleTech);
    parameters.SetFirstModSize(firstMod);

    uint32_t approxBootstrapDepth     = 8;

    uint32_t levelsUsedBeforeBootstrap = 9;

    circuit_depth = levelsUsedBeforeBootstrap + FHECKKSRNS::GetBootstrapDepth(approxBootstrapDepth, level_budget, UNIFORM_TERNARY);

    cout << endl << "Ciphertexts depth: " << circuit_depth << ", available multiplications: " << levelsUsedBeforeBootstrap - 2 << endl;

    parameters.SetMultiplicativeDepth(circuit_depth);

    context = GenCryptoContext(parameters);

    cout << "Context built, generating keys..." << endl;

    context->Enable(PKE);
    context->Enable(KEYSWITCH);
    context->Enable(LEVELEDSHE);
    context->Enable(ADVANCEDSHE);
    context->Enable(FHE);

    key_pair = context->KeyGen();

    context->EvalMultKeyGen(key_pair.secretKey);

    cout << "Generated." << endl;

    if (!serialize) {
        return;
    }

    cout << "Now serializing keys ..." << endl;

    ofstream multKeyFile("../parameters/mult-keys.txt", ios::out | ios::binary);
    if (multKeyFile.is_open()) {
        if (!context->SerializeEvalMultKey(multKeyFile, SerType::BINARY)) {
            cerr << "Error writing eval mult keys" << std::endl;
            exit(1);
        }
        cout << "Relinearization Keys have been serialized" << std::endl;
        multKeyFile.close();
    }
    else {
        cerr << "Error serializing EvalMult keys" << std::endl;
        exit(1);
    }

    if (!Serial::SerializeToFile("../parameters/crypto-context.txt", context, SerType::BINARY)) {
        cerr << "Error writing serialization of the crypto context to crypto-context.txt" << endl;
    } else {
        cout << "Crypto Context have been serialized" << std::endl;
    }

    if (!Serial::SerializeToFile("../parameters/public-key.txt", key_pair.publicKey, SerType::BINARY)) {
        cerr << "Error writing serialization of public key to public-key.txt" << endl;
    } else {
        cout << "Public Key has been serialized" << std::endl;
    }

    if (!Serial::SerializeToFile("../parameters/secret-key.txt", key_pair.secretKey, SerType::BINARY)) {
        cerr << "Error writing serialization of public key to secret-key.txt" << endl;
    } else {
        cout << "Secret Key has been serialized" << std::endl;
    }
}

void FHEController::load_context() {
    context->ClearEvalMultKeys();
    context->ClearEvalAutomorphismKeys();

    CryptoContextFactory<lbcrypto::DCRTPoly>::ReleaseAllContexts();

    cout << "Reading serialized context..." << endl;

    if (!Serial::DeserializeFromFile("../parameters/crypto-context.txt", context, SerType::BINARY)) {
        cerr << "I cannot read serialized data from: crypto-context.txt" << endl;
        exit(1);
    }

    PublicKey<DCRTPoly> clientPublicKey;
    if (!Serial::DeserializeFromFile("../parameters/public-key.txt", clientPublicKey, SerType::BINARY)) {
        cerr << "I cannot read serialized data from public-key.txt" << endl;
        exit(1);
    }

    PrivateKey<DCRTPoly> serverSecretKey;
    if (!Serial::DeserializeFromFile("../parameters/secret-key.txt", serverSecretKey, SerType::BINARY)) {
        cerr << "I cannot read serialized data from public-key.txt" << endl;
        exit(1);
    }

    key_pair.publicKey = clientPublicKey;
    key_pair.secretKey = serverSecretKey;

    std::ifstream multKeyIStream("../parameters/mult-keys.txt", ios::in | ios::binary);
    if (!multKeyIStream.is_open()) {
        cerr << "Cannot read serialization from " << "mult-keys.txt" << endl;
        exit(1);
    }
    if (!context->DeserializeEvalMultKey(multKeyIStream, SerType::BINARY)) {
        cerr << "Could not deserialize eval mult key file" << endl;
        exit(1);
    }

    uint32_t approxBootstrapDepth = 8;

    uint32_t levelsUsedBeforeBootstrap = 9;

    circuit_depth = levelsUsedBeforeBootstrap + FHECKKSRNS::GetBootstrapDepth(approxBootstrapDepth, level_budget, UNIFORM_TERNARY);

    cout << "Circuit depth: " << circuit_depth << ", available multiplications: " << levelsUsedBeforeBootstrap - 2 << endl;

    num_slots = 1 << 14;
}

void FHEController::generate_bootstrapping_keys(usint bootstrap_slots) {
    context->EvalBootstrapSetup(level_budget, {0, 0}, bootstrap_slots);
    context->EvalBootstrapKeyGen(key_pair.secretKey, bootstrap_slots);
}

void FHEController::generate_rotation_keys(vector<int> rotations, bool serialize, std::string filename) {
    if (serialize && filename.size() == 0) {
        cout << "Filename cannot be empty when serializing rotation keys." << endl;
        return;
    }

    context->EvalRotateKeyGen(key_pair.secretKey, rotations);

    ofstream rotationKeyFile("../parameters/rot_" + filename, ios::out | ios::binary);
    if (rotationKeyFile.is_open()) {
        if (!context->SerializeEvalAutomorphismKey(rotationKeyFile, SerType::BINARY)) {
            cerr << "Error writing rotation keys" << std::endl;
            exit(1);
        }
        cout << "Rotation keys have been serialized" << std::endl;
    }
    else {
        cerr << "Error serializing Rotation keys" << "../parameters/rot_" + filename << std::endl;
        exit(1);
    }
}

void FHEController::generate_bootstrapping_and_rotation_keys(vector<int> rotations, usint bootstrap_slots, bool serialize, const string& filename) {
    if (serialize && filename.empty()) {
        cout << "Filename cannot be empty when serializing bootstrapping and rotation keys." << endl;
        return;
    }

    generate_bootstrapping_keys(bootstrap_slots);
    generate_rotation_keys(rotations, serialize, filename);
}

void FHEController::load_bootstrapping_and_rotation_keys(const string& filename, usint bootstrap_slots) {
    cout << endl << "Loading bootstrapping and rotations keys from " << filename << "..." << endl;

    auto start = start_time();

    context->EvalBootstrapSetup(level_budget, {0, 0}, bootstrap_slots);

    cout << "(1/2) Bootstrapping precomputations completed!" << endl;


    ifstream rotKeyIStream("../parameters/rot_" + filename, ios::in | ios::binary);
    if (!rotKeyIStream.is_open()) {
        cerr << "Cannot read serialization from " << "../parameters/" << "rot_" << filename << std::endl;
        exit(1);
    }

    if (!context->DeserializeEvalAutomorphismKey(rotKeyIStream, SerType::BINARY)) {
        cerr << "Could not deserialize eval rot key file" << std::endl;
        exit(1);
    }

    cout << "(2/2) Rotation keys read!" << endl;

    print_duration(start, "Loading bootstrapping pre-computations + rotations");

    cout << endl;
}

void FHEController::load_rotation_keys(const string& filename) {
    cout << endl << "Loading rotations keys from " << filename << "..." << endl;

    auto start = start_time();

    ifstream rotKeyIStream("../parameters/rot_" + filename, ios::in | ios::binary);
    if (!rotKeyIStream.is_open()) {
        cerr << "Cannot read serialization from " << "../parameters/" << "rot_" << filename << std::endl;
        exit(1);
    }

    if (!context->DeserializeEvalAutomorphismKey(rotKeyIStream, SerType::BINARY)) {
        cerr << "Could not deserialize eval rot key file" << std::endl;
        exit(1);
    }

    cout << "(1/1) Rotation keys read!" << endl;

    print_duration(start, "Loading rotation keys");

    cout << endl;
}

void FHEController::clear_bootstrapping_and_rotation_keys(usint bootstrap_num_slots) {
    FHECKKSRNS* derivedPtr = dynamic_cast<FHECKKSRNS*>(context->GetScheme()->GetFHE().get());
    derivedPtr->m_bootPrecomMap.erase(bootstrap_num_slots);
    context->ClearEvalAutomorphismKeys();
}

void FHEController::clear_rotation_keys() {
    context->ClearEvalAutomorphismKeys();
}

/*
 * CKKS Encoding/Decoding/Encryption/Decryption
 */
Ptxt FHEController::encode(const vector<double> &vec, usint level, usint plaintext_num_slots) {
    if (plaintext_num_slots == 0) {
        plaintext_num_slots = num_slots;
    }

    Ptxt p = context->MakeCKKSPackedPlaintext(vec, 1, level, nullptr, plaintext_num_slots);
    p->SetLength(plaintext_num_slots);
    return p;
}

Ctxt FHEController::encrypt(const vector<double> &vec, usint level, usint plaintext_num_slots) {
    if (plaintext_num_slots == 0) {
        plaintext_num_slots = num_slots;
    }

    Ptxt p = encode(vec, level, plaintext_num_slots);

    return context->Encrypt(p, key_pair.publicKey);
}

/*
 * Homomorphic operations
 */
Ctxt FHEController::add(const Ctxt &c1, const Ctxt &c2) {
    return context->EvalAdd(c1, c2);
}

Ctxt FHEController::mult(const Ctxt &c1, double d) {
    return context->EvalMult(c1, d);
}

Ctxt FHEController::mult(const Ctxt &c, const Ptxt& p) {
    return context->EvalMult(c, p);
}

Ctxt FHEController::bootstrap(const Ctxt &c, bool timing) {
    if (c->GetLevel() + 2 < circuit_depth) {
        cout << "You are bootstrapping with remaining levels! You are at " << to_string(c->GetLevel()) << "/" << circuit_depth - 2 << endl;
    }

    auto start = start_time();

    Ctxt res = context->EvalBootstrap(c);

    if (timing) {
        print_duration(start, "Bootstrapping " + to_string(c->GetSlots()) + " slots");
    }

    return res;
}

Ctxt FHEController::relu(const Ctxt &c, double scale, bool timing) {
    auto start = start_time();

    /*
     * Max min
     */
    Ptxt result;
    context->Decrypt(key_pair.secretKey, c, &result);
    vector<double> v = result->GetRealPackedValue();

    cout << "min: " << *min_element(v.begin(), v.end()) << ", max: " << *max_element(v.begin(), v.end()) << endl;
    /*
     * Max min
     */

    usint degree = 59;
    Ctxt res = context->EvalChebyshevFunction([scale](double x) -> double { if (x < 0) return 0; else return (1 / scale) * x; }, c,
                                              -1,
                                              1, degree);
    if (timing) {
        print_duration(start, "ReLU d = " + to_string(degree) + " evaluation");
    }

    return res;
}


/*
 * I/O
 */

Ctxt FHEController::read_input(const string& filename, double scale) {
    vector<double> input = read_values_from_file(filename);

    if (scale != 1) {
        for (int i = 0;i<input.size(); i++) {
            input[i] = input[i] * scale;
        }
    }

    return context->Encrypt(key_pair.publicKey, context->MakeCKKSPackedPlaintext(input, 1, circuit_depth - 2, nullptr, num_slots));
}

void FHEController::print(const Ctxt &c, usint slots, string prefix) {
    if (slots == 0) {
        slots = num_slots;
    }

    cout << prefix;

    Ptxt result;
    context->Decrypt(key_pair.secretKey, c, &result);
    result->SetSlots(num_slots);
    vector<double> v = result->GetRealPackedValue();

    cout << setprecision(10) << fixed;
    cout << "[ ";

    for (int i = 0; i < slots; i += 1) {
        string segno = "";
        if (v[i] > 0) {
            segno = " ";
        } else {
            segno = "-";
            v[i] = -v[i];
        }


        if (i == slots - 1) {
            cout << segno << v[i] << " ]";
        } else {
            if (abs(v[i]) < 0.00000001)
                cout << " 0.0000000000" << ", ";
            else
                cout << segno << v[i] << ", ";
        }
    }

    cout << endl;
}

/*
 * Convolutional Neural Network functions
 */
Ctxt FHEController::convbn(const Ctxt &in, int layer, int n, double scale, bool timing) {
    auto start = start_time();

    vector<Ctxt> c_rotations;

    int img_width = 32;
    int padding = 1;

    auto digits = context->EvalFastRotationPrecompute(in);

    c_rotations.push_back(
            context->EvalRotate(context->EvalFastRotation(in, -padding, context->GetCyclotomicOrder(), digits), -img_width ));
    c_rotations.push_back(context->EvalFastRotation(in, -img_width, context->GetCyclotomicOrder(), digits));
    c_rotations.push_back(
            context->EvalRotate(context->EvalFastRotation(in, padding, context->GetCyclotomicOrder(), digits), -img_width ));
    c_rotations.push_back(context->EvalFastRotation(in, -padding, context->GetCyclotomicOrder(), digits));
    c_rotations.push_back(in);
    c_rotations.push_back(context->EvalFastRotation(in, padding, context->GetCyclotomicOrder(), digits));
    c_rotations.push_back(
            context->EvalRotate(context->EvalFastRotation(in, -padding, context->GetCyclotomicOrder(), digits), img_width));
    c_rotations.push_back(context->EvalFastRotation(in, img_width, context->GetCyclotomicOrder(), digits));
    c_rotations.push_back(
            context->EvalRotate(context->EvalFastRotation(in, padding, context->GetCyclotomicOrder(), digits), img_width ));

    Ptxt bias = encode(read_values_from_file("../weights/layer" + to_string(layer) + "-conv" + to_string(n) + "bn" + to_string(n) + "-bias.bin", 0.5));

    Ctxt finalsum;

    for (int j = 0; j < 16; j++) {
        vector<Ctxt> k_rows;

        for (int k = 0; k < 9; k++) {
            vector<double> values = read_values_from_file("../weights/layer" + to_string(layer) + "-conv" + to_string(n) + "bn" + to_string(n) + "-ch" +
                                                      to_string(j) + "-k" + to_string(k+1) + ".bin", scale);
            Ptxt encoded = encode(values, circuit_depth - 2, 16384);
            k_rows.push_back(context->EvalMult(c_rotations[k], encoded));
        }

        Ctxt sum = context->EvalAddMany(k_rows);
        if (j == 0) {
            finalsum = sum->Clone();
            finalsum = context->EvalRotate(finalsum, -1024);
        } else {
            finalsum = context->EvalAdd(finalsum, sum);
            finalsum = context->EvalRotate(finalsum, -1024);
        }

    }

    finalsum = context->EvalAdd(finalsum, bias);

    if (timing) {
        print_duration(start, "Layer" + to_string(layer) + " - convbn" + to_string(n));
    }

    return finalsum;
}

Ctxt FHEController::convbn2(const Ctxt &in, int layer, int n, double scale, bool timing) {
    auto start = start_time();

    vector<Ctxt> c_rotations;

    int img_width = 16;
    int padding = 1;

    auto digits = context->EvalFastRotationPrecompute(in);

    c_rotations.push_back(
            context->EvalRotate(context->EvalFastRotation(in, -padding, context->GetCyclotomicOrder(), digits), -img_width ));
    c_rotations.push_back(context->EvalFastRotation(in, -img_width, context->GetCyclotomicOrder(), digits));
    c_rotations.push_back(
            context->EvalRotate(context->EvalFastRotation(in, padding, context->GetCyclotomicOrder(), digits), -img_width ));
    c_rotations.push_back(context->EvalFastRotation(in, -padding, context->GetCyclotomicOrder(), digits));
    c_rotations.push_back(in);
    c_rotations.push_back(context->EvalFastRotation(in, padding, context->GetCyclotomicOrder(), digits));
    c_rotations.push_back(
            context->EvalRotate(context->EvalFastRotation(in, -padding, context->GetCyclotomicOrder(), digits), img_width));
    c_rotations.push_back(context->EvalFastRotation(in, img_width, context->GetCyclotomicOrder(), digits));
    c_rotations.push_back(
            context->EvalRotate(context->EvalFastRotation(in, padding, context->GetCyclotomicOrder(), digits), img_width ));

    Ptxt bias = encode(read_values_from_file("../weights/layer" + to_string(layer) + "-conv" + to_string(n) + "bn" + to_string(n) + "-bias.bin", 0.5), circuit_depth-2, 8192);

    Ctxt finalsum;

    for (int j = 0; j < 32; j++) {
        vector<Ctxt> k_rows;

        for (int k = 0; k < 9; k++) {
            vector<double> values = read_values_from_file("../weights/layer" + to_string(layer) + "-conv" + to_string(n) + "bn" + to_string(n) + "-ch" +
                                                          to_string(j) + "-k" + to_string(k+1) + ".bin", scale);
            Ptxt encoded = encode(values, circuit_depth - 2, 8192);
            k_rows.push_back(context->EvalMult(c_rotations[k], encoded));
        }

        Ctxt sum = context->EvalAddMany(k_rows);
        if (j == 0) {
            finalsum = sum->Clone();
            finalsum = context->EvalRotate(finalsum, -256);
        } else {
            finalsum = context->EvalAdd(finalsum, sum);
            finalsum = context->EvalRotate(finalsum, -256);
        }

    }

    finalsum = context->EvalAdd(finalsum, bias);

    if (timing) {
        print_duration(start, "Layer" + to_string(layer) + " - convbn" + to_string(n));
    }

    return finalsum;
}

vector<Ctxt> FHEController::convbn1632sx(const Ctxt &in, int layer, int n, double scale, bool timing) {
    auto start = start_time();

    vector<Ctxt> c_rotations;

    int img_width = 32;
    int padding = 1;

    auto digits = context->EvalFastRotationPrecompute(in);

    c_rotations.push_back(
            context->EvalRotate(context->EvalFastRotation(in, -(img_width), context->GetCyclotomicOrder(), digits), -padding));
    c_rotations.push_back(context->EvalFastRotation(in, -img_width, context->GetCyclotomicOrder(), digits));
    c_rotations.push_back(
            context->EvalRotate(context->EvalFastRotation(in, -(img_width), context->GetCyclotomicOrder(), digits), padding));
    c_rotations.push_back(context->EvalFastRotation(in, -padding, context->GetCyclotomicOrder(), digits));
    c_rotations.push_back(in);
    c_rotations.push_back(context->EvalFastRotation(in, padding, context->GetCyclotomicOrder(), digits));
    c_rotations.push_back(
            context->EvalRotate(context->EvalFastRotation(in, (img_width), context->GetCyclotomicOrder(), digits), -padding));
    c_rotations.push_back(context->EvalFastRotation(in, img_width, context->GetCyclotomicOrder(), digits));
    c_rotations.push_back(
            context->EvalRotate(context->EvalFastRotation(in, (img_width), context->GetCyclotomicOrder(), digits), padding));

    vector<Ctxt> applied_filters16;
    vector<Ctxt> applied_filters32;


    Ptxt bias1 = encode(read_values_from_file("../weights/layer" + to_string(layer) + "-conv" + to_string(n) + "bn" + to_string(n) + "-bias1.bin", scale));
    Ptxt bias2 = encode(read_values_from_file("../weights/layer" + to_string(layer) + "-conv" + to_string(n) + "bn" + to_string(n) + "-bias2.bin", scale));

    Ctxt finalSum016;
    Ctxt finalSum1632;

    for (int j = 0; j < 16; j++) {
        vector<Ctxt> k_rows016;
        vector<Ctxt> k_rows1632;

        for (int k = 0; k < 9; k++) {
            vector<double> values = read_values_from_file("../weights/layer" + to_string(layer) + "-conv" + to_string(n) + "bn" + to_string(n) + "-ch" +
                                                      to_string(j) + "-k" + to_string(k+1) + ".bin", scale);
            k_rows016.push_back(context->EvalMult(c_rotations[k], encode(values)));

            values = read_values_from_file("../weights/layer" + to_string(layer) + "-conv" + to_string(n) + "bn" + to_string(n) + "-ch" +
                                       to_string(j+16) + "-k" + to_string(k+1) + ".bin", scale);
            k_rows1632.push_back(context->EvalMult(c_rotations[k], encode(values)));
        }

        Ctxt sum016 = context->EvalAddMany(k_rows016);
        Ctxt sum1632 = context->EvalAddMany(k_rows1632);

        if (j == 0) {
            finalSum016 = sum016->Clone();
            finalSum016 = context->EvalRotate(finalSum016, -1024);
            finalSum1632 = sum1632->Clone();
            finalSum1632 = context->EvalRotate(finalSum1632, -1024);
        } else {
            finalSum016 = context->EvalAdd(finalSum016, sum016);
            finalSum016 = context->EvalRotate(finalSum016, -1024);
            finalSum1632 = context->EvalAdd(finalSum1632, sum1632);
            finalSum1632 = context->EvalRotate(finalSum1632, -1024);
        }

    }

    finalSum016 = context->EvalAdd(finalSum016, bias1);
    finalSum1632 = context->EvalAdd(finalSum1632, bias2);

    if (timing) {
        print_duration(start, "Layer" + to_string(layer) + " - convbnSx" + to_string(n));
    }

    return {finalSum016, finalSum1632};
}

vector<Ctxt> FHEController::convbn1632dx(const Ctxt &in, int layer, int n, double scale, bool timing) {
    auto start = start_time();

    vector<Ctxt> applied_filters16;
    vector<Ctxt> applied_filters32;

    Ptxt bias1 = encode(read_values_from_file("../weights/layer" + to_string(layer) + "dx-conv" + to_string(n) + "bn" + to_string(n) + "-bias1.bin", scale));
    Ptxt bias2 = encode(read_values_from_file("../weights/layer" + to_string(layer) + "dx-conv" + to_string(n) + "bn" + to_string(n) + "-bias2.bin", scale));

    Ctxt finalSum016;
    Ctxt finalSum1632;

    for (int j = 0; j < 16; j++) {
        vector<Ctxt> k_rows016;
        vector<Ctxt> k_rows1632;

        vector<double> values = read_values_from_file("../weights/layer" + to_string(layer) + "dx-conv" + to_string(n) + "bn" + to_string(n) + "-ch" +
                                                      to_string(j) + "-k" + to_string(1) + ".bin", scale);
        k_rows016.push_back(context->EvalMult(in, encode(values, in->GetLevel(), num_slots)));

        values = read_values_from_file("../weights/layer" + to_string(layer) + "dx-conv" + to_string(n) + "bn" + to_string(n) + "-ch" +
                                       to_string(j+16) + "-k" + to_string(1) + ".bin", scale);

        k_rows1632.push_back(context->EvalMult(in, encode(values, in->GetLevel(), num_slots)));

        Ctxt sum016 = context->EvalAddMany(k_rows016);
        Ctxt sum1632 = context->EvalAddMany(k_rows1632);

        if (j == 0) {
            finalSum016 = sum016->Clone();
            finalSum016 = context->EvalRotate(finalSum016, -1024);
            finalSum1632 = sum1632->Clone();
            finalSum1632 = context->EvalRotate(finalSum1632, -1024);
        } else {
            finalSum016 = context->EvalAdd(finalSum016, sum016);
            finalSum016 = context->EvalRotate(finalSum016, -1024);
            finalSum1632 = context->EvalAdd(finalSum1632, sum1632);
            finalSum1632 = context->EvalRotate(finalSum1632, -1024);
        }

    }

    finalSum016 = context->EvalAdd(finalSum016, bias1);
    finalSum1632 = context->EvalAdd(finalSum1632, bias2);

    if (timing) {
        print_duration(start, "Layer" + to_string(layer) + " - convbnDx" + to_string(n));
    }

    return {finalSum016, finalSum1632};
}

Ctxt FHEController::downsample1024to256(const Ctxt &c1, const Ctxt &c2) {
    c1->SetSlots(32768);
    c2->SetSlots(32768);
    num_slots = 16384*2;
    Ctxt fullpack = add(mult(c1, mask_first_n(16384)), mult(c2, mask_second_n(16384)));

    //Affianco tutte le righe
    fullpack = context->EvalMult(context->EvalAdd(fullpack, context->EvalRotate(fullpack, 1)), gen_mask(2));
    fullpack = context->EvalMult(context->EvalAdd(fullpack, context->EvalRotate(context->EvalRotate(fullpack, 1), 1)), gen_mask(4));
    fullpack = context->EvalMult(context->EvalAdd(fullpack, context->EvalRotate(fullpack, 4)), gen_mask(8));
    fullpack = context->EvalAdd(fullpack, context->EvalRotate(fullpack, 8));

    Ctxt downsampledrows = encrypt({0});

    for (int i = 0; i < 16; i++) {
        Ctxt masked = context->EvalMult(fullpack, mask_first_n_mod(16, 1024, i, fullpack->GetLevel()));
        downsampledrows = context->EvalAdd(downsampledrows, masked);
        if (i < 15) {
            fullpack = context->EvalRotate(fullpack, 64 - 16); //Si può fare fast
        }
    }

    Ctxt downsampledchannels = encrypt({0});
    for (int i = 0; i < 32; i++) {
        Ctxt masked = context->EvalMult(downsampledrows, mask_channel(i, downsampledrows->GetLevel()));
        downsampledchannels = context->EvalAdd(downsampledchannels, masked);
        downsampledchannels = context->EvalRotate(downsampledchannels, -(1024 - 256));
    }

    downsampledchannels = context->EvalRotate(downsampledchannels, (1024 - 256) * 32);
    downsampledchannels = context->EvalAdd(downsampledchannels, context->EvalRotate(downsampledchannels, -8192));
    downsampledchannels = context->EvalAdd(downsampledchannels, context->EvalRotate(context->EvalRotate(downsampledchannels, -8192), -8192));

    downsampledchannels->SetSlots(8192);

    return downsampledchannels;

}
Ctxt FHEController::downsample1024to256(const Ctxt &in) {
    int lvl_iniz = in->GetLevel();

    Ctxt fullpack = in;

    //Affianco tutte le righe
    fullpack = context->EvalMult(context->EvalAdd(fullpack, context->EvalRotate(fullpack, 1)), gen_mask(2));
    fullpack = context->EvalMult(context->EvalAdd(fullpack, context->EvalRotate(context->EvalRotate(fullpack, 1), 1)), gen_mask(4));
    fullpack = context->EvalMult(context->EvalAdd(fullpack, context->EvalRotate(fullpack, 4)), gen_mask(8));
    fullpack = context->EvalAdd(fullpack, context->EvalRotate(fullpack, 8));

    Ctxt downsampledrows = encrypt({0});

    for (int i = 0; i < 16; i++) {
        Ctxt masked = context->EvalMult(fullpack, mask_first_n_mod(16, 1024, i, fullpack->GetLevel()));
        downsampledrows = context->EvalAdd(downsampledrows, masked);
        if (i < 15) {
            fullpack = context->EvalRotate(fullpack, 64 - 16); //Si può fare fast
        }
    }

    Ctxt downsampledchannels = encrypt({0});
    for (int i = 0; i < 32; i++) {
        Ctxt masked = context->EvalMult(downsampledrows, mask_channel(i, downsampledrows->GetLevel()));
        downsampledchannels = context->EvalAdd(downsampledchannels, masked);
        downsampledchannels = context->EvalRotate(downsampledchannels, -(1024 - 256));
    }

    downsampledchannels = context->EvalRotate(downsampledchannels, (1024 - 256) * 32);
    downsampledchannels = context->EvalAdd(downsampledchannels, context->EvalRotate(downsampledchannels, -8192));
    downsampledchannels = context->EvalAdd(downsampledchannels, context->EvalRotate(context->EvalRotate(downsampledchannels, -8192), -8192));

    downsampledchannels->SetSlots(8192);

    cout << "Downsampling used: " << downsampledchannels->GetLevel() - lvl_iniz << " levels." << endl;

    print(downsampledchannels, 8192);

    return downsampledchannels;

}

Ctxt FHEController::convbn1632sxV2(const Ctxt &in, int layer, int n, double scale, bool timing) {
    auto start = start_time();

    vector<Ctxt> c_rotations;

    int img_width = 32;
    int padding = 1;

    in->SetSlots(16384 * 2);

    auto digits = context->EvalFastRotationPrecompute(in);

    c_rotations.push_back(
            context->EvalRotate(context->EvalFastRotation(in, -(img_width), context->GetCyclotomicOrder(), digits), -padding));
    c_rotations.push_back(context->EvalFastRotation(in, -img_width, context->GetCyclotomicOrder(), digits));
    c_rotations.push_back(
            context->EvalRotate(context->EvalFastRotation(in, -(img_width), context->GetCyclotomicOrder(), digits), padding));
    c_rotations.push_back(context->EvalFastRotation(in, -padding, context->GetCyclotomicOrder(), digits));
    c_rotations.push_back(in);
    c_rotations.push_back(context->EvalFastRotation(in, padding, context->GetCyclotomicOrder(), digits));
    c_rotations.push_back(
            context->EvalRotate(context->EvalFastRotation(in, (img_width), context->GetCyclotomicOrder(), digits), -padding));
    c_rotations.push_back(context->EvalFastRotation(in, img_width, context->GetCyclotomicOrder(), digits));
    c_rotations.push_back(
            context->EvalRotate(context->EvalFastRotation(in, (img_width), context->GetCyclotomicOrder(), digits), padding));

    vector<Ctxt> applied_filters16;
    vector<Ctxt> applied_filters32;

    vector<double> bias1_v = read_values_from_file("../weights/layer" + to_string(layer) + "-conv" + to_string(n) + "bn" + to_string(n) + "-bias1.bin", scale);
    vector<double> bias2_v = read_values_from_file("../weights/layer" + to_string(layer) + "-conv" + to_string(n) + "bn" + to_string(n) + "-bias2.bin", scale);

    bias1_v.insert(bias1_v.end(), bias2_v.begin(), bias2_v.end());

    Ptxt bias1 = encode(bias1_v, circuit_depth - 10, 16384 * 2);

    Ctxt finalSum;

    for (int j = 0; j < 16; j++) {
        vector<Ctxt> k_rows;

        for (int k = 0; k < 9; k++) {
            vector<double> values1 = read_values_from_file("../weights/layer" + to_string(layer) + "-conv" + to_string(n) + "bn" + to_string(n) + "-ch" +
                                                          to_string(j) + "-k" + to_string(k+1) + ".bin", scale);
            vector<double> values2 = read_values_from_file("../weights/layer" + to_string(layer) + "-conv" + to_string(n) + "bn" + to_string(n) + "-ch" +
                                                            to_string(j+16) + "-k" + to_string(k+1) + ".bin", scale);

            values1.insert(values1.end(), values2.begin(), values2.end());

            k_rows.push_back(context->EvalMult(c_rotations[k], encode(values1, circuit_depth - 10, 16384*2)));
        }

        Ctxt sum = context->EvalAddMany(k_rows);

        if (j == 0) {
            finalSum = sum->Clone();
            finalSum = context->EvalRotate(finalSum, -1024);
        } else {
            finalSum = context->EvalAdd(finalSum, sum);
            finalSum = context->EvalRotate(finalSum, -1024);
        }

    }

    finalSum = context->EvalRotate(finalSum, 16384);
    finalSum = context->EvalAdd(finalSum, bias1);

    if (timing) {
        print_duration(start, "Layer" + to_string(layer) + " - convbn" + to_string(n));
    }

    return finalSum;
}

Ctxt FHEController::convbn1632dxV2(const Ctxt &in, int layer, int n, double scale, bool timing) {
    auto start = start_time();

    in->SetSlots(16384 * 2);

    vector<double> bias1_v = read_values_from_file("../weights/layer" + to_string(layer) + "dx-conv" + to_string(n) + "bn" + to_string(n) + "-bias1.bin", scale);
    vector<double> bias2_v = read_values_from_file("../weights/layer" + to_string(layer) + "dx-conv" + to_string(n) + "bn" + to_string(n) + "-bias2.bin", scale);

    bias1_v.insert(bias1_v.end(), bias2_v.begin(), bias2_v.end());

    Ptxt bias = encode(bias1_v, circuit_depth - 10, 16384 * 2);

    Ctxt finalSum;

    for (int j = 0; j < 16; j++) {
        Ctxt k_row;

        vector<double> values1 = read_values_from_file("../weights/layer" + to_string(layer) + "dx-conv" + to_string(n) + "bn" + to_string(n) + "-ch" +
                                                       to_string(j) + "-k1.bin", scale);
        vector<double> values2 = read_values_from_file("../weights/layer" + to_string(layer) + "dx-conv" + to_string(n) + "bn" + to_string(n) + "-ch" +
                                                       to_string(j+16) + "-k1.bin", scale);

        values1.insert(values1.end(), values2.begin(), values2.end());

        k_row = context->EvalMult(in, encode(values1, circuit_depth - 10, 16384*2));

        Ctxt sum = k_row;

        if (j == 0) {
            finalSum = sum->Clone();
            finalSum = context->EvalRotate(finalSum, -1024);
        } else {
            finalSum = context->EvalAdd(finalSum, sum);
            finalSum = context->EvalRotate(finalSum, -1024);
        }

    }

    finalSum = context->EvalRotate(finalSum, 16384);
    finalSum = context->EvalAdd(finalSum, bias);

    if (timing) {
        print_duration(start, "Layer" + to_string(layer) + " - convbn" + to_string(n));
    }

    return finalSum;
}

Ctxt FHEController::convbnV2(const Ctxt &in, int layer, int n, double scale, bool timing) {
    auto start = start_time();

    vector<Ctxt> c_rotations;

    int img_width = 32;
    int padding = 1;

    auto digits = context->EvalFastRotationPrecompute(in);

    c_rotations.push_back(
            context->EvalRotate(context->EvalFastRotation(in, -padding, context->GetCyclotomicOrder(), digits), -img_width ));
    c_rotations.push_back(context->EvalFastRotation(in, -img_width, context->GetCyclotomicOrder(), digits));
    c_rotations.push_back(
            context->EvalRotate(context->EvalFastRotation(in, padding, context->GetCyclotomicOrder(), digits), -img_width ));
    c_rotations.push_back(context->EvalFastRotation(in, -padding, context->GetCyclotomicOrder(), digits));
    c_rotations.push_back(in);
    c_rotations.push_back(context->EvalFastRotation(in, padding, context->GetCyclotomicOrder(), digits));
    c_rotations.push_back(
            context->EvalRotate(context->EvalFastRotation(in, -padding, context->GetCyclotomicOrder(), digits), img_width));
    c_rotations.push_back(context->EvalFastRotation(in, img_width, context->GetCyclotomicOrder(), digits));
    c_rotations.push_back(
            context->EvalRotate(context->EvalFastRotation(in, padding, context->GetCyclotomicOrder(), digits), img_width ));

    Ptxt bias = encode(read_values_from_file("../weights/layer" + to_string(layer) + "-conv" + to_string(n) + "bn" + to_string(n) + "-bias.bin", 0.5));

    Ctxt finalsum;

    for (int j = 0; j < 8; j++) {
        vector<Ctxt> k_rows;

        for (int k = 0; k < 9; k++) {
            vector<double> values1 = read_values_from_file("../weights/layer" + to_string(layer) + "-conv" + to_string(n) + "bn" + to_string(n) + "-ch" +
                                                          to_string(j) + "-k" + to_string(k+1) + ".bin", scale);
            vector<double> values2 = read_values_from_file("../weights/layer" + to_string(layer) + "-conv" + to_string(n) + "bn" + to_string(n) + "-ch" +
                                                           to_string(j+8) + "-k" + to_string(k+1) + ".bin", scale);

            values1.insert(values1.end(), values2.begin(), values2.end());

            Ptxt encoded = encode(values1, circuit_depth - 3, 32768);
            c_rotations[k]->SetSlots(32768);
            k_rows.push_back(context->EvalMult(c_rotations[k], encoded));
        }

        Ctxt sum = context->EvalAddMany(k_rows);
        if (j == 0) {
            finalsum = sum->Clone();
            finalsum = context->EvalRotate(finalsum, -1024);
        } else {
            finalsum = context->EvalAdd(finalsum, sum);
            finalsum = context->EvalRotate(finalsum, -1024);
        }

    }

    finalsum = context->EvalAdd(finalsum, context->EvalRotate(finalsum, 16384));
    finalsum->SetSlots(16384);

    finalsum = context->EvalAdd(finalsum, bias);

    if (timing) {
        print_duration(start, "  Layer" + to_string(layer) + " - convbn" + to_string(n));
    }

    return finalsum;
}

Ptxt FHEController::gen_mask(int n, usint level) {
    vector<double> mask;

    int copy_interval = n;

    for (int i = 0; i < num_slots; i++) {
        if (copy_interval > 0) {
            mask.push_back(1);
        } else {
            mask.push_back(0);
        }

        copy_interval--;

        if (copy_interval <= -n) {
            copy_interval = n;
        }
    }

    return encode(mask, level, num_slots);
}

Ptxt FHEController::mask_first_n(int n, usint level) {
    vector<double> mask;

    for (int i = 0; i < num_slots; i++) {
        if (i < n) {
            mask.push_back(1);
        } else {
            mask.push_back(0);
        }
    }

    return encode(mask, level, num_slots);
}

Ptxt FHEController::mask_second_n(int n, usint level) {
    vector<double> mask;

    for (int i = 0; i < num_slots; i++) {
        if (i >= n) {
            mask.push_back(1);
        } else {
            mask.push_back(0);
        }
    }

    return encode(mask, level, num_slots);
}

Ptxt FHEController::mask_first_n_mod(int n, int padding, int pos, usint level) {
    vector<double> mask;
    for (int i = 0; i < 32; i++) {
        for (int j = 0; j < (pos * n); j++) {
            mask.push_back(0);
        }
        for (int j = 0; j < n; j++) {
            mask.push_back(1);
        }
        for (int j = 0; j < (padding - n - (pos * n)); j++) {
            mask.push_back(0);
        }
    }

    return encode(mask, level, 16384 * 2);
}

Ptxt FHEController::mask_channel(int n, usint level) {
    vector<double> mask;

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < 1024; j++) {
            mask.push_back(0);
        }
    }

    for (int i = 0; i < 256; i++) {
        mask.push_back(1);
    }

    for (int i = 0; i < 1024 - 256; i++) {
        mask.push_back(0);
    }

    for (int i = 0; i < 31 - n; i++) {
        for (int j = 0; j < 1024; j++) {
            mask.push_back(0);
        }
    }

    return encode(mask, level, 16384 * 2);
}