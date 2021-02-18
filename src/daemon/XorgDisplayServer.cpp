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

    bool XorgDisplayServer::addCookie(const QString &file) {
        // log message
        qDebug() << "Adding cookie to" << file << "for" << m_display;

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

    bool XorgDisplayServer::start() {
        // check flag
        if (m_started)
            return false;
        // Start socket server
        if (m_socketServer)
            m_socketServer->deleteLater();
        m_socketServer = new QLocalServer(this);
        connect(m_socketServer, &QLocalServer::newConnection,
                this, &XorgDisplayServer::handleNewConnection);
        m_socketServer->listen(QStringLiteral("sddm-x11-helper-%1").arg(QUuid::createUuid().toString().replace(QRegExp(QStringLiteral("[{}]")), QString())));
        changeOwner(m_socketServer->fullServerName());

        // Command arguments
        m_args = QStringList { QStringLiteral("--seat"), displayPtr()->seat()->name()
                             , QStringLiteral("--vt"),  QString::number(displayPtr()->terminalId())
                             , QStringLiteral("--socket"),  m_socketServer->fullServerName()
                             , QStringLiteral("--auth"),  m_authPath };
        if (daemonApp->testing())
            m_args << QStringLiteral("--test-mode");

        qInfo() << "Starting X11 server..." << mainConfig.DisplayServer.get();

        // Run helper
        if (mainConfig.DisplayServer.get() == QStringLiteral("x11")) {
            if (m_process)
                m_process->deleteLater();
            m_process = new QProcess(this);
            connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                    this, &XorgDisplayServer::finished);

            m_process->start(QStringLiteral(LIBEXEC_INSTALL_DIR "/sddm-x11-helper"), m_args);
            if (!m_process->waitForStarted()) {
                qCritical("Failed to start display server");
                return false;
            }
        } else {
            //Should be using rootless xorg
        }
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

    void XorgDisplayServer::changeOwner(const QString &fileName)
    {
        // Change the owner and group of the auth file to the sddm user
        struct passwd *pw = getpwnam("test");
        if (pw) {
            if (chown(qPrintable(fileName), pw->pw_uid, pw->pw_gid) == -1)
                qWarning() << "Failed to change owner of the auth file." << fileName;
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

    QString XorgDisplayServer::userCompositorCommand() const
    {
        QString cmd;
        if (mainConfig.DisplayServer.get() == QStringLiteral("x11-user") || mainConfig.DisplayServer.get() == QStringLiteral("wayland")) {
            cmd = QStringLiteral(LIBEXEC_INSTALL_DIR "/sddm-x11-helper ") + m_args.join(QLatin1Char(' '));
        }
        qDebug() << "userCompositorCommand" << cmd << mainConfig.DisplayServer.get();
        return cmd;
    }
}
