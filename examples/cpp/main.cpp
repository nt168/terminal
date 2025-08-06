/*  版权所有 (C) 2008 e_k (e_k@users.sourceforge.net)

    该库是自由软件；您可以按照自由软件基金会发布的
    GNU 通用公共许可证的条款重新分发和/或修改它；
    可以是许可证的第 2 版，或（由您选择）任何更高版本。

    发布该库是希望它能发挥作用，
    但没有任何担保；甚至不包含适销性或
    适用于特定目的的默示保证。更多细节请参阅
    GNU 通用公共许可证。

    您应该已经随本库一起收到 GNU 通用公共许可证的副本；
    若没有，请写信至自由软件基金会：
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA。
*/


#include <QApplication>
#include <QtDebug>
#include <QIcon>
#include <QMainWindow>
#include <QMenuBar>

#include "qtermwidget.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QIcon::setThemeName(QStringLiteral("oxygen"));
    QMainWindow *mainWindow = new QMainWindow();

    QTermWidget *console = new QTermWidget();

    QMenuBar *menuBar = new QMenuBar(mainWindow);
    QMenu *actionsMenu = new QMenu(QStringLiteral("Actions"), menuBar);
    menuBar->addMenu(actionsMenu);
    actionsMenu->addAction(QStringLiteral("Find..."), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F), 
        console, &QTermWidget::toggleShowSearchBar);
    actionsMenu->addAction(QStringLiteral("Copy"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C), 
        console, &QTermWidget::copyClipboard);
    actionsMenu->addAction(QStringLiteral("Paste"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V), 
        console, &QTermWidget::pasteClipboard);
    actionsMenu->addAction(QStringLiteral("About Qt"), &app, &QApplication::aboutQt);
    mainWindow->setMenuBar(menuBar);

    QFont font = QApplication::font();
#ifdef Q_OS_MACOS
    font.setFamily(QStringLiteral("Monaco"));
#elif defined(Q_WS_QWS)
    font.setFamily(QStringLiteral("fixed"));
#else
    font.setFamily(QStringLiteral("Monospace"));
#endif
    font.setPointSize(12);

    console->setTerminalFont(font);

   // console->setColorScheme(COLOR_SCHEME_BLACK_ON_LIGHT_YELLOW);
    console->setScrollBarPosition(QTermWidget::ScrollBarRight);

    const auto arguments = QApplication::arguments();
    for (const QString& arg : arguments)
    {
        if (console->availableColorSchemes().contains(arg))
            console->setColorScheme(arg);
        if (console->availableKeyBindings().contains(arg))
            console->setKeyBindings(arg);
    }

    mainWindow->setCentralWidget(console);
    mainWindow->resize(600, 400);

    // 信息输出
    qDebug() << "* INFO *************************";
    qDebug() << " availableKeyBindings:" << console->availableKeyBindings();
    qDebug() << " keyBindings:" << console->keyBindings();
    qDebug() << " availableColorSchemes:" << console->availableColorSchemes();
    qDebug() << "* INFO END *********************";

    // 实际启动
    QObject::connect(console, &QTermWidget::finished, mainWindow, &QMainWindow::close);

    mainWindow->show();
    return app.exec();
}
