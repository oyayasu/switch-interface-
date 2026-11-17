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
constexpr int VisibleGames = 6;
const QColor Ink(47, 50, 55);
const QColor Muted(111, 117, 124);
const QColor Blue(0, 168, 240);
const QColor Background(245, 246, 248);
const char* kActions[] = {"Library", "Add games", "Controllers", "Settings", "Power"};

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
    QPen pen(QColor(96, 100, 107), 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    const QPointF center = area.center();
    switch (index) {
    case 0: // Library
        p.drawRoundedRect(area.adjusted(7, 10, -7, -10), 4, 4);
        p.drawLine(QPointF(area.left() + 12, center.y() - 6),
                   QPointF(area.right() - 12, center.y() - 6));
        break;
    case 1: // Add
        p.drawEllipse(area.adjusted(7, 7, -7, -7));
        p.drawLine(QPointF(center.x() - 11, center.y()), QPointF(center.x() + 11, center.y()));
        p.drawLine(QPointF(center.x(), center.y() - 11), QPointF(center.x(), center.y() + 11));
        break;
    case 2: // Controllers
        p.drawRoundedRect(area.adjusted(5, 15, -5, -15), 8, 8);
        p.drawLine(QPointF(area.left() + 15, center.y()), QPointF(area.left() + 31, center.y()));
        p.drawLine(QPointF(area.left() + 23, center.y() - 8), QPointF(area.left() + 23, center.y() + 8));
        p.setBrush(QColor(96, 100, 107));
        p.drawEllipse(QPointF(area.right() - 22, center.y() - 5), 3, 3);
        p.drawEllipse(QPointF(area.right() - 14, center.y() + 5), 3, 3);
        break;
    case 3: // Settings
        p.drawEllipse(area.adjusted(9, 9, -9, -9));
        p.drawEllipse(area.adjusted(21, 21, -21, -21));
        for (int i = 0; i < 8; ++i) {
            const qreal angle = i * 3.141592653589793 / 4;
            const QPointF a(center.x() + 20 * std::cos(angle), center.y() + 20 * std::sin(angle));
            const QPointF b(center.x() + 28 * std::cos(angle), center.y() + 28 * std::sin(angle));
            p.drawLine(a, b);
        }
        break;
    case 4: // Power
        p.drawArc(area.adjusted(11, 11, -11, -11), 45 * 16, 270 * 16);
        p.drawLine(QPointF(center.x(), area.top() + 5), QPointF(center.x(), center.y() + 5));
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

    label(p, QRectF(58, 41, 400, 45), QStringLiteral("Home"), 28, Ink,
          Qt::AlignVCenter | Qt::AlignLeft, true);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(240, 91, 77));
    p.drawEllipse(QRectF(1042, 46, 36, 36));
    label(p, QRectF(1042, 46, 36, 36), QStringLiteral("1"), 17, Qt::white, Qt::AlignCenter, true);
    label(p, QRectF(1100, 46, 125, 36),
          QDateTime::currentDateTime().toString(QStringLiteral("HH:mm")), 23, Ink,
          Qt::AlignVCenter | Qt::AlignRight, true);

    gameTargets_.clear();
    if (games_.isEmpty()) {
        label(p, QRectF(70, 175, 1140, 55), QStringLiteral("No games in the library"), 29, Ink, Qt::AlignCenter, true);
        label(p, QRectF(70, 233, 1140, 36),
              QStringLiteral("Add a game directory using the + button below"), 18, Muted);
    } else {
        if (selectedGame_ < firstVisible_) firstVisible_ = selectedGame_;
        if (selectedGame_ >= firstVisible_ + VisibleGames)
            firstVisible_ = selectedGame_ - VisibleGames + 1;
        firstVisible_ = std::clamp(firstVisible_, 0, std::max(0, static_cast<int>(games_.size()) - VisibleGames));

        const Game& current = games_[selectedGame_];
        label(p, QRectF(64, 123, 1145, 55), current.title, 31, Ink,
              Qt::AlignVCenter | Qt::AlignLeft, true);
        const int visible = std::min(VisibleGames, static_cast<int>(games_.size()) - firstVisible_);
        for (int slot = 0; slot < visible; ++slot) {
            const int index = slot + firstVisible_;
            const bool active = selectedGame_ == index && !toolbarFocused_;
            const QRectF imageRect(62 + slot * 195, 230, 175, 175);
            gameTargets_.append(imageRect);
            p.setPen(active ? QPen(Blue, 5) : QPen(QColor(219, 222, 225), 1));
            p.setBrush(Qt::white);
            p.drawRoundedRect(imageRect.adjusted(active ? -5 : 0, active ? -5 : 0,
                                                  active ? 5 : 0, active ? 5 : 0), 4, 4);
            if (!games_[index].icon.isNull()) {
                p.drawPixmap(imageRect.adjusted(3, 3, -3, -3).toRect(),
                             games_[index].icon);
            } else {
                p.fillRect(imageRect.adjusted(3, 3, -3, -3), QColor(224, 231, 236));
                label(p, imageRect.adjusted(16, 48, -16, -48),
                      games_[index].title.left(2).toUpper(), 40, QColor(110, 125, 140),
                      Qt::AlignCenter, true);
            }
            label(p, QRectF(imageRect.left(), 419, 175, 43), games_[index].title, 15,
                  active ? Ink : Muted, Qt::AlignTop | Qt::AlignHCenter, active);
        }
        if (firstVisible_ > 0)
            label(p, QRectF(19, 288, 30, 45), QStringLiteral("‹"), 42, Muted);
        if (firstVisible_ + visible < games_.size())
            label(p, QRectF(1225, 288, 30, 45), QStringLiteral("›"), 42, Muted);
    }

    p.setPen(QPen(QColor(223, 226, 230), 1));
    p.drawLine(QPointF(58, 501), QPointF(1220, 501));
    toolbarTargets_.clear();
    const qreal spacing = 119;
    const qreal startX = (CanvasWidth - spacing * 4) / 2 - 39;
    for (int i = 0; i < 5; ++i) {
        const QRectF area(startX + i * spacing, 524, 78, 78);
        toolbarTargets_.append(area);
        if (toolbarFocused_ && selectedToolbar_ == i) {
            p.setBrush(Qt::white);
            p.setPen(QPen(Blue, 4));
            p.drawEllipse(area.adjusted(-3, -3, 3, 3));
        } else {
            p.setBrush(Qt::white);
            p.setPen(QPen(QColor(228, 230, 233), 1));
            p.drawEllipse(area);
        }
        drawToolbarSymbol(p, i, area);
        label(p, QRectF(area.left() - 30, 606, 138, 20),
              QString::fromLatin1(kActions[i]), 13, Muted);
    }
    p.setPen(QPen(QColor(218, 222, 225), 1));
    p.drawLine(QPointF(58, 652), QPointF(1220, 652));
    label(p, QRectF(930, 661, 280, 34), QStringLiteral("A  Select      B  Back"),
          16, Ink, Qt::AlignRight | Qt::AlignVCenter);
}

void SwitchHomeWidget::moveSelection(int distance) {
    if (toolbarFocused_) {
        selectedToolbar_ = std::clamp(selectedToolbar_ + distance, 0, 4);
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
    case 0: if (onNativeLibrary) onNativeLibrary(); break;
    case 1: if (onAddDirectory) onAddDirectory(); break;
    case 2: if (onControllers) onControllers(); break;
    case 3: if (onSettings) onSettings(); break;
    case 4: if (onClose) onClose(); break;
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
