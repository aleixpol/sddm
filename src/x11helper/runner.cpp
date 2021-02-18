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

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QLocalSocket>
#include <QProcess>

#include "runner.h"

#include "Configuration.h"
#include "Constants.h"
#include "SafeDataStream.h"
#include "x11helperprotocol.h"

#include <unistd.h>

Runner::Runner(QObject *parent)
    : QObject(parent)
    , m_socket(new QLocalSocket(this))
{
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit,
            this, &Runner::deleteLater);
}

Runner::~Runner()
{
    m_socket->close();

    stop();
}

QString Runner::socketName() const
{
    return m_socketName;
}

void Runner::setSocketName(const QString &name)
{
    if (!m_socketName.isEmpty()) {
        qWarning("Cannot set socket name after initialization");
        return;
    }

    m_socketName = name;

    m_socket->connectToServer(m_socketName, QIODevice::ReadWrite | QIODevice::Unbuffered);
    if (!m_socket->waitForConnected(2500)) {
        qCritical("Failed to connect to the daemon. %s: %s", qPrintable(m_socketName), qPrintable(m_socket->errorString()));
    }
}

bool Runner::isTestMode() const
{
    return m_testMode;
}

void Runner::setTestMode(bool testMode)
{
    m_testMode = testMode;
}

QString Runner::display() const
{
    return m_display;
}

QString Runner::seat() const
{
    return m_seat;
}

void Runner::setSeat(const QString &seat)
{
    if (!m_seat.isEmpty()) {
        qWarning("Cannot set seat name after initialization");
        return;
    }

    m_seat = seat;
}

int Runner::terminalId() const
{
    return m_terminalId;
}

void Runner::setTerminalId(int id)
{
    m_terminalId = id;
}

QString Runner::authPath() const
{
    return m_authPath;
}

void Runner::setAuthPath(const QString &path)
{
    if (!m_authPath.isEmpty()) {
        qWarning("Cannot set auth file path after initialization");
        return;
    }

    m_authPath = path;
}

bool Runner::start()
{
    if (m_process)
        m_process->deleteLater();

    m_process = new QProcess(this);
//     m_process->setProcessChannelMode(QProcess::ForwardedChannels);
    connect(m_process, &QProcess::started,
            this, &Runner::started);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &Runner::finished);

    if (m_testMode) {
        QStringList args;
        QDir x11socketDir(QStringLiteral("/tmp/.X11-unix"));
        int display = 100;
        while (x11socketDir.exists(QStringLiteral("X%1").arg(display))) {
            ++display;
        }
        m_display = QStringLiteral(":%1").arg(display);
        args << m_display << QStringLiteral("-ac")
             << QStringLiteral("-br")
             << QStringLiteral("-noreset")
             << QStringLiteral("-screen") << QStringLiteral("800x600");
        m_process->start(SDDM::mainConfig.X11.XephyrPath.get(), args);
        if (!m_process->waitForStarted()) {
            qCritical("Failed to start X11 server process");
            return false;
        }
    } else {
        // Set process environment
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("XCURSOR_THEME"), SDDM::mainConfig.Theme.CursorTheme.get());
        env.insert(QStringLiteral("XORG_RUN_AS_USER_OK"), QStringLiteral("1"));
        m_process->setProcessEnvironment(env);

        // Create pipe for communicating with X server
        // 0 == read from X, 1 == write to from X
        int pipeFds[2];
        if (pipe(pipeFds) != 0) {
            qCritical("Could not create pipe to start X server");
            return false;
        }

        // Start display server
        QStringList args = SDDM::mainConfig.X11.ServerArguments.get().split(QLatin1Char(' '), Qt::SkipEmptyParts);
        args << QStringLiteral("-auth") << m_authPath
             << QStringLiteral("-background") << QStringLiteral("none")
             << QStringLiteral("-noreset")
             << QStringLiteral("-keeptty")
             << QStringLiteral("-verbose") << QStringLiteral("7")
             << QStringLiteral("-displayfd") << QString::number(pipeFds[1])
                << QStringLiteral("-seat") << m_seat;
        if (m_seat == QLatin1String("seat0"))
            args << QStringLiteral("vt%1").arg(m_terminalId);
        qDebug() << "Running:"
                 << qPrintable(SDDM::mainConfig.X11.ServerPath.get())
                 << qPrintable(args.join(QLatin1Char(' ')));
        m_process->start(SDDM::mainConfig.X11.ServerPath.get(), args);
        if (!m_process->waitForStarted()) {
            qCritical("Failed to start X11 server process");
            close(pipeFds[0]);
            return false;
        }

        // Close the other side of pipe in our process, otherwise reading
        // from it may stuck even X server exit
        close(pipeFds[1]);

        QFile readPipe;
        if (!readPipe.open(pipeFds[0], QIODevice::ReadOnly)) {
            qCritical("Failed to open pipe to start X11 server");
            close(pipeFds[0]);
            return false;
        }
        qDebug() << "started" << readPipe.waitForReadyRead(1000);
        QByteArray displayNumber = readPipe.readLine();
        if (displayNumber.size() < 2) {
            // X server gave nothing (or a whitespace)
            qCritical() << "Failed to read display number from pipe" << displayNumber << m_process->state() << m_process->program();
            close(pipeFds[0]);
            return false;
        }
        displayNumber.prepend(QByteArray(":"));
        displayNumber.remove(displayNumber.size() -1, 1); // trim trailing whitespace
        m_display = QString::fromLocal8Bit(displayNumber);
        close(pipeFds[0]);
    }

    // Send it back to the daemon
    SDDM::SafeDataStream str(m_socket);
    str << SDDM::X11HelperMessage::Started << m_display;
    str.send();

    return true;
}

void Runner::stop()
{
    // Terminate process
    if (m_process) {
        m_process->terminate();
        if (!m_process->waitForFinished(5000))
            m_process->kill();
    }
}

void Runner::started()
{
    if (m_display.isEmpty())
        return;

    QString displayCommand = SDDM::mainConfig.X11.DisplayCommand.get();

    // Create cursor setup process
    QProcess *setCursor = new QProcess();

    // Create display setup script process
    QProcess *displayScript = new QProcess();

    // Create session process
    QProcess *session = new QProcess();

    // Set process environment
    QProcessEnvironment env;
    env.insert(QStringLiteral("DISPLAY"), m_display);
    env.insert(QStringLiteral("HOME"), QStringLiteral("/"));
    env.insert(QStringLiteral("PATH"), SDDM::mainConfig.Users.DefaultPath.get());
    env.insert(QStringLiteral("XAUTHORITY"), m_authPath);
    env.insert(QStringLiteral("SHELL"), QStringLiteral("/bin/sh"));
    env.insert(QStringLiteral("XCURSOR_THEME"), SDDM::mainConfig.Theme.CursorTheme.get());
    setCursor->setProcessEnvironment(env);
    displayScript->setProcessEnvironment(env);
    session->setProcessEnvironment(env);

    qDebug() << "Setting default cursor";
    setCursor->start(QStringLiteral("xsetroot -cursor_name left_ptr"));

    // Delete setCursor on finish
    connect(setCursor, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            setCursor, &QProcess::deleteLater);

    // Wait for finished
    if (!setCursor->waitForFinished(1000)) {
        qWarning() << "Could not set default cursor";
        setCursor->kill();
    }

    // Start display setup script
    qDebug() << "Running display setup script:" << displayCommand;
    displayScript->start(displayCommand);

    // Delete displayScript on finish
    connect(displayScript, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            displayScript, &QProcess::deleteLater);
    connect(session, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            session, &QProcess::deleteLater);

    // Wait for finished
    if (!displayScript->waitForFinished(30000))
        displayScript->kill();

    session->start(m_session);
    Q_ASSERT(session->waitForStarted());
    // Reload config if needed
    SDDM::mainConfig.load();
}

void Runner::finished()
{
    if (!m_display.isEmpty()) {
        QString displayStopCommand = SDDM::mainConfig.X11.DisplayStopCommand.get();

        // create display setup script process
        QProcess *displayStopScript = new QProcess();

        // Set process environment
        QProcessEnvironment env;
        env.insert(QStringLiteral("DISPLAY"), m_display);
        env.insert(QStringLiteral("HOME"), QStringLiteral("/"));
        env.insert(QStringLiteral("PATH"), SDDM::mainConfig.Users.DefaultPath.get());
        env.insert(QStringLiteral("SHELL"), QStringLiteral("/bin/sh"));
        displayStopScript->setProcessEnvironment(env);

        // Start display stop script
        qDebug() << "Running display stop script:" << displayStopCommand;
        displayStopScript->start(displayStopCommand);

        // Wait for finished
        if (!displayStopScript->waitForFinished(5000))
            displayStopScript->kill();

        // Clean up the script process
        displayStopScript->deleteLater();
        displayStopScript = nullptr;
    }

    // Clean up
    m_process->deleteLater();
    m_process = nullptr;
}
