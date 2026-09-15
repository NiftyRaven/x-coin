// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef RAVEN_QT_XTHEME_H
#define RAVEN_QT_XTHEME_H

class QWidget;
class QString;

QString XPageStyleSheet();
void ApplyXPageTheme(QWidget* widget);
QString XNavStyleSheet();

#endif
