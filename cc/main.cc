#include <cstdio>
#include <iostream>
#include <stdbool.h>
#include <signal.h>
#include "Manager.h"
#include "MasterClock.h"
#include "Structures.h"
#include <chrono>
#include <thread>
#include <condition_variable>
#include <string>
#include <experimental/filesystem>

// ####################################################################################
#define BUFFER_THRESHOLD 2048
// ####################################################################################

// Signal handler function to handle termination signal (SIGINT)
void handleTermination(int signal) {
    printf("\nReceived termination signal. Cleaning up resources...\n");
    // Handle cleanup process here if needed
    SDL_Quit();
    exit(EXIT_SUCCESS);
}

namespace fs = std::experimental::filesystem;

bool endsWith(const std::string& str, const std::string& suffix) {
    if (str.size() < suffix.size()) {
        return false;
    }
    return str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string argumentHandler(int argc, char* argv[]) {
    std::string configFilePath;

    // Check if the argument count is at least 2 (the first argument is the program name)
    if (argc < 2) {
        std::cout << "Error: Config file is missing." << std::endl;
        std::cout << "Usage: " << argv[0] << " [-c|--config] config_file.yml" << std::endl;
        handleTermination(1);
    }

    // Loop through the arguments to find the config file path
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-c" || arg == "--config") {
            // Check if the next argument exists (the config file path)
            if (i + 1 < argc) {
                configFilePath = argv[i + 1];
                break;
            } else {
                std::cout << "Error: Config file path is missing." << std::endl;
                std::cout << "Usage: " << argv[0] << " [-c|--config] config_file.yml" << std::endl;
                handleTermination(1);
            }
        }
    }

    // Check if the config file path is provided
    if (configFilePath.empty()) {
        std::cout << "Error: Config file path is missing." << std::endl;
        std::cout << "Usage: " << argv[0] << " [-c|--config] config_file.yml" << std::endl;
        handleTermination(-1);
    }

    // Check if the config file exists and is a .yml file
    if (fs::exists(configFilePath) && endsWith(configFilePath, ".yml")) {
        std::cout << "Config file: " << configFilePath << std::endl;

        std::vector<std::string> yamlFiles;
        for (const auto& entry : fs::directory_iterator(".")) {
            if (fs::is_regular_file(entry) && endsWith(entry.path().string(), ".yml")) {
                yamlFiles.push_back(entry.path().filename().string());
            }
        }

        if (!yamlFiles.empty()) {
            std::cout << "YAML files in the current directory:" << std::endl;
            for (const auto& file : yamlFiles) {
                std::cout << " - " << file << std::endl;
            }
        } else {
            std::cout << "No .yml files found in the current directory." << std::endl;
        }
    } else {
        std::cout << "Error: Invalid or missing config file: " << configFilePath << std::endl;
    }

    return configFilePath;
}

int main(int argc, char* argv[]) {
    // Set up the termination signal handler
    signal(SIGINT, handleTermination);
    const std::string configFile = argumentHandler(argc, argv);
    YAML::Node config = YAML::LoadFile(configFile);
    
    YAML::Node audioMixerConfig = config["audioMixer"];
    YAML::Node windowConfig = config["window"];
    YAML::Node notesConfig = config["notes"];
    YAML::Node verbosity = config["verbosity"];
    bool mainVerbose = verbosity["mainVerbose"].as<bool>();

    if (mainVerbose) {
        printf("Main::Verbosity Initialized.\n");
    }

    // Read the BPM value from the YAML config
    double bpm = config["bpm"].as<double>();
    double beatDivisions = config["beatDivisions"].as<double>();
    double kp1LoopDuration = 1/config["kp1LoopDuration"].as<double>();
    double kp2LoopDuration = 1/config["kp2LoopDuration"].as<double>();
    double kp3LoopDuration = 1/config["kp3LoopDuration"].as<double>();
    double kp4LoopDuration = 1/config["kp4LoopDuration"].as<double>();
    double kp5LoopDuration = 1/config["kp5LoopDuration"].as<double>();
    double kp6LoopDuration = 1/config["kp6LoopDuration"].as<double>();
    double kp7LoopDuration = 1/config["kp7LoopDuration"].as<double>();
    double kp8LoopDuration = 1/config["kp8LoopDuration"].as<double>();
    double kp9LoopDuration = 1/config["kp9LoopDuration"].as<double>();

    bool loopKP1State = false;
    bool loopKP2State = false;
    bool loopKP3State = false;
    bool loopKP4State = false;
    bool loopKP5State = false;
    bool loopKP6State = false;
    bool loopKP7State = false;
    bool loopKP8State = false;
    bool loopKP9State = false;

    const std::unordered_map<std::string, std::pair<bool*, double>> stringBoolPairs = {
        {"KP1", {&loopKP1State, kp1LoopDuration}},
        {"KP2", {&loopKP2State, kp2LoopDuration}},
        {"KP3", {&loopKP3State, kp3LoopDuration}},
        {"KP4", {&loopKP4State, kp4LoopDuration}},
        {"KP5", {&loopKP5State, kp5LoopDuration}},
        {"KP6", {&loopKP6State, kp6LoopDuration}},
        {"KP7", {&loopKP7State, kp7LoopDuration}},
        {"KP8", {&loopKP8State, kp8LoopDuration}},
        {"KP9", {&loopKP9State, kp9LoopDuration}}
    };

    if (mainVerbose) {
        printf("Main::Starting MasterClock.\n");
    }
    bool superVerbose = verbosity["superVerbose"].as<bool>();
    bool timeVerbose = verbosity["timeVerbose"].as<bool>();

    MasterClock masterClock(bpm, beatDivisions, 
        verbosity["masterClockVerbose"].as<bool>(), 
        superVerbose, 
        timeVerbose);
    masterClock.start();
    printf("superVerbose from main: %d.\n", superVerbose);
    if (mainVerbose) {
        printf("Main::Starting SDL Video, Audio, and Events.\n");
    }
    // Intialize SDL and audio subsystem
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) < 0) {
        printf("---SDL initialization failed: %s\n", SDL_GetError());
        return 1;
    }
    if (mainVerbose) {
        printf("Main::Starting Manager.\n");
    }

    std::unique_ptr<Manager> manager(new Manager(masterClock, 
        stringBoolPairs, verbosity, notesConfig, windowConfig, audioMixerConfig, superVerbose, timeVerbose));
    std::thread mainThread([&]() {
        try {
            masterClock.executeScheduledBatches();
        } catch (const std::exception& e) {
            std::cerr << "An exception occurred while running the MasterCLock: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "An unknown exception occurred while running the MasterClock." << std::endl;
        }
    });
    
    mainThread.join();
    masterClock.stop();

    // Clean up SDL and other resources
    SDL_Quit();

    return 0;
}