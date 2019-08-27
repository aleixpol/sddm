/***************************************************************************
* Copyright (c) 2014-2019 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
* Copyright (c) 2013 Abdurrahman AVCI <abdurrahmanavci@gmail.com>
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

#include <QLocalServer>
#include <QLocalSocket>
#include <QUuid>
#include <QRegExp>

#include "XorgDisplayServer.h"

#include "Configuration.h"
#include "Constants.h"
#include "DaemonApp.h"
#include "Display.h"
#include "SafeDataStream.h"
#include "SignalHandler.h"
#include "Seat.h"
#include "x11helperprotocol.h"

#include <QDebug>
#include <QProcess>

#include <random>

#include <xcb/xcb.h>

#include <pwd.h>
#include <unistd.h>

namespace SDDM {
    XorgDisplayServer::XorgDisplayServer(Display *parent) : DisplayServer(parent) {
        createAuthFile();
        changeOwner(m_authPath);
    }

    XorgDisplayServer::~XorgDisplayServer() {
        stop();
    }

    const QString &XorgDisplayServer::display() const {
        return m_display;
    }

    const QString &XorgDisplayServer::authPath() const {
        return m_authPath;
    }

    QString XorgDisplayServer::sessionType() const {
        return QStringLiteral("x11");
    }

    const QString &XorgDisplayServer::cookie() const {
        return m_cookie;
    }

<<<<<<< HEAD
    bool XorgDisplayServer::addCookie(const QString &file) {
        // log message
        qDebug() << "Adding cookie to" << file;

        // Touch file
        QFile file_handler(file);
        file_handler.open(QIODevice::Append);
        file_handler.close();

        QString cmd = QStringLiteral("%1 -f %2 -q").arg(mainConfig.X11.XauthPath.get()).arg(file);

        // execute xauth
        FILE *fp = popen(qPrintable(cmd), "w");

        // check file
        if (!fp)
            return false;
        fprintf(fp, "remove %s\n", qPrintable(m_display));
        fprintf(fp, "add %s . %s\n", qPrintable(m_display), qPrintable(m_cookie));
        fprintf(fp, "exit\n");

        // close pipe
        return pclose(fp) == 0;
    }

=======
>>>>>>> b1ba4e7 (WIP Xorg user session)
    bool XorgDisplayServer::start() {
        // check flag
        if (m_started)
            return false;

<<<<<<< HEAD
        // create process
        process = new QProcess(this);

        // delete process on finish
        connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &XorgDisplayServer::finished);

        // log message
        qDebug() << "Display server starting...";

        // generate auth file.
        // For the X server's copy, the display number doesn't matter.
        // An empty file would result in no access control!
        m_display = QStringLiteral(":0");
        if(!addCookie(m_authPath)) {
            qCritical() << "Failed to write xauth file";
            return false;
        }

        if (daemonApp->testing()) {
            QStringList args;
            QDir x11socketDir(QStringLiteral("/tmp/.X11-unix"));
            int display = 100;
            while (x11socketDir.exists(QStringLiteral("X%1").arg(display))) {
                ++display;
            }
            m_display = QStringLiteral(":%1").arg(display);
            args << m_display << QStringLiteral("-auth") << m_authPath << QStringLiteral("-br") << QStringLiteral("-noreset") << QStringLiteral("-screen") << QStringLiteral("800x600");
            process->start(mainConfig.X11.XephyrPath.get(), args);


            // wait for display server to start
            if (!process->waitForStarted()) {
                // log message
                qCritical() << "Failed to start display server process.";

                // return fail
                return false;
            }
            emit started();
        } else {
            // set process environment
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            env.insert(QStringLiteral("XCURSOR_THEME"), mainConfig.Theme.CursorTheme.get());
            process->setProcessEnvironment(env);
            process->setProcessChannelMode(QProcess::ForwardedChannels);

            //create pipe for communicating with X server
            //0 == read from X, 1== write to from X
            int pipeFds[2];
            if (pipe(pipeFds) != 0) {
                qCritical("Could not create pipe to start X server");
            }

            // start display server
            QStringList args = mainConfig.X11.ServerArguments.get().split(QLatin1Char(' '), QString::SkipEmptyParts);
            args << QStringLiteral("-auth") << m_authPath
                 << QStringLiteral("-background") << QStringLiteral("none")
                 << QStringLiteral("-noreset")
                 << QStringLiteral("-displayfd") << QString::number(pipeFds[1])
                 << QStringLiteral("-seat") << displayPtr()->seat()->name();

            if (displayPtr()->seat()->name() == QLatin1String("seat0")) {
                args << QStringLiteral("vt%1").arg(displayPtr()->terminalId());
            }
            qDebug() << "Running:"
                     << qPrintable(mainConfig.X11.ServerPath.get())
                     << qPrintable(args.join(QLatin1Char(' ')));
            process->start(mainConfig.X11.ServerPath.get(), args);

            // wait for display server to start
            if (!process->waitForStarted()) {
                // log message
                qCritical() << "Failed to start display server process.";

                // return fail
                close(pipeFds[0]);
                return false;
            }

            // close the other side of pipe in our process, otherwise reading
            // from it may stuck even X server exit.
            close(pipeFds[1]);
=======
        // Start socket server
        if (m_socketServer)
            m_socketServer->deleteLater();
        m_socketServer = new QLocalServer(this);
        connect(m_socketServer, &QLocalServer::newConnection,
                this, &XorgDisplayServer::handleNewConnection);
        m_socketServer->listen(QStringLiteral("sddm-x11-helper-%1").arg(QUuid::createUuid().toString().replace(QRegExp(QStringLiteral("[{}]")), QString())));
        changeOwner(m_socketServer->fullServerName());

        // Command arguments
        QStringList args;
        args << QStringLiteral("--seat") << displayPtr()->seat()->name()
             << QStringLiteral("--vt") << QString::number(displayPtr()->terminalId())
             << QStringLiteral("--socket") << m_socketServer->fullServerName()
             << QStringLiteral("--auth") << m_authPath;
        if (daemonApp->testing())
            args << QStringLiteral("--test-mode");
>>>>>>> b1ba4e7 (WIP Xorg user session)

        qInfo("Starting X11 server...");

        // Run helper
        if (mainConfig.DisplayServer.get() == QStringLiteral("x11")) {
            if (m_process)
                m_process->deleteLater();
            m_process = new QProcess(this);
            connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                    this, &XorgDisplayServer::finished);

            m_process->start(QStringLiteral(LIBEXEC_INSTALL_DIR "/sddm-x11-helper"), args);
            if (!m_process->waitForStarted()) {
                qCritical("Failed to start display server");
                return false;
            }
        } else if (mainConfig.DisplayServer.get() == QStringLiteral("x11-user")) {
            if (m_auth)
                m_auth->deleteLater();
            m_auth = new Auth(this);
            m_auth->setVerbose(true);
            m_auth->setUser(QStringLiteral("sddm"));
            m_auth->setGreeter(true);
            connect(m_auth, &Auth::requestChanged, this, &XorgDisplayServer::onRequestChanged);
            connect(m_auth, &Auth::sessionStarted, this, &XorgDisplayServer::onSessionStarted);
            connect(m_auth, &Auth::finished, this, &XorgDisplayServer::onHelperFinished);
            connect(m_auth, &Auth::info, this, &XorgDisplayServer::authInfo);
            connect(m_auth, &Auth::error, this, &XorgDisplayServer::authError);

            QStringList cmd;
            cmd << QStringLiteral(LIBEXEC_INSTALL_DIR "/sddm-x11-helper")
                << args;

            QProcessEnvironment env;
            env.insert(QStringLiteral("XDG_SEAT"), displayPtr()->seat()->name());
            env.insert(QStringLiteral("XDG_SEAT_PATH"), daemonApp->displayManager()->seatPath(displayPtr()->seat()->name()));
            env.insert(QStringLiteral("XDG_SESSION_PATH"), daemonApp->displayManager()->sessionPath(QStringLiteral("Session%1").arg(daemonApp->newSessionId())));
            env.insert(QStringLiteral("XDG_VTNR"), QString::number(displayPtr()->terminalId()));
            env.insert(QStringLiteral("XDG_SESSION_CLASS"), QStringLiteral("greeter"));
            env.insert(QStringLiteral("XDG_SESSION_TYPE"), QStringLiteral("x11"));
            env.insert(QStringLiteral("XORG_RUN_AS_USER_OK"), QStringLiteral("1"));
            m_auth->insertEnvironment(env);

            m_auth->setSession(cmd.join(QLatin1Char(' ')));
            m_auth->start();
        }

<<<<<<< HEAD
        // The file is also used by the greeter, which does care about the
        // display number. Write the proper entry, if it's different.
        if(m_display != QStringLiteral(":0")) {
            if(!addCookie(m_authPath)) {
                qCritical() << "Failed to write xauth file";
                return false;
            }
        }
        changeOwner(m_authPath);

        // set flag
        m_started = true;

        // return success
=======
>>>>>>> b1ba4e7 (WIP Xorg user session)
        return true;
    }

    void XorgDisplayServer::stop() {
        // check flag
        if (!m_started)
            return;

        // log message
        qDebug() << "Stopping X11 server";

        if (m_process) {
            // terminate process
            m_process->terminate();

            // wait for finished
            if (!m_process->waitForFinished(5000))
                m_process->kill();
        }
    }

    void XorgDisplayServer::finished() {
        // check flag
        if (!m_started)
            return;

        // reset flag
        m_started = false;

        // log message
        qInfo("X11 server stopped");

        // Stop socket server
        if (m_socketServer) {
            m_socketServer->close();
            m_socketServer->deleteLater();
            m_socketServer = nullptr;
        }

        // clean up
        if (m_process) {
            m_process->deleteLater();
            m_process = nullptr;
        }

        // Remove authority file
        if (!m_authPath.isEmpty() && QFile::exists(m_authPath))
            QFile::remove(m_authPath);

        // emit signal
        emit stopped();
    }

    void XorgDisplayServer::createAuthFile()
    {
        // Create auth directory
        QString authDir = QStringLiteral(RUNTIME_DIR);
        if (daemonApp->testing())
            authDir = QStringLiteral(".");
        QDir().mkpath(authDir);

        // Set auth path
        m_authPath = QStringLiteral("%1/%2").arg(authDir).arg(QUuid::createUuid().toString());

        // Generate cookie
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 15);

        // Reseve 32 bytes
        m_cookie.reserve(32);

        // Create a random hexadecimal number
        const char *digits = "0123456789abcdef";
        for (int i = 0; i < 32; ++i)
            m_cookie[i] = digits[dis(gen)];
    }

    void XorgDisplayServer::addCookie(const QString &fileName)
    {
        qDebug() << "Adding cookie to" << fileName;

        // Touch file
        QFile fileHandler(fileName);
        fileHandler.open(QIODevice::Append);
        fileHandler.close();

        // Run xauth
        QString cmd = QStringLiteral("%1 -f %2 -q").arg(mainConfig.X11.XauthPath.get()).arg(fileName);
        FILE *fp = popen(qPrintable(cmd), "w");
        if (!fp)
            return;
        fprintf(fp, "remove %s\n", qPrintable(m_display));
        fprintf(fp, "add %s . %s\n", qPrintable(m_display), qPrintable(m_cookie));
        fprintf(fp, "exit\n");
        pclose(fp);
    }

    void XorgDisplayServer::changeOwner(const QString &fileName)
    {
        // Change the owner and group of the auth file to the sddm user
        struct passwd *pw = getpwnam("sddm");
        if (pw) {
            if (chown(qPrintable(fileName), pw->pw_uid, pw->pw_gid) == -1)
                qWarning() << "Failed to change owner of the auth file.";
        } else {
            qWarning() << "Failed to find the sddm user. Owner of the auth file will not be changed.";
        }
    }

    void XorgDisplayServer::handleNewConnection()
    {
        while (m_socketServer->hasPendingConnections()) {
            X11HelperMessage m = X11HelperMessage::Unknown;
            QLocalSocket *socket = m_socketServer->nextPendingConnection();
            SafeDataStream str(socket);
            str.receive();
            str >> m;

            if (m == X11HelperMessage::Started) {
                str >> m_display;

                qInfo("X11 server started");

                // Add cookie to auth file
                addCookie(m_authPath);

                m_started = true;
                emit started();
            }
        }
    }

    void XorgDisplayServer::onRequestChanged()
    {
        m_auth->request()->setFinishAutomatically(true);
    }

    void XorgDisplayServer::onSessionStarted(bool success)
    {
        if (!success)
            qWarning("X11 server failed to start");
    }

    void XorgDisplayServer::onHelperFinished(Auth::HelperExitStatus status)
    {
        finished();

        m_auth->deleteLater();
        m_auth = nullptr;
    }

    void XorgDisplayServer::authInfo(const QString &message, Auth::Info info)
    {
        Q_UNUSED(info)
        qDebug("Message from X11 server: %s", qPrintable(message));
    }

    void XorgDisplayServer::authError(const QString &message, Auth::Error error)
    {
        Q_UNUSED(error)
        qWarning("Error from X11 server: %s", qPrintable(message));
    }
}
