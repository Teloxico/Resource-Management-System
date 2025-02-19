// File: src/gui/main.cpp

#include <QApplication>
#include "gui_interface.h"

/**
 * @brief Entry point for the GUI application.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return int Exit code.
 */
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    GUIInterface window;
    window.setWindowTitle("Resource Monitor");
    window.resize(800, 600);
    window.show();
    return app.exec();
}

