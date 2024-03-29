// Copyright (c) 2021-2023, K9spud LLC.
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

#ifndef BROWSERVIEW_H
#define BROWSERVIEW_H

#include "history.h"

#include <QTextEdit>
#include <QStringList>

#include <QHash>
#include <QVector>
#include <QVariant>
#include <QRegularExpression>
#include <QIcon>
#include <QPointF>
#include <QTimer>
#include <QProcess>

class QLabel;
class QMenu;
class QContextMenuEvent;
class QSqlQuery;
class BrowserWindow;
class CompositeView;
class BrowserView : public QTextEdit
{
    Q_OBJECT
public:
    explicit BrowserView(QWidget *parent = nullptr);

    QString currentUrl;
    QPoint scrollPosition();
    void setScrollPosition(const QPoint& pos);
    QPoint saveScrollPosition();

    CompositeView* composite;

    bool find(const QString &text, QTextDocument::FindFlags options = QTextDocument::FindFlags());

    QString appNoVersion(QString app);
    QString appVersion(QString app);

    void contextMenuUninstallLink(QMenu* menu, QString urlPath);
    void contextMenuInstallLink(QMenu* menu, QString urlPath);
    void contextMenuAppLink(QMenu* menu, QString urlPath);
    void contextMenuAppWhitespace(QMenu* menu);

signals:
    void urlChanged(const QUrl& url);
    void titleChanged(const QString& title);
    void loadProgress(int percent);
    void loadFinished();
    void openInNewTab(const QString& url);

    void appendHistory(const History::State& state);
    void updateState(const History::State& state);

public slots:
    void navigateTo(QString text, bool changeHistory = true, bool feelingLucky = false);
    void jumpTo(const History::State& s);
    void setUrl(const QUrl& url);
    void error(QString text);
    void reload(bool hardReload = true);
    void stop();

    void viewAbout();
    void reloadingDatabase();
    void viewFile(QString fileName);
    void viewFolder(QString folderPath);
    void viewApp(const QUrl& url);
    void viewProcess(QString cmd, QStringList options);
    void viewUseFlag(const QUrl& url);
    void viewAppFiles(const QUrl& url);
    void viewUpdates(QString action, QString filter);
    void reloadApp(const QUrl& url);
    void searchApps(QString search, bool feelingLucky = false);
    void whatsNew(QString search, bool feelingLucky = false);

protected slots:
    void swipeUpdate(void);
    void longPressTimeout();
    void copyAvailableEvent(bool yes);

    void quseProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void qlistProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void reloadProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void processFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void processReadStandardError(void);
    void processReadStandardOutput(void);

protected:
    void processReadOutput(bool readLast, int exitCode, QProcess::ExitStatus exitStatus);

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    virtual void keyPressEvent(QKeyEvent *event) override;

    bool scrollGrabbed;
    bool swiping;
    bool selecting;
    int oldScrollValue;
    int oldY;
    int dy;
    QTimer animationTimer;
    QTimer longPressTimer;
    QPoint startPress;
    QPoint startLongPress;
    bool eatPress;
    virtual void contextMenuEvent(QContextMenuEvent *event) override;

    QString lastSearch;
    QString oldLink;
    QString context;
    QString useApp;
    QString markdown;
    bool isWorld;

    void fetch(QSqlQuery* query);
    void upgrade(QSqlQuery* query);
    void showUpdates(QSqlQuery* query, QString header, QString filter);
    void showQueryResult(QSqlQuery* query, QString header, QString search, bool feelingLucky = false);
    void printApp(QString& result, QString& app, QString& description, QString& latestVersion, QStringList& installedVersions, QStringList& obsoletedVersions);

    QProcess* process;
    bool processReadFirst;
    QString oldTitle;
};

#endif // BROWSERVIEW_H
