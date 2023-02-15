// Copyright (c) 2023, K9spud LLC.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

#ifndef IMAGEVIEW_H
#define IMAGEVIEW_H

#include "history.h"

#include <QWidget>
#include <QIcon>

class QRubberBand;
class QImage;
class QTimer;
class CompositeView;
class ImageView : public QWidget
{
    Q_OBJECT
public:
    explicit ImageView(QWidget *parent = nullptr);

    CompositeView* composite;

    void open(QString path);
    void showFile(QString file);

    QString fileName;
    QString path;
    int currentItem;
    QIcon icon;

public slots:
    void hideMouse();
    void moveNext();
    void movePrevious();
    void showFullResolution();

signals:
    void urlChanged(const QUrl& url);
    void titleChanged(const QString& title);
    void updateState(const History::State& state);
    void iconChanged(const QIcon &icon);
    void openInNewTab(const QString& url);

protected:
    virtual void paintEvent(QPaintEvent *event) override;
    virtual void resizeEvent(QResizeEvent* event) override;

    virtual void keyPressEvent(QKeyEvent *event) override;

    virtual void mouseMoveEvent(QMouseEvent *event) override;
    virtual void mousePressEvent(QMouseEvent *event) override;
    virtual void mouseReleaseEvent(QMouseEvent *event) override;
    virtual void wheelEvent(class QWheelEvent* event) override;

    QSize availableSize();
    void resizeIdeal();
    void moveViewSource(QPoint delta);
    void loadImageList();

    void showError(QString filePath, QString message = "");

    QImage* image;
    QString imageInfo;
    void freeImage(void);

    QTimer* idleMouseTimer;

    bool showInfo;
    bool adjustWindow;
    bool fullResolution;
    bool fullScreen;
    int idealWidth;
    int idealHeight;
    int paintWidth;
    int paintHeight;

    int animationWidth;
    int animationHeight;

    QRect source;   // source image to view rectangle (coordinates within image)
    QRect target;   // output rectangle (coordinates within window frame)
    QRect clipRect; // mouse clipping rubber band box rectangle (coordinates within window frame)
    bool clipped;
    bool mouseClipping;
    QRubberBand* rubberBand;
    QPoint mousePressPoint;
    QRect mousePressSource;
    bool pictureGrabbed;

    QStringList fileList;

};

#endif // IMAGEVIEW_H
