// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XCOIN_DATADIR_CHOICE_H
#define XCOIN_DATADIR_CHOICE_H

#include <string>

/**
 * Which data directory a GUI launch should use.
 *
 * A -datadir on the command line wins and is left alone.
 * Otherwise the saved strDataDir is used when it is set and is not the
 * OS default. An empty saved value, or a saved value equal to the OS
 * default, does not set -datadir, so xcoin.conf in the default folder
 * can still override it (same rule as 1.0.24).
 */
struct DataDirChoice {
    bool commandLineWins;
    bool setDataDirArg;
    std::string path;
};

inline DataDirChoice ChooseStartupDataDir(bool commandLineSet,
                                          const std::string& commandLine,
                                          const std::string& saved,
                                          const std::string& osDefault)
{
    DataDirChoice choice;
    choice.commandLineWins = commandLineSet && !commandLine.empty();
    if (choice.commandLineWins) {
        choice.setDataDirArg = false;
        choice.path = commandLine;
        return choice;
    }
    choice.path = saved.empty() ? osDefault : saved;
    choice.setDataDirArg = (choice.path != osDefault);
    return choice;
}

#endif
