#include <iostream>
#include "FHEController.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

void check_arguments(int argc, char *argv[], bool default_generate);
vector<double> read_jpg_image(const char *filename);

void executeResNet20(const string& filename);


Ctxt initial_layer(const Ctxt& in);
Ctxt layer1(const Ctxt& in);
Ctxt layer2(const Ctxt& in);
Ctxt layer3(const Ctxt& in);
Ctxt final_layer(const Ctxt& in);

FHEController controller;

bool generate_context;
string input_filename;

/*
 * TODO:
 * 1) Migliorare convbn sfruttando tutti gli slot del ciphertext
 */

int main(int argc, char *argv[]) {
    //TODO: possibile che il bootstrap a 8192 ci metta lo stesso tempo? indaga

    check_arguments(argc, argv, false);

    if (generate_context) {
        controller.generate_context(true);

        cout << "Context built." << endl;

        controller.generate_bootstrapping_and_rotation_keys({1, -1, 32, -32, -1024},
                                                            16384,
                                                            true, "rotations-layer1.bin");
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

        controller.generate_rotation_keys({1, 2, 4, 8, 16, 32, -15, 64, 128, 256, 512, 1024, 2048}, true, "rotations-finallayer.bin");

        */

    } else {
        controller.load_context();
    }

    executeResNet20("../test_images/input_louis.bin");
}

void executeResNet20(const string& filename) {
    cout << "Starting ResNet20 classification." << endl;

    Ctxt firstLayer, resLayer1, resLayer2, resLayer3, finalRes;

    auto start = start_time();

    bool print_intermediate_values = true;
    bool print_bootstrap_precision = false;

    if (input_filename.empty()) {
        input_filename = "../inputs/louis.jpg";
        cout << "You did not set any input, I use ../'inputs/louis.jpg'." << endl;
    }

    vector<double> input_image = read_jpg_image(input_filename.c_str());

    Ctxt in = controller.encrypt(input_image, controller.circuit_depth - 4 - get_relu_depth(controller.relu_degree));

    controller.load_bootstrapping_and_rotation_keys("rotations-layer1.bin", 16384);

    if (print_bootstrap_precision){
        controller.bootstrap_precision(in);
    }

    firstLayer = initial_layer(in);

    resLayer1 = layer1(firstLayer);
    Serial::SerializeToFile("../checkpoints/layer1.bin", resLayer1, SerType::BINARY);
    if (print_intermediate_values) controller.print(resLayer1, 16384, "Layer 1: ");

    Serial::DeserializeFromFile("../checkpoints/layer1.bin", resLayer1, SerType::BINARY);
    resLayer2 = layer2(resLayer1);
    Serial::SerializeToFile("../checkpoints/layer2.bin", resLayer2, SerType::BINARY);
    if (print_intermediate_values) controller.print(resLayer2, 8192, "Layer 2: ");

    Serial::DeserializeFromFile("../checkpoints/layer2.bin", resLayer2, SerType::BINARY);
    resLayer3 = layer3(resLayer2);
    Serial::SerializeToFile("../checkpoints/layer3.bin", resLayer3, SerType::BINARY);
    if (print_intermediate_values) controller.print(resLayer3, 4096, "Layer 3: ");

    Serial::DeserializeFromFile("../checkpoints/layer3.bin", resLayer3, SerType::BINARY);
    finalRes = final_layer(resLayer3);
    Serial::SerializeToFile("../checkpoints/finalres.bin", finalRes, SerType::BINARY);

    print_duration(start, "Whole network");
}

Ctxt initial_layer(const Ctxt& in) {
    double scale = 0.5;

    Ctxt res = controller.convbn_initial(in, scale, true);
    res = controller.relu(res, scale, true);

    return res;
}

Ctxt final_layer(const Ctxt& in) {
    controller.clear_bootstrapping_and_rotation_keys(4096);
    controller.load_rotation_keys("rotations-finallayer.bin");

    controller.num_slots = 4096;

    Ptxt weight = controller.encode(read_fc_weight("../weights/fc.bin"), in->GetLevel(), controller.num_slots);

    Ctxt res = controller.rotsum(in, 64);
    res = controller.mult(res, controller.mask_mod(64, res->GetLevel(), 1.0 / 64.0));

    //From here, I need 10 repetitons, but I use 16 since *repeat* goes exponentially
    res = controller.repeat(res, 16);
    res = controller.mult(res, weight);
    res = controller.rotsum_padded(res, 64);

    controller.print(res, 10, "FC Output: ");

    return res;
}

Ctxt layer3(const Ctxt& in) {
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
    res2 = controller.convbn3(res1, 8, 1, scale, true);
    res2 = controller.bootstrap(res2, true);
    res2 = controller.relu(res2, scale, true);
    res2 = controller.convbn3(res2, 8, 2, scale, true);
    res2 = controller.add(res2, controller.mult(res1, scale));
    res2 = controller.bootstrap(res2, true);
    res2 = controller.relu(res2, scale, true);
    print_duration(start, "Total");
    cout << "---End  : Layer3 - Block 2---" << endl;

    cout << "---Start: Layer3 - Block 3---" << endl;
    start = start_time();
    Ctxt res3;

    res3 = controller.convbn3(res2, 9, 1, scale, true);
    res3 = controller.bootstrap(res3, true);
    res3 = controller.relu(res3, scale, true);
    res3 = controller.convbn3(res3, 9, 2, scale, true);
    res3 = controller.add(res3, controller.mult(res2, 0.2));
    res3 = controller.bootstrap(res3, true);
    res3 = controller.relu(res3, 0.2, true);
    res3 = controller.bootstrap(res3, true);
    print_duration(start, "Total");
    cout << "---End  : Layer3 - Block 3---" << endl;



    return res3;
}

Ctxt layer2(const Ctxt& in) {
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

    return res3;
}

void check_arguments(int argc, char *argv[], bool default_generate) {
    for (int i = 1; i < argc; ++i) {
        if (string(argv[i]) == "context") {
            if (i + 1 < argc) { // Verifica se c'è un argomento successivo a "context"
                controller.parameters_folder = string(argv[i + 1]);
                cout << "Context folder set to: \"" << controller.parameters_folder << "\"." << endl;
                generate_context = false;
            }
        }
        if (string(argv[i]) == "input") {
            if (i + 1 < argc) { // Verifica se c'è un argomento successivo a "input"
                input_filename = "../" + string(argv[i + 1]);
                cout << "Input image set to: \"" << input_filename << "\"." << endl;
            }
        }
    }

    generate_context = default_generate;
}

vector<double> read_jpg_image(const char *filename) {
    int width = 32;
    int height = 32;
    int channels = 3;
    unsigned char* image_data = stbi_load(filename, &width, &height, &channels, 0);

    if (!image_data) {
        cerr << "Could not load the image in " << filename << endl;
        return vector<double>();
    }

    vector<double> imageVector;
    imageVector.reserve(width * height * channels);

    for (int i = 0; i < width * height; ++i) {
        //Channel R
        imageVector.push_back(static_cast<double>(image_data[3 * i]) / 255.0f);
    }
    for (int i = 0; i < width * height; ++i) {
        //Channel G
        imageVector.push_back(static_cast<double>(image_data[1 + 3 * i]) / 255.0f);
    }
    for (int i = 0; i < width * height; ++i) {
        //Channel B
        imageVector.push_back(static_cast<double>(image_data[2 + 3 * i]) / 255.0f);
    }

    stbi_image_free(image_data);

    return imageVector;
}