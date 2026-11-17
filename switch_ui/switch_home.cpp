// SPDX-License-Identifier: GPL-3.0-or-later
#include "switch_home.h"

#include <algorithm>
#include <mutex>
#include <cmath>
#include <functional>
#include <QAbstractItemModel>
#include <QConicalGradient>
#include <QLinearGradient>
#include <QDir>
#include <QFileDialog>
#include <QImage>
#include <QInputDialog>
#include <QLineEdit>
#include <QCursor>
#include <QMenu>
#include <QPainterPath>
#include <QSettings>
#include <QStandardPaths>
#include <QToolTip>
#include <QStringList>
#include "common/controller_console_state.h"
#include <QDateTime>
#include <QFileInfo>
#include <QFont>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QSet>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTimer>
#include <QVariant>

#include "qt_common/game_list/game_list_p.h"

namespace {
constexpr qreal CanvasWidth = 1280.0;
constexpr qreal CanvasHeight = 720.0;
constexpr int VisibleGames = 5;
const QColor Ink(47, 50, 55);
const QColor Muted(111, 117, 124);
const QColor Blue(0, 168, 240);
const QColor Background(234, 234, 234);
const char* kActions[] = {"Online", "Classics", "News", "eShop", "Album", "Add games", "Controllers", "Library", "Settings", "Power"};

void label(QPainter& painter, const QRectF& rect, const QString& value,
           int size, const QColor& color, Qt::Alignment align = Qt::AlignCenter,
           bool strong = false) {
    painter.save();
    QFont font(QStringLiteral("Arial"));
    font.setPixelSize(size);
    font.setBold(strong);
    painter.setFont(font);
    painter.setPen(color);
    painter.drawText(rect, static_cast<int>(align) | Qt::TextSingleLine, value);
    painter.restore();
}
}

SwitchHomeWidget::SwitchHomeWidget(QStandardItemModel* model, QWidget* parent)
    : QWidget(parent), model_(model) {
    setObjectName(QStringLiteral("SwitchHome"));
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumSize(800, 450);
    if (model_) {
        connect(model_, &QAbstractItemModel::modelReset, this, [this] { reload(); });
        connect(model_, &QAbstractItemModel::rowsInserted, this, [this] { reload(); });
        connect(model_, &QAbstractItemModel::rowsRemoved, this, [this] { reload(); });
        connect(model_, &QAbstractItemModel::dataChanged, this, [this] { reload(); });
    }
    auto* clock = new QTimer(this);
    connect(clock, &QTimer::timeout, this, [this] { update(); });
    clock->start(30000);
    loadProfile();
    auto* animation = new QTimer(this);
    connect(animation, &QTimer::timeout, this, [this] {
        gradientTick_ = (gradientTick_ + 1) % 360;
        update();
    });
    animation->start(70);
    auto* controllers = new QTimer(this);
    connect(controllers, &QTimer::timeout, this, [this] {
        refreshControllerCount();
    });
    controllers->start(500);
    refreshControllerCount();
    auto* navTimer = new QTimer(this);
    connect(navTimer, &QTimer::timeout, this, [this] { pollGamepadNavigation(); });
    navTimer->start(80);
    reload();
}

SwitchHomeWidget::~SwitchHomeWidget() {
    if (navigationPad_ && SDL_WasInit(SDL_INIT_GAMEPAD)) {
        SDL_CloseGamepad(navigationPad_);
        navigationPad_ = nullptr;
    }
}

void SwitchHomeWidget::reload() {
    const QString previousPath =
        selectedGame_ >= 0 && selectedGame_ < games_.size()
            ? games_[selectedGame_].path : QString();
    games_.clear();
    QSet<QString> uniquePaths;
    std::function<void(QStandardItem*)> visit = [&](QStandardItem* item) {
        if (!item) return;
        if (item->type() == static_cast<int>(GameListItemType::Game)) {
            const QString path = item->data(GameListItemPath::FullPathRole).toString();
            if (!path.isEmpty() && !uniquePaths.contains(path)) {
                uniquePaths.insert(path);
                QString title = item->data(GameListItemPath::TitleRole).toString().trimmed();
                if (title.isEmpty()) title = QFileInfo(path).completeBaseName();
                QPixmap icon = item->data(Qt::DecorationRole).value<QPixmap>();
                games_.append(Game{title, path, icon});
            }
        }
        for (int row = 0; row < item->rowCount(); ++row)
            visit(item->child(row, 0));
    };
    if (model_) {
        for (int row = 0; row < model_->rowCount(); ++row)
            visit(model_->item(row, 0));
    }
    selectedGame_ = 0;
    for (int index = 0; index < games_.size(); ++index) {
        if (games_[index].path == previousPath) {
            selectedGame_ = index;
            break;
        }
    }
    firstVisible_ = std::clamp(firstVisible_, 0, std::max(0, static_cast<int>(games_.size()) - VisibleGames));
    update();
}

void SwitchHomeWidget::drawToolbarSymbol(QPainter& p, int index, const QRectF& area) const {
    p.save();
    const QPointF c = area.center();
    p.translate(c);
    p.setRenderHint(QPainter::Antialiasing);
    QColor ink(121, 123, 128);
    switch (index) {
    case 0: ink = QColor(229, 0, 20); break;
    case 1: ink = QColor(237, 135, 13); break;
    case 2: ink = QColor(58, 176, 8); break;
    case 3: ink = QColor(218, 52, 41); break;
    case 4: ink = QColor(53, 105, 207); break;
    case 5: ink = QColor(14, 175, 179); break;
    }
    p.setPen(QPen(ink, 3.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    switch (index) {
    case 0: {
        p.setPen(Qt::NoPen);
        p.setBrush(ink);
        p.drawEllipse(QRectF(-31, -31, 62, 62));
        p.setBrush(Qt::white);
        p.drawRoundedRect(QRectF(-13, -15, 11, 21), 4, 4);
        p.drawRoundedRect(QRectF(2, -15, 11, 21), 4, 4);
        p.setBrush(ink);
        p.drawEllipse(QPointF(-8, -9), 2, 2);
        p.drawEllipse(QPointF(8, -1), 2, 2);
        p.drawEllipse(QPointF(8, -10), 2, 2);
        p.setPen(Qt::white);
        QFont f(QStringLiteral("Arial"), 8, QFont::Bold);
        p.setFont(f);
        p.drawText(QRectF(-28, 9, 56, 15), Qt::AlignCenter, QStringLiteral("ONLINE"));
        break;
    }
    case 1: {
        p.drawRect(QRectF(-16, -19, 32, 38));
        p.setPen(QPen(ink, 3.6));
        p.drawArc(QRectF(-9, -10, 21, 22), 66 * 16, 270 * 16);
        break;
    }
    case 2: {
        p.drawRect(QRectF(-9, -18, 25, 36));
        p.drawRoundedRect(QRectF(-18, -6, 11, 24), 3, 3);
        p.drawLine(QPointF(-4, -11), QPointF(10, -11));
        p.drawLine(QPointF(-4, -2), QPointF(10, -2));
        p.drawLine(QPointF(-4, 7), QPointF(10, 7));
        break;
    }
    case 3: {
        QPainterPath bag;
        bag.moveTo(-18, 17); bag.lineTo(-16, -12); bag.lineTo(17, -12);
        bag.lineTo(20, 17); bag.closeSubpath();
        p.drawPath(bag);
        p.drawArc(QRectF(-9, -22, 18, 22), 0, 180 * 16);
        break;
    }
    case 4: {
        p.drawRoundedRect(QRectF(-19, -15, 38, 30), 2, 2);
        p.setPen(QPen(ink, 3.7));
        QPainterPath inside;
        inside.moveTo(-10, 8);
        inside.lineTo(-10, -4);
        inside.cubicTo(-9, -10, -3, -10, 1, -4);
        inside.cubicTo(7, -10, 15, -7, 12, 4);
        p.drawPath(inside);
        break;
    }
    case 5: {
        QPainterPath teal;
        teal.moveTo(-18, -12);
        teal.lineTo(-18, 15); teal.lineTo(18, 15); teal.lineTo(18, -12);
        teal.moveTo(-11, -18);
        teal.cubicTo(-2, -14, 5, -13, 13, -18);
        p.drawPath(teal);
        p.drawArc(QRectF(-11, -17, 22, 21), 200 * 16, 140 * 16);
        p.drawArc(QRectF(-12, -7, 24, 16), 195 * 16, 150 * 16);
        break;
    }
    case 6: {
        p.rotate(42);
        p.drawRoundedRect(QRectF(-13, -29, 26, 58), 10, 10);
        p.setBrush(ink);
        p.drawEllipse(QPointF(0, -12), 3, 3);
        p.drawEllipse(QPointF(-4, 3), 2.3, 2.3);
        p.drawEllipse(QPointF(5, 3), 2.3, 2.3);
        p.drawEllipse(QPointF(0, 10), 2.3, 2.3);
        p.setBrush(Qt::NoBrush);
        break;
    }
    case 7: {
        p.drawRoundedRect(QRectF(-14, -21, 28, 41), 1, 1);
        p.setPen(Qt::NoPen);
        p.setBrush(ink);
        QPolygonF arrow;
        arrow << QPointF(-8, 7) << QPointF(8, 7) << QPointF(0, 15);
        p.drawPolygon(arrow);
        break;
    }
    case 8: {
        p.drawEllipse(QRectF(-13, -13, 26, 26));
        for (int j = 0; j < 8; ++j) {
            qreal a = j * 3.141592653589793 / 4;
            p.drawLine(QPointF(15 * std::cos(a), 15 * std::sin(a)),
                       QPointF(21 * std::cos(a), 21 * std::sin(a)));
        }
        break;
    }
    case 9:
        p.setPen(QPen(ink, 3.6));
        p.drawArc(QRectF(-17, -17, 34, 34), 45 * 16, 270 * 16);
        p.drawLine(QPointF(0, -19), QPointF(0, 2));
        break;
    }
    p.restore();
}

void SwitchHomeWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.scale(width() / CanvasWidth, height() / CanvasHeight);
    p.fillRect(QRectF(0, 0, CanvasWidth, CanvasHeight), Background);
    if (page_ != Page::Home) {
        paintSubpage(p);
        return;
    }

    // The profile uses a live conical gradient. No pre-rendered ring is rotated.
    const QRectF ring(48, 30, 65, 65);
    QConicalGradient gradient(ring.center(), -gradientTick_);
    gradient.setColorAt(0, QColor(246, 194, 233));
    gradient.setColorAt(.24, QColor(118, 86, 248));
    gradient.setColorAt(.51, QColor(34, 124, 245));
    gradient.setColorAt(.74, QColor(112, 232, 240));
    gradient.setColorAt(1, QColor(246, 194, 233));
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QBrush(gradient), 5.7));
    p.drawEllipse(ring);
    p.setPen(QPen(QColor(255, 255, 255, 205), 1.4));
    p.drawEllipse(ring.adjusted(5, 5, -5, -5));
    const QRectF photo(55, 37, 51, 51);
    QPainterPath clipping;
    clipping.addEllipse(photo);
    p.save();
    p.setClipPath(clipping);
    if (!profilePicture_.isNull()) {
        p.drawPixmap(photo.toRect(), profilePicture_);
    } else {
        p.fillRect(photo, QColor(26, 91, 205));
        label(p, photo, QStringLiteral("P1"), 18, Qt::white, Qt::AlignCenter, true);
    }
    p.restore();
    label(p, QRectF(22, 105, 250, 30), profileName_,
          20, QColor(27, 107, 198), Qt::AlignLeft | Qt::AlignVCenter);

    // Top-right clock, Wi-Fi icon and battery; battery and Wi-Fi are visual
    // indicators until platform status plumbing is added.
    const QString clock = QDateTime::currentDateTime().toString(QStringLiteral("h:mm"));
    const QString period = QDateTime::currentDateTime().toString(QStringLiteral("AP"));
    label(p, QRectF(1013, 42, 124, 40), clock, 26, QColor(10, 10, 10),
          Qt::AlignVCenter | Qt::AlignRight);
    label(p, QRectF(1140, 49, 37, 31), period, 14, QColor(10, 10, 10),
          Qt::AlignVCenter | Qt::AlignLeft);
    p.save();
    p.setPen(QPen(QColor(12, 14, 16), 3.2, Qt::SolidLine, Qt::RoundCap));
    p.drawArc(QRectF(1180, 48, 30, 28), 40 * 16, 100 * 16);
    p.drawArc(QRectF(1186, 55, 19, 16), 40 * 16, 100 * 16);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(12, 14, 16));
    p.drawEllipse(QPointF(1196, 72), 2.5, 2.5);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(12, 14, 16), 2.6, Qt::SolidLine, Qt::RoundCap));
    p.drawRoundedRect(QRectF(1222, 51, 35, 21), 3, 3);
    p.drawLine(QPointF(1260, 57), QPointF(1260, 66));
    p.setBrush(QColor(12, 14, 16));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(QRectF(1226, 55, 27, 13), 1, 1);
    p.restore();

    gameTargets_.clear();
    if (games_.isEmpty()) {
        label(p, QRectF(170, 240, 940, 48), QStringLiteral("No games available"),
              28, Ink, Qt::AlignCenter, true);
        label(p, QRectF(160, 300, 960, 34),
              QStringLiteral("Choose Add games to select your games directory"),
              19, Muted);
    } else {
        firstVisible_ = std::min(firstVisible_, std::max(0, static_cast<int>(games_.size()) - VisibleGames));
        if (selectedGame_ < firstVisible_) firstVisible_ = selectedGame_;
        if (selectedGame_ >= firstVisible_ + VisibleGames)
            firstVisible_ = selectedGame_ - VisibleGames + 1;

        if (!toolbarFocused_) {
            label(p, QRectF(50, 149, 1060, 35), games_[selectedGame_].title,
                  23, QColor(44, 108, 189), Qt::AlignLeft | Qt::AlignVCenter);
        }
        int visible = std::min(VisibleGames, static_cast<int>(games_.size()) - firstVisible_);
        for (int j = 0; j < visible; ++j) {
            int index = firstVisible_ + j;
            const bool active = !toolbarFocused_ && selectedGame_ == index;
            QRectF rect(106 + j * 269, 201, 256, 256);
            gameTargets_.append(rect);
            p.setBrush(Qt::white);
            p.setPen(QPen(active ? Blue : QColor(225, 225, 225), active ? 4.6 : 1));
            p.drawRoundedRect(rect.adjusted(active ? -4 : 0, active ? -4 : 0,
                                            active ? 4 : 0, active ? 4 : 0), 7, 7);
            QRectF contents = rect.adjusted(3, 3, -3, -3);
            QPainterPath clip;
            clip.addRoundedRect(contents, 5, 5);
            p.save();
            p.setClipPath(clip);
            if (!games_[index].icon.isNull()) {
                p.drawPixmap(contents.toRect(), games_[index].icon);
            } else {
                p.fillRect(contents, QColor(205, 219, 232));
                label(p, contents, games_[index].title.left(2).toUpper(),
                      50, QColor(93, 112, 139), Qt::AlignCenter, true);
            }
            p.restore();
        }
    }

    // Exact order and capsule proportions from the user's navigation reference.
    QRectF capsule(220, 510, 845, 92);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(248, 248, 249));
    p.drawRoundedRect(capsule, 46, 46);
    toolbarTargets_.clear();
    for (int i = 0; i < 10; ++i) {
        QRectF target(232 + i * 83, 520, 71, 71);
        toolbarTargets_.append(target);
        if (toolbarFocused_ && selectedToolbar_ == i) {
            QConicalGradient glow(target.center(), gradientTick_);
            glow.setColorAt(0, QColor(255, 189, 228));
            glow.setColorAt(.3, QColor(131, 102, 238));
            glow.setColorAt(.7, QColor(72, 147, 250));
            glow.setColorAt(1, QColor(255, 189, 228));
            p.setPen(QPen(QBrush(glow), 3));
            p.setBrush(QColor(248, 248, 249));
            p.drawEllipse(target);
        }
        drawToolbarSymbol(p, i, target);
    }
    if (toolbarFocused_) {
        label(p, QRectF(330, 608, 620, 34),
              QString::fromLatin1(kActions[selectedToolbar_]), 22,
              QColor(56, 113, 193));
    }
    if (!toast_.isEmpty()) {
        label(p, QRectF(320, 631, 635, 26), toast_, 16, Muted);
    }

    // Joy-Cons: four live status cells, green for assigned players (up to four).
    const int startX = 50;
    for (int i = 0; i < 4; ++i) {
        p.setPen(Qt::NoPen);
        p.setBrush(i < controllers_ ? QColor(27, 195, 16) : QColor(161, 161, 161));
        p.drawRect(QRectF(startX, 642 + i * 12, 9, 9));
    }
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(8, 8, 10));
    p.drawRoundedRect(QRectF(71, 635, 28, 56), 13, 13);
    p.drawRoundedRect(QRectF(104, 635, 28, 56), 13, 13);
    p.setBrush(Qt::white);
    p.drawEllipse(QPointF(90, 649), 5, 5);
    p.drawEllipse(QPointF(112, 650), 4.5, 4.5);
    for (int i = 0; i < 3; ++i) {
        p.drawEllipse(QPointF(88, 667 + i * 7), 2, 2);
        p.drawEllipse(QPointF(117, 667 + i * 7), 2, 2);
    }
    p.setBrush(QColor(8, 8, 10));
    p.drawEllipse(QPointF(94, 649), 2, 2);

    // Bottom-right A/OK key, matching the provided black circular A badge.
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(7, 7, 9));
    p.drawEllipse(QRectF(1128, 650, 30, 30));
    label(p, QRectF(1128, 650, 30, 30), QStringLiteral("A"), 20,
          Qt::white, Qt::AlignCenter);
    label(p, QRectF(1170, 649, 75, 32), QStringLiteral("OK"), 21,
          QColor(8, 8, 8), Qt::AlignLeft | Qt::AlignVCenter);
}


namespace {
void highlightRow(QPainter& p, const QRectF& box, int tick) {
    QLinearGradient gradient(box.topLeft(), box.bottomRight());
    gradient.setColorAt(0.0, QColor(79, 100, 249));
    gradient.setColorAt(.32, QColor(146, 116, 248));
    gradient.setColorAt(.67, QColor(245, 166, 210));
    gradient.setColorAt(.85, QColor(85, 195, 242));
    gradient.setColorAt(1.0, QColor(13, 92, 222));
    p.setBrush(QColor(247, 247, 248));
    p.setPen(QPen(QBrush(gradient), 3.3, Qt::SolidLine, Qt::RoundCap,
                  Qt::RoundJoin));
    p.drawRoundedRect(box, 9, 9);
    Q_UNUSED(tick);
}

void paragraph(QPainter& p, const QRectF& bounds, const QString& text,
               int fontSize = 21, QColor color = QColor(65, 65, 69)) {
    p.save();
    QFont font(QStringLiteral("Arial"));
    font.setPixelSize(fontSize);
    p.setFont(font);
    p.setPen(color);
    p.drawText(bounds, Qt::TextWordWrap | Qt::AlignTop | Qt::AlignLeft, text);
    p.restore();
}

void iconButton(QPainter& p, const QRectF& circle, const QString& letter) {
    p.save();
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(10, 10, 12));
    p.drawEllipse(circle);
    label(p, circle, letter, 18, Qt::white);
    p.restore();
}

QString optionTitle(int option) {
    switch (option) {
    case 0: return QStringLiteral("Change Grip/Order");
    case 1: return QStringLiteral("Search for Controllers");
    case 2: return QStringLiteral("How to Pair Controllers");
    case 3: return QStringLiteral("How to Use the GL/GR Buttons");
    case 4: return QStringLiteral("How to Use Mouse Controls");
    default: return QStringLiteral("Controllers");
    }
}
} // namespace

void SwitchHomeWidget::drawJoycons(QPainter& p, qreal x, qreal y,
                                   qreal scale, bool colorful) const {
    p.save();
    p.translate(x, y);
    p.scale(scale, scale);
    const QColor base = colorful ? QColor(31, 32, 35) : QColor(207, 207, 209);
    const QColor white = colorful ? QColor(168, 173, 179) : QColor(245, 245, 246);
    p.setPen(Qt::NoPen);
    p.setBrush(base);
    QPainterPath left;
    left.moveTo(22, 0); left.lineTo(31, 0); left.lineTo(31, 78);
    left.lineTo(22, 78); left.cubicTo(8, 78, 0, 66, 0, 52);
    left.lineTo(0, 25); left.cubicTo(0, 10, 8, 0, 22, 0);
    p.drawPath(left);
    QPainterPath right;
    right.moveTo(52, 0); right.lineTo(62, 0);
    right.cubicTo(78, 0, 83, 12, 83, 25);
    right.lineTo(83, 52); right.cubicTo(83, 69, 74, 78, 62, 78);
    right.lineTo(52, 78); right.closeSubpath();
    p.drawPath(right);
    p.setBrush(white);
    p.drawEllipse(QPointF(19, 32), 7, 7);
    p.drawEllipse(QPointF(65, 48), 6.2, 6.2);
    for (int i = 0; i < 4; ++i) {
        const QPointF a(19 + (i == 1 ? 7 : i == 3 ? -7 : 0),
                        53 + (i == 0 ? -7 : i == 2 ? 7 : 0));
        const QPointF b(65 + (i == 1 ? 7 : i == 3 ? -7 : 0),
                        25 + (i == 0 ? -7 : i == 2 ? 7 : 0));
        p.drawEllipse(a, 3, 3);
        p.drawEllipse(b, 3, 3);
    }
    p.setBrush(colorful ? QColor(100, 228, 238) : white);
    p.drawEllipse(QPointF(19, 32), 3, 3);
    p.setBrush(colorful ? QColor(255, 145, 91) : white);
    p.drawEllipse(QPointF(65, 48), 2.9, 2.9);
    p.restore();
}

void SwitchHomeWidget::drawFooter(QPainter& p) const {
    p.save();
    p.setPen(QPen(QColor(174, 174, 177), .85));
    p.drawLine(QPointF(43, 650), QPointF(1238, 650));

    for (int i = 0; i < 4; ++i) {
        p.setPen(Qt::NoPen);
        p.setBrush(i < controllers_ ? QColor(19, 199, 21) : QColor(170, 170, 172));
        p.drawRect(QRectF(45, 663 + i * 10.5, 8, 8));
    }
    drawJoycons(p, 62, 662, .55, true);
    if (page_ != Page::Home) {
        iconButton(p, QRectF(1039, 666, 25, 25), QStringLiteral("B"));
        label(p, QRectF(1071, 662, 59, 32), QStringLiteral("Back"), 20,
              QColor(17, 17, 18), Qt::AlignLeft | Qt::AlignVCenter);
    }
    iconButton(p, QRectF(1152, 666, 25, 25), QStringLiteral("A"));
    label(p, QRectF(1184, 662, 48, 32), QStringLiteral("OK"), 20,
          QColor(17, 17, 18), Qt::AlignLeft | Qt::AlignVCenter);
    p.restore();
}

void SwitchHomeWidget::paintSubpage(QPainter& p) {
    const bool controllerPage = page_ == Page::Controllers || page_ == Page::ControllerDetail;
    const bool settingsPage = page_ == Page::Settings || page_ == Page::SettingsDetail;
    QString title = controllerPage ? QStringLiteral("Controllers") :
                    settingsPage ? QStringLiteral("System Settings") :
                    page_ == Page::Library ? QStringLiteral("All Software") :
                    page_ == Page::Profile ? QStringLiteral("User Settings") :
                    page_ == Page::Power ? QStringLiteral("Power Options") :
                    serviceTitle_;
    if (title.isEmpty()) title = QStringLiteral("Home");

    if (controllerPage) {
        drawToolbarSymbol(p, 6, QRectF(68, 28, 55, 55));
        label(p, QRectF(121, 21, 620, 63), title, 32, QColor(15, 15, 18),
              Qt::AlignLeft | Qt::AlignVCenter);
    } else {
        label(p, QRectF(48, 25, 950, 63), title, 32, QColor(15, 15, 18),
              Qt::AlignLeft | Qt::AlignVCenter);
    }
    p.setPen(QPen(QColor(169, 169, 170), .8));
    p.drawLine(QPointF(43, 84), QPointF(1238, 84));

    if (controllerPage) {
        // Exactly the two-column structure of the supplied Switch 2 reference.
        p.setPen(QPen(QColor(175, 175, 177), .8));
        p.drawLine(QPointF(865, 91), QPointF(865, 633));
        label(p, QRectF(265, 115, 330, 32), QStringLiteral("System"), 19,
              QColor(99, 99, 101));
        // Draw only the left half of the controller silhouette.
        p.save();
        p.setClipRect(QRectF(271, 169, 49, 115));
        drawJoycons(p, 272, 171, 1.38, false);
        p.restore();
        // console screen between the detached system Joy-Cons
        p.setPen(QPen(QColor(4, 4, 5), 5));
        p.setBrush(QColor(17, 17, 18));
        p.drawRect(QRectF(344, 173, 180, 103));
        p.setPen(QPen(QColor(1, 1, 2), 5));
        p.setBrush(QColor(247, 247, 247));
        p.drawRect(QRectF(353, 180, 161, 87));
        // Right Joy-Con has its own clipped silhouette.
        p.save();
        p.setClipRect(QRectF(542, 169, 62, 115));
        drawJoycons(p, 542 - 52 * 1.38, 171, 1.38, false);
        p.restore();
        // battery under the system screen (decorative, unknown real charge)
        p.setPen(QPen(QColor(10, 10, 11), 2));
        p.setBrush(QColor(9, 9, 9));
        p.drawRoundedRect(QRectF(411, 297, 34, 19), 2, 2);
        p.drawRect(QRectF(446, 303, 3, 7));
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(244, 244, 246));
        p.drawRoundedRect(QRectF(416, 301, 24, 11), 1, 1);

        p.setPen(QPen(QColor(173, 173, 175), .8));
        p.drawLine(QPointF(86, 367), QPointF(777, 367));
        label(p, QRectF(285, 396, 285, 36), QStringLiteral("Controllers"), 20,
              QColor(100, 100, 101));

        // Controller status groups based on Eden assignments.
        const int visible = std::min(controllers_, 4);
        for (int i = 0; i < std::max(visible, 1); ++i) {
            const qreal x = 158 + i * 118;
            for (int b = 0; b < 4; ++b) {
                p.setPen(Qt::NoPen);
                p.setBrush(b <= i && i < visible ? QColor(19, 202, 20)
                                                    : QColor(165, 165, 167));
                p.drawRect(QRectF(x + b * 18, 467, 11, 11));
            }
            drawJoycons(p, x - 3, 491, 0.85, i < visible);
            // outlines only for device batteries: never imply charge data.
            p.setPen(QPen(QColor(14, 14, 14), 2));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(QRectF(x - 3, 593, 30, 16), 3, 3);
            p.drawRoundedRect(QRectF(x + 38, 593, 30, 16), 3, 3);
        }

        const QRectF rowRects[] = {
            QRectF(886, 151, 335, 76),
            QRectF(886, 238, 335, 85),
            QRectF(886, 329, 335, 77),
            QRectF(886, 415, 335, 85),
            QRectF(886, 501, 335, 84)
        };
        if (page_ == Page::Controllers) {
            p.setPen(QPen(QColor(165, 165, 167), .85));
            p.drawLine(QPointF(891, 149), QPointF(1216, 149));
            p.drawLine(QPointF(891, 411), QPointF(1216, 411));
            p.drawLine(QPointF(891, 501), QPointF(1216, 501));
            p.drawLine(QPointF(891, 588), QPointF(1216, 588));
            for (int i = 0; i < 5; ++i) {
                if (menuSelection_ == i) highlightRow(p, rowRects[i], gradientTick_);
                label(p, QRectF(908, rowRects[i].top(), 302, rowRects[i].height()),
                      optionTitle(i), i == 3 ? 18 : 21, QColor(18, 18, 21),
                      Qt::AlignLeft | Qt::AlignVCenter);
            }
        } else {
            p.setPen(QPen(QColor(170, 170, 172), .8));
            p.drawLine(QPointF(886, 151), QPointF(1215, 151));
            label(p, QRectF(899, 169, 300, 58), optionTitle(controllerDetail_),
                  24, QColor(15, 15, 18), Qt::AlignLeft | Qt::AlignVCenter);
            QString body;
            switch (controllerDetail_) {
            case 0:
                body = QStringLiteral("Controller assignments are managed by Eden. "
                         "Connected players are shown at left. This page keeps "
                         "the console interface without opening the original menus.");
                break;
            case 1:
                body = QStringLiteral("Connected players: %1.\n\n"
                         "Connect a controller to Windows. Eden will detect "
                         "supported controllers automatically.")
                         .arg(controllers_);
                break;
            case 2:
                body = QStringLiteral("Put your controller into pairing mode, "
                         "then connect it through Windows Bluetooth settings "
                         "or its USB receiver. Return here to see the assigned players.");
                break;
            case 3:
                body = QStringLiteral("GL and GR are additional buttons on compatible "
                         "controllers. Their availability depends on your device "
                         "and controller mapping.");
                break;
            default:
                body = QStringLiteral("Mouse controls are available in selected "
                         "games and configurations. Use the pointer to select "
                         "items on this interface.");
                break;
            }
            paragraph(p, QRectF(900, 244, 298, 310), body, 19);
            if (controllerDetail_ == 1) {
                highlightRow(p, QRectF(901, 542, 297, 51), gradientTick_);
                label(p, QRectF(901, 542, 297, 51), QStringLiteral("Refresh status"),
                      20, QColor(20, 20, 23));
            }
        }
    } else if (settingsPage) {
        p.setPen(QPen(QColor(175, 175, 177), .8));
        p.drawLine(QPointF(420, 91), QPointF(420, 633));
        const QStringList settings{
            QStringLiteral("Display"), QStringLiteral("Audio"),
            QStringLiteral("Controllers"), QStringLiteral("System"),
            QStringLiteral("Profile"), QStringLiteral("About")
        };
        for (int i = 0; i < settings.size(); ++i) {
            QRectF row(48, 132 + i * 76, 349, 68);
            if (settingsSelection_ == i) highlightRow(p, row, gradientTick_);
            label(p, row.adjusted(20, 0, -5, 0), settings[i], 23, QColor(24, 24, 24),
                  Qt::AlignLeft | Qt::AlignVCenter);
        }
        const QString selected = settings.at(std::clamp(settingsSelection_, 0, 5));
        label(p, QRectF(462, 153, 690, 75), selected, 31, Ink,
              Qt::AlignLeft | Qt::AlignVCenter);
        paragraph(p, QRectF(463, 250, 700, 260),
                  page_ == Page::Settings
                    ? QStringLiteral("Select a category to open its console-style page.")
                    : QStringLiteral("This section uses the Switch-style interface. "
                        "Advanced emulator options remain unchanged in Eden."),
                  23);
    } else if (page_ == Page::Library) {
        if (games_.isEmpty()) {
            label(p, QRectF(90, 250, 1100, 60), QStringLiteral("No games found"),
                  30, Ink);
            label(p, QRectF(90, 330, 1100, 50),
                  QStringLiteral("Return to Home and add a games folder."), 22, Muted);
        } else {
            const int offset = (librarySelection_ / 4) * 4;
            for (int i = offset; i < std::min(offset + 4, static_cast<int>(games_.size())); ++i) {
                const int col = i - offset;
                const QRectF rect(84 + col * 301, 195, 256, 256);
                if (i == librarySelection_) highlightRow(p, rect.adjusted(-5, -5, 5, 5), gradientTick_);
                if (!games_[i].icon.isNull()) {
                    p.drawPixmap(rect.toRect(), games_[i].icon);
                } else {
                    p.fillRect(rect, QColor(205, 218, 229));
                    label(p, rect, games_[i].title.left(2).toUpper(),
                          51, QColor(99, 117, 139), Qt::AlignCenter, true);
                }
                label(p, QRectF(rect.left(), 469, 270, 45), games_[i].title,
                      17, Ink, Qt::AlignLeft | Qt::AlignVCenter);
            }
        }
    } else if (page_ == Page::Profile) {
        label(p, QRectF(112, 160, 470, 65), profileName_, 28, Ink,
              Qt::AlignLeft | Qt::AlignVCenter);
        p.setPen(QPen(QColor(138, 122, 231), 7));
        p.setBrush(QColor(44, 106, 194));
        p.drawEllipse(QRectF(51, 155, 58, 58));
        if (!profilePicture_.isNull()) {
            p.drawPixmap(QRect(57, 161, 46, 46), profilePicture_);
        }
        const QStringList items{QStringLiteral("Choose Profile Picture"),
                                QStringLiteral("Rename Profile"),
                                QStringLiteral("Remove Picture")};
        for (int i = 0; i < items.size(); ++i) {
            QRectF row(480, 177 + i * 103, 700, 79);
            if (profileSelection_ == i) highlightRow(p, row, gradientTick_);
            label(p, row.adjusted(24, 0, -8, 0), items[i], 24, Ink,
                  Qt::AlignLeft | Qt::AlignVCenter);
        }
    } else if (page_ == Page::Power) {
        const QStringList items{QStringLiteral("Return to Home"),
                                QStringLiteral("Close Eden")};
        paragraph(p, QRectF(133, 157, 930, 64),
                  QStringLiteral("Choose an option."), 23);
        for (int i = 0; i < items.size(); ++i) {
            QRectF row(146, 290 + i * 116, 980, 86);
            if (powerSelection_ == i) highlightRow(p, row, gradientTick_);
            label(p, row, items[i], 27, Ink);
        }
    } else {
        label(p, QRectF(100, 230, 1080, 65), QStringLiteral("Not available in Eden"),
              32, Ink, Qt::AlignCenter, true);
        paragraph(p, QRectF(230, 329, 820, 150),
                  QStringLiteral("This console service is not part of the emulator. "
                  "Press B to return to Home."), 23);
    }
    drawFooter(p);
}

int SwitchHomeWidget::pageRowCount() const {
    switch (page_) {
    case Page::Controllers: return 5;
    case Page::Settings: return 6;
    case Page::Profile: return 3;
    case Page::Power: return 2;
    case Page::Library: return static_cast<int>(games_.size());
    default: return 1;
    }
}

void SwitchHomeWidget::back() {
    switch (page_) {
    case Page::Home: toolbarFocused_ = false; break;
    case Page::ControllerDetail: page_ = Page::Controllers; break;
    case Page::SettingsDetail: page_ = Page::Settings; break;
    default: page_ = Page::Home; break;
    }
    update();
}

void SwitchHomeWidget::activateSubpage() {
    switch (page_) {
    case Page::Controllers:
        controllerDetail_ = menuSelection_;
        page_ = Page::ControllerDetail;
        break;
    case Page::ControllerDetail:
        if (controllerDetail_ == 1) refreshControllerCount();
        else back();
        break;
    case Page::Settings:
        if (settingsSelection_ == 2) {
            page_ = Page::Controllers;
            menuSelection_ = 1;
        } else if (settingsSelection_ == 4) {
            page_ = Page::Profile;
        } else {
            page_ = Page::SettingsDetail;
        }
        break;
    case Page::SettingsDetail:
        back();
        break;
    case Page::Library:
        if (librarySelection_ >= 0 && librarySelection_ < games_.size() && onLaunch) {
            const QString chosen = games_[librarySelection_].path;
            page_ = Page::Home;
            onLaunch(chosen);
        }
        break;
    case Page::Profile:
        if (profileSelection_ == 0) chooseProfilePhoto();
        else if (profileSelection_ == 1) renameProfile();
        else removeProfilePhoto();
        break;
    case Page::Power:
        if (powerSelection_ == 1 && onClose) onClose();
        else page_ = Page::Home;
        break;
    case Page::Service: page_ = Page::Home; break;
    case Page::Home: break;
    }
    update();
}

void SwitchHomeWidget::moveSelection(int distance) {
    if (toolbarFocused_) {
        selectedToolbar_ = std::clamp(selectedToolbar_ + distance, 0, 9);
    } else if (!games_.isEmpty()) {
        selectedGame_ = std::clamp(selectedGame_ + distance, 0, static_cast<int>(games_.size()) - 1);
    }
    update();
}

void SwitchHomeWidget::activate() {
    if (!toolbarFocused_) {
        if (!games_.isEmpty() && onLaunch) onLaunch(games_[selectedGame_].path);
        return;
    }
    switch (selectedToolbar_) {
    case 0: case 1: case 2: case 3: case 4:
        serviceTitle_ = QString::fromLatin1(kActions[selectedToolbar_]);
        page_ = Page::Service;
        break;
    case 5: if (onAddDirectory) onAddDirectory(); break;
    case 6: page_ = Page::Controllers; menuSelection_ = 1; break;
    case 7: page_ = Page::Library; librarySelection_ = 0; break;
    case 8: page_ = Page::Settings; settingsSelection_ = 0; break;
    case 9: page_ = Page::Power; powerSelection_ = 0; break;
    }
    update();
}

void SwitchHomeWidget::keyPressEvent(QKeyEvent* event) {
    if (page_ != Page::Home) {
        const int key = event->key();
        if (key == Qt::Key_Escape || key == Qt::Key_Backspace || key == Qt::Key_B) {
            back();
            return;
        }
        if (key == Qt::Key_Enter || key == Qt::Key_Return ||
            key == Qt::Key_Space || key == Qt::Key_A) {
            activateSubpage();
            return;
        }
        if (key == Qt::Key_Up || key == Qt::Key_Down ||
            (page_ == Page::Library && (key == Qt::Key_Left || key == Qt::Key_Right))) {
            const int shift = (key == Qt::Key_Up || key == Qt::Key_Left) ? -1 : 1;
            const int limit = pageRowCount();
            auto navigate = [shift, limit](int& selected) {
                selected = std::clamp(selected + shift, 0, std::max(0, limit - 1));
            };
            switch (page_) {
            case Page::Controllers: navigate(menuSelection_); break;
            case Page::Settings: navigate(settingsSelection_); break;
            case Page::Library: navigate(librarySelection_); break;
            case Page::Profile: navigate(profileSelection_); break;
            case Page::Power: navigate(powerSelection_); break;
            default: break;
            }
            update();
            return;
        }
        QWidget::keyPressEvent(event);
        return;
    }
    switch (event->key()) {
    case Qt::Key_Left: moveSelection(-1); return;
    case Qt::Key_Right: moveSelection(1); return;
    case Qt::Key_Up: toolbarFocused_ = false; update(); return;
    case Qt::Key_Down: toolbarFocused_ = true; update(); return;
    case Qt::Key_Enter:
    case Qt::Key_Return:
    case Qt::Key_Space:
    case Qt::Key_A: activate(); return;
    case Qt::Key_Escape:
    case Qt::Key_B: toolbarFocused_ = false; update(); return;
    case Qt::Key_P: editProfile(); return;
    default: QWidget::keyPressEvent(event); return;
    }
}

void SwitchHomeWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;
    const QPointF pos(event->position().x() * CanvasWidth / width(),
                      event->position().y() * CanvasHeight / height());

    if (page_ != Page::Home) {
        // Footer A / B controls are deliberately clickable, just like the
        // on-screen console affordances.
        if (QRectF(1027, 658, 108, 49).contains(pos)) {
            back();
            return;
        }
        if (QRectF(1142, 658, 111, 49).contains(pos)) {
            activateSubpage();
            return;
        }
        if (page_ == Page::Controllers && pos.x() >= 880 && pos.x() <= 1230) {
            const QRectF rows[] = {
                QRectF(886, 151, 335, 76), QRectF(886, 238, 335, 85),
                QRectF(886, 329, 335, 77), QRectF(886, 415, 335, 85),
                QRectF(886, 501, 335, 84)
            };
            for (int i = 0; i < 5; ++i) {
                if (rows[i].contains(pos)) {
                    menuSelection_ = i;
                    activateSubpage();
                    return;
                }
            }
        } else if (page_ == Page::ControllerDetail) {
            if (controllerDetail_ == 1 && QRectF(901, 542, 297, 51).contains(pos)) {
                refreshControllerCount();
                update();
            }
        } else if (page_ == Page::Settings) {
            for (int i = 0; i < 6; ++i) {
                if (QRectF(48, 132 + i * 76, 349, 68).contains(pos)) {
                    settingsSelection_ = i;
                    activateSubpage();
                    return;
                }
            }
        } else if (page_ == Page::Profile) {
            for (int i = 0; i < 3; ++i) {
                if (QRectF(480, 177 + i * 103, 700, 79).contains(pos)) {
                    profileSelection_ = i;
                    activateSubpage();
                    return;
                }
            }
        } else if (page_ == Page::Power) {
            for (int i = 0; i < 2; ++i) {
                if (QRectF(146, 290 + i * 116, 980, 86).contains(pos)) {
                    powerSelection_ = i;
                    activateSubpage();
                    return;
                }
            }
        } else if (page_ == Page::Library) {
            const int offset = (librarySelection_ / 4) * 4;
            for (int i = offset; i < std::min(offset + 4, static_cast<int>(games_.size())); ++i) {
                const QRectF rect(84 + (i - offset) * 301, 195, 256, 256);
                if (rect.contains(pos)) {
                    librarySelection_ = i;
                    activateSubpage();
                    return;
                }
            }
        } else if (page_ == Page::Service || page_ == Page::SettingsDetail) {
            back();
            return;
        }
        update();
        return;
    }

    if (avatarTarget_.contains(pos)) {
        editProfile();
        return;
    }
    for (int slot = 0; slot < gameTargets_.size(); ++slot) {
        if (gameTargets_[slot].contains(pos)) {
            const int newIndex = slot + firstVisible_;
            if (newIndex >= games_.size()) return;
            const bool wasSelected = !toolbarFocused_ && selectedGame_ == newIndex;
            selectedGame_ = newIndex;
            toolbarFocused_ = false;
            update();
            if (wasSelected) activate();
            return;
        }
    }
    for (int slot = 0; slot < toolbarTargets_.size(); ++slot) {
        if (toolbarTargets_[slot].contains(pos)) {
            selectedToolbar_ = slot;
            toolbarFocused_ = true;
            update();
            activate();
            return;
        }
    }
}


void SwitchHomeWidget::showUnavailable() {
    toast_ = QStringLiteral("This Nintendo service is not available in Eden.");
    update();
    QTimer::singleShot(2500, this, [this] {
        toast_.clear();
        update();
    });
}


void SwitchHomeWidget::pollGamepadNavigation() {
    if (!isVisible() || !isActiveWindow() || !SDL_WasInit(SDL_INIT_GAMEPAD)) return;

    if (navigationPad_ && !SDL_GamepadConnected(navigationPad_)) {
        SDL_CloseGamepad(navigationPad_);
        navigationPad_ = nullptr;
        lastGamepadButtons_.fill(false);
    }
    if (!navigationPad_ && (++navigationPollTick_ % 8 == 0)) {
        int count = 0;
        SDL_JoystickID* ids = SDL_GetGamepads(&count);
        if (ids && count > 0) navigationPad_ = SDL_OpenGamepad(ids[0]);
        SDL_free(ids);
    }
    if (!navigationPad_) return;

    // Use edge-triggered presses to avoid repeated menu activations.
    const bool pressed[] = {
        SDL_GetGamepadButton(navigationPad_, SDL_GAMEPAD_BUTTON_DPAD_LEFT) ||
            SDL_GetGamepadAxis(navigationPad_, SDL_GAMEPAD_AXIS_LEFTX) < -18000,
        SDL_GetGamepadButton(navigationPad_, SDL_GAMEPAD_BUTTON_DPAD_RIGHT) ||
            SDL_GetGamepadAxis(navigationPad_, SDL_GAMEPAD_AXIS_LEFTX) > 18000,
        SDL_GetGamepadButton(navigationPad_, SDL_GAMEPAD_BUTTON_DPAD_UP) ||
            SDL_GetGamepadAxis(navigationPad_, SDL_GAMEPAD_AXIS_LEFTY) < -18000,
        SDL_GetGamepadButton(navigationPad_, SDL_GAMEPAD_BUTTON_DPAD_DOWN) ||
            SDL_GetGamepadAxis(navigationPad_, SDL_GAMEPAD_AXIS_LEFTY) > 18000,
        SDL_GetGamepadButton(navigationPad_, SDL_GAMEPAD_BUTTON_SOUTH),
        SDL_GetGamepadButton(navigationPad_, SDL_GAMEPAD_BUTTON_EAST),
    };
    constexpr int keyCodes[] = {
        Qt::Key_Left, Qt::Key_Right, Qt::Key_Up, Qt::Key_Down,
        Qt::Key_Return, Qt::Key_Escape
    };
    for (std::size_t i = 0; i < lastGamepadButtons_.size(); ++i) {
        if (pressed[i] && !lastGamepadButtons_[i]) {
            QKeyEvent input(QEvent::KeyPress, keyCodes[i], Qt::NoModifier);
            keyPressEvent(&input);
        }
        lastGamepadButtons_[i] = pressed[i];
    }
}

void SwitchHomeWidget::refreshControllerCount() {
    // Keep assignments tied to Eden's real state, not a fake demo counter.
    int next = 0;
    auto& state = Common::GetControllerConsoleState();
    {
        std::scoped_lock guard(state.mutex);
        if (state.engine_active) {
            for (const auto& player : state.players) {
                if (!player.leaving) ++next;
            }
        } else {
            // Standard/manual input mode uses the configured emulated players.
            for (const auto& player : Settings::values.players.GetValue()) {
                if (player.connected) ++next;
            }
        }
    }
    next = std::clamp(next, 0, 4);
    if (next != controllers_) {
        controllers_ = next;
        update();
    }
}

void SwitchHomeWidget::loadProfile() {
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("SwitchInterface"), QStringLiteral("Eden"));
    profileName_ = settings.value(QStringLiteral("profile/name"),
                                  QStringLiteral("Player's Page")).toString();
    const QString picturePath = settings.value(QStringLiteral("profile/image")).toString();
    if (!picturePath.isEmpty()) {
        QPixmap picture(picturePath);
        if (!picture.isNull()) {
            profilePicture_ = picture.scaled(144, 144, Qt::KeepAspectRatioByExpanding,
                                             Qt::SmoothTransformation);
        }
    }
}

void SwitchHomeWidget::editProfile() {
    profileSelection_ = 0;
    page_ = Page::Profile;
    update();
}

void SwitchHomeWidget::chooseProfilePhoto() {
    // The requested native file picker is the only OS dialog on this page.
    const QString filename = QFileDialog::getOpenFileName(
        this, QStringLiteral("Choose profile picture"), QString(),
        QStringLiteral("Images (*.png *.jpg *.jpeg *.webp *.bmp)"));
    if (filename.isEmpty()) return;
    const QImage original(filename);
    if (original.isNull()) {
        toast_ = QStringLiteral("Could not open that picture.");
        update();
        return;
    }
    const QString directory = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation);
    if (!QDir().mkpath(directory)) return;
    const QString destination = QDir(directory).filePath(
        QStringLiteral("switch-interface-profile.png"));
    const QImage expanded = original.scaled(
        256, 256, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    const QPoint origin((expanded.width() - 256) / 2,
                        (expanded.height() - 256) / 2);
    if (!expanded.copy(QRect(origin, QSize(256, 256))).save(destination, "PNG")) return;

    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("SwitchInterface"), QStringLiteral("Eden"));
    settings.setValue(QStringLiteral("profile/image"), destination);
    settings.sync();
    profilePicture_ = QPixmap(destination);
    update();
}

void SwitchHomeWidget::renameProfile() {
    // Inline QLineEdit instead of a native-looking Windows input dialog.
    auto* editor = new QLineEdit(this);
    editor->setObjectName(QStringLiteral("switchProfileInlineEdit"));
    editor->setText(profileName_);
    editor->setMaxLength(28);
    const qreal horizontal = width() / CanvasWidth;
    const qreal vertical = height() / CanvasHeight;
    editor->setGeometry(QRectF(480 * horizontal, 300 * vertical,
                               640 * horizontal, 62 * vertical).toRect());
    editor->setStyleSheet(QStringLiteral(
        "QLineEdit { background: #FAFAFA; color: #202025;"
        "border: 3px solid #8078FA; border-radius: 8px;"
        "padding-left: 18px; font-family: Arial; font-size: 21px; }"));
    editor->show();
    editor->setFocus();
    editor->selectAll();
    connect(editor, &QLineEdit::returnPressed, this, [this, editor] {
        const QString candidate = editor->text().trimmed();
        if (!candidate.isEmpty()) {
            profileName_ = candidate;
            QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                               QStringLiteral("SwitchInterface"), QStringLiteral("Eden"));
            settings.setValue(QStringLiteral("profile/name"), profileName_);
            settings.sync();
        }
        editor->deleteLater();
        setFocus();
        update();
    });
}

void SwitchHomeWidget::removeProfilePhoto() {
    profilePicture_ = QPixmap();
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("SwitchInterface"), QStringLiteral("Eden"));
    settings.remove(QStringLiteral("profile/image"));
    settings.sync();
    update();
}
