#include "sandboxwindow.h"
#include <QApplication>
#include <iostream>
#include <vector>
#include <QtWidgets>

using namespace std;

int main(int argc, char *argv[])
{
    // make an application
    QApplication a(argc, argv);

    // initialize window with optional filename
    std::vector<std::string> filenames;
    for (int i = 1; i < argc; i++) {
        char* arg = argv[i];
        //std::cout << "arg: " << arg << std::endl; // MM: testing

        // if arg isn't in dict of flag options, assume it's a filename
        filenames.push_back(arg);
    }

    /*
    QWidget window;
    window.resize(320, 240);
    window.setWindowTitle
          (QApplication::translate("childwidget", "Child widget"));
    window.show();
    */
    
    SandboxWindow* box = new SandboxWindow(filenames);
 
    cout << "hello" << endl;  

    // return the executable
    return a.exec();
}
