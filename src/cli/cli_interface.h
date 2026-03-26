// File: src/cli/cli_interface.h

#ifndef CLI_INTERFACE_H
#define CLI_INTERFACE_H

#include <string>

/**
 * @class CLIInterface
 * @brief Handles Command Line Interface interactions.
 *
 * Provides methods to display help messages and control the monitoring process.
 */
class CLIInterface {
public:
    /**
     * @brief Display help information to the user.
     */
    static void displayHelp();

    /**
     * @brief Start the resource monitoring process.
     */
    static void startMonitoring();

    // Other CLI-related methods can be declared here
};

#endif // CLI_INTERFACE_H

