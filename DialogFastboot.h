#ifndef DIALOGFASTBOOT_H
#define DIALOGFASTBOOT_H

#include <QDialog>

#include <QListWidgetItem>
#include <QProcess>
#include <QSettings>

namespace Ui {
    class DialogFastboot;
}

class DialogFastboot : public QDialog {
    Q_OBJECT

public:
    explicit DialogFastboot(QWidget* parent, QSettings& settings);
    ~DialogFastboot();

protected:
    //from QDialog
    void closeEvent(QCloseEvent* event);

private slots:
    void on_pushBrowseFastboot_clicked();
    void on_pushBrowseImage_clicked();
    void on_lineFastbootFile_textChanged(const QString& text);
    void on_lineImageFile_textChanged(const QString& text);
    void on_pushClearLog_clicked();
    void on_pushCancel_clicked();
    void on_pushFlash_clicked();
    void on_pushFastbootDevices_clicked();
    void on_pushFastbootReboot_clicked();
    void on_pushFastbootRebootBootloader_clicked();
    void on_pushClose_clicked();
    void on_processStdOutput();
    void on_processStdError();
    void on_processFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void on_boxPartition_currentIndexChanged(const QString& text);

private:
    void analyzeSelections();
    QString findFastboot();
    bool validateFastbootFile(const QString& fastbootFile);
    bool validateImageFile(const QString& imageFile);
    void execFastboot(const QStringList& args);
    void log(const QString& text);

private:
    Ui::DialogFastboot* mUi;
    QSettings& mSettings;
    bool mGotFastbootFile;
    bool mGotImageFile;
    QProcess* mProcess;
};

#endif  //DIALOGFASTBOOT_H
