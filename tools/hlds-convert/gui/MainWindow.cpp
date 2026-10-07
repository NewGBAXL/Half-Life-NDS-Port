#include "MainWindow.hpp"

#include <QCheckBox>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

#include "CookWorker.hpp"

namespace
{
    QWidget *folderRow(QLineEdit *edit, QPushButton *button)
    {
        auto *row = new QWidget;
        auto *layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(edit, 1);
        layout->addWidget(button);
        return row;
    }
} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(tr("Half-Life DS converter"));

    gameEdit_ = new QLineEdit;
    gameEdit_->setPlaceholderText(tr("Half-Life's \"valve\" folder"));
    cardEdit_ = new QLineEdit;
    cardEdit_->setPlaceholderText(tr("The SD card (its top folder)"));
    auto *gameBrowse = new QPushButton(tr("Browse..."));
    auto *cardBrowse = new QPushButton(tr("Browse..."));

    mapList_ = new QListWidget;
    auto *allButton = new QPushButton(tr("All"));
    auto *noneButton = new QPushButton(tr("None"));
    texturesBox_ = new QCheckBox(tr("Convert textures"));
    texturesBox_->setChecked(true);
    startButton_ = new QPushButton(tr("Convert"));
    startButton_->setDefault(true);
    progress_ = new QProgressBar;
    progress_->setTextVisible(true);
    log_ = new QPlainTextEdit;
    log_->setReadOnly(true);
    log_->setMaximumBlockCount(5000);

    auto *form = new QFormLayout;
    form->addRow(tr("Game folder"), folderRow(gameEdit_, gameBrowse));
    form->addRow(tr("Card"), folderRow(cardEdit_, cardBrowse));

    auto *listButtons = new QHBoxLayout;
    listButtons->addWidget(new QLabel(tr("Maps")));
    listButtons->addStretch(1);
    listButtons->addWidget(allButton);
    listButtons->addWidget(noneButton);

    auto *bottom = new QHBoxLayout;
    bottom->addWidget(texturesBox_);
    bottom->addStretch(1);
    bottom->addWidget(startButton_);

    auto *central = new QWidget;
    auto *layout = new QVBoxLayout(central);
    layout->addLayout(form);
    layout->addLayout(listButtons);
    layout->addWidget(mapList_, 2);
    layout->addLayout(bottom);
    layout->addWidget(progress_);
    layout->addWidget(log_, 1);
    setCentralWidget(central);
    resize(560, 640);

    connect(gameBrowse, &QPushButton::clicked, this, &MainWindow::browseGame);
    connect(cardBrowse, &QPushButton::clicked, this, &MainWindow::browseCard);
    connect(gameEdit_, &QLineEdit::editingFinished, this, &MainWindow::reloadMaps);
    connect(allButton, &QPushButton::clicked, this, [this] { setAllChecked(true); });
    connect(noneButton, &QPushButton::clicked, this, [this] { setAllChecked(false); });
    connect(startButton_, &QPushButton::clicked, this, &MainWindow::startOrCancel);

    // The two folders are the same every time for most people.
    QSettings settings;
    gameEdit_->setText(settings.value(QStringLiteral("gameDir")).toString());
    cardEdit_->setText(settings.value(QStringLiteral("cardDir")).toString());
    reloadMaps();
}

MainWindow::~MainWindow()
{
    if (worker_ != nullptr)
    {
        worker_->cancel();
        worker_->wait();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (worker_ != nullptr)
    {
        // Every file is written whole or not at all, so stopping is safe; it
        // just has to wait for the map in progress.
        worker_->cancel();
        log_->appendPlainText(tr("Stopping after the current map..."));
        worker_->wait();
    }
    QSettings settings;
    settings.setValue(QStringLiteral("gameDir"), gameEdit_->text());
    settings.setValue(QStringLiteral("cardDir"), cardEdit_->text());
    event->accept();
}

void MainWindow::browseGame()
{
    const QString dir =
        QFileDialog::getExistingDirectory(this, tr("Half-Life's \"valve\" folder"), gameEdit_->text());
    if (!dir.isEmpty())
    {
        gameEdit_->setText(dir);
        reloadMaps();
    }
}

void MainWindow::browseCard()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("The SD card"), cardEdit_->text());
    if (!dir.isEmpty())
        cardEdit_->setText(dir);
}

hlds::ConvertOptions MainWindow::options() const
{
    hlds::ConvertOptions options;
    options.gameDir = gameEdit_->text().toStdString();
    options.cardRoot = cardEdit_->text().toStdString();
    options.textures = texturesBox_->isChecked();
    return options;
}

void MainWindow::reloadMaps()
{
    mapList_->clear();
    if (gameEdit_->text().isEmpty())
        return;
    for (const std::string &name : hlds::Converter(options()).listMaps())
    {
        auto *item = new QListWidgetItem(QString::fromStdString(name), mapList_);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
    }
}

void MainWindow::setAllChecked(bool checked)
{
    for (int i = 0; i < mapList_->count(); ++i)
        mapList_->item(i)->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
}

void MainWindow::setRunning(bool running)
{
    gameEdit_->setEnabled(!running);
    cardEdit_->setEnabled(!running);
    mapList_->setEnabled(!running);
    texturesBox_->setEnabled(!running);
    startButton_->setText(running ? tr("Cancel") : tr("Convert"));
    startButton_->setEnabled(true);
}

void MainWindow::startOrCancel()
{
    if (worker_ != nullptr)
    {
        worker_->cancel();
        startButton_->setEnabled(false); // until the current map is done
        log_->appendPlainText(tr("Stopping after the current map..."));
        return;
    }

    const hlds::ConvertOptions opts = options();
    const std::string problem = hlds::Converter(opts).preflight();
    if (!problem.empty())
    {
        QMessageBox::warning(this, windowTitle(), QString::fromStdString(problem));
        return;
    }
    QStringList maps;
    for (int i = 0; i < mapList_->count(); ++i)
    {
        if (mapList_->item(i)->checkState() == Qt::Checked)
            maps << mapList_->item(i)->text();
    }
    if (maps.isEmpty())
    {
        QMessageBox::information(this, windowTitle(), tr("No map is ticked."));
        return;
    }

    log_->clear();
    progress_->setRange(0, static_cast<int>(maps.size()));
    progress_->setValue(0);
    worker_ = new CookWorker(opts, maps, this);
    connect(worker_, &CookWorker::mapStarted, this,
            [this](int index, int total, const QString &name)
            {
                progress_->setValue(index);
                log_->appendPlainText(QStringLiteral("[%1/%2] %3").arg(index + 1).arg(total).arg(name));
            });
    connect(worker_, &CookWorker::logLine, log_, &QPlainTextEdit::appendPlainText);
    connect(worker_, &CookWorker::finishedRun, this, &MainWindow::onFinished);
    setRunning(true);
    worker_->start();
}

void MainWindow::onFinished(int failed, bool cancelled)
{
    worker_->wait();
    worker_->deleteLater();
    worker_ = nullptr;
    setRunning(false);
    if (cancelled)
    {
        log_->appendPlainText(tr("Stopped. What was converted so far is usable."));
    }
    else
    {
        progress_->setValue(progress_->maximum());
        log_->appendPlainText(failed == 0 ? tr("Done.")
                                          : tr("Done, with %n map(s) failed.", nullptr, failed));
    }
}
