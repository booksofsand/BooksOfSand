#include "helpers.h"

// NOTIFY
// Print received message but do not exit program
void notify(std::string msg) {
    std::cout << "NOTICE: " << msg << std::endl;
}

// ERROR
// Print received message and exit program
void error(std::string msg) {
    if (msg == "")
        std::cerr << "UNKNOWN ERROR." << std::endl;
    else
        std::cerr << "ERROR: " << msg << std::endl;
    exit(1);
}
