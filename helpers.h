#ifndef HELPERS_H
#define HELPERS_H

#include <string>
#include <iostream>

// NOTIFY
// Print received message but do not exit program
void notify(std::string msg);

// ERROR
// Print received message and exit program
void error(std::string msg="");

#endif // HELPERS_H
