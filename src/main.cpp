#include <iostream>
#include "FHEController.h"
#include <mach/mach.h>


void test();

void executeResNet20(string filename);
Ctxt layer1(const Ctxt& in);
Ctxt layer2(const Ctxt& in);

FHEController controller;

int main() {
    bool generate = false;

    if (generate) {
        controller.generate_context(true);
        controller.generate_bootstrapping_and_rotation_keys({1, -1, 32, -32, -1024, 16384},
                                                            16384,
                                                            true,
                                                            "rotations-layer1.bin");
        controller.generate_rotation_keys({1, 2, 4, 8, 64-16, -(1024 - 256), (1024 - 256) * 32, -8192},
                                          true,
                                          "rotations-layer2-downsample.bin");
        controller.generate_bootstrapping_and_rotation_keys({1, -1, 16, -16, -256, 8192},
                                          8192,
                                          true,
                                          "rotations-layer2.bin");
    } else {
        controller.load_context();
        controller.load_bootstrapping_and_rotation_keys("rotations-layer1.bin", 16384);
    }

    executeResNet20("../test_images/input_louis.bin");
}

void executeResNet20(string filename) {
    cout << "Starting ResNet20 classification." << endl;
    Ctxt in = controller.read_input(filename);


    Ctxt resLayer1, resLayer2, resLayer3;

    auto start = start_time();

    resLayer1 = layer1(in);
    Serial::SerializeToFile("../checkpoints/layer1.bin", resLayer1, SerType::BINARY);

    Serial::DeserializeFromFile("../checkpoints/layer1.bin", resLayer1, SerType::BINARY);
    resLayer2 = layer2(resLayer1);
    Serial::SerializeToFile("../checkpoints/layer2.bin", resLayer2, SerType::BINARY);

    print_duration(start, "Whole network");

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
    cout << "---End  : Layer1 - Block 2---" << endl;

    controller.print(res3, 8192);

    return in;
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

    cout << "Expected: [0.2059338669,  0.1945351973,  0.2480461048,  0.3422931948,  0.3457289039, ...]" << endl;
    controller.print(res3, 5, "Obtained: ");

    //controller.print(res3, 16384);
    return res3;
}

void test() {
    //TODO: Fare le convbn estendendo a 32000 slot, facendo 8 rotazioni :) e vedere se ci mette meno

    Ctxt c = controller.encrypt({0}, 30);
    auto start = start_time();
    c = controller.bootstrap(c);
    print_duration(start, "Bootstrapping for 16384 slots");

    start = start_time();
    controller.clear_bootstrapping_and_rotation_keys(16384);
    print_duration(start, "Clearing 16384 slots");

    start = start_time();
    controller.generate_bootstrapping_keys(8192);
    print_duration(start, "Generating 8192 keys");

    start = start_time();
    c = controller.encrypt({0}, 30, 8192);
    c = controller.bootstrap(c);
    print_duration(start, "Bootstrapping for 8192 slots");

    start = start_time();
    controller.clear_bootstrapping_and_rotation_keys(8192);
    print_duration(start, "Clearing 8192 slots");

    start = start_time();
    controller.generate_bootstrapping_keys(4096);
    print_duration(start, "Generating 4096 keys");

    start = start_time();
    c = controller.encrypt({0}, 30, 4096);
    c = controller.bootstrap(c);
    print_duration(start, "Bootstrapping for 4096 slots");


    controller.read_input("../test_images/input_louis.bin");
}
