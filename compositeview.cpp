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

#include "compositeview.h"
#include "browserview.h"
#include "imageview.h"

#include <QLayout>
#include <QClipboard>
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileInfo>
#include <QLabel>
#include <QScrollBar>
#include <QDir>

CompositeView::CompositeView(QWidget* parent) : QWidget(parent)
{
    browserView = nullptr;
    imageView = nullptr;
    delayLoading = false;

    QBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    statusLabel = new QLabel(this);
    statusLabel->setStyleSheet(R"EOF(
padding-left: 1px;
padding-right: 1px;
background-color: rgb(57, 57, 57);
border-top-right-radius: 8px;
border-top-left-radius: 8px;
)EOF");
    statusLabel->setVisible(false);

    history = new History(this);
    connect(history, &History::hovered, this, &CompositeView::setStatus);
}

CompositeView::~CompositeView()
{
    delete history;
}

QString CompositeView::url()
{
    if(delayLoading)
    {
        return delayState.target;
    }

    return history->currentTarget();
}

QString CompositeView::title() const
{
    if(delayLoading)
    {
        return delayState.title;
    }

    return history->currentTitle();
}

void CompositeView::navigateTo(QString text, bool changeHistory, bool feelingLucky)
{
    if(text.isEmpty())
    {
        browser()->viewAbout();
        return;
    }

    if(text.startsWith("clip:"))
    {
       QClipboard* clip = qApp->clipboard();
       clip->setText(text.mid(5));
       setStatus(QString("'%1' copied to the clipboard.").arg(text.mid(5)));
       return;
    }

    if((text.startsWith("~/") || text.count('/') > 1) && text.contains(':') == false)
    {
        if(text.startsWith("~/"))
        {
            text = QDir::homePath() + text.mid(1);
        }
        QFileInfo fi(text);
        if(fi.exists())
        {
            text.prepend("file:");
        }
    }

    if(text.startsWith("file:"))
    {
        QString fileType = text.mid(text.lastIndexOf('.') + 1);
        QStringList imageFormats;
        imageFormats << "jpg" << "jpeg" << "png" << "gif" << "pbm" << "bmp" << "pgm" << "ppm" << "xbm" << "xpm" << "svg";
        imageFormats << "icns" << "jp2" << "mng" << "tga" << "tiff" << "wbmp" << "webp";
        if(imageFormats.contains(fileType, Qt::CaseInsensitive))
        {
            QUrl url(text);
            History::State state;
            state.pos = scrollPosition();

            ImageView* view = image();
            view->showFile(url.toLocalFile());
            if(changeHistory)
            {
                state.target = text;
                state.title = view->windowTitle();
                history->appendHistory(state);
            }
            emit urlChanged(url);
            delayLoading = false;
            return;
        }
    }

    browser()->navigateTo(text, changeHistory, feelingLucky);
}

void CompositeView::jumpTo(const History::State& s)
{
    if(browserView != nullptr)
    {
        browserView->jumpTo(s);
    }
    else if(imageView != nullptr)
    {
    }
}

void CompositeView::setUrl(const QUrl& url)
{
    if(browserView != nullptr)
    {
        browserView->setUrl(url);
    }
    else if(imageView != nullptr)
    {
    }
}

void CompositeView::error(QString text)
{
    if(browserView != nullptr)
    {
        browserView->error(text);
    }
    else if(imageView != nullptr)
    {
    }
}

void CompositeView::reload(bool hardReload)
{
    if(browserView != nullptr)
    {
        browserView->reload(hardReload);
    }
    else if(imageView != nullptr)
    {
    }
}

void CompositeView::reloadingDatabase()
{
    if(browserView == nullptr)
    {
        browser()->reloadingDatabase();
    }
    else
    {
        browserView->reloadingDatabase();
    }
}

void CompositeView::applyDelayedScroll()
{
    if(delayScrolling)
    {
        setScrollPosition(delayState.pos);
        delayScrolling = false;
    }
}

void CompositeView::stateChanged(const History::State& s)
{
    saveScrollPosition();
    delayScroll(s.pos);
    navigateTo(s.target, false);
    setFocus();
    emit urlChanged(s.target);
}

BrowserView* CompositeView::browser()
{
    if(browserView == nullptr)
    {
        browserView = new BrowserView(this);
        layout()->addWidget(browserView);
        statusLabel->setParent(browserView);
        setFocusProxy(browserView);
        connect(browserView, &BrowserView::updateState, history, &History::updateState);
        connect(browserView, &BrowserView::appendHistory, history, &History::appendHistory);
        connect(browserView, &BrowserView::urlChanged, this, &CompositeView::urlChanged);
        connect(browserView, &BrowserView::titleChanged, this, &CompositeView::titleChanged);
        connect(browserView, &BrowserView::loadProgress, this, &CompositeView::loadProgress);
        connect(browserView, &BrowserView::loadFinished, this, &CompositeView::applyDelayedScroll, Qt::QueuedConnection);
        connect(browserView, &BrowserView::openInNewTab, this, &CompositeView::openInNewTab);

        discardView(imageView);
        imageView = nullptr;
    }

    return browserView;
}

ImageView* CompositeView::image()
{
    if(imageView == nullptr)
    {
        imageView = new ImageView(this);
        layout()->addWidget(imageView);
        statusLabel->setParent(imageView);
        setFocusProxy(imageView);
        connect(imageView, &ImageView::updateState, history, &History::updateState);
        connect(imageView, &ImageView::urlChanged, this, &CompositeView::urlChanged);
        connect(imageView, &ImageView::titleChanged, this, &CompositeView::titleChanged);
        connect(imageView, &ImageView::openInNewTab, this, &CompositeView::openInNewTab);
        connect(imageView, &ImageView::iconChanged, this, &CompositeView::iconChanged);

        discardView(browserView);
        browserView = nullptr;
    }
    return imageView;
}

void CompositeView::discardView(QWidget* view)
{
    if(view != nullptr)
    {
        layout()->removeWidget(view);
        view->deleteLater();
    }
}

void CompositeView::resizeStatusBar()
{
    QString text = statusLabel->text();
    QFont font = statusLabel->font();
    font = QFont("Roboto", 13);

    QFontMetrics fm(font);
    QRect rect = fm.boundingRect(text);
    int pad = 14;
    int maxWidth = width() - pad;
    if(browserView != nullptr)
    {
        maxWidth -= browserView->verticalScrollBar()->width();
    }

    int lineHeight = rect.height();
    if(rect.width() > maxWidth)
    {
        rect.setHeight(lineHeight * (rect.width() / maxWidth + 1));
        rect.setWidth(maxWidth);
        rect = fm.boundingRect(rect, Qt::TextWordWrap, text);
        statusLabel->setWordWrap(true);
    }
    else
    {
        statusLabel->setWordWrap(false);
    }

    int lines = text.count("\n") + 1;
    rect.setHeight(rect.height() * lines + 5);
    int y = height() - rect.height();

    if(browserView != nullptr)
    {
        QScrollBar *sb = browserView->horizontalScrollBar();
        if(sb != nullptr)
        {
            if(sb->isVisible())
            {
                y -= sb->height();
            }
        }
    }

    statusLabel->setGeometry(0, y, rect.width() + pad, rect.height());
}

QIcon CompositeView::icon()
{
    if(imageView != nullptr)
    {
        return imageView->icon;
    }

    if(siteIcon.isNull())
    {
        siteIcon = QIcon(":/img/page.svg");
    }

    return siteIcon;
}

void CompositeView::setIcon(QString fileName)
{
    QIcon icon(fileName);
    iconFileName = fileName;

    if(icon.isNull())
    {
        siteIcon = QIcon(":/img/page.svg");
    }
    else
    {
        siteIcon = icon;
    }
    emit iconChanged(siteIcon);
}

void CompositeView::delayLoad(QString theUrl, QString theTitle, int scrollX, int scrollY)
{
    delayState.target = theUrl;
    delayState.title = theTitle;
    delayState.pos = QPoint(scrollX, scrollY);
    delayLoading = true;
    delayScrolling = true;
}

void CompositeView::delayScroll(QPoint pos)
{
    delayState.pos = pos;
    delayScrolling = true;
}

void CompositeView::load()
{
    if(delayLoading)
    {
        delayLoading = false;
        navigateTo(delayState.target);
        if(delayScrolling)
        {
            qApp->processEvents();
            delayScrolling = false;
            setScrollPosition(delayState.pos);
        }
    }

}

QPoint CompositeView::scrollPosition()
{
    if(delayScrolling)
    {
        return delayState.pos;
    }

    if(browserView != nullptr)
    {
        return browserView->scrollPosition();
    }

    return QPoint(0, 0);
}

void CompositeView::setScrollPosition(const QPoint& pos)
{
    if(browserView != nullptr)
    {
        browserView->setScrollPosition(pos);
    }
}

void CompositeView::saveScrollPosition()
{
    if(browserView != nullptr)
    {
        browserView->saveScrollPosition();
    }
}

History::State* CompositeView::currentState()
{
    if(delayLoading)
    {
        return &delayState;
    }

    return history->currentState();
}

void CompositeView::setText(const QString& text)
{
    if(browserView != nullptr)
    {
        browserView->setText(text);
    }
}

void CompositeView::clear()
{
    if(browserView != nullptr)
    {
        browserView->clear();
    }
}

void CompositeView::setTitle(QString newTitle)
{
    History::State* state = currentState();

    if(state == nullptr || state->title == newTitle)
    {
        return;
    }

    state->title = newTitle;
    emit titleChanged(newTitle);
}

QString CompositeView::status()
{
    return statusLabel->text();
}

void CompositeView::setStatus(QString text)
{
    if(text != statusLabel->text())
    {
        if(text.isEmpty())
        {
            statusLabel->setVisible(false);
            statusLabel->clear();
            return;
        }

        statusLabel->setText(text);
        resizeStatusBar();
        statusLabel->setVisible(true);
    }
}

void CompositeView::forward()
{
    saveScrollPosition();
    history->forward();
}

void CompositeView::back()
{
    saveScrollPosition();
    history->back();
}

void CompositeView::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    resizeStatusBar();
}
