#include <iostream>
#include "FHEController.h"

void check_arguments(int argc, char *argv[], bool default_generate);

void executeResNet20(const string& filename);
Ctxt layer1(const Ctxt& in);
Ctxt layer2(const Ctxt& in);
Ctxt layer3(const Ctxt& in);

FHEController controller;

bool generate_context;

/*
 * TODO:
 * 1) Migliorare convbn sfruttando tutti gli slot del ciphertext
 * 2) Completare anche con layer3 (magari prendi l'output di layer 2 in chiaro così non ti fottono gli errori)
 */

int main(int argc, char *argv[]) {
    //TODO: prova con ReLU a 119
    //TODO: possibile che il bootstrap a 8192 ci metta lo stesso tempo? indaga

    check_arguments(argc, argv, false);

    if (generate_context) {
        controller.generate_context(true);
        /*
        controller.generate_bootstrapping_and_rotation_keys({1, -1, 32, -32, -1024},
                                                            16384,
                                                            true,
                                                            "rotations-layer1.bin");
        controller.generate_rotation_keys({1, 2, 4, 8, 64-16, -(1024 - 256), (1024 - 256) * 32, -8192},
                                          true,
                                          "rotations-layer2-downsample.bin");
        controller.generate_bootstrapping_and_rotation_keys({1, -1, 16, -16, -256},
                                          8192,
                                          true,
                                          "rotations-layer2.bin");
        controller.generate_rotation_keys({1, 2, 4, 32 - 8, -(256 - 64), (256 - 64) * 64, -4096},
                                          true,
                                          "rotations-layer3-downsample.bin");
        controller.generate_bootstrapping_and_rotation_keys({1, -1, 8, -8, -64},
                                          4096,
                                          true,
                                          "rotations-layer3.bin");

        */

    } else {
        controller.load_context();
        controller.load_bootstrapping_and_rotation_keys("rotations-layer1.bin", 16384);
        //Un-comment if you start from Layer 3
        //controller.load_bootstrapping_and_rotation_keys("rotations-layer2.bin", 8192);
    }

    executeResNet20("../test_images/input_louis.bin");
}

void executeResNet20(const string& filename) {
    cout << "Starting ResNet20 classification." << endl;
    Ctxt in = controller.read_input(filename);


    Ctxt resLayer1, resLayer2, resLayer3;

    auto start = start_time();

    resLayer1 = layer1(in);
    Serial::SerializeToFile("../checkpoints/layer1.bin", resLayer1, SerType::BINARY);

    Serial::DeserializeFromFile("../checkpoints/layer1.bin", resLayer1, SerType::BINARY);
    resLayer2 = layer2(resLayer1);
    Serial::SerializeToFile("../checkpoints/layer2.bin", resLayer2, SerType::BINARY);

    Serial::DeserializeFromFile("../checkpoints/layer2.bin", resLayer2, SerType::BINARY);
    resLayer3 = layer3(resLayer2);
    Serial::SerializeToFile("../checkpoints/layer3.bin", resLayer3, SerType::BINARY);

    controller.print(resLayer3, 4096);
    print_duration(start, "Whole network");

}

Ctxt layer3(const Ctxt& in) {
    cout << "Expected: [  0.0010650244, -0.0032426584, -0.0019287463, -0.0013246684,  0.0000994662,  ...]" << endl;
    controller.print(in, 5, "Received: ");

    double scale = 0.5;

    cout << "---Start: Layer3 - Block 1---" << endl;
    auto start = start_time();
    Ctxt boot_in = controller.bootstrap(in, true);
    vector<Ctxt> res1sx = controller.convbn3264sx(boot_in, 7, 1, scale, true); //Questo è lento
    vector<Ctxt> res1dx = controller.convbn3264dx(boot_in, 7, 1, scale, true); //Questo è lento
    controller.clear_bootstrapping_and_rotation_keys(8192);
    controller.load_rotation_keys("rotations-layer3-downsample.bin");
    //N.B. questo downsampling usa un chain index in meno - posso accelerare convbn3264sx
    Ctxt fullpackSx = controller.downsample256to64(res1sx[0], res1sx[1]);
    Ctxt fullpackDx = controller.downsample256to64(res1dx[0], res1dx[1]);
    res1sx.clear();
    res1dx.clear();
    controller.clear_rotation_keys();
    controller.load_bootstrapping_and_rotation_keys("rotations-layer3.bin", 4096);
    controller.num_slots = 4096;
    fullpackSx = controller.bootstrap(fullpackSx, true);
    fullpackSx = controller.relu(fullpackSx, scale, true);
    fullpackSx = controller.convbn3(fullpackSx, 7, 2, scale, true);
    Ctxt res1 = controller.add(fullpackSx, fullpackDx);
    res1 = controller.bootstrap(res1, true);
    res1 = controller.relu(res1, scale, true);
    print_duration(start, "Total");
    cout << "---End  : Layer3 - Block 1---" << endl;

    cout << "---Start: Layer3 - Block 2---" << endl;
    start = start_time();
    Ctxt res2;
    res2 = controller.convbn3(res1, 7, 2, scale, true);
    res2 = controller.bootstrap(res2, true);
    res2 = controller.relu(res2, scale, true);
    res2 = controller.convbn3(res2, 7, 2, scale, true);
    res2 = controller.add(res2, controller.mult(res1, scale));
    res2 = controller.bootstrap(res2, true);
    res2 = controller.relu(res2, scale, true);
    print_duration(start, "Total");
    cout << "---End  : Layer3 - Block 2---" << endl;

    cout << "---Start: Layer3 - Block 3---" << endl;
    start = start_time();
    Ctxt res3;
    res3 = controller.convbn3(res2, 7, 2, scale, true);
    res3 = controller.bootstrap(res3, true);
    res3 = controller.relu(res3, scale, true);
    res3 = controller.convbn3(res3, 7, 2, scale, true);
    res3 = controller.add(res3, controller.mult(res2, scale));
    res3 = controller.bootstrap(res3, true);
    res3 = controller.relu(res3, scale, true);
    print_duration(start, "Total");
    cout << "---End  : Layer3 - Block 3---" << endl;

    controller.print(res3, 4096);

    return res3;
}

Ctxt layer2(const Ctxt& in) {
    cout << "Expected: [  0.2059338669,  0.1945351973,  0.2480461048,  0.3422931948,  0.3457289039, ...]" << endl;
    controller.print(in, 5, "Received: ");

    double scale = 0.5;

    cout << "---Start: Layer2 - Block 1---" << endl;
    auto start = start_time();
    Ctxt boot_in = controller.bootstrap(in, true);
    vector<Ctxt> res1sx = controller.convbn1632sx(boot_in, 4, 1, scale, true); //Questo è lento
    vector<Ctxt> res1dx = controller.convbn1632dx(boot_in, 4, 1, scale, true); //Questo è lento
    controller.clear_bootstrapping_and_rotation_keys(16384);
    controller.load_rotation_keys("rotations-layer2-downsample.bin");
    Ctxt fullpackSx = controller.downsample1024to256(res1sx[0], res1sx[1]);
    Ctxt fullpackDx = controller.downsample1024to256(res1dx[0], res1dx[1]);
    res1sx.clear();
    res1dx.clear();
    controller.clear_rotation_keys();
    controller.load_bootstrapping_and_rotation_keys("rotations-layer2.bin", 8192);
    controller.num_slots = 8192;
    fullpackSx = controller.bootstrap(fullpackSx, true);
    fullpackSx = controller.relu(fullpackSx, scale, true);
    fullpackSx = controller.convbn2(fullpackSx, 4, 2, scale, true);
    Ctxt res1 = controller.add(fullpackSx, fullpackDx);
    res1 = controller.bootstrap(res1, true);
    res1 = controller.relu(res1, scale, true);
    print_duration(start, "Total");
    cout << "---End  : Layer2 - Block 1---" << endl;

    cout << "---Start: Layer2 - Block 2---" << endl;
    start = start_time();
    Ctxt res2;
    res2 = controller.convbn2(res1, 5, 1, scale, true);
    res2 = controller.bootstrap(res2, true);
    res2 = controller.relu(res2, scale, true);
    res2 = controller.convbn2(res2, 5, 2, scale, true);
    res2 = controller.add(res2, controller.mult(res1, scale));
    res2 = controller.bootstrap(res2, true);
    res2 = controller.relu(res2, scale, true);
    print_duration(start, "Total");
    cout << "---End  : Layer2 - Block 2---" << endl;

    cout << "---Start: Layer2 - Block 3---" << endl;
    start = start_time();
    Ctxt res3;
    res3 = controller.convbn2(res2, 6, 1, scale, true);
    res3 = controller.bootstrap(res3, true);
    res3 = controller.relu(res3, scale, true);
    res3 = controller.convbn2(res3, 6, 2, scale, true);
    res3 = controller.add(res3, controller.mult(res2, scale));
    res3 = controller.bootstrap(res3, true);
    res3 = controller.relu(res3, scale, true);
    print_duration(start, "Total");
    cout << "---End  : Layer2 - Block 3---" << endl;

    return res3;
}

Ctxt layer1(const Ctxt& in) {
    double scale = 0.5;

    cout << "---Start: Layer1 - Block 1---" << endl;
    auto start = start_time();
    Ctxt res1;
    res1 = controller.convbn(in, 1, 1, scale, true);
    res1 = controller.bootstrap(res1, true);
    res1 = controller.relu(res1, scale, true);
    res1 = controller.convbn(res1, 1, 2, scale, true);
    res1 = controller.add(res1, controller.mult(in, scale));
    res1 = controller.bootstrap(res1, true);
    res1 = controller.relu(res1, scale, true);
    print_duration(start, "Total");
    cout << "---End  : Layer1 - Block 1---" << endl;

    cout << "---Start: Layer1 - Block 2---" << endl;
    start = start_time();
    Ctxt res2;
    res2 = controller.convbn(res1, 2, 1, scale, true);
    res2 = controller.bootstrap(res2, true);
    res2 = controller.relu(res2, scale, true);
    res2 = controller.convbn(res2, 2, 2, scale, true);
    res2 = controller.add(res2, controller.mult(res1, scale));
    res2 = controller.bootstrap(res2, true);
    res2 = controller.relu(res2, scale, true);
    print_duration(start, "Total");
    cout << "---End  : Layer1 - Block 2---" << endl;

    cout << "---Start: Layer1 - Block 3---" << endl;
    start = start_time();
    Ctxt res3;
    res3 = controller.convbn(res2, 3, 1, scale, true);
    res3 = controller.bootstrap(res3, true);
    res3 = controller.relu(res3, scale, true);
    res3 = controller.convbn(res3, 3, 2, scale, true);
    res3 = controller.add(res3, controller.mult(res2, scale));
    res3 = controller.bootstrap(res3, true);
    res3 = controller.relu(res3, scale, true);
    print_duration(start, "Total");
    cout << "---End  : Layer1 - Block 3---" << endl;

    //controller.print(res3, 16384);
    return res3;
}

void check_arguments(int argc, char *argv[], bool default_generate) {
    for (int i = 1; i < argc; ++i) {
        if (string(argv[i]) == "context") {
            if (i + 1 < argc) { // Verifica se c'è un argomento successivo a "context"
                if (string(argv[i + 1]) == "generate") {
                    generate_context = true;
                    return;
                } else if (string(argv[i + 1]) == "load") {
                    generate_context = false;
                    return;
                } else {
                    cerr << R"(Unknown option passed to "context", insert either "generate" or "load")" << endl;
                    exit(1);
                }
            }
        }
    }

    generate_context = default_generate;
}
