// src/main.cpp - CueForge Qt6 Application Entry Point
#include <QApplication>
#include <QStyleFactory>
#include <QDir>
#include <QStandardPaths>
#include <QFileInfo>
#include <QMessageBox>
#include <QSplashScreen>
#include <QPixmap>
#include <QTimer>

#include "core/Application.h"
#include "utils/Settings.h"

// Function to load and apply stylesheet
QString loadStyleSheet(const QString& filePath) {
    QFile file(filePath);
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        QTextStream stream(&file);
        return stream.readAll();
    }
    return QString();
}

// Function to setup application directories
void setupApplicationDirectories() {
    // Create necessary application directories
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appDataPath);
    QDir().mkpath(appDataPath + "/workspaces");
    QDir().mkpath(appDataPath + "/logs");
    QDir().mkpath(appDataPath + "/temp");
    QDir().mkpath(appDataPath + "/cache");
}

// Function to check system requirements
bool checkSystemRequirements() {
    // Check Qt version
    if (QT_VERSION < QT_VERSION_CHECK(6, 5, 0)) {
        QMessageBox::critical(nullptr, "System Requirements",
            "CueForge requires Qt 6.5.0 or later.");
        return false;
    }

    // Check for audio subsystem availability
    // This will be expanded when audio engine is integrated

    return true;
}

int main(int argc, char* argv[])
{
    // Enable high DPI scaling
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication app(argc, argv);

    // Application metadata
    app.setApplicationName("CueForge");
    app.setApplicationVersion("2.0.0");
    app.setApplicationDisplayName("CueForge 2.0");
    app.setOrganizationName("CueForge");
    app.setOrganizationDomain("cueforge.app");

    // Set application icon
    app.setWindowIcon(QIcon(":/icons/cueforge.ico"));

    // Check system requirements
    if (!checkSystemRequirements()) {
        return 1;
    }

    // Setup application directories
    setupApplicationDirectories();

    // Create splash screen
    QPixmap splashPixmap(":/icons/cueforge_splash.png");
    if (splashPixmap.isNull()) {
        // Create a simple splash if image not found
        splashPixmap = QPixmap(400, 300);
        splashPixmap.fill(Qt::darkGray);
    }

    QSplashScreen splash(splashPixmap);
    splash.show();

    splash.showMessage("Loading CueForge...", Qt::AlignBottom | Qt::AlignCenter, Qt::white);
    app.processEvents();

    // Set application style for professional look
    app.setStyle(QStyleFactory::create("Fusion"));

    // Load and apply theme from settings
    Settings settings;
    QString theme = settings.value("ui/theme", "dark").toString();
    QString styleSheetPath = QString(":/styles/cueforge-%1.qss").arg(theme);
    QString styleSheet = loadStyleSheet(styleSheetPath);

    if (!styleSheet.isEmpty()) {
        app.setStyleSheet(styleSheet);
    }
    else {
        // Fallback to built-in dark theme if stylesheet loading fails
        app.setStyleSheet(R"(
            QMainWindow {
                background-color: #2b2b2b;
                color: #ffffff;
            }
            QWidget {
                background-color: #2b2b2b;
                color: #ffffff;
                selection-background-color: #3daee9;
            }
            QPushButton {
                background-color: #3c3c3c;
                border: 1px solid #555555;
                padding: 6px 12px;
                border-radius: 4px;
            }
            QPushButton:hover {
                background-color: #4c4c4c;
            }
            QPushButton:pressed {
                background-color: #2c2c2c;
            }
            QMenuBar {
                background-color: #2b2b2b;
                border-bottom: 1px solid #555555;
            }
            QMenuBar::item {
                padding: 6px 12px;
            }
            QMenuBar::item:selected {
                background-color: #3daee9;
            }
            QMenu {
                background-color: #3c3c3c;
                border: 1px solid #555555;
            }
            QMenu::item:selected {
                background-color: #3daee9;
            }
            QToolBar {
                background-color: #2b2b2b;
                border: 1px solid #555555;
                spacing: 3px;
            }
            QStatusBar {
                background-color: #2b2b2b;
                border-top: 1px solid #555555;
            }
        )");
    }

    splash.showMessage("Initializing audio engine...", Qt::AlignBottom | Qt::AlignCenter, Qt::white);
    app.processEvents();

    // Create and initialize CueForge application
    CueForgeApplication cueforge;

    if (!cueforge.initialize()) {
        splash.close();
        QMessageBox::critical(nullptr, "Initialization Error",
            "Failed to initialize CueForge. Please check your audio setup and try again.");
        return 1;
    }

    splash.showMessage("Starting CueForge...", Qt::AlignBottom | Qt::AlignCenter, Qt::white);
    app.processEvents();

    // Close splash screen after a brief delay
    QTimer::singleShot(1000, &splash, &QSplashScreen::close);

    // Run the application
    int result = cueforge.exec();

    // Cleanup
    splash.close();

    return result;
}