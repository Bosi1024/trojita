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

#include <algorithm>
#include <QtTest>
#include "test_Imap_SelectedMailboxUpdates.h"
#include "../headless_test.h"
#include "Streams/FakeSocket.h"
#include "Imap/Model/ItemRoles.h"

/** @short Test that we survive a new message arrival and its subsequent removal in rapid sequence

The code won't notice that the message got expunged immediately (simply because it has no idea of a
next response when processing the first EXISTS), will ask for UID and FLAGS, but the message gets
deleted (and EXPUNGE sent) before the server receives the UID FETCH command. This leads to us having
a flawed idea of UIDNEXT, unless the server provides a hint via the * OK [UIDNEXT xyz] response.

It's a question whether or not to at least increment the UIDNEXT by one when the response doesn't
arrive. Doing that would probably break when the server employs an Outlook-workaround for IDLE with
a fake EXISTS/EXPUNGE responses.
*/
void ImapModelSelectedMailboxUpdatesTest::helperTestExpungeImmediatelyAfterArrival(bool sendUidNext)
{
    existsA = 3;
    uidValidityA = 6;
    uidMapA << 1 << 7 << 9;
    uidNextA = 16;
    helperSyncAWithMessagesEmptyState();
    cEmpty();

    cServer(QString::fromAscii("* %1 EXISTS\r\n* %1 EXPUNGE\r\n").arg(QString::number(existsA + 1)).toAscii());
    cClient(QString(t.mk("UID FETCH %1:* (FLAGS)\r\n")).arg(QString::number(qMax(uidMapA.last() + 1, uidNextA))).toAscii());

    // Add message with this UID to our internal list
    uint addedUid = 33;
    uidNextA = addedUid + 1;

    QByteArray uidUpdateResponse = sendUidNext ? QString("* OK [UIDNEXT %1] courtesy of the server\r\n").arg(
            QString::number(uidNextA)).toAscii() : QByteArray();

    // ...but because it got deleted, here we go
    cServer(t.last("OK empty fetch\r\n") + uidUpdateResponse);
    cEmpty();
    QVERIFY(errorSpy->isEmpty());

    helperCheckCache( ! sendUidNext );
    helperVerifyUidMapA();
}

void ImapModelSelectedMailboxUpdatesTest::testUnsolicitedFetch()
{
    existsA = 2;
    uidValidityA = 666;
    uidMapA << 3 << 9;
    uidNextA = 33;
    helperSyncAWithMessagesEmptyState();
    cEmpty();

    cServer(QString::fromAscii("* %1 EXISTS\r\n* %1 FETCH (FLAGS (\\Seen \\Recent $NotJunk NotJunk))\r\n").arg(QString::number(existsA + 1)).toAscii());
    cClient(QString(t.mk("UID FETCH %1:* (FLAGS)\r\n")).arg(
                 QString::number(qMax(uidMapA.last() + 1, uidNextA))).toAscii());

    // Add message with this UID to our internal list
    uint addedUid = 42;
    ++existsA;
    uidMapA << addedUid;
    uidNextA = addedUid + 1;

    cServer(QString("* %1 FETCH (FLAGS (\\Seen \\Recent $NotJunk NotJunk) UID %2)\r\n").arg(
            QString::number(existsA), QString::number(addedUid)).toAscii() +
                       t.last("OK flags returned\r\n"));
    cEmpty();
    QVERIFY(errorSpy->isEmpty());

    helperCheckCache();
    helperVerifyUidMapA();
}

/** @short Test a rapid EXISTS/EXPUNGE sequence

@see helperTestExpungeImmediatelyAfterArrival for details
*/
void ImapModelSelectedMailboxUpdatesTest::testExpungeImmediatelyAfterArrival()
{
    helperTestExpungeImmediatelyAfterArrival(false);
}

/** @short Test a rapid EXISTS/EXPUNGE sequence with an added UIDNEXT response

@see helperTestExpungeImmediatelyAfterArrival for details
*/
void ImapModelSelectedMailboxUpdatesTest::testExpungeImmediatelyAfterArrivalWithUidNext()
{
    helperTestExpungeImmediatelyAfterArrival(true);
}

/** @short Test generic traffic to an opened mailbox without asking for message data too soon */
void ImapModelSelectedMailboxUpdatesTest::testGenericTraffic()
{
    helperGenericTraffic(false);
}

/** @short Test generic traffic to an opened mailbox when we ask for message metadata as soon as they arrive */
void ImapModelSelectedMailboxUpdatesTest::testGenericTrafficWithEnvelopes()
{
    helperGenericTraffic(true);
}

void ImapModelSelectedMailboxUpdatesTest::helperGenericTraffic(bool askForEnvelopes)
{
    // At first, sync to an empty state
    uidNextA = 12;
    uidValidityA = 333;
    helperSyncANoMessagesCompleteState();

    // Fake delivery of A, B and C
    helperGenericTrafficFirstArrivals(askForEnvelopes);

    // Now add one more message, the D
    helperGenericTrafficArrive2(askForEnvelopes);

    // Remove B
    helperDeleteOneMessage(1, QStringList() << QLatin1String("A") << QLatin1String("C") << QLatin1String("D"));

    // Remove D
    helperDeleteOneMessage(2, QStringList() << QLatin1String("A") << QLatin1String("C"));

    // Remove A
    helperDeleteOneMessage(0, QStringList() << QLatin1String("C"));

    // Remove C
    helperDeleteOneMessage(0, QStringList());

    // Add completely different messages, A, B, C and D
    helperGenericTrafficArrive3(askForEnvelopes);

    // remove C and B
    helperDeleteOneMessage(2, QStringList() << QLatin1String("A") << QLatin1String("B") << QLatin1String("D"));
    helperDeleteOneMessage(1, QStringList() << QLatin1String("A") << QLatin1String("D"));

    // Add E, F and G
    helperGenericTrafficArrive4(askForEnvelopes);

    // Delete D and F
    helperDeleteTwoMessages(3, 1, QStringList() << QLatin1String("A") << QLatin1String("E") << QLatin1String("G"));

    // Delete G and A
    helperDeleteTwoMessages(2, 0, QStringList() << QLatin1String("E"));
}

/** @short Test an arrival of three brand new messages to an already synced mailbox

This function assumes that mailbox A is already openened and is active and synced. It will fake an arrival
of three brand new messages, A, B and C. This arrival will get noticed and processed.
 */
void ImapModelSelectedMailboxUpdatesTest::helperGenericTrafficFirstArrivals(bool askForEnvelopes)
{
    // Fake delivery of A, B and C
    cServer(QByteArray("* 3 EXISTS\r\n* 3 RECENT\r\n"));
    // This should trigger a request for flags
    cClient(t.mk("UID FETCH 12:* (FLAGS)\r\n"));
    // The messages sgould be there already
    QVERIFY(msgListA.child(0,0).isValid());
    QModelIndex msgB = msgListA.child(1, 0);
    QVERIFY(msgB.isValid());
    QVERIFY(msgListA.child(2,0).isValid());
    QVERIFY( ! msgListA.child(3,0).isValid());
    // We shouldn't have the UID yet
    QCOMPARE(msgB.data(Imap::Mailbox::RoleMessageUid).toUInt(), 0u);

    // Verify various message counts
    // This one is parsed from RECENT
    QCOMPARE(idxA.data(Imap::Mailbox::RoleRecentMessageCount).toInt(), 3);
    // This one is easy, too
    QCOMPARE(idxA.data(Imap::Mailbox::RoleTotalMessageCount).toInt(), 3);
    // We have to wait for FLAGS for this one
    QCOMPARE(idxA.data(Imap::Mailbox::RoleUnreadMessageCount).toInt(), 0);

    if ( askForEnvelopes ) {
        // In the meanwhile, ask for ENVELOPE etc -- but it shouldn't be there yet
        QVERIFY( ! msgB.data(Imap::Mailbox::RoleMessageFrom).isValid() );
    }

    // FLAGS arrive, bringing the UID with them
    cServer(QByteArray("* 1 FETCH (UID 43 FLAGS (\\Recent))\r\n"
                                  "* 2 FETCH (UID 44 FLAGS (\\Recent))\r\n"
                                  "* 3 FETCH (UID 45 FLAGS (\\Recent))\r\n") +
                       t.last("OK fetched\r\n"));
    // The UIDs shall be known at this point
    QCOMPARE(msgB.data(Imap::Mailbox::RoleMessageUid).toUInt(), 44u);
    // The unread message count should be correct now, too
    QCOMPARE(idxA.data(Imap::Mailbox::RoleUnreadMessageCount).toInt(), 3);
    if ( askForEnvelopes ) {
        // The ENVELOPE and related fields should be requested now
        cClient(t.mk("UID FETCH 43:44 (ENVELOPE INTERNALDATE BODYSTRUCTURE RFC822.SIZE)\r\n"));
    }

    existsA = 3;
    uidNextA = 46;
    uidMapA << 43 << 44 << 45;
    helperCheckCache();
    helperVerifyUidMapA();

    // Yes, we're counting to one more than what actually is here; that's because we want to prevent a possible out-of-bounds access
    for ( int i = 0; i < static_cast<int>(existsA) + 1; ++i )
        msgListA.child(i,0).data(Imap::Mailbox::RoleMessageSubject);

    if ( askForEnvelopes ) {
        // Envelopes for A and B already got requested
        // We have to preserve the previous command, too
        QByteArray completionForFirstFetch = t.last("OK fetched\r\n");
        cClient(t.mk("UID FETCH 45 (ENVELOPE INTERNALDATE BODYSTRUCTURE RFC822.SIZE)\r\n"));
        // Simulate out-of-order execution here -- the completion responses for both commands arrive only
        // after the actual data for both of them got pushed
        cServer(helperCreateTrivialEnvelope(1, 43, QLatin1String("A")) +
                           helperCreateTrivialEnvelope(2, 44, QLatin1String("B")) +
                           helperCreateTrivialEnvelope(3, 45, QLatin1String("C")) +
                           completionForFirstFetch +
                           t.last("OK fetched\r\n"));
    } else {
        // Requesting all envelopes at once
        cClient(t.mk("UID FETCH 43:45 (ENVELOPE INTERNALDATE BODYSTRUCTURE RFC822.SIZE)\r\n"));
        cServer(helperCreateTrivialEnvelope(1, 43, QLatin1String("A")) +
                           helperCreateTrivialEnvelope(2, 44, QLatin1String("B")) +
                           helperCreateTrivialEnvelope(3, 45, QLatin1String("C")) +
                           t.last("OK fetched\r\n"));
    }
    helperCheckSubjects(QStringList() << QLatin1String("A") << QLatin1String("B") << QLatin1String("C"));
    cEmpty();
    QVERIFY( errorSpy->isEmpty() );
}

/** @short Fake delivery of D to a mailbox which already contains A, B and C */
void  ImapModelSelectedMailboxUpdatesTest::helperGenericTrafficArrive2(bool askForEnvelopes)
{
    // Fake delivery of D
    cServer(QByteArray("* 4 EXISTS\r\n* 4 RECENT\r\n"));
    // This should trigger a request for flags
    cClient(t.mk("UID FETCH 46:* (FLAGS)\r\n"));
    // The messages sgould be there already
    QVERIFY(msgListA.child(0,0).isValid());
    QModelIndex msgD = msgListA.child(3, 0);
    QVERIFY(msgD.isValid());
    QVERIFY( ! msgListA.child(4,0).isValid());
    // We shouldn't have the UID yet
    QCOMPARE(msgD.data(Imap::Mailbox::RoleMessageUid).toUInt(), 0u);

    // Verify various message counts
    // This one is parsed from RECENT
    QCOMPARE(idxA.data(Imap::Mailbox::RoleRecentMessageCount).toInt(), 4);
    // This one is easy, too
    QCOMPARE(idxA.data(Imap::Mailbox::RoleTotalMessageCount).toInt(), 4);
    // this one isn't available yet
    QCOMPARE(idxA.data(Imap::Mailbox::RoleUnreadMessageCount).toInt(), 3);

    if ( askForEnvelopes ) {
        // In the meanwhile, ask for ENVELOPE etc -- but it shouldn't be there yet
        QVERIFY( ! msgD.data(Imap::Mailbox::RoleMessageFrom).isValid() );
    }

    // FLAGS arrive, bringing the UID with them
    cServer(QByteArray("* 4 FETCH (UID 46 FLAGS (\\Recent))\r\n") +
                       t.last("OK fetched\r\n"));
    // The UIDs shall be known at this point
    QCOMPARE(msgD.data(Imap::Mailbox::RoleMessageUid).toUInt(), 46u);
    // The unread message count should be correct now, too
    QCOMPARE(idxA.data(Imap::Mailbox::RoleUnreadMessageCount).toInt(), 4);

    if ( askForEnvelopes ) {
        QCoreApplication::processEvents();
        // The ENVELOPE and related fields should be requested now
        cClient(t.mk("UID FETCH 46 (ENVELOPE INTERNALDATE BODYSTRUCTURE RFC822.SIZE)\r\n"));
    }

    existsA = 4;
    uidNextA = 47;
    uidMapA << 46;
    helperCheckCache();
    helperVerifyUidMapA();

    // Request the subject once again
    msgListA.child(3,0).data(Imap::Mailbox::RoleMessageSubject);
    // In contrast to helperGenericTrafficFirstArrivals, we won't query "message #5",ie. one more than how many are
    // actually there. The main motivation is trying to behave in a different manner than before. Hope this helps.

    if ( askForEnvelopes ) {
        // Envelope for D already got requested -> nothing to do here
    } else {
        // The D got requested by an explicit query above
        cClient(t.mk("UID FETCH 46 (ENVELOPE INTERNALDATE BODYSTRUCTURE RFC822.SIZE)\r\n"));
    }
    cServer(helperCreateTrivialEnvelope(4, 46, QLatin1String("D")) + t.last("OK fetched\r\n"));
    helperCheckSubjects(QStringList() << QLatin1String("A") << QLatin1String("B") << QLatin1String("C") << QLatin1String("D"));
    cEmpty();
    QVERIFY( errorSpy->isEmpty() );
}

/** @short Check subjects of all messages in a given mailbox

This function will check subjects of all mailboxes in the mailbox A against a list of subjects specified in @arg subjects.
*/
void ImapModelSelectedMailboxUpdatesTest::helperCheckSubjects(const QStringList &subjects)
{
    for ( int i = 0; i < subjects.size(); ++i ) {
        QModelIndex index = msgListA.child(i,0);
        QVERIFY(index.isValid());
        QCOMPARE(index.data(Imap::Mailbox::RoleMessageSubject).toString(), subjects[i]);
    }
    // Me sure thet there are no more messages
    QVERIFY( ! msgListA.child(subjects.size(),0).isValid() );
}

/** @short Test what happens when the server tells us that one message got deleted */
void ImapModelSelectedMailboxUpdatesTest::helperDeleteOneMessage(const uint seq, const QStringList &remainingSubjects)
{
    // Fake deleting one message
    --existsA;
    uidMapA.removeAt(seq);
    Q_ASSERT(remainingSubjects.size() == static_cast<int>(existsA));
    Q_ASSERT(uidMapA.size() == static_cast<int>(existsA));
    cServer(QString::fromAscii("* %1 EXPUNGE\r\n* %2 RECENT\r\n").arg(QString::number(seq+1), QString::number(existsA)).toAscii());

    // Verify the model's idea about the current state
    helperCheckSubjects(remainingSubjects);
    helperCheckCache();
    helperVerifyUidMapA();
}

/** @short Test what happens when the server tells us that one message got deleted */
void ImapModelSelectedMailboxUpdatesTest::helperDeleteTwoMessages(const uint seq1, const uint seq2, const QStringList &remainingSubjects)
{
    // Fake deleting one message
    existsA -= 2;
    uidMapA.removeAt(seq1);
    uidMapA.removeAt(seq2);
    Q_ASSERT(remainingSubjects.size() == static_cast<int>(existsA));
    Q_ASSERT(uidMapA.size() == static_cast<int>(existsA));
    cServer(QString::fromAscii("* %1 EXPUNGE\r\n* %2 EXPUNGE\r\n* %3 RECENT\r\n").arg(QString::number(seq1+1), QString::number(seq2+1), QString::number(existsA)).toAscii());

    // Verify the model's idea about the current state
    helperCheckSubjects(remainingSubjects);
    helperCheckCache();
    helperVerifyUidMapA();
}

/** @short Fake delivery of a completely new set of messages, the A, B, C and D */
void ImapModelSelectedMailboxUpdatesTest::helperGenericTrafficArrive3(bool askForEnvelopes)
{
    // Fake the announcement of the delivery
    cServer(QByteArray("* 4 EXISTS\r\n* 4 RECENT\r\n"));
    // This should trigger a request for flags
    cClient(t.mk("UID FETCH 47:* (FLAGS)\r\n"));

    // The messages should be there already
    QVERIFY(msgListA.child(0,0).isValid());
    QModelIndex msgD = msgListA.child(3, 0);
    QVERIFY(msgD.isValid());
    QVERIFY( ! msgListA.child(4,0).isValid());
    // We shouldn't have the UID yet
    QCOMPARE(msgD.data(Imap::Mailbox::RoleMessageUid).toUInt(), 0u);

    // Verify various message counts
    // This one is parsed from RECENT
    QCOMPARE(idxA.data(Imap::Mailbox::RoleRecentMessageCount).toInt(), 4);
    // This one is easy, too
    QCOMPARE(idxA.data(Imap::Mailbox::RoleTotalMessageCount).toInt(), 4);
    // this one isn't available yet
    QCOMPARE(idxA.data(Imap::Mailbox::RoleUnreadMessageCount).toInt(), 0);

    if ( askForEnvelopes ) {
        // In the meanwhile, ask for ENVELOPE etc -- but it shouldn't be there yet
        QVERIFY( ! msgD.data(Imap::Mailbox::RoleMessageFrom).isValid() );
    }

    // FLAGS arrive, bringing the UID with them
    cServer(QByteArray("* 1 FETCH (UID 47 FLAGS (\\Recent))\r\n"
                                  "* 2 FETCH (UID 48 FLAGS (\\Recent))\r\n") );
    // Try to insert a pause here; this could be used to properly play with preloading in future...
    for ( int i = 0; i < 10; ++i)
        QCoreApplication::processEvents();
    cServer(QByteArray("* 3 FETCH (UID 49 FLAGS (\\Recent))\r\n"
                                  "* 4 FETCH (UID 50 FLAGS (\\Recent))\r\n") +
                       t.last("OK fetched\r\n"));
    // The UIDs shall be known at this point
    QCOMPARE(msgD.data(Imap::Mailbox::RoleMessageUid).toUInt(), 50u);
    // The unread message count should be correct now, too
    QCOMPARE(idxA.data(Imap::Mailbox::RoleUnreadMessageCount).toInt(), 4);

    if ( askForEnvelopes ) {
        // The ENVELOPE and related fields should be requested now
        cClient(t.mk("UID FETCH 47:50 (ENVELOPE INTERNALDATE BODYSTRUCTURE RFC822.SIZE)\r\n"));
    }

    existsA = 4;
    uidNextA = 51;
    uidMapA.clear();
    uidMapA << 47 << 48 << 49 << 50;
    helperCheckCache();
    helperVerifyUidMapA();

    if ( askForEnvelopes ) {
        // Envelopes already requested -> nothing to do here
    } else {
        // Not requested yet -> do it now
        QVERIFY( ! msgD.data(Imap::Mailbox::RoleMessageFrom).isValid() );
        cClient(t.mk("UID FETCH 47:50 (ENVELOPE INTERNALDATE BODYSTRUCTURE RFC822.SIZE)\r\n"));
    }
    // This is common for both cases; the data should finally arrive
    cServer(helperCreateTrivialEnvelope(1, 47, QLatin1String("A")) +
                       helperCreateTrivialEnvelope(2, 48, QLatin1String("B")) +
                       helperCreateTrivialEnvelope(3, 49, QLatin1String("C")) +
                       helperCreateTrivialEnvelope(4, 50, QLatin1String("D")) +
                       t.last("OK fetched\r\n"));
    helperCheckSubjects(QStringList() << QLatin1String("A") << QLatin1String("B") << QLatin1String("C") << QLatin1String("D"));

    // Verify UIDd and cache stuff once again to make sure reading data doesn't mess anything up
    helperCheckCache();
    helperVerifyUidMapA();
    cEmpty();
    QVERIFY( errorSpy->isEmpty() );
}

/** @short Fake delivery of three new messages, E, F and G, to a mailbox with A and D */
void ImapModelSelectedMailboxUpdatesTest::helperGenericTrafficArrive4(bool askForEnvelopes)
{
    // Fake the announcement of the delivery
    cServer(QByteArray("* 5 EXISTS\r\n* 5 RECENT\r\n"));
    // This should trigger a request for flags
    cClient(t.mk("UID FETCH 51:* (FLAGS)\r\n"));

    QModelIndex msgE = msgListA.child(2, 0);
    QVERIFY(msgE.isValid());
    // We shouldn't have the UID yet
    QCOMPARE(msgE.data(Imap::Mailbox::RoleMessageUid).toUInt(), 0u);

    // Verify various message counts
    // This one is parsed from RECENT
    QCOMPARE(idxA.data(Imap::Mailbox::RoleRecentMessageCount).toInt(), 5);
    // This one is easy, too
    QCOMPARE(idxA.data(Imap::Mailbox::RoleTotalMessageCount).toInt(), 5);
    // this one isn't available yet
    QCOMPARE(idxA.data(Imap::Mailbox::RoleUnreadMessageCount).toInt(), 2);

    if ( askForEnvelopes ) {
        // In the meanwhile, ask for ENVELOPE etc -- but it shouldn't be there yet
        QVERIFY( ! msgE.data(Imap::Mailbox::RoleMessageFrom).isValid() );
    }

    // FLAGS arrive, bringing the UID with them
    cServer(QByteArray("* 3 FETCH (UID 52 FLAGS (\\Recent))\r\n"
                                  "* 4 FETCH (UID 53 FLAGS (\\Recent))\r\n"
                                  "* 5 FETCH (UID 63 FLAGS (\\Recent))\r\n") +
                       t.last("OK fetched\r\n"));
    // The UIDs shall be known at this point
    QCOMPARE(msgE.data(Imap::Mailbox::RoleMessageUid).toUInt(), 52u);
    // The unread message count should be correct now, too
    QCOMPARE(idxA.data(Imap::Mailbox::RoleUnreadMessageCount).toInt(), 5);

    if ( askForEnvelopes ) {
        QCoreApplication::processEvents();
        // The ENVELOPE and related fields should be requested now
        cClient(t.mk("UID FETCH 52 (ENVELOPE INTERNALDATE BODYSTRUCTURE RFC822.SIZE)\r\n"));
    }

    existsA = 5;
    uidNextA = 64;
    uidMapA << 52 << 53 << 63;
    helperCheckCache();
    helperVerifyUidMapA();

    if ( askForEnvelopes ) {
        // Envelopes already requested -> nothing to do here
    } else {
        // Not requested yet -> do it now
        QVERIFY( ! msgE.data(Imap::Mailbox::RoleMessageFrom).isValid() );
        cClient(t.mk("UID FETCH 52:53,63 (ENVELOPE INTERNALDATE BODYSTRUCTURE RFC822.SIZE)\r\n"));
    }
    // This is common for both cases; the data should finally arrive
    cServer(helperCreateTrivialEnvelope(3, 52, QLatin1String("E")) +
                       helperCreateTrivialEnvelope(4, 53, QLatin1String("F")) +
                       helperCreateTrivialEnvelope(5, 63, QLatin1String("G")) +
                       t.last("OK fetched\r\n"));
    helperCheckSubjects(QStringList() << QLatin1String("A") << QLatin1String("D") << QLatin1String("E") << QLatin1String("F") << QLatin1String("G"));

    // Verify UIDd and cache stuff once again to make sure reading data doesn't mess anything up
    helperCheckCache();
    helperVerifyUidMapA();
    cEmpty();
    QVERIFY( errorSpy->isEmpty() );
}

/** @short Check the UIDs in the mailbox against the uidMapA list */
#define helperCheckUidMapFromModel() \
{ \
    QCOMPARE(model->rowCount(msgListA), uidMapA.size()); \
    QList<uint> actual; \
    for (int i = 0; i < uidMapA.size(); ++i) { \
        actual << msgListA.child(i, 0).data(Imap::Mailbox::RoleMessageUid).toUInt(); \
    } \
    QCOMPARE(actual, uidMapA); \
} \

void ImapModelSelectedMailboxUpdatesTest::testVanishedUpdates()
{
    initialMessages(10);

    // Deleting a message in the middle of the range
    cServer("* VANISHED 9\r\n");
    uidMapA.removeOne(9);
    helperCheckUidMapFromModel();

    // Deleting the last one
    cServer("* VANISHED 10\r\n");
    uidMapA.removeOne(10);
    helperCheckUidMapFromModel();

    // Dleting three at the very start
    cServer("* VANISHED 1:3\r\n");
    uidMapA.removeOne(1);
    uidMapA.removeOne(2);
    uidMapA.removeOne(3);
    helperCheckUidMapFromModel();

    QCOMPARE(uidMapA, QList<uint>() << 4 << 5 << 6 << 7 << 8);

    // A new arrival...
    cServer("* 6 EXISTS\r\n");
    cClient(t.mk("UID FETCH 11:* (FLAGS)\r\n"));
    cServer("* 6 FETCH (UID 11 FLAGS (x))\r\n" + t.last("OK fetched\r\n"));
    uidMapA << 11;
    helperCheckUidMapFromModel();

    // Yet another arrival which, unfortunately, gets removed immediately
    cServer("* 7 EXISTS\r\n* VANISHED 12\r\n");
    cClient(t.mk("UID FETCH 12:* (FLAGS)\r\n"));
    cServer(t.last("OK fetched\r\n"));
    helperCheckUidMapFromModel();

    // Two messages arrive, the server sends UID for the last of them and the first one gets expunged
    cServer("* 8 EXISTS\r\n");
    uidMapA << 0 << 0;
    helperCheckUidMapFromModel();
    cServer("* 8 FETCH (UID 14 FLAGS ())\r\n");
    uidMapA[7] = 14;
    helperCheckUidMapFromModel();
    cServer("* VANISHED 13\r\n");
    uidMapA.removeAt(6);
    helperCheckUidMapFromModel();
    cClient(t.mk("UID FETCH 13:* (FLAGS)\r\n"));
    cServer("* 7 FETCH (UID 14 FLAGS (x))\r\n" + t.last("OK fetched\r\n"));
    helperCheckUidMapFromModel();

    cEmpty();
}

inline bool isOdd(const uint i)
{
    return i & 1;
}

inline bool isEven(const uint i)
{
    return ! isOdd(i);
}

void ImapModelSelectedMailboxUpdatesTest::testVanishedWithNonExisting()
{
    initialMessages(10);
    cServer("* VANISHED 0,2,4,6,6,6,8,2,0,10,12\r\n");
    uidMapA.erase(std::remove_if(uidMapA.begin(), uidMapA.end(), isEven), uidMapA.end());
    helperCheckUidMapFromModel();

    // This one has been already reported
    cServer("* VANISHED 2\r\n");
    helperCheckUidMapFromModel();

    // This one wasn't there yet at all
    cServer("* VANISHED 666\r\n");
    helperCheckUidMapFromModel();

    cEmpty();
}

/** @short Test what happens when the server informs about new message arrivals twice in a row */
void ImapModelSelectedMailboxUpdatesTest::testMultipleArrivals()
{
    initialMessages(1);
    cServer("* 2 EXISTS\r\n* 3 EXISTS\r\n");
    QByteArray req1 = t.mk("UID FETCH 2:* (FLAGS)\r\n");
    QByteArray resp1 = t.last("OK fetched\r\n");
    // The UIDs are still unknown at this point, and at the same time the client pessimistically assumes that the first command
    // can easily arrive to the server before it sent the second EXISTS, which is why it has no other choice but repeat
    // essentially the same command once again.
    QByteArray req2 = t.mk("UID FETCH 2:* (FLAGS)\r\n");
    QByteArray resp2 = t.last("OK fetched\r\n");
    cClient(req1 + req2);
    // The server will, however, try to be smart in this case and will send the responses back just one time.
    // It's OK to do so per the relevant standards and Trojita won't care.
    cServer("* 2 FETCH (UID 2 FLAGS (m2))\r\n"
            "* 3 FETCH (UID 3 FLAGS (m3))\r\n"
            + resp1 + resp2);
    uidMapA << 2 << 3;
    helperCheckUidMapFromModel();
    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState("a").exists()), uidMapA.size());
    QCOMPARE(model->cache()->uidMapping("a"), uidMapA);
    // flags for UID 1 arre determined deep inside the test helpers, better not check that
    QCOMPARE(model->cache()->msgFlags("a", 2), QStringList() << "m2");
    QCOMPARE(model->cache()->msgFlags("a", 3), QStringList() << "m3");
    cEmpty();
}

/** @short Similar to testMultipleArrivals, but also check that a request to go to another task is delayed until everything is synced again */
void ImapModelSelectedMailboxUpdatesTest::testMultipleArrivalsBlockingFurtherActivity()
{
    initialMessages(1);
    cServer("* 2 EXISTS\r\n* 3 EXISTS\r\n");
    QByteArray req1 = t.mk("UID FETCH 2:* (FLAGS)\r\n");
    QByteArray resp1 = t.last("OK fetched\r\n");
    // The UIDs are still unknown at this point, and at the same time the client pessimistically assumes that the first command
    // can easily arrive to the server before it sent the second EXISTS, which is why it has no other choice but repeat
    // essentially the same command once again.
    QByteArray req2 = t.mk("UID FETCH 2:* (FLAGS)\r\n");
    QByteArray resp2 = t.last("OK fetched\r\n");

    // Issue a request to go to another mailbox; it will be queued until the responses are finished
    QCOMPARE(model->rowCount(msgListB), 0);

    cClient(req1 + req2);
    // The server will, however, try to be smart in this case and will send the responses back just one time.
    // It's OK to do so per the relevant standards and Trojita won't care.
    cServer("* 2 FETCH (UID 2 FLAGS (m2))\r\n"
            "* 3 FETCH (UID 3 FLAGS (m3))\r\n"
            + resp1 + resp2);
    uidMapA << 2 << 3;
    helperCheckUidMapFromModel();
    QCOMPARE(static_cast<int>(model->cache()->mailboxSyncState("a").exists()), uidMapA.size());
    QCOMPARE(model->cache()->uidMapping("a"), uidMapA);
    // flags for UID 1 arre determined deep inside the test helpers, better not check that
    QCOMPARE(model->cache()->msgFlags("a", 2), QStringList() << "m2");
    QCOMPARE(model->cache()->msgFlags("a", 3), QStringList() << "m3");

    // Only now the second SELECT shall be queued
    cClient(t.mk("SELECT b\r\n"));
    cServer(t.last("OK selected\r\n"));
    cEmpty();
}

TROJITA_HEADLESS_TEST( ImapModelSelectedMailboxUpdatesTest )
