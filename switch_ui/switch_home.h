// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <functional>
#include <QPixmap>
#include <QRectF>
#include <QString>
#include <QVector>
#include <QWidget>

class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QPainter;
class QStandardItemModel;

// Switch 2-style home shell embedded inside Eden's existing Qt main window.
// Game titles, covers, paths and boot action remain owned by Eden.
class SwitchHomeWidget final : public QWidget {
public:
    explicit SwitchHomeWidget(QStandardItemModel* model, QWidget* parent = nullptr);

    std::function<void(const QString&)> onLaunch;
    std::function<void()> onNativeLibrary;
    std::function<void()> onAddDirectory;
    std::function<void()> onSettings;
    std::function<void()> onControllers;
    std::function<void()> onClose;

protected:
    void paintEvent(QPaintEvent*) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    struct Game {
        QString title;
        QString path;
        QPixmap icon;
    };

    void reload();
    void activate();
    void moveSelection(int distance);
    void editProfile();
    void loadProfile();
    void refreshControllerCount();
    void showUnavailable();
    void drawToolbarSymbol(QPainter& painter, int index, const QRectF& target) const;

    QStandardItemModel* model_ = nullptr;
    QVector<Game> games_;
    QVector<QRectF> gameTargets_;
    QVector<QRectF> toolbarTargets_;
    QPixmap profilePicture_;
    QString profileName_ = QStringLiteral("Player's Page");
    QString toast_;
    int selectedGame_ = 0;
    int selectedToolbar_ = 0;
    int firstVisible_ = 0;
    int controllers_ = 0;
    int gradientTick_ = 0;
    bool toolbarFocused_ = false;
    QRectF avatarTarget_{40, 35, 90, 104};
};
