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

// A native Qt home surface. The emulator's game model and boot path remain authoritative.
// No emulator-owned files, keys, saves, or configurations are modified.
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
    void drawToolbarSymbol(QPainter& painter, int index, const QRectF& target) const;

    QStandardItemModel* model_ = nullptr;
    QVector<Game> games_;
    QVector<QRectF> gameTargets_;
    QVector<QRectF> toolbarTargets_;
    int selectedGame_ = 0;
    int selectedToolbar_ = 0;
    bool toolbarFocused_ = false;
    QString toast_;
    int firstVisible_ = 0;
};
