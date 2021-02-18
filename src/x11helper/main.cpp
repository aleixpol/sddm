/***************************************************************************
* Copyright (C) 2019 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the
* Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
***************************************************************************/

#include <QCommandLineParser>
#include <QCoreApplication>

#include "runner.h"

#define TR(x) QT_TRANSLATE_NOOP("Command line parser", QStringLiteral(x))

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("sddm-x11-helper"));
    app.setOrganizationName(QStringLiteral("SDDM"));

    QCommandLineParser parser;
    parser.setApplicationDescription(TR("SDDM X11 Helper"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption socketOption(QStringLiteral("socket"),
                                    TR("Socket name"),
                                    TR("name"));
    parser.addOption(socketOption);

    QCommandLineOption testModeOption(QStringLiteral("test-mode"),
                                      TR("Enable test mode"));
    parser.addOption(testModeOption);

    QCommandLineOption seatOption(QStringLiteral("seat"),
                                  TR("Seat name"),
                                  TR("seat"));
    parser.addOption(seatOption);

    QCommandLineOption vtOption(QStringLiteral("vt"),
                                TR("Terminal identifier"),
                                TR("number"));
    parser.addOption(vtOption);

    QCommandLineOption authFileOption(QStringLiteral("auth"),
                                TR("Auth file path"),
                                TR("path"));
    parser.addOption(authFileOption);

    QCommandLineOption sessionOption(QStringLiteral("session"),
                                TR("Session command"),
                                TR("command"));
    parser.addOption(sessionOption);

    parser.process(app);

    if (!parser.isSet(socketOption)) {
        qCritical("Please specify a socket name with --socket");
        return 127;
    }
    if (!parser.isSet(seatOption)) {
        qCritical("Please specify a seat name with --seat");
        return 127;
    }
    if (!parser.isSet(vtOption)) {
        qCritical("Please specify a terminal identifier with --vt");
        return 127;
    }
    if (!parser.isSet(authFileOption)) {
        qCritical("Please specify an auth file with --auth");
        return 127;
    }

    bool vtOptionValid = false;
    int vt = parser.value(vtOption).toInt(&vtOptionValid);
    if (!vtOptionValid) {
        qCritical("Terminal identifier must be a number");
        return 127;
    }

    auto *runner = new Runner();
    runner->setSocketName(parser.value(socketOption));
    runner->setTestMode(parser.isSet(testModeOption));
    runner->setSeat(parser.value(seatOption));
    runner->setTerminalId(vt);
    runner->setAuthPath(parser.value(authFileOption));
    runner->setSession(parser.value(sessionOption));
    if (!runner->start()) {
        runner->stop();
        return 1;
    }

    return app.exec();
}
