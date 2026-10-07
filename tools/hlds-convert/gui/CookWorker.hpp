#pragma once

// The conversion, off the GUI thread.
//
// A cook takes minutes and the window has to keep drawing and keep its Cancel
// button alive, so the core's Converter runs here and talks back only through
// queued signals. Nothing in this class touches a widget.

#include <atomic>

#include <QString>
#include <QStringList>
#include <QThread>

#include "hlds/Converter.hpp"

class CookWorker : public QThread
{
    Q_OBJECT

public:
    CookWorker(hlds::ConvertOptions options, QStringList maps, QObject *parent = nullptr);

    /// Stop after the map in progress. Safe from any thread.
    void cancel() { cancel_.store(true); }

signals:
    void mapStarted(int index, int total, const QString &name);
    void logLine(const QString &text);
    /// Emitted once, last. `failed` counts maps that did not cook.
    void finishedRun(int failed, bool cancelled);

protected:
    void run() override;

private:
    hlds::ConvertOptions options_;
    QStringList maps_;
    std::atomic<bool> cancel_{false};
};
