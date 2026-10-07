#pragma once

// The converter's one window: two folders, a list of maps, a button.
//
// Everything it does is the core's Converter (hlds/Converter.hpp); this class
// only collects the options, shows progress and refuses to start a run the
// core's preflight says cannot work.

#include <QMainWindow>

#include "hlds/Converter.hpp"

class CookWorker;
class QCheckBox;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void browseGame();
    void browseCard();
    /// Fill the map list from the game folder, all ticked.
    void reloadMaps();
    void setAllChecked(bool checked);
    void startOrCancel();
    void onFinished(int failed, bool cancelled);
    hlds::ConvertOptions options() const;
    void setRunning(bool running);

    QLineEdit *gameEdit_ = nullptr;
    QLineEdit *cardEdit_ = nullptr;
    QListWidget *mapList_ = nullptr;
    QCheckBox *texturesBox_ = nullptr;
    QPushButton *startButton_ = nullptr;
    QProgressBar *progress_ = nullptr;
    QPlainTextEdit *log_ = nullptr;
    CookWorker *worker_ = nullptr;
};
