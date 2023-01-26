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

#include "imageview.h"
#include "main.h"
#include "globals.h"
#include "compositeview.h"
#include "browserwindow.h"
#include "history.h"
#include "tabwidget.h"

#include <QPainter>
#include <QDir>
#include <QFileInfo>
#include <QList>
#include <QKeyEvent>
#include <QDebug>
#include <QClipboard>
#include <QTimer>
#include <QImageReader>
#include <QSqlQuery>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QScreen>
#include <QMessageBox>
#include <QInputDialog>
#include <QProcess>
#include <QWindow>
#include <QRubberBand>
#include <QApplication>
#include <QPixmap>

#define MOUSE_IDLE_TIMEOUT 1500

ImageView::ImageView(QWidget *parent) : QWidget(parent)
{
    composite = qobject_cast<CompositeView*>(parent);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAcceptDrops(true);

    image = nullptr;
    rubberBand = nullptr;
    fullResolution = false;
    fullScreen = false;
    showInfo = false;
    pictureGrabbed = false;
    mouseClipping = false;
    currentItem = 0;

    idleMouseTimer = new QTimer(this);
    connect(idleMouseTimer, &QTimer::timeout, this, &ImageView::hideMouse);
}

void ImageView::showFile(QString file)
{
    QFileInfo fi(file);
    fileName = fi.fileName();
    if(fi.canonicalPath() != path)
    {
        fileList.clear();
        path = fi.canonicalPath();
    }
    composite->iconFileName = QString("%1/%2").arg(path).arg(fileName);
    setWindowTitle(QString("%1").arg(fileName));

    freeImage();

    QImageReader reader;
    reader.setDecideFormatFromContent(true);
    reader.setFileName(file);

    image = new QImage();

    if(reader.read(image) == false || image->isNull())
    {
        QString msg = QString("%1:\n%2\n%3 bytes").arg(reader.errorString()).arg(file).arg(fi.size());
        showError(file, msg + "\n");
        return;
    }

    setWindowTitle(QString("%1 %2x%3").arg(fi.fileName()).arg(image->width()).arg(image->height()));
    if(image->width() == 0 && image->height() == 0)
    {
        showError(file, "Image contains no data.\n\n" + file);
        return;
    }

    clipped = false;
    resizeIdeal();
    if(fullScreen == false)
    {
        adjustWindow = true;
    }

    composite->setStatus("");
    imageInfo = QString("%1x%2 %3").arg(image->width()).arg(image->height()).arg(fileSize(fi.size()));
    QPixmap pixmap = QPixmap::fromImage(*image); //->scaled(32, 32));
    icon = pixmap;
    emit iconChanged(icon);
    update();
}

void ImageView::hideMouse()
{
    idleMouseTimer->stop();
    setCursor(Qt::BlankCursor);
}

void ImageView::freeImage()
{
    if(image != nullptr)
    {
        delete image;
        image = nullptr;
    }

    pictureGrabbed = false;
}

void ImageView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    if(image == nullptr)
    {
        QPainter painter;
        painter.begin(this);
        painter.fillRect(0, 0, width(), height(), QColor(0, 0, 0));
        painter.end();

        if(width() != animationWidth || height() != animationHeight)
        {
            adjustWindow = false;
            if(fullScreen == false)
            {
                resize(animationWidth, animationHeight);
            }
            update();
        }
        return;
    }

    target.setRect(0, 0, width(), height());
    if(adjustWindow && fullResolution == false)
    {
        QSize pic = image->size();

        adjustWindow = false;
        paintWidth = pic.width();
        paintHeight = pic.height();
        resizeIdeal();
    }

    QPainter painter;
    painter.begin(this);
    QColor shade(0, 0, 0, 60);

    target.setRect(0, 0, paintWidth, paintHeight);
    if(paintWidth < width())
    {
        target.setX((width() - paintWidth) / 2);
        target.setWidth(paintWidth);
    }
    if(paintHeight < height() && composite->iconFileName != ":/img/kytka1.svg")
    {
        target.setY((height() - paintHeight) / 2);
        target.setHeight(paintHeight);
    }
    painter.fillRect(0, 0, width(), height(), QColor(0, 0, 0));

    if(fullResolution ||
       (target.width() == source.width() && target.height() == source.height()))
    {
        painter.drawImage(target, *image, source);
    }
    else
    {
        QImage scaled = image->scaled(target.width(), target.height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        painter.drawImage(target, scaled);
    }

    if(showInfo)
    {
        QFont font("Roboto Condensed", 10);
        QFontMetrics fm(font);
        painter.setFont(font);
        QRect textRect;
        int textHeight;
        int textWidth;

        textRect = fm.boundingRect(imageInfo);
        textHeight = textRect.height();
        textWidth = textRect.width() + 12;
        int x = width() - textWidth;
        QRect r(0, textHeight - height(), width(), height());

        painter.fillRect(x, 0, textWidth, textHeight, shade);
        painter.setPen(Qt::white);
        painter.drawText(r, Qt::AlignRight| Qt::AlignBottom, imageInfo);
    }

    painter.end();
}

void ImageView::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if(fullResolution == false)
    {
        resizeIdeal();
    }
}

void ImageView::keyPressEvent(QKeyEvent* event)
{
    QFileInfo fi;
    if(event->modifiers() == Qt::ControlModifier)
    {
        switch(event->key())
        {
            case Qt::Key_Left:
                if(image->width() > width())
                {
                    int d = 10;
                    QPoint delta(d, 0);
                    moveViewSource(delta);
                }
                break;

            case Qt::Key_Right:
                if(image->width() > width())
                {
                    int d = 10;
                    QPoint delta(-d, 0);
                    moveViewSource(delta);
                }
                break;

            case Qt::Key_Up:
                {
                    int d = 10;
                    QPoint delta(0, d);
                    moveViewSource(delta);
                }
                break;

            case Qt::Key_Down:
                {
                    int d = 10;
                    QPoint delta(0, -d);
                    moveViewSource(delta);
                }
                break;
        }
    }
    else if(event->modifiers() == Qt::ShiftModifier)
    {
        switch(event->key())
        {
            case Qt::Key_Left:
                if(image->width() > width())
                {
                    int d = 1;
                    QPoint delta(d, 0);
                    moveViewSource(delta);
                }
                break;

            case Qt::Key_Right:
                if(image->width() > width())
                {
                    int d = 1;
                    QPoint delta(-d, 0);
                    moveViewSource(delta);
                }
                break;

            case Qt::Key_Up:
                {
                    int d = 1;
                    QPoint delta(0, d);
                    moveViewSource(delta);
                }
                break;

            case Qt::Key_Down:
                {
                    int d = 1;
                    QPoint delta(0, -d);
                    moveViewSource(delta);
                }
                break;
        }
    }
    else if(event->modifiers() == Qt::AltModifier || event->key() == Qt::Key_F12)
    {
        switch(event->key())
        {
            case Qt::Key_F12:
            case Qt::Key_Enter:
            case Qt::Key_Return:
                setWindowState(windowState() ^ Qt::WindowFullScreen);
                fullScreen = isFullScreen();
                resizeIdeal();
                update();
                event->ignore();
                return;

            case Qt::Key_Left:
                if(image->width() > width())
                {
                    int d = width() / 4;
                    if(d <= 0)
                    {
                        d = 2;
                    }
                    QPoint delta(d, 0);
                    moveViewSource(delta);
                }
                else
                {
                    movePrevious();
                }
                break;

            case Qt::Key_Right:
                if(image->width() > width())
                {
                    int d = height() / 4;
                    if(d <= 0)
                    {
                        d = 2;
                    }
                    QPoint delta(-d, 0);
                    moveViewSource(delta);
                }
                else
                {
                    moveNext();
                }
                break;

            case Qt::Key_Up:
                {
                    int d = height() / 4;
                    if(d <= 0)
                    {
                        d = 2;
                    }
                    QPoint delta(0, d);
                    moveViewSource(delta);
                }
                break;

            case Qt::Key_Down:
                {
                    int d = height() / 4;
                    if(d <= 0)
                    {
                        d = 2;
                    }
                    QPoint delta(0, -d);
                    moveViewSource(delta);
                }
                break;
        }
    }
    else
    {
        QString text;
        bool ok;

        switch(event->key())
        {
            case Qt::Key_Escape:
                if(mouseClipping)
                {
                    mouseClipping = false;
                    clipped = false;
                    rubberBand->hide();
                }
                break;

            case Qt::Key_F:
                showFullResolution();
                break;

            case Qt::Key_I:
                if(showInfo)
                {
                    showInfo = false;
                }
                else
                {
                    showInfo = true;
                }
                update();
                break;

            case Qt::Key_Space:
            case Qt::Key_PageDown:
                if(fullResolution && image != nullptr && source.y() < image->height() - height())
                {
                    QPoint delta(0, -height());
                    moveViewSource(delta);
                    break;
                }
                else
                {
                    // already at bottom of image. assume user wants to view next image...
                    int oldX = source.x();
                    moveNext();
                    QPoint delta(oldX, 0);
                    moveViewSource(delta);
                }
                break;

            case Qt::Key_Return:
            case Qt::Key_Enter:
                moveNext();
                break;

            case Qt::Key_PageUp:
                if(fullResolution && image != nullptr)
                {
                    int y = source.y();
                    if(y >= height())
                    {
                        QPoint delta(0, height());
                        moveViewSource(delta);
                        break;
                    }
                    else if(y > 0)
                    {
                        QPoint delta(0, source.y());
                        moveViewSource(delta);
                        break;
                    }
                    else if(y == 0)
                    {
                        // already at top of image. assume user wants to view previous image.
                        int oldX = source.x();
                        movePrevious();
                        QSize screen = availableSize();
                        if(oldX + screen.width() > image->width())
                        {
                            oldX = image->width() - screen.width();
                        }
                        source.setRect(oldX, image->height() - height(), source.width(), source.height());
                        update();
                    }
                }
                else
                {
                    movePrevious();
                }
                break;

            case Qt::Key_Left:
                if(fullResolution && image->width() > width())
                {
                    int d = width() / 10;
                    if(d <= 0)
                    {
                        d = 1;
                    }
                    QPoint delta(d, 0);
                    moveViewSource(delta);
                }
                else
                {
                    movePrevious();
                }
                break;

            case Qt::Key_Right:
                if(fullResolution && image->width() > width())
                {
                    int d = height() / 10;
                    if(d <= 0)
                    {
                        d = 1;
                    }
                    QPoint delta(-d, 0);
                    moveViewSource(delta);
                }
                else
                {
                    moveNext();
                }
                break;

            case Qt::Key_Up:
                {
                    int d = height() / 10;
                    if(d <= 0)
                    {
                        d = 1;
                    }
                    QPoint delta(0, d);
                    moveViewSource(delta);
                }
                break;

            case Qt::Key_Down:
                {
                    int d = height() / 10;
                    if(d <= 0)
                    {
                        d = 1;
                    }
                    QPoint delta(0, -d);
                    moveViewSource(delta);
                }
                break;

            case Qt::Key_Home:
                {
                    QSize screen = availableSize();
                    source.setRect(source.x(), 0, source.width(), screen.height());
                    update();
                }
                break;

            case Qt::Key_End:
                {
                    QSize screen = availableSize();
                    source.setRect(source.x(), image->height() - screen.height(), source.width(), screen.height());
                    update();
                }
                break;

            case Qt::Key_R:
            case Qt::Key_F2:
                text = QInputDialog::getText(this, tr("Rename"),
                                               tr("Rename:"), QLineEdit::Normal,
                                               fileName, &ok);
                if(ok && text != fileName)
                {
                    QString oldName = QString("%1/%2").arg(path).arg(fileName);
                    QString newName = QString("%1/%2").arg(path).arg(text);
                    if(QFile::rename(oldName, newName) == false)
                    {
                        QMessageBox::critical(this, tr("Error"), tr("Could not rename file:\n") + oldName + "\nto:\n" + newName);
                    }
                    else
                    {
                        fileName = text;
                        if(fileList.count() && currentItem >= 0 && currentItem < fileList.count())
                        {
                            fileList[currentItem] = newName;
                        }
                        composite->iconFileName = QString("%1/%2").arg(path).arg(fileName);
                        setWindowTitle(QString("%1 %2x%3").arg(fi.fileName()).arg(image->width()).arg(image->height()));
                        QString target = QString("file://%1").arg(newName);
                        emit urlChanged(target);
                        emit titleChanged(windowTitle());
                        History::State* state;
                        state = composite->history->currentState();
                        state->target = target;
                        state->title = windowTitle();
                        update();
                    }
                }
                break;
        }
    }

    QWidget::keyPressEvent(event);
}

void ImageView::mousePressEvent(QMouseEvent* event)
{
    switch(event->button())
    {
        case Qt::LeftButton:
            mousePressPoint = event->pos();
            mousePressSource = source;
            pictureGrabbed = true;
            break;

        case Qt::RightButton:
            moveNext();
            break;

        case Qt::MiddleButton:
            mousePressPoint = event->pos();
            mousePressSource = source;
            pictureGrabbed = true;
            break;

        default:
            break;
    }

    event->accept();
}

void ImageView::mouseMoveEvent(QMouseEvent *event)
{
    if(pictureGrabbed)
    {
        source = mousePressSource;
        QPoint delta = event->pos() - mousePressPoint;
        moveViewSource(delta);
    }

    idleMouseTimer->stop();
    setCursor(Qt::ArrowCursor);
    idleMouseTimer->start(MOUSE_IDLE_TIMEOUT);
    event->accept();
}

void ImageView::mouseReleaseEvent(QMouseEvent* event)
{
    switch(event->button())
    {
        case Qt::LeftButton:
            if(pictureGrabbed)
            {
                source = mousePressSource;
                QPoint delta = event->pos() - mousePressPoint;
                moveViewSource(delta);
                pictureGrabbed = false;
            }
            break;

        case Qt::BackButton:
            composite->back();
            event->accept();
            return;

        case Qt::ForwardButton:
            composite->forward();
            event->accept();
            return;

        case Qt::RightButton:
            break;

        case Qt::MiddleButton:
            if(pictureGrabbed)
            {
                source = mousePressSource;
                QPoint delta = event->pos() - mousePressPoint;
                if(delta == QPoint(0, 0))
                {
                    emit openInNewTab(QString("file://%1/%2").arg(path).arg(fileName));
                }
                else
                {
                    moveViewSource(delta);
                }
                pictureGrabbed = false;
            }
            break;

        default:
            break;
    }

    event->accept();
}

void ImageView::wheelEvent(QWheelEvent* event)
{
    if(fullResolution)
    {
        QPoint numDegrees = event->angleDelta() / 4 ;
        moveViewSource(numDegrees);
        event->accept();
    }
    else
    {
        event->ignore();
    }
}

void ImageView::moveViewSource(QPoint delta)
{
    if(fullResolution == false ||
       (delta.x() == 0 && delta.y() == 0))
    {
        return;
    }

    int amount;
    if(image->height() > height())
    {
        amount = delta.y();
        if(amount < 0)
        {
            // scrolling down
            if(source.y() - amount > image->height() - height())
            {
                source.setRect(source.x(), image->height() - height(), source.width(), source.height());
            }
            else
            {
                source.setRect(source.x(), source.y() - amount, source.width(), source.height());
            }
        }

        if(amount > 0)
        {
            // scrolling up
            if(source.y() - amount < 0)
            {
                QSize screen = availableSize();

                source.setRect(source.x(), 0, source.width(), screen.height());
            }
            else
            {
                source.setRect(source.x(), source.y() - amount, source.width(), source.height());
            }
        }
    }

    if(image->width() > width())
    {
        amount = delta.x();
        if(amount < 0)
        {
            // scrolling left
            if(source.x() - amount > image->width() - width())
            {
                source.setRect(image->width() - width(), source.y(), source.width(), source.height());
            }
            else
            {
                source.setRect(source.x() - amount, source.y(), source.width(), source.height());
            }
        }

        if(amount > 0)
        {
            // scrolling right
            if(source.x() - amount < 0)
            {
                source.setRect(0, source.y(), source.width(), source.height());
            }
            else
            {
                source.setRect(source.x() - amount, source.y(), source.width(), source.height());
            }
        }
    }
    update();
}

QSize ImageView::availableSize()
{
    if(fullScreen)
    {
        return qApp->primaryScreen()->size();
    }

    QSize size = parentWidget()->size();

    return size;
}

void ImageView::resizeIdeal()
{
    if(image == nullptr)
    {
        // might be an animation
        return; // just skip for now
    }

    QSize screen = availableSize();
    if(fullResolution)
    {
        paintWidth = image->width();
        paintHeight = image->height();
        if(paintWidth > screen.width())
        {
            paintWidth = screen.width();
        }

        if(paintHeight > screen.height())
        {
            paintHeight = screen.height();
        }

        source.setRect(0, 0, paintWidth, paintHeight);
    }
    else
    {
        idealWidth = image->width();
        idealHeight = image->height();
        source.setRect(0, 0, idealWidth, idealHeight);

        if(idealWidth > screen.width())
        {
            idealWidth = screen.width();
            idealHeight = (static_cast<float>(idealWidth) / static_cast<float>(image->width())) * static_cast<float>(image->height());
        }

        if(idealHeight > screen.height())
        {
            idealHeight = screen.height();
            idealWidth = (static_cast<float>(idealHeight) / static_cast<float>(image->height())) * static_cast<float>(image->width());
        }

        paintWidth = idealWidth;
        paintHeight = idealHeight;
    }
}

void ImageView::moveNext()
{
    loadImageList();

    if(fileList.count() == 0)
    {
        showError("Error", "No files to show.");
        return;
    }

    currentItem++;
    if(currentItem >= fileList.count())
    {
        currentItem = 0;
        composite->setStatus("Reached end of files, looping back to beginning.");
    }
    QFileInfo fi = fileList.at(currentItem);
    if(fi.size() == 0)
    {
        if(currentItem != 0)
        {
            moveNext();
            return;
        }
        showError(fi.canonicalFilePath(), "Zero length file size.\n\n" + fi.filePath());
    }
    else
    {
        showFile(fi.absoluteFilePath());

        History::State state;
        state.target = QString("file://%1/%2").arg(path).arg(fileName);
        state.title = QString("%1 %2x%3").arg(fileName).arg(image->width()).arg(image->height());
        emit updateState(state);
        emit urlChanged(state.target);
        emit titleChanged(state.title);
    }
}

void ImageView::movePrevious()
{
    loadImageList();

    if(fileList.count() == 0)
    {
        showError("Error", "No files to show.");
        return;
    }

    currentItem--;
    if(currentItem < 0)
    {
        currentItem = fileList.count() - 1;
    }
    QFileInfo fi = fileList.at(currentItem);

    if(fi.size() == 0)
    {
        if(currentItem != 0)
        {
            movePrevious();
            return;
        }
    }
    else
    {
        showFile(fi.absoluteFilePath());

        History::State state;
        state.target = QString("file://%1/%2").arg(path).arg(fileName);
        state.title = QString("%1 %2x%3").arg(fileName).arg(image->width()).arg(image->height());
        emit updateState(state);
        emit urlChanged(state.target);
        emit titleChanged(state.title);
    }
}

void ImageView::showFullResolution()
{
    if(fullResolution)
    {
        fullResolution = false;
    }
    else
    {
        fullResolution = true;
    }
    resizeIdeal();
    update();
}

void ImageView::loadImageList()
{
    if(fileList.count())
    {
        return;
    }

    QStringList filters;
    filters << "*.jpg" << "*.jpeg" << "*.png" << "*.gif" << "*.pbm" << "*.bmp" << "*.svg";
    filters << "*.icns" << "*.jp2" << "*.mng" << "*.tga" << "*.tiff" << "*.wbmp" << "*.webp";
    filters << "*.pgm" << "*.ppm" << "*.xbm" << "*.xpm";

    QDir dir(path, "", QDir::Name | QDir::IgnoreCase, QDir::Files | QDir::NoDotAndDotDot);
    dir.setNameFilters(filters);
    if(dir.exists() == false)
    {
        return;
    }

    int i = 0;
    foreach(QFileInfo fi, dir.entryInfoList())
    {
        fileList.append(fi.canonicalFilePath());
        if(fi.fileName() == fileName)
        {
            currentItem = i;
        }
        i++;
    }
}

void ImageView::showError(QString filePath, QString message)
{
    setWindowTitle(QString("%1").arg(filePath));
    composite->iconFileName = ":/img/kytka1.svg";
    composite->setStatus(message);

    freeImage();
    QImageReader reader;
    reader.setDecideFormatFromContent(true);
    reader.setFileName(":/img/kytka1.svg");

    image = new QImage();

    reader.read(image);

    clipped = false;
    resizeIdeal();
    if(fullScreen == false)
    {
        adjustWindow = true;
    }

    QPixmap pixmap = QPixmap::fromImage(*image);
    icon = pixmap;
    emit iconChanged(icon);
    update();
}
