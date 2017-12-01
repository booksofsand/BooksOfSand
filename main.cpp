#include "sandboxwindow.h"
#include <QApplication>
#include <iostream>
#include <vector>
#include <QtWidgets>

// MM: preliminary attempt to merge QApplication with SARndbox Vrui Application

using namespace std;

int main(int argc, char *argv[])
{
    // make an application
    QApplication a(argc, argv);

    // initialize window with optional filename
    std::vector<std::string> filenames;
    for (int i = 1; i < argc; i++) {
        char* arg = argv[i];

        // if arg isn't in dict of flag options, assume it's a filename
        filenames.push_back(arg);
    }

    SandboxWindow* box = new SandboxWindow(filenames);

    // return the executable
    return a.exec();
}
