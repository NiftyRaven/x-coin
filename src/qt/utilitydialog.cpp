// Copyright (c) 2011-2016 The Bitcoin Core developers
// Copyright (c) 2017-2019 The Raven Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include "config/raven-config.h"
#endif

#include "utilitydialog.h"

#include "ui_helpmessagedialog.h"

#include "ravengui.h"
#include "clientmodel.h"
#include "guiconstants.h"
#include "intro.h"
#include "paymentrequestplus.h"
#include "guiutil.h"

#include "init.h"
#include "util.h"
#include "xrelease.h"

#include <stdio.h>

#include <QCloseEvent>
#include <QLabel>
#include <QRegExp>
#include <QTextTable>
#include <QTextCursor>
#include <QVBoxLayout>

/** "Help message" or "About" dialog box */
HelpMessageDialog::HelpMessageDialog(QWidget *parent, bool about) :
    QDialog(parent),
    ui(new Ui::HelpMessageDialog)
{
    ui->setupUi(this);

    QString version = tr("X Coin (XFER)") + " " + tr("version") + " " +
        QString::fromStdString(xrelease::RunningVersionString());
    /* On x86 add a bit specifier to the version so that users can distinguish between
     * 32 and 64 bit builds. On other architectures, 32/64 bit may be more ambiguous.
     */
#if defined(__x86_64__)
    version += " " + tr("(%1-bit)").arg(64);
#elif defined(__i386__ )
    version += " " + tr("(%1-bit)").arg(32);
#endif

    if (about)
    {
        setWindowTitle(tr("About %1").arg(tr(PACKAGE_NAME)));

        /// HTML-format the license message from the core
        QString licenseInfo = QString::fromStdString(LicenseInfo());
        QString licenseInfoHTML = licenseInfo;
        // Make URLs clickable
        QRegExp uri("<(.*)>", Qt::CaseSensitive, QRegExp::RegExp2);
        uri.setMinimal(true); // use non-greedy matching
        licenseInfoHTML.replace(uri, "<a href=\"\\1\">\\1</a>");
        // Replace newlines with HTML breaks
        licenseInfoHTML.replace("\n", "<br>");

        ui->aboutMessage->setTextFormat(Qt::RichText);
        ui->scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        text = version + "\n" + licenseInfo;
        ui->aboutMessage->setText(version + "<br><br>" + licenseInfoHTML);
        ui->aboutMessage->setWordWrap(true);
        ui->helpMessage->setVisible(false);
    } else {
        setWindowTitle(tr("Command-line options"));
        QString header = tr("Usage:") + "\n" +
            "  xcoin-qt [" + tr("command-line options") + "]                     " + "\n";
        QTextCursor cursor(ui->helpMessage->document());
        cursor.insertText(version);
        cursor.insertBlock();
        cursor.insertText(header);
        cursor.insertBlock();

        // Wallet-facing list. xcoind -help still prints the full node list.
        std::string strUsage;
        strUsage += HelpMessageGroup(_("Options:"));
        strUsage += HelpMessageOpt("-?", _("Print this help message and exit"));
        strUsage += HelpMessageOpt("-version", _("Print version and exit"));
        strUsage += HelpMessageOpt("-datadir=<dir>", _("Specify data directory"));
        strUsage += HelpMessageOpt("-conf=<file>", strprintf(_("Specify configuration file (default: %s)"), "xcoin.conf"));
        strUsage += HelpMessageOpt("-choosedatadir", strprintf(_("Choose data directory on startup (default: %u)"), DEFAULT_CHOOSE_DATADIR));
        strUsage += HelpMessageOpt("-lang=<lang>", _("Set language, for example \"de_DE\" (default: system locale)"));
        strUsage += HelpMessageOpt("-min", _("Start minimized"));
        strUsage += HelpMessageOpt("-splash", strprintf(_("Show splash screen on startup (default: %u)"), DEFAULT_SPLASHSCREEN));
        strUsage += HelpMessageOpt("-resetguisettings", _("Reset all settings changed in the GUI"));

        strUsage += HelpMessageGroup(_("Network:"));
        strUsage += HelpMessageOpt("-regtest", _("Use the practice chain. X Coin Practice Wallet always sets this. Practice coins are not main XFER."));
        strUsage += HelpMessageOpt("-testnet", _("Use the test chain"));
        strUsage += HelpMessageOpt("-addnode=<ip>", _("Add a node to connect to and attempt to keep the connection open"));
        strUsage += HelpMessageOpt("-connect=<ip>", _("Connect only to the specified node(s); -connect=0 disables automatic connections"));
        strUsage += HelpMessageOpt("-packageconf=<file>", _("Read extra defaults from the package xcoin.conf (addnode, xoauthclientid). Default: xcoin.conf next to the wallet, or one folder up. Use -packageconf=0 to disable."));
        strUsage += HelpMessageOpt("-port=<port>", _("Listen for connections on <port> (default: 38443, testnet: 48443, regtest: 28443)"));
        strUsage += HelpMessageOpt("-listen", _("Accept connections from outside (default: 1 if no -proxy or -connect)"));
        strUsage += HelpMessageOpt("-server", _("Accept command line and JSON-RPC commands"));

        strUsage += HelpMessageGroup(_("Wallet:"));
        strUsage += HelpMessageOpt("-wallet=<file>", _("Specify wallet file within the data directory (default: wallet.dat)"));
        strUsage += HelpMessageOpt("-disablewallet", _("Do not load the wallet and disable wallet RPC calls"));
        strUsage += HelpMessageOpt("-rescan", _("Rescan the blockchain for missing wallet transactions on startup"));
        strUsage += HelpMessageOpt("-reindex", _("Rebuild chain state and block index from the blk*.dat files on disk"));
        strUsage += HelpMessageOpt("-salvagewallet", _("Attempt to recover private keys from a corrupt wallet on startup"));
        strUsage += HelpMessageOpt("-zapwallettxes=<mode>", _("Delete wallet transactions and recover them with -rescan on startup (1 = keep metadata, 2 = drop metadata)"));
        strUsage += HelpMessageOpt("-paytxfee=<amt>", _("Fee (in XFER/kB) to add to transactions you send"));
        strUsage += HelpMessageOpt("-upgradewallet", _("Upgrade wallet to latest format on startup"));

        strUsage += HelpMessageGroup(_("Indexes:"));
        strUsage += HelpMessageOpt("-txindex", _("Maintain a full transaction index, used by getrawtransaction (default: 0)"));
        strUsage += HelpMessageOpt("-assetindex", _("Keep an index of assets, used by holder lookup (default: 0). This Light package does not set assetindex=1. Changing this requires -reindex."));

        strUsage += HelpMessageGroup(_("Sign in with X:"));
        strUsage += HelpMessageOpt("-xoauthclientid=<id>", _("Operator X OAuth client id baked into the packaged xcoin.conf. Callback http://127.0.0.1:18791/callback."));
        strUsage += HelpMessageOpt("-xoauthcallbackport=<n>", _("Loopback callback port for Sign in with X (default: 18791)"));
        strUsage += HelpMessageOpt("-xreleaseurl=<url>", _("HTTPS JSON feed for Help → What's new (default: the X Coin GitHub releases API). No wallet data is sent."));
        strUsage += HelpMessageOpt("-nocheckupdates", _("Do not fetch the release feed when the wallet starts. Help → What's new still shows notes for this version."));

        strUsage += HelpMessageGroup(_("RPC:"));
        strUsage += HelpMessageOpt("-rpcuser=<user>", _("Username for JSON-RPC connections"));
        strUsage += HelpMessageOpt("-rpcpassword=<pw>", _("Password for JSON-RPC connections"));
        strUsage += HelpMessageOpt("-rpcport=<port>", _("Listen for JSON-RPC connections on <port> (default: 38442, testnet: 48442, regtest: 28442)"));
        strUsage += HelpMessageOpt("-rpcallowip=<ip>", _("Allow JSON-RPC connections from the specified source. Can be specified multiple times"));

        strUsage += HelpMessageGroup(_("More:"));
        strUsage += HelpMessageOpt("-help", _("This page. For proxy, prune, debug log, and relay policy, run xcoind -help. Those options still work in this wallet."));
        QString coreOptions = QString::fromStdString(strUsage);
        text = version + "\n" + header + "\n" + coreOptions;

        QTextTableFormat tf;
        tf.setBorderStyle(QTextFrameFormat::BorderStyle_None);
        tf.setCellPadding(2);
        QVector<QTextLength> widths;
        widths << QTextLength(QTextLength::PercentageLength, 35);
        widths << QTextLength(QTextLength::PercentageLength, 65);
        tf.setColumnWidthConstraints(widths);

        QTextCharFormat bold;
        bold.setFontWeight(QFont::Bold);

        for (const QString &line : coreOptions.split("\n")) {
            if (line.startsWith("  -"))
            {
                cursor.currentTable()->appendRows(1);
                cursor.movePosition(QTextCursor::PreviousCell);
                cursor.movePosition(QTextCursor::NextRow);
                cursor.insertText(line.trimmed());
                cursor.movePosition(QTextCursor::NextCell);
            } else if (line.startsWith("   ")) {
                cursor.insertText(line.trimmed()+' ');
            } else if (line.size() > 0) {
                //Title of a group
                if (cursor.currentTable())
                    cursor.currentTable()->appendRows(1);
                cursor.movePosition(QTextCursor::Down);
                cursor.insertText(line.trimmed(), bold);
                cursor.insertTable(1, 2, tf);
            }
        }

        ui->helpMessage->moveCursor(QTextCursor::Start);
        ui->scrollArea->setVisible(false);
        ui->aboutLogo->setVisible(false);
    }
}

HelpMessageDialog::~HelpMessageDialog()
{
    delete ui;
}

void HelpMessageDialog::printToConsole()
{
    // On other operating systems, the expected action is to print the message to the console.
    fprintf(stdout, "%s\n", qPrintable(text));
}

void HelpMessageDialog::showOrPrint()
{
#if defined(WIN32)
    // On Windows, show a message box, as there is no stderr/stdout in windowed applications
    exec();
#else
    // On other operating systems, print help text to console
    printToConsole();
#endif
}

void HelpMessageDialog::on_okButton_accepted()
{
    close();
}




/** "Shutdown" window */
ShutdownWindow::ShutdownWindow(QWidget *parent, Qt::WindowFlags f):
    QWidget(parent, f)
{
    QVBoxLayout *layout = new QVBoxLayout();
    layout->addWidget(new QLabel(
        tr("%1 is shutting down...").arg(tr(PACKAGE_NAME)) + "<br /><br />" +
        tr("Do not shut down the computer until this window disappears.")));
    setLayout(layout);
}

QWidget *ShutdownWindow::showShutdownWindow(RavenGUI *window)
{
    if (!window)
        return nullptr;

    // Show a simple window indicating shutdown status
    QWidget *shutdownWindow = new ShutdownWindow();
    shutdownWindow->setWindowTitle(window->windowTitle());

    // Center shutdown window at where main window was
    const QPoint global = window->mapToGlobal(window->rect().center());
    shutdownWindow->move(global.x() - shutdownWindow->width() / 2, global.y() - shutdownWindow->height() / 2);
    shutdownWindow->show();
    return shutdownWindow;
}



void ShutdownWindow::closeEvent(QCloseEvent *event)
{
    event->ignore();
}


