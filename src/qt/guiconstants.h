// Copyright (c) 2011-2016 The Bitcoin Core developers
// Copyright (c) 2017-2021 The Raven Core developers
// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef RAVEN_QT_GUICONSTANTS_H
#define RAVEN_QT_GUICONSTANTS_H

/* Milliseconds between model updates */
static const int MODEL_UPDATE_DELAY = 250;

/* AskPassphraseDialog -- Maximum passphrase length */
static const int MAX_PASSPHRASE_SIZE = 1024;

/* RavenGUI -- Size of icons in status bar */
static const int STATUSBAR_ICONSIZE = 16;

static const bool DEFAULT_SPLASHSCREEN = true;

/* Invalid field background style */
#define STYLE_INVALID "background:#3a0000; border: 1px solid #ffffff; padding: 0px;"
#define STYLE_VALID "border: 1px solid #555555; padding: 0px;"

/* Transaction list -- unconfirmed transaction */
#define COLOR_UNCONFIRMED QColor(160, 160, 160)
/* Transaction list -- negative amount */
#define COLOR_NEGATIVE QColor(255, 255, 255)
/* Transaction list -- bare address (without label) */
#define COLOR_BAREADDRESS QColor(170, 170, 170)
/* Transaction list -- TX status decoration - open until date */
#define COLOR_TX_STATUS_OPENUNTILDATE QColor(200, 200, 200)
/* Transaction list -- TX status decoration - danger, tx needs attention */
#define COLOR_TX_STATUS_DANGER QColor(200, 100, 100)
/* Transaction list -- TX status decoration - default color */
#define COLOR_BLACK QColor(0, 0, 0)
/* Widget Background color - default color */
#define COLOR_WHITE QColor(255, 255, 255)

#define COLOR_WALLETFRAME_SHADOW QColor(0,0,0,180)

/* Color of labels — X theme is black / white / sharp */
#define COLOR_LABELS QColor("#ffffff")

/** LIGHT MODE (still X: white field, black type, no orange/green) */
#define COLOR_BACKGROUND_LIGHT QColor("#f4f4f4")
#define COLOR_DARK_ORANGE QColor("#000000")
#define COLOR_LIGHT_ORANGE QColor("#111111")
#define COLOR_DARK_BLUE QColor("#000000")
#define COLOR_LIGHT_BLUE QColor("#1a1a1a")
#define COLOR_ASSET_TEXT QColor(255, 255, 255)
#define COLOR_SHADOW_LIGHT QColor("#d0d0d0")
#define COLOR_TOOLBAR_NOT_SELECTED_TEXT QColor("#666666")
#define COLOR_TOOLBAR_SELECTED_TEXT COLOR_WHITE
#define COLOR_SENDENTRIES_BACKGROUND QColor("#f4f4f4")


/** DARK MODE — default X look */
#define COLOR_WIDGET_BACKGROUND_DARK QColor("#0a0a0a")
#define COLOR_SHADOW_DARK QColor("#000000")
#define COLOR_LIGHT_BLUE_DARK QColor("#111111")
#define COLOR_DARK_BLUE_DARK QColor("#000000")
#define COLOR_PRICING_WIDGET QColor("#000000")
#define COLOR_ADMIN_CARD_DARK QColor("#1a1a1a")
#define COLOR_REGULAR_CARD_DARK_BLUE_DARK_MODE QColor("#111111")
#define COLOR_REGULAR_CARD_LIGHT_BLUE_DARK_MODE QColor("#1a1a1a")
#define COLOR_TOOLBAR_NOT_SELECTED_TEXT_DARK_MODE QColor("#8a8a8a")
#define COLOR_TOOLBAR_SELECTED_TEXT_DARK_MODE QColor("#ffffff")
#define COLOR_SENDENTRIES_BACKGROUND_DARK QColor("#0a0a0a")


#define STRING_LABEL_COLOR "color: #ffffff"
#define STRING_LABEL_COLOR_WARNING "color: #FF8080"


/* Tooltips longer than this (in characters) are converted into rich text,
   so that they can be word-wrapped.
 */
static const int TOOLTIP_WRAP_THRESHOLD = 80;

/* Maximum allowed URI length */
static const int MAX_URI_LENGTH = 255;

/* QRCodeDialog -- size of exported QR Code image */
#define QR_IMAGE_SIZE 300

/* Number of frames in spinner animation */
#define SPINNER_FRAMES 36

#define QAPP_ORG_NAME "X Coin"
#define QAPP_ORG_DOMAIN "xcoin"
#define QAPP_APP_NAME_DEFAULT "X Coin"
#define QAPP_APP_NAME_TESTNET "X Coin-testnet"

/* No public explorers for this private chain */
#define DEFAULT_THIRD_PARTY_BROWSERS ""

/* Default IPFS viewer */
#define DEFAULT_IPFS_VIEWER "https://ipfs.io/ipfs/%s"

#endif // RAVEN_QT_GUICONSTANTS_H
