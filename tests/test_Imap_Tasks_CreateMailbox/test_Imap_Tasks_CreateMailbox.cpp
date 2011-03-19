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

#include <QtTest>
#include "test_Imap_Tasks_CreateMailbox.h"
#include "../headless_test.h"
#include "Streams/FakeSocket.h"
#include "Imap/Model/MemoryCache.h"
#include "Imap/Model/Model.h"

void ImapModelCreateMailboxTest::init()
{
    Imap::Mailbox::AbstractCache* cache = new Imap::Mailbox::MemoryCache( this, QString() );
    factory = new Imap::Mailbox::FakeSocketFactory();
    Imap::Mailbox::TaskFactoryPtr taskFactory( new Imap::Mailbox::TestingTaskFactory() );
    taskFactoryUnsafe = static_cast<Imap::Mailbox::TestingTaskFactory*>( taskFactory.get() );
    taskFactoryUnsafe->fakeOpenConnectionTask = true;
    taskFactoryUnsafe->fakeListChildMailboxes = true;
    model = new Imap::Mailbox::Model( this, cache, Imap::Mailbox::SocketFactoryPtr( factory ), taskFactory, false );
    createdSpy = new QSignalSpy( model, SIGNAL(mailboxCreationSucceded(QString)) );
    failedSpy = new QSignalSpy( model, SIGNAL(mailboxCreationFailed(QString,QString)) );
    errorSpy = new QSignalSpy( model, SIGNAL(connectionError(QString)) );
}

void ImapModelCreateMailboxTest::cleanup()
{
    delete model;
    model = 0;
    taskFactoryUnsafe = 0;
    delete createdSpy;
    createdSpy = 0;
    delete failedSpy;
    failedSpy = 0;
    delete errorSpy;
    errorSpy = 0;
    QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);
}

void ImapModelCreateMailboxTest::initTestCase()
{
    model = 0;
    createdSpy = 0;
    failedSpy = 0;
    errorSpy = 0;
}

#define SOCK static_cast<Imap::FakeSocket*>( factory->lastSocket() )

void ImapModelCreateMailboxTest::_initWithOne()
{
    // Init with one example mailbox
    taskFactoryUnsafe->fakeListChildMailboxesMap[ QString::fromAscii("") ] = QStringList() << QString::fromAscii("a");
    model->rowCount( QModelIndex() );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( model->rowCount( QModelIndex() ), 2 );
    QModelIndex idxA = model->index( 1, 0, QModelIndex() );
    QCOMPARE( model->data( idxA, Qt::DisplayRole ), QVariant(QString::fromAscii("a")) );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray() );
    QVERIFY( errorSpy->isEmpty() );
}

void ImapModelCreateMailboxTest::_initWithEmpty()
{
    // Init with empty set of mailboxes
    model->rowCount( QModelIndex() );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( model->rowCount( QModelIndex() ), 1 );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray() );
    QVERIFY( errorSpy->isEmpty() );
}

void ImapModelCreateMailboxTest::testCreateOneMore()
{
    _initWithOne();

    // Now test the actual creating process
    model->createMailbox( "ahoj" );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y0 CREATE ahoj\r\n") );
    SOCK->fakeReading( QByteArray("y0 OK created\r\n") );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y1 LIST \"\" ahoj\r\n") );
    SOCK->fakeReading( QByteArray("* LIST (\\HasNoChildren) \"^\" \"ahoj\"\r\n"
            "y1 OK list\r\n") );
    QCoreApplication::processEvents();
    QCOMPARE( model->rowCount( QModelIndex() ), 3 );
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray() );
    QCOMPARE( createdSpy->size(), 1 );
    QVERIFY( failedSpy->isEmpty() );
    QVERIFY( errorSpy->isEmpty() );
}

void ImapModelCreateMailboxTest::testCreateEmpty()
{
    _initWithEmpty();

    // Now test the actual creating process
    model->createMailbox( "ahoj" );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y0 CREATE ahoj\r\n") );
    SOCK->fakeReading( QByteArray("y0 OK created\r\n") );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y1 LIST \"\" ahoj\r\n") );
    SOCK->fakeReading( QByteArray("* LIST (\\HasNoChildren) \"^\" \"ahoj\"\r\n"
            "y1 OK list\r\n") );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( model->rowCount( QModelIndex() ), 2 );
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray() );
    QCOMPARE( createdSpy->size(), 1 );
    QVERIFY( failedSpy->isEmpty() );
    QVERIFY( errorSpy->isEmpty() );
}

void ImapModelCreateMailboxTest::testCreateFail()
{
    _initWithEmpty();

    // Test failure of the CREATE command
    model->createMailbox( "ahoj" );
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray("y0 CREATE ahoj\r\n") );
    SOCK->fakeReading( QByteArray("y0 NO muhehe\r\n") );
    QCoreApplication::processEvents();

    QCOMPARE( model->rowCount( QModelIndex() ), 1 );
    QCoreApplication::processEvents();
    QCOMPARE( SOCK->writtenStuff(), QByteArray() );
    QCOMPARE( failedSpy->size(), 1 );
    QVERIFY( createdSpy->isEmpty() );
    QVERIFY( errorSpy->isEmpty() );
}

TROJITA_HEADLESS_TEST( ImapModelCreateMailboxTest )
