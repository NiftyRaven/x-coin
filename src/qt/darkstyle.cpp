/*
###############################################################################
#                                                                             #
# The MIT License                                                             #
#                                                                             #
# Copyright (C) 2017 by Juergen Skrotzky (JorgenVikingGod@gmail.com)          #
#               >> https://github.com/Jorgen-VikingGod                        #
#                                                                             #
# Sources: https://github.com/Jorgen-VikingGod/Qt-Frameless-Window-DarkStyle  #
# X Coin 1.0 restyle: black / white / sharp.                                  #
#                                                                             #
###############################################################################
*/

#include <QDebug>
#include "darkstyle.h"

DarkStyle::DarkStyle():
  DarkStyle(styleBase())
{ }

DarkStyle::DarkStyle(QStyle *style):
  QProxyStyle(style)
{ }

QStyle *DarkStyle::styleBase(QStyle *style) const {
  static QStyle *base = !style ? QStyleFactory::create(QStringLiteral("Fusion")) : style;
  return base;
}

QStyle *DarkStyle::baseStyle() const
{
  return styleBase();
}

void DarkStyle::polish(QPalette &palette)
{
  palette.setColor(QPalette::Window, QColor(0, 0, 0));
  palette.setColor(QPalette::WindowText, Qt::white);
  palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(120, 120, 120));
  palette.setColor(QPalette::Base, QColor(10, 10, 10));
  palette.setColor(QPalette::AlternateBase, QColor(22, 22, 22));
  palette.setColor(QPalette::ToolTipBase, QColor(0, 0, 0));
  palette.setColor(QPalette::ToolTipText, Qt::white);
  palette.setColor(QPalette::Text, Qt::white);
  palette.setColor(QPalette::Disabled, QPalette::Text, QColor(120, 120, 120));
  palette.setColor(QPalette::Dark, QColor(0, 0, 0));
  palette.setColor(QPalette::Shadow, QColor(0, 0, 0));
  palette.setColor(QPalette::Button, QColor(18, 18, 18));
  palette.setColor(QPalette::ButtonText, Qt::white);
  palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(120, 120, 120));
  palette.setColor(QPalette::BrightText, Qt::white);
  palette.setColor(QPalette::Link, QColor(255, 255, 255));
  palette.setColor(QPalette::Highlight, QColor(255, 255, 255));
  palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(60, 60, 60));
  palette.setColor(QPalette::HighlightedText, Qt::black);
  palette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(120, 120, 120));
}

void DarkStyle::polish(QApplication *app)
{
  if (!app) return;

  QFont defaultFont = QApplication::font();
  defaultFont.setFamily(QStringLiteral("Open Sans"));
  defaultFont.setLetterSpacing(QFont::AbsoluteSpacing, -0.2);
  app->setFont(defaultFont);

  QFile qfDarkstyle(QStringLiteral(":/darkstyle/qss"));
  if (qfDarkstyle.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    QString qsStylesheet = QString::fromLatin1(qfDarkstyle.readAll());
    app->setStyleSheet(qsStylesheet);
    qfDarkstyle.close();
  }
}
