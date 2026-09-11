// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xreleasedialog.h"
#include "xrelease.h"

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

XReleaseDialog::XReleaseDialog(QWidget* parent,
                               const QString& title,
                               const QString& notes,
                               const QString& url,
                               bool newerAvailable)
    : QDialog(parent)
    , m_url(xrelease::IsSafeDownloadUrl(url.toStdString()) ? url : QString())
    , m_dismissed(false)
{
    setWindowTitle(newerAvailable ? tr("New X Coin release") : tr("What's new"));
    setMinimumSize(520, 420);
    resize(560, 480);

    QVBoxLayout* root = new QVBoxLayout(this);
    QLabel* head = new QLabel(title);
    head->setWordWrap(true);
    QFont f = head->font();
    f.setBold(true);
    f.setPointSize(f.pointSize() + 2);
    head->setFont(f);
    root->addWidget(head);

    if (newerAvailable) {
        QLabel* hint = new QLabel(tr("A newer wallet is on GitHub Releases. Notes are below. This copy keeps working until you install the new package."));
        hint->setWordWrap(true);
        root->addWidget(hint);
    }

    QPlainTextEdit* body = new QPlainTextEdit;
    body->setReadOnly(true);
    body->setPlainText(notes);
    root->addWidget(body, 1);

    QHBoxLayout* row = new QHBoxLayout;
    row->addStretch(1);
    if (!m_url.isEmpty()) {
        QPushButton* open = new QPushButton(tr("Open download page"));
        connect(open, SIGNAL(clicked()), this, SLOT(onOpenDownload()));
        row->addWidget(open);
    }
    if (newerAvailable) {
        QPushButton* later = new QPushButton(tr("Later"));
        connect(later, SIGNAL(clicked()), this, SLOT(onLater()));
        row->addWidget(later);
    }
    QPushButton* close = new QPushButton(newerAvailable ? tr("Keep using this wallet") : tr("OK"));
    connect(close, SIGNAL(clicked()), this, SLOT(accept()));
    row->addWidget(close);
    root->addLayout(row);
}

void XReleaseDialog::onOpenDownload()
{
    if (!m_url.isEmpty())
        QDesktopServices::openUrl(QUrl(m_url));
}

void XReleaseDialog::onLater()
{
    m_dismissed = true;
    reject();
}
