// Copyright (c) 2011-2015 The Bitcoin Core developers
// Copyright (c) 2014-2020 The Dash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/guiutil.h>

#include <qt/appearancewidget.h>
#include <qt/bitcoinaddressvalidator.h>
#include <qt/bitcoingui.h>
#include <qt/bitcoinunits.h>
#include <qt/optionsdialog.h>
#include <qt/qvalidatedlineedit.h>
#include <qt/walletmodel.h>

#include <primitives/transaction.h>
#include <init.h>
#include <policy/policy.h>
#include <protocol.h>
#include <script/script.h>
#include <script/standard.h>
#include <ui_interface.h>
#include <util.h>

#ifdef WIN32
#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0501
#ifdef _WIN32_IE
#undef _WIN32_IE
#endif
#define _WIN32_IE 0x0501
#define WIN32_LEAN_AND_MEAN 1
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#endif

#include <boost/scoped_array.hpp>

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QDesktopWidget>
#include <QDialogButtonBox>
#include <QDoubleValidator>
#include <QFileDialog>
#include <QFont>
#include <QLineEdit>
#include <QSettings>
#include <QTextDocument> // for Qt::mightBeRichText
#include <QThread>
#include <QTimer>
#include <QMouseEvent>
#include <QVBoxLayout>

#if QT_VERSION < 0x050000
#include <QUrl>
#else
#include <QUrlQuery>
#endif

#if QT_VERSION >= 0x50200
#include <QFontDatabase>
#endif

static fs::detail::utf8_codecvt_facet utf8;

#if defined(Q_OS_MAC)
extern double NSAppKitVersionNumber;
#if !defined(NSAppKitVersionNumber10_8)
#define NSAppKitVersionNumber10_8 1187
#endif
#if !defined(NSAppKitVersionNumber10_9)
#define NSAppKitVersionNumber10_9 1265
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include <CoreServices/CoreServices.h>
#include <QProcess>

void ForceActivation();
#endif

namespace GUIUtil {

static CCriticalSection cs_css;
// The default stylesheet directory
static const QString defaultStylesheetDirectory = ":css";
// The actual stylesheet directory
static QString stylesheetDirectory = defaultStylesheetDirectory;
// The name of the traditional theme
static const QString traditionalTheme = "Traditional";
// The theme to set by default if settings are missing or incorrect
static const QString defaultTheme = "Light";
// The prefix a theme name should have if we want to apply dark colors and styles to it
static const QString darkThemePrefix = "Dark";
// Mapping css file => theme.
static const std::map<QString, QString> mapStyleToTheme{
    {"general.css", ""},
    {"dark.css", "Dark"},
    {"light.css", "Light"},
    {"traditional.css", "Traditional"}
};

/** loadFonts stores the SystemDefault font in osDefaultFont to be able to reference it later again */
static std::unique_ptr<QFont> osDefaultFont;
/** Font related default values. */
static const FontFamily defaultFontFamily = FontFamily::SystemDefault;
static const int defaultFontSize = 12;
static const double fontScaleSteps = 0.01;
#ifdef Q_OS_MAC
static const QFont::Weight defaultFontWeightNormal = QFont::ExtraLight;
static const QFont::Weight defaultFontWeightBold = QFont::Medium;
static const int defaultFontScale = 0;
#else
static const QFont::Weight defaultFontWeightNormal = QFont::Light;
static const QFont::Weight defaultFontWeightBold = QFont::Medium;
static const int defaultFontScale = 0;
#endif

/** Font related variables. */
// Application font family. May be overwritten by -font-family.
static FontFamily fontFamily = defaultFontFamily;
// Application font scale value. May be overwritten by -font-scale.
static int fontScale = defaultFontScale;
// Application font weight for normal text. May be overwritten by -font-weight-normal.
static QFont::Weight fontWeightNormal = defaultFontWeightNormal;
// Application font weight for bold text. May be overwritten by -font-weight-bold.
static QFont::Weight fontWeightBold = defaultFontWeightBold;

// Contains all widgets and its font attributes (weight, italic, size) with font changes due to GUIUtil::setFont
static std::map<QWidget*, std::tuple<FontWeight, bool, int>> mapNormalFontUpdates;
// Contains a list of supported font weights for all members of GUIUtil::FontFamily
static std::map<FontFamily, std::vector<QFont::Weight>> mapSupportedWeights;

#ifdef Q_OS_MAC
// Contains all widgets where the macOS focus rect has been disabled.
static std::set<QWidget*> setRectsDisabled;
#endif

static const std::map<ThemedColor, QColor> themedColors = {
    { ThemedColor::DEFAULT, QColor(85, 85, 85) },
    { ThemedColor::UNCONFIRMED, QColor(128, 128, 128) },
    { ThemedColor::BLUE, QColor(0, 141, 228) },
    { ThemedColor::ORANGE, QColor(199, 147, 4) },
    { ThemedColor::RED, QColor(168, 72, 50) },
    { ThemedColor::GREEN, QColor(94, 140, 65) },
    { ThemedColor::BAREADDRESS, QColor(140, 140, 140) },
    { ThemedColor::TX_STATUS_OPENUNTILDATE, QColor(64, 64, 255) },
    { ThemedColor::BACKGROUND_WIDGET, QColor(234, 234, 236) },
    { ThemedColor::BORDER_WIDGET, QColor(220, 220, 220) },
    { ThemedColor::BACKGROUND_NETSTATS, QColor(210, 210, 210, 230) },
    { ThemedColor::BORDER_NETSTATS, QColor(180, 180, 180) },
    { ThemedColor::QR_PIXEL, QColor(85, 85, 85) },
    { ThemedColor::LOGO, QColor(242, 25, 25) },
    { ThemedColor::ICON_ALTERNATIVE_COLOR, QColor(167, 167, 167) },
};

static const std::map<ThemedColor, QColor> themedDarkColors = {
    { ThemedColor::DEFAULT, QColor(199, 199, 199) },
    { ThemedColor::UNCONFIRMED, QColor(170, 170, 170) },
    { ThemedColor::BLUE, QColor(0, 89, 154) },
    { ThemedColor::ORANGE, QColor(199, 147, 4) },
    { ThemedColor::RED, QColor(168, 72, 50) },
    { ThemedColor::GREEN, QColor(94, 140, 65) },
    { ThemedColor::BAREADDRESS, QColor(140, 140, 140) },
    { ThemedColor::TX_STATUS_OPENUNTILDATE, QColor(64, 64, 255) },
    { ThemedColor::BACKGROUND_WIDGET, QColor(45, 45, 46) },
    { ThemedColor::BORDER_WIDGET, QColor(74, 74, 75) },
    { ThemedColor::BACKGROUND_NETSTATS, QColor(45, 45, 46, 230) },
    { ThemedColor::BORDER_NETSTATS, QColor(74, 74, 75) },
    { ThemedColor::QR_PIXEL, QColor(199, 199, 199) },
    { ThemedColor::LOGO, QColor(242, 25, 25) },
    { ThemedColor::ICON_ALTERNATIVE_COLOR, QColor(74, 74, 75) },
};

static const std::map<ThemedStyle, QString> themedStyles = {
    { ThemedStyle::TS_INVALID, "background:#a84832;" },
    { ThemedStyle::TS_ERROR, "color:#a84832;" },
    { ThemedStyle::TS_SUCCESS, "color:#5e8c41;" },
    { ThemedStyle::TS_COMMAND, "color:#e60000;" },
    { ThemedStyle::TS_PRIMARY, "color:#333;" },
    { ThemedStyle::TS_SECONDARY, "color:#444;" },
};

static const std::map<ThemedStyle, QString> themedDarkStyles = {
    { ThemedStyle::TS_INVALID, "background:#a84832;" },
    { ThemedStyle::TS_ERROR, "color:#a84832;" },
    { ThemedStyle::TS_SUCCESS, "color:#5e8c41;" },
    { ThemedStyle::TS_COMMAND, "color:#990000;" },
    { ThemedStyle::TS_PRIMARY, "color:#c7c7c7;" },
    { ThemedStyle::TS_SECONDARY, "color:#aaa;" },
};

QColor getThemedQColor(ThemedColor color)
{
    QString theme = QSettings().value("theme", "").toString();
    return theme.startsWith(darkThemePrefix) ? themedDarkColors.at(color) : themedColors.at(color);
}

QString getThemedStyleQString(ThemedStyle style)
{
    QString theme = QSettings().value("theme", "").toString();
    return theme.startsWith(darkThemePrefix) ? themedDarkStyles.at(style) : themedStyles.at(style);
}

QIcon getIcon(const QString& strIcon, const ThemedColor color, const ThemedColor colorAlternative, const QString& strIconPath)
{
    QColor qcolor = getThemedQColor(color);
    QColor qcolorAlternative = getThemedQColor(colorAlternative);
    QIcon icon(strIconPath + strIcon);
    QIcon themedIcon;
    for (const QSize& size : icon.availableSizes()) {
        QImage image(icon.pixmap(size).toImage());
        image = image.convertToFormat(QImage::Format_ARGB32);
        for (int x = 0; x < image.width(); ++x) {
            for (int y = 0; y < image.height(); ++y) {
                const QRgb rgb = image.pixel(x, y);
                QColor* pColor;
                if ((rgb & RGB_MASK) < RGB_HALF) {
                    pColor = &qcolor;
                } else {
                    pColor = &qcolorAlternative;
                }
                image.setPixel(x, y, qRgba(pColor->red(), pColor->green(), pColor->blue(), qAlpha(rgb)));
            }
        }
        themedIcon.addPixmap(QPixmap::fromImage(image));
    }
    return themedIcon;
}

QIcon getIcon(const QString& strIcon, const ThemedColor color, const QString& strIconPath)
{
    return getIcon(strIcon, color, ThemedColor::ICON_ALTERNATIVE_COLOR, strIconPath);
}

void setIcon(QAbstractButton* button, const QString& strIcon, const ThemedColor color, const ThemedColor colorAlternative, const QSize& size)
{
    button->setIcon(getIcon(strIcon, color, colorAlternative));
    button->setIconSize(size);
}

void setIcon(QAbstractButton* button, const QString& strIcon, const ThemedColor color, const QSize& size)
{
    setIcon(button, strIcon, color, ThemedColor::ICON_ALTERNATIVE_COLOR, size);
}

QString dateTimeStr(const QDateTime &date)
{
    return date.date().toString(Qt::SystemLocaleShortDate) + QString(" ") + date.toString("hh:mm");
}

QString dateTimeStr(qint64 nTime)
{
    return dateTimeStr(QDateTime::fromTime_t((qint32)nTime));
}

// Just some dummy data to generate an convincing random-looking (but consistent) address
static const uint8_t dummydata[] = {0xeb,0x15,0x23,0x1d,0xfc,0xeb,0x60,0x92,0x58,0x86,0xb6,0x7d,0x06,0x52,0x99,0x92,0x59,0x15,0xae,0xb1,0x72,0xc0,0x66,0x47};

// Generate a dummy address with invalid CRC, starting with the network prefix.
static std::string DummyAddress(const CChainParams &params)
{
    std::vector<unsigned char> sourcedata = params.Base58Prefix(CChainParams::PUBKEY_ADDRESS);
    sourcedata.insert(sourcedata.end(), dummydata, dummydata + sizeof(dummydata));
    for(int i=0; i<256; ++i) { // Try every trailing byte
        std::string s = EncodeBase58(sourcedata.data(), sourcedata.data() + sourcedata.size());
        if (!IsValidDestinationString(s)) {
            return s;
        }
        sourcedata[sourcedata.size()-1] += 1;
    }
    return "";
}

void setupAddressWidget(QValidatedLineEdit *widget, QWidget *parent, bool fAllowURI)
{
    parent->setFocusProxy(widget);

#if QT_VERSION >= 0x040700
    // We don't want translators to use own addresses in translations
    // and this is the only place, where this address is supplied.
    widget->setPlaceholderText(QObject::tr("Enter a Axe address (e.g. %1)").arg(
        QString::fromStdString(DummyAddress(Params()))));
#endif
    widget->setValidator(new BitcoinAddressEntryValidator(parent, fAllowURI));
    widget->setCheckValidator(new BitcoinAddressCheckValidator(parent));
}

void setupAmountWidget(QLineEdit *widget, QWidget *parent)
{
    QDoubleValidator *amountValidator = new QDoubleValidator(parent);
    amountValidator->setDecimals(8);
    amountValidator->setBottom(0.0);
    widget->setValidator(amountValidator);
    widget->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
}

void setupAppearance(QWidget* parent, OptionsModel* model)
{
    if (!QSettings().value("fAppearanceSetupDone", false).toBool()) {
        // First make sure SystemDefault has reasonable default values if it does not support the full range of weights.
        if (fontFamily == FontFamily::SystemDefault && getSupportedWeights().size() < 4) {
            fontWeightNormal = mapSupportedWeights[FontFamily::SystemDefault].front();
            fontWeightBold = mapSupportedWeights[FontFamily::SystemDefault].back();
            QSettings().setValue("fontWeightNormal", weightToArg(fontWeightNormal));
            QSettings().setValue("fontWeightBold", weightToArg(fontWeightBold));
        }
        // Create the dialog
        QDialog dlg(parent);
        dlg.setObjectName("AppearanceSetup");
        dlg.setWindowTitle(QObject::tr("Appearance Setup"));
        dlg.setWindowIcon(QIcon(":icons/bitcoin"));
        // And the widgets we add to it
        QLabel lblHeading(QObject::tr("Please choose your preferred settings for the appearance of %1").arg(QObject::tr(PACKAGE_NAME)), &dlg);
        lblHeading.setObjectName("lblHeading");
        lblHeading.setWordWrap(true);
        QLabel lblSubHeading(QObject::tr("This can also be adjusted later in the \"Appearance\" tab of the preferences."), &dlg);
        lblSubHeading.setObjectName("lblSubHeading");
        lblSubHeading.setWordWrap(true);
        AppearanceWidget appearance(&dlg);
        appearance.setModel(model);
        QFrame line(&dlg);
        line.setFrameShape(QFrame::HLine);
        QDialogButtonBox buttonBox(QDialogButtonBox::Save);
        // Put them into a vbox and add the vbox to the dialog
        QVBoxLayout layout;
        layout.addWidget(&lblHeading);
        layout.addWidget(&lblSubHeading);
        layout.addWidget(&line);
        layout.addWidget(&appearance);
        layout.addWidget(&buttonBox);
        dlg.setLayout(&layout);
        // Adjust the headings
        setFont({&lblHeading}, FontWeight::Bold, 16);
        setFont({&lblSubHeading}, FontWeight::Normal, 14, true);
        // Make sure the dialog closes and accepts the settings if save has been pressed
        QObject::connect(&buttonBox, &QDialogButtonBox::accepted, [&]() {
            QSettings().setValue("fAppearanceSetupDone", true);
            appearance.accept();
            dlg.accept();
        });
        // And fire it!
        dlg.exec();
    }
}

bool parseBitcoinURI(const QUrl &uri, SendCoinsRecipient *out)
{
    // return if URI is not valid or is no axe: URI
    if(!uri.isValid() || uri.scheme() != QString("axe"))
        return false;

    SendCoinsRecipient rv;
    rv.address = uri.path();
    // Trim any following forward slash which may have been added by the OS
    if (rv.address.endsWith("/")) {
        rv.address.truncate(rv.address.length() - 1);
    }
    rv.amount = 0;

#if QT_VERSION < 0x050000
    QList<QPair<QString, QString> > items = uri.queryItems();
#else
    QUrlQuery uriQuery(uri);
    QList<QPair<QString, QString> > items = uriQuery.queryItems();
#endif

    for (QList<QPair<QString, QString> >::iterator i = items.begin(); i != items.end(); i++)
    {
        bool fShouldReturnFalse = false;
        if (i->first.startsWith("req-"))
        {
            i->first.remove(0, 4);
            fShouldReturnFalse = true;
        }

        if (i->first == "label")
        {
            rv.label = i->second;
            fShouldReturnFalse = false;
        }
        if (i->first == "IS")
        {
            // we simply ignore IS
            fShouldReturnFalse = false;
        }
        if (i->first == "message")
        {
            rv.message = i->second;
            fShouldReturnFalse = false;
        }
        else if (i->first == "amount")
        {
            if(!i->second.isEmpty())
            {
                if(!BitcoinUnits::parse(BitcoinUnits::AXE, i->second, &rv.amount))
                {
                    return false;
                }
            }
            fShouldReturnFalse = false;
        }

        if (fShouldReturnFalse)
            return false;
    }
    if(out)
    {
        *out = rv;
    }
    return true;
}

bool parseBitcoinURI(QString uri, SendCoinsRecipient *out)
{
    // Convert axe:// to axe:
    //
    //    Cannot handle this later, because axe:// will cause Qt to see the part after // as host,
    //    which will lower-case it (and thus invalidate the address).
    if(uri.startsWith("axe://", Qt::CaseInsensitive))
    {
        uri.replace(0, 6, "axe:");
    }
    QUrl uriInstance(uri);
    return parseBitcoinURI(uriInstance, out);
}

bool validateBitcoinURI(const QString& uri)
{
    SendCoinsRecipient rcp;
    return parseBitcoinURI(uri, &rcp);
}

QString formatBitcoinURI(const SendCoinsRecipient &info)
{
    QString ret = QString("axe:%1").arg(info.address);
    int paramCount = 0;

    if (info.amount)
    {
        ret += QString("?amount=%1").arg(BitcoinUnits::format(BitcoinUnits::AXE, info.amount, false, BitcoinUnits::separatorNever));
        paramCount++;
    }

    if (!info.label.isEmpty())
    {
        QString lbl(QUrl::toPercentEncoding(info.label));
        ret += QString("%1label=%2").arg(paramCount == 0 ? "?" : "&").arg(lbl);
        paramCount++;
    }

    if (!info.message.isEmpty())
    {
        QString msg(QUrl::toPercentEncoding(info.message));
        ret += QString("%1message=%2").arg(paramCount == 0 ? "?" : "&").arg(msg);
        paramCount++;
    }

    return ret;
}

bool isDust(const QString& address, const CAmount& amount)
{
    CTxDestination dest = DecodeDestination(address.toStdString());
    CScript script = GetScriptForDestination(dest);
    CTxOut txOut(amount, script);
    return IsDust(txOut, ::dustRelayFee);
}

QString HtmlEscape(const QString& str, bool fMultiLine)
{
#if QT_VERSION < 0x050000
    QString escaped = Qt::escape(str);
#else
    QString escaped = str.toHtmlEscaped();
#endif
    escaped = escaped.replace(" ", "&nbsp;");
    if(fMultiLine)
    {
        escaped = escaped.replace("\n", "<br>\n");
    }
    return escaped;
}

QString HtmlEscape(const std::string& str, bool fMultiLine)
{
    return HtmlEscape(QString::fromStdString(str), fMultiLine);
}

void copyEntryData(QAbstractItemView *view, int column, int role)
{
    if(!view || !view->selectionModel())
        return;
    QModelIndexList selection = view->selectionModel()->selectedRows(column);

    if(!selection.isEmpty())
    {
        // Copy first item
        setClipboard(selection.at(0).data(role).toString());
    }
}

QList<QModelIndex> getEntryData(QAbstractItemView *view, int column)
{
    if(!view || !view->selectionModel())
        return QList<QModelIndex>();
    return view->selectionModel()->selectedRows(column);
}

QString getSaveFileName(QWidget *parent, const QString &caption, const QString &dir,
    const QString &filter,
    QString *selectedSuffixOut)
{
    QString selectedFilter;
    QString myDir;
    if(dir.isEmpty()) // Default to user documents location
    {
#if QT_VERSION < 0x050000
        myDir = QDesktopServices::storageLocation(QDesktopServices::DocumentsLocation);
#else
        myDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
#endif
    }
    else
    {
        myDir = dir;
    }
    /* Directly convert path to native OS path separators */
    QString result = QDir::toNativeSeparators(QFileDialog::getSaveFileName(parent, caption, myDir, filter, &selectedFilter));

    /* Extract first suffix from filter pattern "Description (*.foo)" or "Description (*.foo *.bar ...) */
    QRegExp filter_re(".* \\(\\*\\.(.*)[ \\)]");
    QString selectedSuffix;
    if(filter_re.exactMatch(selectedFilter))
    {
        selectedSuffix = filter_re.cap(1);
    }

    /* Add suffix if needed */
    QFileInfo info(result);
    if(!result.isEmpty())
    {
        if(info.suffix().isEmpty() && !selectedSuffix.isEmpty())
        {
            /* No suffix specified, add selected suffix */
            if(!result.endsWith("."))
                result.append(".");
            result.append(selectedSuffix);
        }
    }

    /* Return selected suffix if asked to */
    if(selectedSuffixOut)
    {
        *selectedSuffixOut = selectedSuffix;
    }
    return result;
}

QString getOpenFileName(QWidget *parent, const QString &caption, const QString &dir,
    const QString &filter,
    QString *selectedSuffixOut)
{
    QString selectedFilter;
    QString myDir;
    if(dir.isEmpty()) // Default to user documents location
    {
#if QT_VERSION < 0x050000
        myDir = QDesktopServices::storageLocation(QDesktopServices::DocumentsLocation);
#else
        myDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
#endif
    }
    else
    {
        myDir = dir;
    }
    /* Directly convert path to native OS path separators */
    QString result = QDir::toNativeSeparators(QFileDialog::getOpenFileName(parent, caption, myDir, filter, &selectedFilter));

    if(selectedSuffixOut)
    {
        /* Extract first suffix from filter pattern "Description (*.foo)" or "Description (*.foo *.bar ...) */
        QRegExp filter_re(".* \\(\\*\\.(.*)[ \\)]");
        QString selectedSuffix;
        if(filter_re.exactMatch(selectedFilter))
        {
            selectedSuffix = filter_re.cap(1);
        }
        *selectedSuffixOut = selectedSuffix;
    }
    return result;
}

Qt::ConnectionType blockingGUIThreadConnection()
{
    if(QThread::currentThread() != qApp->thread())
    {
        return Qt::BlockingQueuedConnection;
    }
    else
    {
        return Qt::DirectConnection;
    }
}

bool checkPoint(const QPoint &p, const QWidget *w)
{
    QWidget *atW = QApplication::widgetAt(w->mapToGlobal(p));
    if (!atW) return false;
    return atW->topLevelWidget() == w;
}

bool isObscured(QWidget *w)
{
    return !(checkPoint(QPoint(0, 0), w)
        && checkPoint(QPoint(w->width() - 1, 0), w)
        && checkPoint(QPoint(0, w->height() - 1), w)
        && checkPoint(QPoint(w->width() - 1, w->height() - 1), w)
        && checkPoint(QPoint(w->width() / 2, w->height() / 2), w));
}

void bringToFront(QWidget* w)
{
#ifdef Q_OS_MAC
    ForceActivation();
#endif

    if (w) {
        // activateWindow() (sometimes) helps with keyboard focus on Windows
        if (w->isMinimized()) {
            w->showNormal();
        } else {
            w->show();
        }
        w->activateWindow();
        w->raise();
    }
}

void openDebugLogfile()
{
    fs::path pathDebug = GetDataDir() / "debug.log";

    /* Open debug.log with the associated application */
    if (fs::exists(pathDebug))
        QDesktopServices::openUrl(QUrl::fromLocalFile(boostPathToQString(pathDebug)));
}

void openConfigfile()
{
    fs::path pathConfig = GetConfigFile(gArgs.GetArg("-conf", BITCOIN_CONF_FILENAME));

    /* Open axe.conf with the associated application */
    if (fs::exists(pathConfig))
        QDesktopServices::openUrl(QUrl::fromLocalFile(boostPathToQString(pathConfig)));
}

void showBackups()
{
    fs::path backupsDir = GetBackupsDir();

    /* Open folder with default browser */
    if (fs::exists(backupsDir))
        QDesktopServices::openUrl(QUrl::fromLocalFile(boostPathToQString(backupsDir)));
}

void SubstituteFonts(const QString& language)
{
#if defined(Q_OS_MAC)
// Background:
// OSX's default font changed in 10.9 and Qt is unable to find it with its
// usual fallback methods when building against the 10.7 sdk or lower.
// The 10.8 SDK added a function to let it find the correct fallback font.
// If this fallback is not properly loaded, some characters may fail to
// render correctly.
//
// The same thing happened with 10.10. .Helvetica Neue DeskInterface is now default.
//
// Solution: If building with the 10.7 SDK or lower and the user's platform
// is 10.9 or higher at runtime, substitute the correct font. This needs to
// happen before the QApplication is created.
#if defined(MAC_OS_X_VERSION_MAX_ALLOWED) && MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_8
    if (floor(NSAppKitVersionNumber) > NSAppKitVersionNumber10_8)
    {
        if (floor(NSAppKitVersionNumber) <= NSAppKitVersionNumber10_9)
            /* On a 10.9 - 10.9.x system */
            QFont::insertSubstitution(".Lucida Grande UI", "Lucida Grande");
        else
        {
            /* 10.10 or later system */
            if (language == "zh_CN" || language == "zh_TW" || language == "zh_HK") // traditional or simplified Chinese
              QFont::insertSubstitution(".Helvetica Neue DeskInterface", "Heiti SC");
            else if (language == "ja") // Japanese
              QFont::insertSubstitution(".Helvetica Neue DeskInterface", "Songti SC");
            else
              QFont::insertSubstitution(".Helvetica Neue DeskInterface", "Lucida Grande");
        }
    }
#endif
#endif
}

ToolTipToRichTextFilter::ToolTipToRichTextFilter(int _size_threshold, QObject *parent) :
    QObject(parent),
    size_threshold(_size_threshold)
{

}

bool ToolTipToRichTextFilter::eventFilter(QObject *obj, QEvent *evt)
{
    if(evt->type() == QEvent::ToolTipChange)
    {
        QWidget *widget = static_cast<QWidget*>(obj);
        QString tooltip = widget->toolTip();
        if(tooltip.size() > size_threshold && !tooltip.startsWith("<qt"))
        {
            // Escape the current message as HTML and replace \n by <br> if it's not rich text
            if(!Qt::mightBeRichText(tooltip))
                tooltip = HtmlEscape(tooltip, true);
            // Envelop with <qt></qt> to make sure Qt detects every tooltip as rich text
            // and style='white-space:pre' to preserve line composition
            tooltip = "<qt style='white-space:pre'>" + tooltip + "</qt>";
            widget->setToolTip(tooltip);
            return true;
        }
    }
    return QObject::eventFilter(obj, evt);
}

void TableViewLastColumnResizingFixer::connectViewHeadersSignals()
{
    connect(tableView->horizontalHeader(), SIGNAL(sectionResized(int,int,int)), this, SLOT(on_sectionResized(int,int,int)));
    connect(tableView->horizontalHeader(), SIGNAL(geometriesChanged()), this, SLOT(on_geometriesChanged()));
}

// We need to disconnect these while handling the resize events, otherwise we can enter infinite loops.
void TableViewLastColumnResizingFixer::disconnectViewHeadersSignals()
{
    disconnect(tableView->horizontalHeader(), SIGNAL(sectionResized(int,int,int)), this, SLOT(on_sectionResized(int,int,int)));
    disconnect(tableView->horizontalHeader(), SIGNAL(geometriesChanged()), this, SLOT(on_geometriesChanged()));
}

// Setup the resize mode, handles compatibility for Qt5 and below as the method signatures changed.
// Refactored here for readability.
void TableViewLastColumnResizingFixer::setViewHeaderResizeMode(int logicalIndex, QHeaderView::ResizeMode resizeMode)
{
#if QT_VERSION < 0x050000
    tableView->horizontalHeader()->setResizeMode(logicalIndex, resizeMode);
#else
    tableView->horizontalHeader()->setSectionResizeMode(logicalIndex, resizeMode);
#endif
}

void TableViewLastColumnResizingFixer::resizeColumn(int nColumnIndex, int width)
{
    tableView->setColumnWidth(nColumnIndex, width);
    tableView->horizontalHeader()->resizeSection(nColumnIndex, width);
}

int TableViewLastColumnResizingFixer::getColumnsWidth()
{
    int nColumnsWidthSum = 0;
    for (int i = 0; i < columnCount; i++)
    {
        nColumnsWidthSum += tableView->horizontalHeader()->sectionSize(i);
    }
    return nColumnsWidthSum;
}

int TableViewLastColumnResizingFixer::getAvailableWidthForColumn(int column)
{
    int nResult = lastColumnMinimumWidth;
    int nTableWidth = tableView->horizontalHeader()->width();

    if (nTableWidth > 0)
    {
        int nOtherColsWidth = getColumnsWidth() - tableView->horizontalHeader()->sectionSize(column);
        nResult = std::max(nResult, nTableWidth - nOtherColsWidth);
    }

    return nResult;
}

// Make sure we don't make the columns wider than the table's viewport width.
void TableViewLastColumnResizingFixer::adjustTableColumnsWidth()
{
    disconnectViewHeadersSignals();
    resizeColumn(lastColumnIndex, getAvailableWidthForColumn(lastColumnIndex));
    connectViewHeadersSignals();

    int nTableWidth = tableView->horizontalHeader()->width();
    int nColsWidth = getColumnsWidth();
    if (nColsWidth > nTableWidth)
    {
        resizeColumn(secondToLastColumnIndex,getAvailableWidthForColumn(secondToLastColumnIndex));
    }
}

// Make column use all the space available, useful during window resizing.
void TableViewLastColumnResizingFixer::stretchColumnWidth(int column)
{
    disconnectViewHeadersSignals();
    resizeColumn(column, getAvailableWidthForColumn(column));
    connectViewHeadersSignals();
}

// When a section is resized this is a slot-proxy for ajustAmountColumnWidth().
void TableViewLastColumnResizingFixer::on_sectionResized(int logicalIndex, int oldSize, int newSize)
{
    adjustTableColumnsWidth();
    int remainingWidth = getAvailableWidthForColumn(logicalIndex);
    if (newSize > remainingWidth)
    {
       resizeColumn(logicalIndex, remainingWidth);
    }
}

// When the table's geometry is ready, we manually perform the stretch of the "Message" column,
// as the "Stretch" resize mode does not allow for interactive resizing.
void TableViewLastColumnResizingFixer::on_geometriesChanged()
{
    if ((getColumnsWidth() - this->tableView->horizontalHeader()->width()) != 0)
    {
        disconnectViewHeadersSignals();
        resizeColumn(secondToLastColumnIndex, getAvailableWidthForColumn(secondToLastColumnIndex));
        connectViewHeadersSignals();
    }
}

/**
 * Initializes all internal variables and prepares the
 * the resize modes of the last 2 columns of the table and
 */
TableViewLastColumnResizingFixer::TableViewLastColumnResizingFixer(QTableView* table, int lastColMinimumWidth, int allColsMinimumWidth, QObject *parent) :
    QObject(parent),
    tableView(table),
    lastColumnMinimumWidth(lastColMinimumWidth),
    allColumnsMinimumWidth(allColsMinimumWidth)
{
    columnCount = tableView->horizontalHeader()->count();
    lastColumnIndex = columnCount - 1;
    secondToLastColumnIndex = columnCount - 2;
    tableView->horizontalHeader()->setMinimumSectionSize(allColumnsMinimumWidth);
    setViewHeaderResizeMode(secondToLastColumnIndex, QHeaderView::Interactive);
    setViewHeaderResizeMode(lastColumnIndex, QHeaderView::Interactive);
}

#ifdef WIN32
fs::path static StartupShortcutPath()
{
    std::string chain = gArgs.GetChainName();
    if (chain == CBaseChainParams::MAIN)
        return GetSpecialFolderPath(CSIDL_STARTUP) / "Axe Core.lnk";
    if (chain == CBaseChainParams::TESTNET) // Remove this special case when CBaseChainParams::TESTNET = "testnet4"
        return GetSpecialFolderPath(CSIDL_STARTUP) / "Axe Core (testnet).lnk";
    return GetSpecialFolderPath(CSIDL_STARTUP) / strprintf("Axe Core (%s).lnk", chain);
}

bool GetStartOnSystemStartup()
{
    // check for "Axe Core*.lnk"
    return fs::exists(StartupShortcutPath());
}

bool SetStartOnSystemStartup(bool fAutoStart)
{
    // If the shortcut exists already, remove it for updating
    fs::remove(StartupShortcutPath());

    if (fAutoStart)
    {
        CoInitialize(nullptr);

        // Get a pointer to the IShellLink interface.
        IShellLink* psl = nullptr;
        HRESULT hres = CoCreateInstance(CLSID_ShellLink, nullptr,
            CLSCTX_INPROC_SERVER, IID_IShellLink,
            reinterpret_cast<void**>(&psl));

        if (SUCCEEDED(hres))
        {
            // Get the current executable path
            TCHAR pszExePath[MAX_PATH];
            GetModuleFileName(nullptr, pszExePath, sizeof(pszExePath));

            // Start client minimized
            QString strArgs = "-min";
            // Set -testnet /-regtest options
            strArgs += QString::fromStdString(strprintf(" -testnet=%d -regtest=%d", gArgs.GetBoolArg("-testnet", false), gArgs.GetBoolArg("-regtest", false)));

#ifdef UNICODE
            boost::scoped_array<TCHAR> args(new TCHAR[strArgs.length() + 1]);
            // Convert the QString to TCHAR*
            strArgs.toWCharArray(args.get());
            // Add missing '\0'-termination to string
            args[strArgs.length()] = '\0';
#endif

            // Set the path to the shortcut target
            psl->SetPath(pszExePath);
            PathRemoveFileSpec(pszExePath);
            psl->SetWorkingDirectory(pszExePath);
            psl->SetShowCmd(SW_SHOWMINNOACTIVE);
#ifndef UNICODE
            psl->SetArguments(strArgs.toStdString().c_str());
#else
            psl->SetArguments(args.get());
#endif

            // Query IShellLink for the IPersistFile interface for
            // saving the shortcut in persistent storage.
            IPersistFile* ppf = nullptr;
            hres = psl->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&ppf));
            if (SUCCEEDED(hres))
            {
                WCHAR pwsz[MAX_PATH];
                // Ensure that the string is ANSI.
                MultiByteToWideChar(CP_ACP, 0, StartupShortcutPath().string().c_str(), -1, pwsz, MAX_PATH);
                // Save the link by calling IPersistFile::Save.
                hres = ppf->Save(pwsz, TRUE);
                ppf->Release();
                psl->Release();
                CoUninitialize();
                return true;
            }
            psl->Release();
        }
        CoUninitialize();
        return false;
    }
    return true;
}
#elif defined(Q_OS_LINUX)

// Follow the Desktop Application Autostart Spec:
// http://standards.freedesktop.org/autostart-spec/autostart-spec-latest.html

fs::path static GetAutostartDir()
{
    char* pszConfigHome = getenv("XDG_CONFIG_HOME");
    if (pszConfigHome) return fs::path(pszConfigHome) / "autostart";
    char* pszHome = getenv("HOME");
    if (pszHome) return fs::path(pszHome) / ".config" / "autostart";
    return fs::path();
}

fs::path static GetAutostartFilePath()
{
    std::string chain = gArgs.GetChainName();
    if (chain == CBaseChainParams::MAIN)
        return GetAutostartDir() / "axecore.desktop";
    return GetAutostartDir() / strprintf("axecore-%s.lnk", chain);
}

bool GetStartOnSystemStartup()
{
    fs::ifstream optionFile(GetAutostartFilePath());
    if (!optionFile.good())
        return false;
    // Scan through file for "Hidden=true":
    std::string line;
    while (!optionFile.eof())
    {
        getline(optionFile, line);
        if (line.find("Hidden") != std::string::npos &&
            line.find("true") != std::string::npos)
            return false;
    }
    optionFile.close();

    return true;
}

bool SetStartOnSystemStartup(bool fAutoStart)
{
    if (!fAutoStart)
        fs::remove(GetAutostartFilePath());
    else
    {
        char pszExePath[MAX_PATH+1];
        ssize_t r = readlink("/proc/self/exe", pszExePath, sizeof(pszExePath) - 1);
        if (r == -1)
            return false;
        pszExePath[r] = '\0';

        fs::create_directories(GetAutostartDir());

        fs::ofstream optionFile(GetAutostartFilePath(), std::ios_base::out|std::ios_base::trunc);
        if (!optionFile.good())
            return false;
        std::string chain = gArgs.GetChainName();
        // Write a axecore.desktop file to the autostart directory:
        optionFile << "[Desktop Entry]\n";
        optionFile << "Type=Application\n";
        if (chain == CBaseChainParams::MAIN)
            optionFile << "Name=Axe Core\n";
        else
            optionFile << strprintf("Name=Axe Core (%s)\n", chain);
        optionFile << "Exec=" << pszExePath << strprintf(" -min -testnet=%d -regtest=%d\n", gArgs.GetBoolArg("-testnet", false), gArgs.GetBoolArg("-regtest", false));
        optionFile << "Terminal=false\n";
        optionFile << "Hidden=false\n";
        optionFile.close();
    }
    return true;
}


#elif defined(Q_OS_MAC)
// based on: https://github.com/Mozketo/LaunchAtLoginController/blob/master/LaunchAtLoginController.m

LSSharedFileListItemRef findStartupItemInList(LSSharedFileListRef list, CFURLRef findUrl);
LSSharedFileListItemRef findStartupItemInList(LSSharedFileListRef list, CFURLRef findUrl)
{
    CFArrayRef listSnapshot = LSSharedFileListCopySnapshot(list, nullptr);
    if (listSnapshot == nullptr) {
        return nullptr;
    }

    // loop through the list of startup items and try to find the Axe Core app
    for(int i = 0; i < CFArrayGetCount(listSnapshot); i++) {
        LSSharedFileListItemRef item = (LSSharedFileListItemRef)CFArrayGetValueAtIndex(listSnapshot, i);
        UInt32 resolutionFlags = kLSSharedFileListNoUserInteraction | kLSSharedFileListDoNotMountVolumes;
        CFURLRef currentItemURL = nullptr;

#if defined(MAC_OS_X_VERSION_MAX_ALLOWED) && MAC_OS_X_VERSION_MAX_ALLOWED >= 10100
        if(&LSSharedFileListItemCopyResolvedURL)
            currentItemURL = LSSharedFileListItemCopyResolvedURL(item, resolutionFlags, nullptr);
#if defined(MAC_OS_X_VERSION_MIN_REQUIRED) && MAC_OS_X_VERSION_MIN_REQUIRED < 10100
        else
            LSSharedFileListItemResolve(item, resolutionFlags, &currentItemURL, nullptr);
#endif
#else
        LSSharedFileListItemResolve(item, resolutionFlags, &currentItemURL, nullptr);
#endif

        if(currentItemURL) {
            if (CFEqual(currentItemURL, findUrl)) {
                // found
                CFRelease(listSnapshot);
                CFRelease(currentItemURL);
                return item;
            }
            CFRelease(currentItemURL);
        }
    }

    CFRelease(listSnapshot);
    return nullptr;
}

bool GetStartOnSystemStartup()
{
    CFURLRef bitcoinAppUrl = CFBundleCopyBundleURL(CFBundleGetMainBundle());
    if (bitcoinAppUrl == nullptr) {
        return false;
    }

    LSSharedFileListRef loginItems = LSSharedFileListCreate(nullptr, kLSSharedFileListSessionLoginItems, nullptr);
    LSSharedFileListItemRef foundItem = findStartupItemInList(loginItems, bitcoinAppUrl);

    CFRelease(bitcoinAppUrl);
    return !!foundItem; // return boolified object
}

bool SetStartOnSystemStartup(bool fAutoStart)
{
    CFURLRef bitcoinAppUrl = CFBundleCopyBundleURL(CFBundleGetMainBundle());
    if (bitcoinAppUrl == nullptr) {
        return false;
    }

    LSSharedFileListRef loginItems = LSSharedFileListCreate(nullptr, kLSSharedFileListSessionLoginItems, nullptr);
    LSSharedFileListItemRef foundItem = findStartupItemInList(loginItems, bitcoinAppUrl);

    if(fAutoStart && !foundItem) {
        // add Axe Core app to startup item list
        LSSharedFileListInsertItemURL(loginItems, kLSSharedFileListItemBeforeFirst, nullptr, nullptr, bitcoinAppUrl, nullptr, nullptr);
    }
    else if(!fAutoStart && foundItem) {
        // remove item
        LSSharedFileListItemRemove(loginItems, foundItem);
    }

    CFRelease(bitcoinAppUrl);
    return true;
}
#pragma GCC diagnostic pop
#else

bool GetStartOnSystemStartup() { return false; }
bool SetStartOnSystemStartup(bool fAutoStart) { return false; }

#endif

void migrateQtSettings()
{
    // Migration (12.1)
    QSettings settings;
    if(!settings.value("fMigrationDone121", false).toBool()) {
        settings.remove("theme");
        settings.remove("nWindowPos");
        settings.remove("nWindowSize");
        settings.setValue("fMigrationDone121", true);
    }
}

void setStyleSheetDirectory(const QString& path)
{
    stylesheetDirectory = path;
}

bool isStyleSheetDirectoryCustom()
{
    return stylesheetDirectory != defaultStylesheetDirectory;
}

const std::vector<QString> listStyleSheets()
{
    std::vector<QString> vecStylesheets;
    for (const auto& it : mapStyleToTheme) {
        vecStylesheets.push_back(it.first);
    }
    return vecStylesheets;
}

const std::vector<QString> listThemes()
{
    std::vector<QString> vecThemes;
    for (const auto& it : mapStyleToTheme) {
        if (!it.second.isEmpty()) {
            vecThemes.push_back(it.second);
        }
    }
    return vecThemes;
}

const QString getDefaultTheme()
{
    return defaultTheme;
}

void loadStyleSheet(QWidget* widget, bool fForceUpdate)
{
    AssertLockNotHeld(cs_css);
    LOCK(cs_css);

    static std::unique_ptr<QString> stylesheet;
    static std::set<QWidget*> setWidgets;

    bool fDebugCustomStyleSheets = gArgs.GetBoolArg("-debug-ui", false) && isStyleSheetDirectoryCustom();
    bool fStyleSheetChanged = false;

    if (stylesheet == nullptr || fForceUpdate || fDebugCustomStyleSheets) {
        auto hasModified = [](const std::vector<QString>& vecFiles) -> bool {
            static std::map<const QString, QDateTime> mapLastModified;

            bool fModified = false;
            for (auto file = vecFiles.begin(); file != vecFiles.end() && !fModified; ++file) {
                QFileInfo info(*file);
                QDateTime lastModified = info.lastModified(), prevLastModified;
                auto it = mapLastModified.emplace(std::make_pair(*file, lastModified));
                prevLastModified = it.second ? QDateTime() : it.first->second;
                it.first->second = lastModified;
                fModified = prevLastModified != lastModified;
            }
            return fModified;
        };

        auto loadFiles = [&](const std::vector<QString>& vecFiles) -> bool {
            if (!fForceUpdate && fDebugCustomStyleSheets && !hasModified(vecFiles)) {
                return false;
            }

            std::string platformName = gArgs.GetArg("-uiplatform", BitcoinGUI::DEFAULT_UIPLATFORM);
            stylesheet = std::make_unique<QString>();

            for (const auto& file : vecFiles) {
                QFile qFile(file);
                if (!qFile.open(QFile::ReadOnly)) {
                    throw std::runtime_error(strprintf("%s: Failed to open file: %s", __func__, file.toStdString()));
                }

                QString strStyle = QLatin1String(qFile.readAll());
                // Process all <os=...></os> groups in the stylesheet first
                QRegularExpressionMatch osStyleMatch;
                QRegularExpression osStyleExp(
                        "^"
                        "(<os=(?:'|\").+(?:'|\")>)" // group 1
                        "((?:.|\n)+?)"              // group 2
                        "(</os>?)"                  // group 3
                        "$");
                osStyleExp.setPatternOptions(QRegularExpression::MultilineOption);
                QRegularExpressionMatchIterator it = osStyleExp.globalMatch(strStyle);

                // For all <os=...></os> sections
                while (it.hasNext() && (osStyleMatch = it.next()).isValid()) {
                    QStringList listMatches = osStyleMatch.capturedTexts();

                    // Full match + 3 group matches
                    if (listMatches.size() % 4) {
                        throw std::runtime_error(strprintf("%s: Invalid <os=...></os> section in file %s", __func__, file.toStdString()));
                    }

                    for (int i = 0; i < listMatches.size(); i += 4) {
                        if (!listMatches[i + 1].contains(QString::fromStdString(platformName))) {
                            // If os is not supported for this styles
                            // just remove the full match
                            strStyle.replace(listMatches[i], "");
                        } else {
                            // If its supported remove the <os=...></os> tags
                            strStyle.replace(listMatches[i + 1], "");
                            strStyle.replace(listMatches[i + 3], "");
                        }
                    }
                }
                stylesheet->append(strStyle);
            }
            return true;
        };

        auto pathToFile = [&](const QString& file) -> QString {
            return stylesheetDirectory + "/" + file + (isStyleSheetDirectoryCustom() ? ".css" : "");
        };

        std::vector<QString> vecFiles;
        // If light/dark theme is used load general styles first
        if (axeThemeActive()) {
            vecFiles.push_back(pathToFile("general"));
        }
        vecFiles.push_back(pathToFile(getActiveTheme()));

        fStyleSheetChanged = loadFiles(vecFiles);
    }

    bool fUpdateStyleSheet = fForceUpdate || (fDebugCustomStyleSheets && fStyleSheetChanged);

    if (widget) {
        setWidgets.insert(widget);
        widget->setStyleSheet(*stylesheet);
    }

    QWidgetList allWidgets = QApplication::allWidgets();
    auto it = setWidgets.begin();
    while (it != setWidgets.end()) {
        if (!allWidgets.contains(*it)) {
            it = setWidgets.erase(it);
            continue;
        }
        if (fUpdateStyleSheet && *it != widget) {
            (*it)->setStyleSheet(*stylesheet);
        }
        ++it;
    }

    if (!ShutdownRequested() && fDebugCustomStyleSheets && !fForceUpdate) {
        QTimer::singleShot(200, [] { loadStyleSheet(); });
    }
}

FontFamily fontFamilyFromString(const QString& strFamily)
{
    if (strFamily == "SystemDefault") {
        return FontFamily::SystemDefault;
    }
    if (strFamily == "Montserrat") {
        return FontFamily::Montserrat;
    }
    throw std::invalid_argument(strprintf("Invalid font-family: %s", strFamily.toStdString()));
}

QString fontFamilyToString(FontFamily family)
{
    switch (family) {
    case FontFamily::SystemDefault:
        return "SystemDefault";
    case FontFamily::Montserrat:
        return "Montserrat";
    default:
        assert(false);
    }
}

void setFontFamily(FontFamily family)
{
    fontFamily = family;
    setApplicationFont();
    updateFonts();
}

FontFamily getFontFamilyDefault()
{
    return defaultFontFamily;
}

FontFamily getFontFamily()
{
    return fontFamily;
}

bool weightFromArg(int nArg, QFont::Weight& weight)
{
    const std::map<int, QFont::Weight> mapWeight{
        {0, QFont::Thin},
        {1, QFont::ExtraLight},
        {2, QFont::Light},
        {3, QFont::Normal},
        {4, QFont::Medium},
        {5, QFont::DemiBold},
        {6, QFont::Bold},
        {7, QFont::ExtraBold},
        {8, QFont::Black}
    };
    auto it = mapWeight.find(nArg);
    if (it == mapWeight.end()) {
        return false;
    }
    weight = it->second;
    return true;
}

int weightToArg(const QFont::Weight weight)
{
    const std::map<QFont::Weight, int> mapWeight{
        {QFont::Thin, 0},
        {QFont::ExtraLight, 1},
        {QFont::Light, 2},
        {QFont::Normal, 3},
        {QFont::Medium, 4},
        {QFont::DemiBold, 5},
        {QFont::Bold, 6},
        {QFont::ExtraBold, 7},
        {QFont::Black, 8}
    };
    assert(mapWeight.count(weight));
    return mapWeight.find(weight)->second;
}

QFont::Weight getFontWeightNormalDefault()
{
    return defaultFontWeightNormal;
}

QFont::Weight toQFontWeight(FontWeight weight)
{
    return weight == FontWeight::Bold ? getFontWeightBold() : getFontWeightNormal();
}

QFont::Weight getFontWeightNormal()
{
    return fontWeightNormal;
}

void setFontWeightNormal(QFont::Weight weight)
{
    fontWeightNormal = weight;
    updateFonts();
}

QFont::Weight getFontWeightBoldDefault()
{
    return defaultFontWeightBold;
}

QFont::Weight getFontWeightBold()
{
    return fontWeightBold;
}

void setFontWeightBold(QFont::Weight weight)
{
    fontWeightBold = weight;
    updateFonts();
}

int getFontScaleDefault()
{
    return defaultFontScale;
}

int getFontScale()
{
    return fontScale;
}

void setFontScale(int nScale)
{
    fontScale = nScale;
    updateFonts();
}

double getScaledFontSize(int nSize)
{
    return std::round(nSize * (1 + (fontScale * fontScaleSteps)) * 4) / 4.0;
}

bool loadFonts()
{
    // Before any font changes store the applications default font to use it as SystemDefault.
    osDefaultFont = std::make_unique<QFont>(QApplication::font());

    QString family = fontFamilyToString(FontFamily::Montserrat);
    QString italic = "Italic";

    std::map<QString, bool> mapStyles{
        {"Thin", true},
        {"ExtraLight", true},
        {"Light", true},
        {"Italic", false},
        {"Regular", false},
        {"Medium", true},
        {"SemiBold", true},
        {"Bold", true},
        {"ExtraBold", true},
        {"Black", true},
    };

    QFontDatabase database;
    std::vector<int> vecFontIds;

    for (const auto& it : mapStyles) {
        QString font = ":fonts/" + family + "-" + it.first;
        vecFontIds.push_back(QFontDatabase::addApplicationFont(font));
        qDebug() << __func__ << ": " << font << " loaded with id " << vecFontIds.back();
        if (it.second) {
            vecFontIds.push_back(QFontDatabase::addApplicationFont(font + italic));
            qDebug() << __func__ << ": " << font + italic << " loaded with id " << vecFontIds.back();
        }
    }

    // Fail if an added id is -1 which means QFontDatabase::addApplicationFont failed.
    if (std::find(vecFontIds.begin(), vecFontIds.end(), -1) != vecFontIds.end()) {
        return false;
    }

    // Print debug logs for added fonts fetched by the added ids
    for (const auto& i : vecFontIds) {
        auto families = QFontDatabase::applicationFontFamilies(i);
        for (const QString& f : families) {
            qDebug() << __func__ << ": - Font id " << i << " is family: " << f;
            const QStringList fontStyles = database.styles(f);
            for (const QString& style : fontStyles) {
                qDebug() << __func__ << ": Style for family " << f << " with id: " << i << ": " << style;
            }
        }
    }
    // Print debug logs for added fonts fetched by the family name
    const QStringList fontFamilies = database.families();
    for (const QString& f : fontFamilies) {
        if (f.contains(family)) {
            const QStringList fontStyles = database.styles(f);
            for (const QString& style : fontStyles) {
                qDebug() << __func__ << ": Family: " << f << ", Style: " << style;
            }
        }
    }

    // Load font related settings
    QSettings settings;
    QFont::Weight weight;

    if (!gArgs.IsArgSet("-font-family")) {
        fontFamily = fontFamilyFromString(settings.value("fontFamily").toString());
    }

    if (!gArgs.IsArgSet("-font-scale")) {
        fontScale = settings.value("fontScale").toInt();
    }

    if (!gArgs.IsArgSet("-font-weight-normal") && weightFromArg(settings.value("fontWeightNormal").toInt(), weight)) {
        fontWeightNormal = weight;
    }

    if (!gArgs.IsArgSet("-font-weight-bold") && weightFromArg(settings.value("fontWeightBold").toInt(), weight)) {
        fontWeightBold = weight;
    }

    setApplicationFont();

    // Initialize supported font weights for all available fonts
    // Generate a vector with supported font weights by comparing the width of a certain test text for all font weights
    auto supportedWeights = [](FontFamily family) -> std::vector<QFont::Weight> {
        auto getTestWidth = [&](QFont::Weight weight) -> int {
            QFont font = getFont(family, weight, false, defaultFontSize);
            return QFontMetrics(font).width("Check the width of this text to see if the weight change has an impact!");
        };
        std::vector<QFont::Weight> vecWeights{QFont::Thin, QFont::ExtraLight, QFont::Light,
                                              QFont::Normal, QFont::Medium, QFont::DemiBold,
                                              QFont::Bold, QFont::Black};
        std::vector<QFont::Weight> vecSupported;
        QFont::Weight prevWeight = vecWeights.front();
        for (auto weight = vecWeights.begin() + 1; weight != vecWeights.end(); ++weight) {
            if (getTestWidth(prevWeight) != getTestWidth(*weight)) {
                if (vecSupported.empty()) {
                    vecSupported.push_back(prevWeight);
                }
                vecSupported.push_back(*weight);
            }
            prevWeight = *weight;
        }
        return vecSupported;
    };

    mapSupportedWeights.insert(std::make_pair(FontFamily::SystemDefault, supportedWeights(FontFamily::SystemDefault)));
    mapSupportedWeights.insert(std::make_pair(FontFamily::Montserrat, supportedWeights(FontFamily::Montserrat)));

    return true;
}

void setApplicationFont()
{
    std::unique_ptr<QFont> font;

    if (fontFamily == FontFamily::Montserrat) {
        QString family = fontFamilyToString(FontFamily::Montserrat);
#ifdef Q_OS_MAC
        if (getFontWeightNormal() != getFontWeightNormalDefault()) {
            font = std::make_unique<QFont>(getFontNormal());
        } else {
            font = std::make_unique<QFont>(family);
            font->setWeight(getFontWeightNormalDefault());
        }
#else
        font = std::make_unique<QFont>(family);
        font->setWeight(getFontWeightNormal());
#endif
    } else {
        font = std::make_unique<QFont>(*osDefaultFont);
    }

    font->setPointSizeF(defaultFontSize);
    qApp->setFont(*font);

    qDebug() << __func__ << ": " << qApp->font().toString() <<
                " family: " << qApp->font().family() <<
                ", style: " << qApp->font().styleName() <<
                " match: " << qApp->font().exactMatch();
}

void setFont(const std::vector<QWidget*>& vecWidgets, FontWeight weight, int nPointSize, bool fItalic)
{
    for (auto it : vecWidgets) {
        auto fontAttributes = std::make_tuple(weight, fItalic, nPointSize);
        auto itFontUpdate = mapNormalFontUpdates.emplace(std::make_pair(it, fontAttributes));
        if (!itFontUpdate.second) {
            itFontUpdate.first->second = fontAttributes;
        }
    }
}

void updateFonts()
{
    // Fonts need to be loaded by GUIIUtil::loadFonts(), if not just return.
    if (!osDefaultFont) {
        return;
    }

    static std::map<QWidget*, int> mapWidgetDefaultFontSizes;
    static std::map<QString, int> mapClassDefaultFontSizes;
    std::map<QWidget*, std::pair<QFont, bool>> mapWidgetFonts;

    for (QWidget* w : qApp->allWidgets()) {
        std::vector<QString> vecIgnore{
            "QWidget", "QDialog", "QFrame", "QStackedWidget", "QDesktopWidget", "QDesktopScreenWidget",
            "QTipLabel", "QMessageBox", "QMenu", "QComboBoxPrivateScroller", "QComboBoxPrivateContainer",
            "QScrollBar", "QListView", "BitcoinGUI", "WalletView", "WalletFrame"
        };
        if (std::find(vecIgnore.begin(), vecIgnore.end(), w->metaObject()->className()) != vecIgnore.end()) {
            continue;
        }
        QFont font = w->font();
        font.setFamily(qApp->font().family());
        font.setWeight(getFontWeightNormal());
        font.setStyleName(qApp->font().styleName());
        font.setStyle(qApp->font().style());
        // Set the font size based on the widgets default font size + the font scale
        bool fAdded = false;
        if (!mapWidgetDefaultFontSizes.count(w)) {
            mapWidgetDefaultFontSizes.emplace(std::make_pair(w, font.pointSize() > 0 ? font.pointSize() : defaultFontSize));
            fAdded = true;
        }
        font.setPointSizeF(getScaledFontSize(mapWidgetDefaultFontSizes[w]));
        bool fUpdateRequired = fAdded || (mapNormalFontUpdates.find(w) == mapNormalFontUpdates.end() && font != w->font());
        mapWidgetFonts.emplace(w, std::make_pair(font, fUpdateRequired));
    }

    auto itn = mapNormalFontUpdates.begin();
    while (itn != mapNormalFontUpdates.end()) {
        auto itw = mapWidgetFonts.find(itn->first);
        if (itw != mapWidgetFonts.end()) {
            int nSize = std::get<2>(itn->second);
            if (nSize == -1) {
                nSize = mapWidgetDefaultFontSizes[itn->first];
            }
            QFont&& font = getFont(std::get<0>(itn->second), std::get<1>(itn->second), nSize);
            if (itn->first->font() != font) {
                itw->second.first = font;
                itw->second.second = true;
            }
            ++itn;
        } else {
            itn = mapNormalFontUpdates.erase(itn);
        }
    }

    for (auto it : mapWidgetFonts) {
        if (it.second.second) {
            it.first->setFont(it.second.first);
        }
    }

    // Cleanup mapDefaultFontSize to remove deleted widgets
    auto itd = mapWidgetDefaultFontSizes.begin();
    while (itd != mapWidgetDefaultFontSizes.end()) {
        if (qApp->allWidgets().contains(itd->first)) {
            ++itd;
        } else {
            itd = mapWidgetDefaultFontSizes.erase(itd);
        }
    }

    // Scale the global font for QToolTip labels, QMenu and QMessageBox instances
    QFont fontToolTip = qApp->font("QTipLabel");
    QFont fontMenu = qApp->font("QMenu");
    QFont fontMessageBox = qApp->font("QMessageBox");
    // Store their default font sizes before ever applying any scale to it
    if (!mapClassDefaultFontSizes.count("QTipLabel")) {
        mapClassDefaultFontSizes.emplace("QTipLabel", fontToolTip.pointSize());
    }
    if (!mapClassDefaultFontSizes.count("QMenu")) {
        mapClassDefaultFontSizes.emplace("QMenu", fontMenu.pointSize());
    }
    if (!mapClassDefaultFontSizes.count("QMessageBox")) {
        mapClassDefaultFontSizes.emplace("QMessageBox", fontMessageBox.pointSize());
    }
    // And give them the proper scaled size based on their default sizes if required
    double dSize = getScaledFontSize(mapClassDefaultFontSizes["QTipLabel"]);
    if (fontToolTip.pointSizeF() != dSize) {
        fontToolTip.setPointSizeF(dSize);
        qApp->setFont(fontToolTip, "QTipLabel");
    }
    dSize = getScaledFontSize(mapClassDefaultFontSizes["QMenu"]);
    if (fontMenu.pointSizeF() != dSize) {
        fontMenu.setPointSizeF(dSize);
        qApp->setFont(fontMenu, "QMenu");
    }
    dSize = getScaledFontSize(getScaledFontSize(mapClassDefaultFontSizes["QMessageBox"]));
    if (fontMessageBox.pointSizeF() != dSize) {
        fontMessageBox.setPointSizeF(dSize);
        qApp->setFont(fontMessageBox, "QMessageBox");
    }
}

QFont getFont(FontFamily family, QFont::Weight qWeight, bool fItalic, int nPointSize)
{
    QFont font;

    if (family == FontFamily::Montserrat) {
        static std::map<QFont::Weight, QString> mapMontserratMapping{
            {QFont::Thin, "Thin"},
            {QFont::ExtraLight, "ExtraLight"},
            {QFont::Light, "Light"},
            {QFont::Medium, "Medium"},
            {QFont::DemiBold, "SemiBold"},
            {QFont::ExtraBold, "ExtraBold"},
            {QFont::Black, "Black"},
#ifdef Q_OS_MAC
            {QFont::Normal, "Regular"},
            {QFont::Bold, "Bold"},
#else
            {QFont::Normal, ""},
            {QFont::Bold, ""},
#endif
        };

        assert(mapMontserratMapping.count(qWeight));

#ifdef Q_OS_MAC

        QString styleName = mapMontserratMapping[qWeight];

        if (fItalic) {
            if (styleName == "Regular") {
                styleName = "Italic";
            } else {
                styleName += " Italic";
            }
        }

        font.setFamily(fontFamilyToString(FontFamily::Montserrat));
        font.setStyleName(styleName);
#else
        font.setFamily(fontFamilyToString(FontFamily::Montserrat) + " " + mapMontserratMapping[qWeight]);
        font.setWeight(qWeight);
        font.setStyle(fItalic ? QFont::StyleItalic : QFont::StyleNormal);
#endif
    } else {
        font.setFamily(osDefaultFont->family());
        font.setWeight(qWeight);
        font.setStyle(fItalic ? QFont::StyleItalic : QFont::StyleNormal);
    }

    if (nPointSize != -1) {
        font.setPointSizeF(getScaledFontSize(nPointSize));
    }

    if (gArgs.GetBoolArg("-debug-ui", false)) {
        qDebug() << __func__ << ": font size: " << font.pointSizeF() << " family: " << font.family() << ", style: " << font.styleName() << ", weight:" << font.weight() << " match: " << font.exactMatch();
    }

    return font;
}

QFont getFont(QFont::Weight qWeight, bool fItalic, int nPointSize)
{
    return getFont(fontFamily, qWeight, fItalic, nPointSize);
}
QFont getFont(FontWeight weight, bool fItalic, int nPointSize)
{
    return getFont(toQFontWeight(weight), fItalic, nPointSize);
}

QFont getFontNormal()
{
    return getFont(FontWeight::Normal);
}

QFont getFontBold()
{
    return getFont(FontWeight::Bold);
}

std::vector<QFont::Weight> getSupportedWeights()
{
    assert(mapSupportedWeights.count(fontFamily));
    return mapSupportedWeights[fontFamily];
}

QFont::Weight supportedWeightFromIndex(int nIndex)
{
    auto vecWeights = getSupportedWeights();
    assert(vecWeights.size() > nIndex);
    return vecWeights[nIndex];
}

int supportedWeightToIndex(QFont::Weight weight)
{
    auto vecWeights = getSupportedWeights();
    for (int index = 0; index < vecWeights.size(); ++index) {
        if (weight == vecWeights[index]) {
            return index;
        }
    }
    assert(false);
}

QString getActiveTheme()
{
    QSettings settings;
    return settings.value("theme", defaultTheme).toString();
}

bool axeThemeActive()
{
    QSettings settings;
    QString theme = settings.value("theme", "").toString();
    return theme != traditionalTheme;
}

void loadTheme(QWidget* widget, bool fForce)
{
    loadStyleSheet(widget, fForce);
    updateFonts();
    updateMacFocusRects();
}

void disableMacFocusRect(const QWidget* w)
{
#ifdef Q_OS_MAC
    for (const auto& c : w->findChildren<QWidget*>()) {
        if (c->testAttribute(Qt::WA_MacShowFocusRect)) {
            c->setAttribute(Qt::WA_MacShowFocusRect, !axeThemeActive());
            setRectsDisabled.emplace(c);
        }
    }
#endif
}

void updateMacFocusRects()
{
#ifdef Q_OS_MAC
    QWidgetList allWidgets = QApplication::allWidgets();
    auto it = setRectsDisabled.begin();
    while (it != setRectsDisabled.end()) {
        if (allWidgets.contains(*it)) {
            (*it)->setAttribute(Qt::WA_MacShowFocusRect, !axeThemeActive());
            ++it;
        } else {
            it = setRectsDisabled.erase(it);
        }
    }
#endif
}

void setClipboard(const QString& str)
{
    QApplication::clipboard()->setText(str, QClipboard::Clipboard);
    QApplication::clipboard()->setText(str, QClipboard::Selection);
}

fs::path qstringToBoostPath(const QString &path)
{
    return fs::path(path.toStdString(), utf8);
}

QString boostPathToQString(const fs::path &path)
{
    return QString::fromStdString(path.string(utf8));
}

QString formatDurationStr(int secs)
{
    QStringList strList;
    int days = secs / 86400;
    int hours = (secs % 86400) / 3600;
    int mins = (secs % 3600) / 60;
    int seconds = secs % 60;

    if (days)
        strList.append(QString(QObject::tr("%1 d")).arg(days));
    if (hours)
        strList.append(QString(QObject::tr("%1 h")).arg(hours));
    if (mins)
        strList.append(QString(QObject::tr("%1 m")).arg(mins));
    if (seconds || (!days && !hours && !mins))
        strList.append(QString(QObject::tr("%1 s")).arg(seconds));

    return strList.join(" ");
}

QString formatServicesStr(quint64 mask)
{
    QStringList strList;

    // Just scan the last 8 bits for now.
    for (int i = 0; i < 8; i++) {
        uint64_t check = 1 << i;
        if (mask & check)
        {
            switch (check)
            {
            case NODE_NETWORK:
                strList.append("NETWORK");
                break;
            case NODE_GETUTXO:
                strList.append("GETUTXO");
                break;
            case NODE_BLOOM:
                strList.append("BLOOM");
                break;
            case NODE_XTHIN:
                strList.append("XTHIN");
                break;
            default:
                strList.append(QString("%1[%2]").arg("UNKNOWN").arg(check));
            }
        }
    }

    if (strList.size())
        return strList.join(" & ");
    else
        return QObject::tr("None");
}

QString formatPingTime(double dPingTime)
{
    return (dPingTime == std::numeric_limits<int64_t>::max()/1e6 || dPingTime == 0) ? QObject::tr("N/A") : QString(QObject::tr("%1 ms")).arg(QString::number((int)(dPingTime * 1000), 10));
}

QString formatTimeOffset(int64_t nTimeOffset)
{
  return QString(QObject::tr("%1 s")).arg(QString::number((int)nTimeOffset, 10));
}

QString formatNiceTimeOffset(qint64 secs)
{
    // Represent time from last generated block in human readable text
    QString timeBehindText;
    const int HOUR_IN_SECONDS = 60*60;
    const int DAY_IN_SECONDS = 24*60*60;
    const int WEEK_IN_SECONDS = 7*24*60*60;
    const int YEAR_IN_SECONDS = 31556952; // Average length of year in Gregorian calendar
    if(secs < 60)
    {
        timeBehindText = QObject::tr("%n second(s)","",secs);
    }
    else if(secs < 2*HOUR_IN_SECONDS)
    {
        timeBehindText = QObject::tr("%n minute(s)","",secs/60);
    }
    else if(secs < 2*DAY_IN_SECONDS)
    {
        timeBehindText = QObject::tr("%n hour(s)","",secs/HOUR_IN_SECONDS);
    }
    else if(secs < 2*WEEK_IN_SECONDS)
    {
        timeBehindText = QObject::tr("%n day(s)","",secs/DAY_IN_SECONDS);
    }
    else if(secs < YEAR_IN_SECONDS)
    {
        timeBehindText = QObject::tr("%n week(s)","",secs/WEEK_IN_SECONDS);
    }
    else
    {
        qint64 years = secs / YEAR_IN_SECONDS;
        qint64 remainder = secs % YEAR_IN_SECONDS;
        timeBehindText = QObject::tr("%1 and %2").arg(QObject::tr("%n year(s)", "", years)).arg(QObject::tr("%n week(s)","", remainder/WEEK_IN_SECONDS));
    }
    return timeBehindText;
}

QString formatBytes(uint64_t bytes)
{
    if(bytes < 1024)
        return QString(QObject::tr("%1 B")).arg(bytes);
    if(bytes < 1024 * 1024)
        return QString(QObject::tr("%1 KB")).arg(bytes / 1024);
    if(bytes < 1024 * 1024 * 1024)
        return QString(QObject::tr("%1 MB")).arg(bytes / 1024 / 1024);

    return QString(QObject::tr("%1 GB")).arg(bytes / 1024 / 1024 / 1024);
}

qreal calculateIdealFontSize(int width, const QString& text, QFont font, qreal minPointSize, qreal font_size) {
    while(font_size >= minPointSize) {
        font.setPointSizeF(font_size);
        QFontMetrics fm(font);
        if (fm.width(text) < width) {
            break;
        }
        font_size -= 0.5;
    }
    return font_size;
}

void ClickableLabel::mouseReleaseEvent(QMouseEvent *event)
{
    Q_EMIT clicked(event->pos());
}

void ClickableProgressBar::mouseReleaseEvent(QMouseEvent *event)
{
    Q_EMIT clicked(event->pos());
}

} // namespace GUIUtil
