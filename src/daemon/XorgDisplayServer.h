/***************************************************************************
* Copyright (c) 2014 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
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

#ifndef SDDM_XORGDISPLAYSERVER_H
#define SDDM_XORGDISPLAYSERVER_H

#include "Auth.h"
#include "DisplayServer.h"

class QProcess;
class QLocalServer;

namespace SDDM {
    class XorgDisplayServer : public DisplayServer {
        Q_OBJECT
        Q_DISABLE_COPY(XorgDisplayServer)
    public:
        explicit XorgDisplayServer(Display *parent);
        ~XorgDisplayServer();

        const QString &display() const;
        const QString &authPath() const;

        QString sessionType() const;

        const QString &cookie() const;

        bool addCookie(const QString &file);

    public slots:
        bool start();
        void stop();
        void finished();

    private:
        QLocalServer *m_socketServer = nullptr;
        QProcess *m_process = nullptr;
        Auth *m_auth = nullptr;

        QString m_authPath;
        QString m_cookie;

        void createAuthFile();
        void changeOwner(const QString &fileName);

    private slots:
        void handleNewConnection();
        void onRequestChanged();
        void onSessionStarted(bool success);
        void onHelperFinished(Auth::HelperExitStatus status);
        void authInfo(const QString &message, Auth::Info info);
        void authError(const QString &message, Auth::Error error);
    };
}

#endif // SDDM_XORGDISPLAYSERVER_H
