/* Copyright (C) 2006 - 2011 Jan Kundrát <jkt@gentoo.org>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or the version 3 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include <QProcess>
#include <QSslSocket>
#include "SocketFactory.h"
#include "IODeviceSocket.h"
#include "FakeSocket.h"

namespace Imap {
namespace Mailbox {

SocketFactory::SocketFactory(): _startTls(false)
{
}

void SocketFactory::setStartTlsRequired( const bool doIt )
{
    _startTls = doIt;
}

bool SocketFactory::startTlsRequired()
{
    return _startTls;
}

ProcessSocketFactory::ProcessSocketFactory(
        const QString& executable, const QStringList& args):
    _executable(executable), _args(args)
{
}

Socket* ProcessSocketFactory::create()
{
    // FIXME: this may leak memory if an exception strikes in this function
    // (before we return the pointer)
    QProcess* proc = new QProcess();
    QString cmdLine = _executable;
    if ( ! _args.isEmpty() )
        cmdLine += QLatin1Char(' ') + _args.join(QLatin1String(" "));
    proc->setProperty("trojita-stream-qprocess-cmdline", cmdLine);
    proc->start( _executable, _args );
    return new IODeviceSocket( proc );
}

SslSocketFactory::SslSocketFactory( const QString& host, const quint16 port ):
    _host(host), _port(port)
{
}

Socket* SslSocketFactory::create()
{
    QSslSocket* sslSock = new QSslSocket();
    sslSock->setProperty("trojita-stream-socket-hostname", _host);
    sslSock->setProperty("trojita-stream-socket-port", QString::number(_port));
    sslSock->ignoreSslErrors(); // big fat FIXME here!!!
    sslSock->setProtocol( QSsl::AnyProtocol );
    sslSock->setPeerVerifyMode( QSslSocket::QueryPeer );
    IODeviceSocket* ioSock = new IODeviceSocket( sslSock, true );
    connect( sslSock, SIGNAL(encrypted()), ioSock, SIGNAL(connected()) );
    sslSock->connectToHostEncrypted( _host, _port );
    return ioSock;
}

TlsAbleSocketFactory::TlsAbleSocketFactory( const QString& host, const quint16 port ):
    _host(host), _port(port)
{
}

Socket* TlsAbleSocketFactory::create()
{
    QSslSocket* sslSock = new QSslSocket();
    sslSock->setProperty("trojita-stream-socket-hostname", _host);
    sslSock->setProperty("trojita-stream-socket-port", QString::number(_port));
    sslSock->ignoreSslErrors(); // big fat FIXME here!!!
    sslSock->setProtocol( QSsl::AnyProtocol );
    sslSock->setPeerVerifyMode( QSslSocket::QueryPeer );
    sslSock->connectToHost( _host, _port );
    return new IODeviceSocket( sslSock );
}

FakeSocketFactory::FakeSocketFactory(): SocketFactory()
{
}

Socket* FakeSocketFactory::create()
{
    return _last = new FakeSocket();
}

Socket* FakeSocketFactory::lastSocket()
{
    return _last;
}



}
}
