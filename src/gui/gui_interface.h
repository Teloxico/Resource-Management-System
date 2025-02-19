// File: src/gui/gui_interface.h

#ifndef GUI_INTERFACE_H
#define GUI_INTERFACE_H

#include <QWidget>

/**
 * @class GUIInterface
 * @brief Graphical User Interface for Resource Monitor.
 *
 * Provides a Qt-based GUI to display real-time resource usage metrics.
 */
class GUIInterface : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Constructs a new GUIInterface object.
     * @param parent Pointer to the parent widget.
     */
    GUIInterface(QWidget *parent = nullptr);

    /**
     * @brief Destructs the GUIInterface object.
     */
    ~GUIInterface();

private:
    // UI components and methods

private slots:
    /**
     * @brief Slot to update the displayed metrics.
     */
    void updateMetrics();
};

#endif // GUI_INTERFACE_H

