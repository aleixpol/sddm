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

#ifndef SDDM_X11HELPERPROTOCOL_H
#define SDDM_X11HELPERPROTOCOL_H

#include <QDataStream>

namespace SDDM {

enum class X11HelperMessage {
    Unknown,
    Started,
    Last
};

inline QDataStream &operator>>(QDataStream &s, X11HelperMessage &m)
{
    qint32 what;
    s >> what;
    if (what < qint32(X11HelperMessage::Unknown) || what >= qint32(X11HelperMessage::Last)) {
        s.setStatus(QDataStream::ReadCorruptData);
        return s;
    }
    m = X11HelperMessage(what);
    return s;
}

inline QDataStream &operator<<(QDataStream &s, const X11HelperMessage &m)
{
    s << qint32(m);
    return s;
}

} // namespace SDDM

#endif // SDDM_X11HELPERPROTOCOL_H
