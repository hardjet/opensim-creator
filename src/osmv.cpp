﻿#include "application.hpp"

#include "fd_simulation.hpp"
#include "loading_screen.hpp"
#include "opensim_wrapper.hpp"
#include "splash_screen.hpp"
#include "config.hpp"

#include <OpenSim/Simulation/Model/Model.h>

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

static const char usage[] = R"(usage: osmv [--help] [fd] MODEL.osim
)";
static const char help[] = R"(OPTIONS
    --help
        Show this help
)";

static bool skip_prefix(char const* prefix, char const* s, char const** out) {
    do {
        if (*prefix == '\0' and (*s == '\0' or *s == '=')) {
            *out = s;
            return true;
        }
    } while (*prefix++ == *s++);

    return false;
}

static bool safe_parse_double(char const* s, double* out) {
    char* end;
    double v = strtod(s, &end);

    if (*end != '\0' or v >= HUGE_VAL or v <= -HUGE_VAL) {
        return false;  // not a number
    }

    *out = v;
    return true;
}

int main(int argc, char** argv) {
    --argc;
    ++argv;

    // handle named flag args (e.g. --help)
    while (argc) {
        char const* arg = *argv;

        if (*arg != '-') {
            break;
        }

        if (skip_prefix("--help", arg, &arg)) {
            std::cout << usage << '\n' << help << '\n';
            return EXIT_SUCCESS;
        }

        ++argv;
        --argc;
    }

    // global init stuff
    {
        std::filesystem::path geometry_dir = osmv::config::resource_path("geometry");
        OpenSim::ModelVisualizer::addDirToGeometrySearchPaths(geometry_dir.string());
    }

    // no args: show splash screen
    if (argc <= 0) {
        osmv::Application application;
        application.start_render_loop<osmv::Splash_screen>();

        return EXIT_SUCCESS;
    }

    // 'fd' command:
    //
    // if the first unnamed argument to osmv is 'fd' then the caller wants to run an fd simulation
    // using the same parameters as the visualizer. This is currently here for debugging
    if (not std::strcmp(argv[0], "fd")) {

        if (argc != 3) {
            std::cerr << "osmv: fd: incorrect number of arguments: two (MODEL.osim final_time) expected\n";
            return EXIT_FAILURE;
        }

        char const* osim_path = argv[1];
        double final_time;
        if (not safe_parse_double(argv[2], &final_time)) {
            std::cerr << "osmv: fd: invalid final time given (not a number)\n";
            return EXIT_FAILURE;
        }
        if (final_time < 0.0) {
            std::cerr << "osmv: fd: invalid final time given (negative)\n";
            return EXIT_FAILURE;
        }

        osmv::Model m{osim_path};
        m->finalizeFromProperties();
        osmv::run_fd_simulation(m);

        return EXIT_SUCCESS;
    }

    // no subcommand command (but args): show the UI
    //
    // the reason the subcommands are designed this way (rather than having a separate 'gui'
    // subcommand) is because most OS desktop managers call `binary.exe <arg>` when users click on
    // a file in the OS's file explorer
    osmv::Application application;
    application.start_render_loop<osmv::Loading_screen>(argv[0]);

    return EXIT_SUCCESS;
}
