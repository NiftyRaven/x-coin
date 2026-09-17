// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xtheme.h"

#include <QFile>
#include <QWidget>

QString XPageStyleSheet()
{
    QFile f(":/darkstyle/xpages");
    if (f.open(QFile::ReadOnly))
        return QString::fromUtf8(f.readAll());
    return QString::fromLatin1(
        "QWidget { background: #000000; color: #ffffff; }"
        "QLabel#xhero { color: #ffffff; font-size: 40px; font-weight: 800; letter-spacing: 8px; }"
        "QLabel#xtag { color: #a1a1aa; font-size: 13px; }"
        "QLabel#xsection { color: #ffffff; font-size: 12px; font-weight: 800; letter-spacing: 3px; margin-top: 8px; }"
        "QLabel#xcard { background: #0a0a0a; border: 1px solid #27272a; padding: 14px; color: #e4e4e7; font-size: 13px; }"
        "QLabel#xbalance { background: #0a0a0a; border: 1px solid #ffffff; padding: 18px; color: #ffffff; font-size: 22px; font-weight: 700; }"
        "QLabel#xlotteryok { background: #0a0a0a; border: 1px solid #ffffff; padding: 16px; color: #ffffff; font-size: 13px; }"
        "QLabel#xlotteryno { background: #0a0a0a; border: 1px solid #3f3f46; padding: 16px; color: #a1a1aa; font-size: 13px; }"
        "QLabel#xlinked { background: #0a0a0a; border: 1px solid #ffffff; padding: 16px; color: #ffffff; font-size: 13px; }"
        "QLabel#xunlinked { background: #0a0a0a; border: 1px solid #3f3f46; padding: 16px; color: #a1a1aa; font-size: 13px; }"
        "QLabel#xrelease { background: #0a0a0a; border: 1px solid #ffffff; padding: 16px; color: #ffffff; font-size: 13px; }"
        "QLabel#xhint { color: #71717a; font-size: 13px; }"
        "QLabel#xversion { color: #a1a1aa; font-size: 12px; }"
        "QLabel#xstatuserr { color: #fca5a5; font-size: 12px; }"
        "QLabel#xaddr { background: #0a0a0a; border: 1px solid #ffffff; padding: 18px; font-size: 16px; font-family: monospace; color: #ffffff; }"
        "QPushButton#xprimary { background: #ffffff; color: #000000; border: none; padding: 10px 22px; font-weight: 700; font-size: 14px; }"
        "QPushButton#xprimary:hover { background: #e4e4e7; }"
        "QPushButton#xprimary:disabled { background: #27272a; color: #71717a; }"
        "QPushButton#xghost { background: #000000; color: #ffffff; border: 1px solid #52525b; padding: 10px 18px; font-weight: 600; }"
        "QPushButton#xghost:hover { border-color: #ffffff; }"
        "QCheckBox#xcheck { color: #a1a1aa; font-size: 13px; spacing: 8px; }"
        "QCheckBox#xcheck:hover { color: #ffffff; }"
        "QCheckBox#xcheck::indicator { width: 16px; height: 16px; border: 1px solid #52525b; background: #000000; }"
        "QCheckBox#xcheck::indicator:checked { background: #ffffff; border-color: #ffffff; }"
        "QLineEdit, QComboBox, QPlainTextEdit, QSpinBox { background: #0a0a0a; color: #ffffff; border: 1px solid #3f3f46; padding: 10px 12px; min-height: 22px; selection-background-color: #ffffff; selection-color: #000000; }"
        "QLineEdit:focus, QComboBox:focus, QPlainTextEdit:focus { border-color: #ffffff; }"
        "QComboBox::drop-down { border: none; width: 22px; background: #0a0a0a; }"
        "QComboBox QAbstractItemView { background: #000000; color: #ffffff; selection-background-color: #ffffff; selection-color: #000000; border: 1px solid #ffffff; outline: 0; }"
        "QComboBox QAbstractItemView::item { min-height: 28px; padding: 6px 12px; }"
        "QTableWidget { background: #000000; color: #ffffff; gridline-color: #27272a; border: 1px solid #27272a; }"
        "QTableWidget::item:selected { background: #ffffff; color: #000000; }"
        "QHeaderView::section { background: #0a0a0a; color: #a1a1aa; border: none; padding: 8px; font-weight: 700; }");
}

void ApplyXPageTheme(QWidget* widget)
{
    if (widget)
        widget->setStyleSheet(XPageStyleSheet());
}

QString XNavStyleSheet()
{
    return QString::fromLatin1(
        "QWidget#xnavWrap { background: #000000; }"
        "QTreeWidget#xnav { background: #000000; color: #ffffff; border: none; outline: none; font-size: 13px; }"
        "QTreeWidget#xnav::item { padding: 7px 10px; }"
        "QTreeWidget#xnav::item:hover { background: #111111; }"
        "QTreeWidget#xnav::item:selected { background: #ffffff; color: #000000; }"
        "QTreeWidget#xnav::branch { background: #000000; }"
        "QLabel#xnavBrand { background: transparent; }");
}
