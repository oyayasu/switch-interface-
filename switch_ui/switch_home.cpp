// SPDX-License-Identifier: GPL-3.0-or-later
#include "switch_home.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <QAbstractItemModel>
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
const QColor Background(236, 236, 239);
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
    reload();
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
    QColor ink(104, 108, 116);
    if (index == 0) ink = QColor(214, 12, 43);
    if (index == 1) ink = QColor(222, 154, 54);
    if (index == 2) ink = QColor(84, 184, 67);
    if (index == 3) ink = QColor(206, 54, 82);
    if (index == 4) ink = QColor(67, 127, 202);
    if (index == 5) ink = QColor(47, 172, 165);
    p.setPen(QPen(ink, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    switch (index) {
    case 0:
        p.setPen(Qt::NoPen);
        p.setBrush(ink);
        p.drawEllipse(area.adjusted(8, 8, -8, -8));
        label(p, area.adjusted(5, 5, -5, -5), QStringLiteral("ONLINE"),
              8, Qt::white, Qt::AlignCenter, true);
        break;
    case 1:
        p.drawRoundedRect(area.adjusted(13, 15, -13, -15), 3, 3);
        label(p, area.adjusted(13, 15, -13, -15),
              QStringLiteral("C"), 29, ink, Qt::AlignCenter, true);
        break;
    case 2:
        p.drawRoundedRect(area.adjusted(17, 11, -17, -11), 2, 2);
        p.drawLine(QPointF(c.x() - 8, c.y() - 8), QPointF(c.x() + 8, c.y() - 8));
        p.drawLine(QPointF(c.x() - 8, c.y()), QPointF(c.x() + 8, c.y()));
        p.drawLine(QPointF(c.x() - 8, c.y() + 8), QPointF(c.x() + 5, c.y() + 8));
        break;
    case 3:
        p.drawRect(area.adjusted(17, 24, -17, -15));
        p.drawArc(QRectF(c.x() - 9, c.y() - 20, 18, 22), 0, 180 * 16);
        break;
    case 4:
        p.drawRoundedRect(area.adjusted(12, 18, -12, -17), 3, 3);
        p.drawRoundedRect(area.adjusted(21, 26, -21, -24), 2, 2);
        break;
    case 5:
        p.drawRoundedRect(area.adjusted(14, 20, -14, -14), 3, 3);
        p.drawLine(QPointF(c.x() - 12, c.y() - 10), QPointF(c.x() + 12, c.y() - 10));
        p.drawLine(QPointF(c.x() - 5, c.y() - 22), QPointF(c.x() + 5, c.y() - 22));
        break;
    case 6:
        p.drawRoundedRect(area.adjusted(9, 23, -9, -23), 8, 8);
        p.drawLine(QPointF(c.x() - 19, c.y()), QPointF(c.x() - 5, c.y()));
        p.drawLine(QPointF(c.x() - 12, c.y() - 7), QPointF(c.x() - 12, c.y() + 7));
        p.setBrush(ink);
        p.drawEllipse(QPointF(c.x() + 10, c.y() - 4), 2.5, 2.5);
        p.drawEllipse(QPointF(c.x() + 19, c.y() + 4), 2.5, 2.5);
        break;
    case 7:
        p.drawRoundedRect(area.adjusted(17, 12, -17, -12), 3, 3);
        p.drawLine(QPointF(c.x() - 10, c.y() + 15), QPointF(c.x() + 10, c.y() + 15));
        break;
    case 8:
        p.drawEllipse(area.adjusted(18, 18, -18, -18));
        p.drawEllipse(area.adjusted(27, 27, -27, -27));
        for (int i = 0; i < 8; ++i) {
            const qreal angle = i * 3.141592653589793 / 4;
            p.drawLine(QPointF(c.x() + 19 * std::cos(angle), c.y() + 19 * std::sin(angle)),
                       QPointF(c.x() + 27 * std::cos(angle), c.y() + 27 * std::sin(angle)));
        }
        break;
    case 9:
        p.drawArc(area.adjusted(17, 17, -17, -17), 45 * 16, 270 * 16);
        p.drawLine(QPointF(c.x(), c.y() - 23), QPointF(c.x(), c.y() + 2));
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

    // System profile + clock occupy the same positions as the Switch 2 Home reference.
    p.setBrush(QColor(37, 88, 203));
    p.setPen(QPen(Qt::white, 3));
    p.drawEllipse(QRectF(49, 46, 56, 56));
    label(p, QRectF(49, 46, 56, 56), QStringLiteral("P1"), 18, Qt::white, Qt::AlignCenter, true);
    label(p, QRectF(987, 47, 195, 48),
          QDateTime::currentDateTime().toString(QStringLiteral("h:mm AP")), 22, Ink,
          Qt::AlignRight | Qt::AlignVCenter, true);
    // Compact, illustrative wireless indicator; no fabricated battery percentage.
    p.setPen(QPen(QColor(43, 47, 52), 3));
    p.drawArc(QRectF(1192, 61, 30, 22), 25 * 16, 130 * 16);
    p.setBrush(QColor(43, 47, 52));
    p.drawEllipse(QPointF(1207, 83), 2.5, 2.5);

    gameTargets_.clear();
    if (games_.isEmpty()) {
        label(p, QRectF(100, 191, 1080, 60), QStringLiteral("No games available"),
              31, Ink, Qt::AlignCenter, true);
        label(p, QRectF(100, 258, 1080, 36),
              QStringLiteral("Select Add games below to choose a folder."),
              19, Muted);
    } else {
        if (selectedGame_ < firstVisible_) firstVisible_ = selectedGame_;
        if (selectedGame_ >= firstVisible_ + VisibleGames)
            firstVisible_ = selectedGame_ - VisibleGames + 1;
        firstVisible_ = std::clamp(
            firstVisible_, 0, std::max(0, static_cast<int>(games_.size()) - VisibleGames));
        if (!toolbarFocused_) {
            label(p, QRectF(20, 142, 1140, 38), games_[selectedGame_].title,
                  23, QColor(89, 110, 170), Qt::AlignVCenter | Qt::AlignLeft);
        }
        const int visible = std::min(
            VisibleGames, static_cast<int>(games_.size()) - firstVisible_);
        for (int slot = 0; slot < visible; ++slot) {
            const int index = slot + firstVisible_;
            const bool active = selectedGame_ == index && !toolbarFocused_;
            const QRectF imageRect(103 + slot * 270, 201, 254, 254);
            gameTargets_.append(imageRect);
            p.setPen(active ? QPen(Blue, 5) : QPen(QColor(217, 219, 223), 1));
            p.setBrush(QColor(248, 248, 250));
            p.drawRoundedRect(imageRect.adjusted(active ? -4 : 0, active ? -4 : 0,
                                                  active ? 4 : 0, active ? 4 : 0), 8, 8);
            const QRectF body = imageRect.adjusted(3, 3, -3, -3);
            QPainterPath rounded;
            rounded.addRoundedRect(body, 7, 7);
            p.save();
            p.setClipPath(rounded);
            if (!games_[index].icon.isNull()) {
                p.drawPixmap(body.toRect(), games_[index].icon);
            } else {
                p.fillRect(body, QColor(220, 229, 236));
                label(p, body.adjusted(16, 75, -16, -75),
                      games_[index].title.left(2).toUpper(), 54,
                      QColor(108, 124, 143), Qt::AlignCenter, true);
            }
            p.restore();
        }
    }

    // Bottom navigation capsule.
    const QRectF toolbarBg(221, 516, 843, 94);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(248, 248, 249));
    p.drawRoundedRect(toolbarBg, 47, 47);
    toolbarTargets_.clear();
    for (int i = 0; i < 10; ++i) {
        const QRectF area(230 + i * 83, 529, 70, 70);
        toolbarTargets_.append(area);
        if (toolbarFocused_ && selectedToolbar_ == i) {
            p.setPen(QPen(QColor(176, 101, 207), 3));
            p.setBrush(QColor(251, 251, 252));
            p.drawEllipse(area.adjusted(-3, -3, 3, 3));
        }
        drawToolbarSymbol(p, i, area);
    }
    if (toolbarFocused_) {
        label(p, QRectF(375, 611, 530, 34),
              QString::fromLatin1(kActions[selectedToolbar_]),
              23, QColor(65, 120, 192));
    }
    if (!toast_.isEmpty()) {
        label(p, QRectF(355, 640, 570, 35), toast_, 17,
              QColor(80, 90, 100), Qt::AlignCenter);
    }
    // Small controller status illustration, as seen at lower left in the reference.
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(30, 32, 35));
    p.drawRoundedRect(QRectF(66, 651, 22, 43), 9, 9);
    p.drawRoundedRect(QRectF(95, 651, 22, 43), 9, 9);
    label(p, QRectF(1055, 660, 180, 35),
          toolbarFocused_ ? QStringLiteral("B  Back      A  OK")
                          : QStringLiteral("A  OK"),
          20, Ink, Qt::AlignRight | Qt::AlignVCenter);
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
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
        toast_ = QStringLiteral("This console-only feature is not available in Eden.");
        QTimer::singleShot(2200, this, [this] { toast_.clear(); update(); });
        update();
        break;
    case 5: if (onAddDirectory) onAddDirectory(); break;
    case 6: if (onControllers) onControllers(); break;
    case 7: if (onNativeLibrary) onNativeLibrary(); break;
    case 8: if (onSettings) onSettings(); break;
    case 9: if (onClose) onClose(); break;
    }
}

void SwitchHomeWidget::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
    case Qt::Key_Left: moveSelection(-1); return;
    case Qt::Key_Right: moveSelection(1); return;
    case Qt::Key_Up: toolbarFocused_ = false; update(); return;
    case Qt::Key_Down: toolbarFocused_ = true; update(); return;
    case Qt::Key_Enter:
    case Qt::Key_Return:
    case Qt::Key_Space: activate(); return;
    case Qt::Key_Escape: toolbarFocused_ = false; update(); return;
    default: QWidget::keyPressEvent(event); return;
    }
}

void SwitchHomeWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;
    const QPointF pos(event->position().x() * CanvasWidth / width(),
                      event->position().y() * CanvasHeight / height());
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
