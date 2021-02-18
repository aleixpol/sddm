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

#ifndef SDDMX11HELPER_RUNNER_H
#define SDDMX11HELPER_RUNNER_H

#include <QObject>

class QLocalSocket;
class QProcess;

class Runner : public QObject
{
    Q_OBJECT
public:
    explicit Runner(QObject *parent = nullptr);
    ~Runner();

    QString socketName() const;
    void setSocketName(const QString &name);

    bool isTestMode() const;
    void setTestMode(bool testMode);

    QString display() const;

    QString seat() const;
    void setSeat(const QString &seat);

    int terminalId() const;
    void setTerminalId(int id);

    QString authPath() const;
    void setAuthPath(const QString &path);

    void setSession(const QString &session) {
        m_session = session;
    }

    bool start();
    void stop();

private:
    QString m_socketName;
    bool m_testMode = false;
    QString m_display;
    QString m_seat;
    int m_terminalId = 0;
    QString m_authPath;
    QString m_session;

    QLocalSocket *m_socket = nullptr;
    QProcess *m_process = nullptr;

private slots:
    void started();
    void finished();
};

#endif // SDDMX11HELPER_RUNNER_H
