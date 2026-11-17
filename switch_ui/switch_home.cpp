// SPDX-License-Identifier: GPL-3.0-or-later
#include "switch_home.h"

#include <algorithm>
#include <mutex>
#include <cmath>
#include <functional>
#include <QAbstractItemModel>
#include <QConicalGradient>
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
        showUnavailable();
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
    case Qt::Key_P: editProfile(); return;
    default: QWidget::keyPressEvent(event); return;
    }
}

void SwitchHomeWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;
    const QPointF pos(event->position().x() * CanvasWidth / width(),
                      event->position().y() * CanvasHeight / height());
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
    QMenu popup(this);
    QAction* photo = popup.addAction(QStringLiteral("Change profile picture..."));
    QAction* rename = popup.addAction(QStringLiteral("Rename profile..."));
    QAction* clear = popup.addAction(QStringLiteral("Remove profile picture"));
    QAction* action = popup.exec(QCursor::pos());
    if (action == nullptr) return;

    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("SwitchInterface"), QStringLiteral("Eden"));
    if (action == photo) {
        const QString filename = QFileDialog::getOpenFileName(
            this, QStringLiteral("Choose a profile picture"), QString(),
            QStringLiteral("Images (*.png *.jpg *.jpeg *.webp *.bmp)"));
        if (filename.isEmpty()) return;
        const QImage input(filename);
        if (input.isNull()) {
            showUnavailable();
            return;
        }
        // Save a copy under the user's own configuration directory so the source
        // image may be moved without breaking the profile across Eden updates.
        const QString directory = QStandardPaths::writableLocation(
            QStandardPaths::AppDataLocation);
        if (!QDir().mkpath(directory)) return;
        const QString destination = QDir(directory).filePath(
            QStringLiteral("switch-interface-profile.png"));
        const QImage square = input.scaled(256, 256,
                                          Qt::KeepAspectRatioByExpanding,
                                          Qt::SmoothTransformation);
        const QPoint origin((square.width() - 256) / 2,
                            (square.height() - 256) / 2);
        if (!square.copy(QRect(origin, QSize(256, 256))).save(destination, "PNG")) return;
        settings.setValue(QStringLiteral("profile/image"), destination);
        profilePicture_ = QPixmap(destination);
    } else if (action == rename) {
        bool ok = false;
        const QString proposed = QInputDialog::getText(
            this, QStringLiteral("Profile name"), QStringLiteral("Name:"),
            QLineEdit::Normal, profileName_, &ok).trimmed();
        if (ok && !proposed.isEmpty()) {
            profileName_ = proposed.left(28);
            settings.setValue(QStringLiteral("profile/name"), profileName_);
        }
    } else if (action == clear) {
        profilePicture_ = QPixmap();
        settings.remove(QStringLiteral("profile/image"));
    }
    settings.sync();
    update();
}
