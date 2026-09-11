// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef RAVEN_QT_XRELEASEDIALOG_H
#define RAVEN_QT_XRELEASEDIALOG_H

#include <QDialog>
#include <QString>

class QPushButton;

/** In-wallet release notes. Open download is optional. */
class XReleaseDialog : public QDialog
{
    Q_OBJECT

public:
    explicit XReleaseDialog(QWidget* parent,
                            const QString& title,
                            const QString& notes,
                            const QString& url,
                            bool newerAvailable);

    bool dismissed() const { return m_dismissed; }

private Q_SLOTS:
    void onOpenDownload();
    void onLater();

private:
    QString m_url;
    bool m_dismissed;
};

#endif
