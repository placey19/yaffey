#include <QDebug>

#include <QFileDialog>
#include <QFileInfo>
#include <QCloseEvent>
#include <QProcessEnvironment>

#include "DialogFastboot.h"
#include "ui_DialogFastboot.h"

#ifdef _WIN32
    static const char* FASTBOOT = "fastboot.exe";
    static const QString FASTBOOT_NOEXT = "fastboot";
    static const char PATH_ENVVAR_SEP = ';';
#else
    static const char* FASTBOOT = "fastboot";
    static const QString FASTBOOT_NOEXT = FASTBOOT;
    static const char PATH_ENVVAR_SEP = ':';
#endif  //_WIN32

static const QString SETTING_FASTBOOT_FILE = "FASTBOOT";
static const QString SETTING_IMAGE_FILE = "IMAGE";
static const QString SETTING_PARTITION = "PARTITION";
static const int FASTBOOT_EXEC_TIMEOUT = 1 * 1000;

static const char* RED_CROSS = ":/icons/icons/red_cross.png";
static const char* GREEN_TICK = ":/icons/icons/green_tick.png";

DialogFastboot::DialogFastboot(QWidget* parent, QSettings& settings) : QDialog(parent),
                                                                       mUi(new Ui::DialogFastboot),
                                                                       mSettings(settings) {
    mUi->setupUi(this);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    mGotFastbootFile = false;
    mGotImageFile = false;
    mProcess = NULL;

    mUi->boxPartition->addItem("boot");
    mUi->boxPartition->addItem("system");
    mUi->boxPartition->addItem("recovery");

    //setup fastboot file editbox
    QString fastboot = findFastboot();
    if (fastboot.length() > 0) {
        mUi->lineFastbootFile->setText(fastboot);
    }

    //setup image file editbox
    QString imageFile = mSettings.value(SETTING_IMAGE_FILE).toString();
    if (imageFile.length() > 0) {
        mUi->lineImageFile->setText(imageFile);
    }

    //setup partition to flash
    int partitionBoxIndex = mSettings.value(SETTING_PARTITION).toInt();
    if (partitionBoxIndex >= 0 && partitionBoxIndex < mUi->boxPartition->count()) {
        mUi->boxPartition->setCurrentIndex(partitionBoxIndex);
    }
}

DialogFastboot::~DialogFastboot() {
    delete mUi;
}

void DialogFastboot::closeEvent(QCloseEvent* event) {
    qDebug() << "closeEvent()";

    //save fastboot file location if it's valid
    QString fastbootFileText = mUi->lineFastbootFile->text();
    if (validateFastbootFile(fastbootFileText)) {
        mSettings.setValue(SETTING_FASTBOOT_FILE, fastbootFileText);
    }

    //save image file
    QString imageFile = mUi->lineImageFile->text();
    if (imageFile.length() > 0) {
        mSettings.setValue(SETTING_IMAGE_FILE, imageFile);
    }

    //save the 'partition to flash' combo box selection
    int partitionBoxIndex = mUi->boxPartition->currentIndex();
    mSettings.setValue(SETTING_PARTITION, partitionBoxIndex);

    if (event) {
        event->ignore();
        hide();
    }
}

void DialogFastboot::on_pushBrowseFastboot_clicked() {
    qDebug() << "on_pushBrowseFastboot_clicked()";

    QString filename = QFileDialog::getOpenFileName(this, "Get fastboot location", ".", FASTBOOT);
    if (filename.length() > 0) {
        mUi->lineFastbootFile->setText(filename);
    }
}

void DialogFastboot::on_pushBrowseImage_clicked() {
    qDebug() << "on_pushBrowseImage_clicked()";

    QString filename = QFileDialog::getOpenFileName(this, "Get fastboot location", ".");
    if (filename.length() > 0) {
        mUi->lineImageFile->setText(filename);
    }
}

void DialogFastboot::on_lineFastbootFile_textChanged(const QString& text) {
    validateFastbootFile(text);
    analyzeSelections();
}

void DialogFastboot::on_lineImageFile_textChanged(const QString& text) {
    validateImageFile(text);
    analyzeSelections();
}

void DialogFastboot::on_pushClearLog_clicked() {
    mUi->textFastbootLog->clear();
}

void DialogFastboot::on_pushCancel_clicked() {
    qDebug() << "on_pushCancel_clicked()";

    if (mProcess != NULL) {
        mProcess->kill();
    }

    analyzeSelections();
}

void DialogFastboot::on_pushFlash_clicked() {
    qDebug() << "on_pushFlash_clicked()";

    analyzeSelections();

    //setup arguments
    QString partition = mUi->boxPartition->currentText();
    QString imageFile = mUi->lineImageFile->text();
    QStringList args;
    args.append("flash");
    args.append(partition);
    args.append(imageFile);

    //run the command
    execFastboot(args);
}

void DialogFastboot::on_pushClose_clicked() {
    qDebug() << "on_pushClose_clicked()";
    close();
}

void DialogFastboot::on_pushFastbootDevices_clicked() {
    qDebug() << "on_pushFastbootDevices_clicked()";
    execFastboot(QStringList("devices"));
}

void DialogFastboot::on_pushFastbootReboot_clicked() {
    qDebug() << "on_pushFastbootReboot_clicked()";
    execFastboot(QStringList("reboot"));
}

void DialogFastboot::on_pushFastbootRebootBootloader_clicked() {
    qDebug() << "on_pushFastbootReboot_clicked()";
    execFastboot(QStringList("reboot-bootloader"));
}

void DialogFastboot::on_processStdOutput() {
    qDebug() << "on_processStdOutput()";

    QByteArray output = mProcess->readAllStandardOutput();
    mUi->textFastbootLog->setTextColor(Qt::black);
    mUi->textFastbootLog->append(output);
}

void DialogFastboot::on_processStdError() {
    qDebug() << "on_processStdError()";

    QByteArray output = mProcess->readAllStandardError();
    mUi->textFastbootLog->setTextColor(Qt::black);
    mUi->textFastbootLog->append(output);
}

void DialogFastboot::on_processFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    qDebug() << "on_processFinished(), exitCode: " << exitCode << ", exitStatus: " << exitStatus;

    QString status = (exitStatus == QProcess::NormalExit ? "NormalExit" : "CrashExit");
    log("Fastboot finished, code: " + QString::number(exitCode) + ", status: " + status);

    delete mProcess;
    mProcess = NULL;

    analyzeSelections();
}

void DialogFastboot::on_boxPartition_currentIndexChanged(const QString& /*text*/) {
    analyzeSelections();
}

void DialogFastboot::analyzeSelections() {
    //setup controls
    if (mGotFastbootFile) {
        mUi->labelFastbootFileStatus->setPixmap(QPixmap(QString::fromUtf8(GREEN_TICK)));
        mUi->lineCommandLine->setEnabled(true);
        mUi->pushFlash->setEnabled(mProcess == NULL && mGotImageFile);
        mUi->pushFastbootDevices->setEnabled(mProcess == NULL);
        mUi->pushFastbootReboot->setEnabled(mProcess == NULL);
        mUi->pushFastbootRebootBootloader->setEnabled(mProcess == NULL);
    } else {
        mUi->labelFastbootFileStatus->setPixmap(QPixmap(QString::fromUtf8(RED_CROSS)));
        mUi->lineCommandLine->setEnabled(false);
        mUi->pushFlash->setEnabled(false);
        mUi->pushFastbootDevices->setEnabled(false);
        mUi->pushFastbootReboot->setEnabled(false);
        mUi->pushFastbootRebootBootloader->setEnabled(false);
    }
    mUi->pushCancel->setEnabled(mProcess != NULL);

    //setup command line
    QString commandLine = FASTBOOT_NOEXT + " flash ";
    commandLine += mUi->boxPartition->currentText();
    if (mGotImageFile) {
        mUi->labelImageFileStatus->setPixmap(QPixmap(QString::fromUtf8(GREEN_TICK)));
        commandLine += " " + mUi->lineImageFile->text();
    } else {
        mUi->labelImageFileStatus->setPixmap(QPixmap(QString::fromUtf8(RED_CROSS)));
    }
    mUi->lineCommandLine->setText(commandLine);
}

QString DialogFastboot::findFastboot() {
    qDebug() << "findFastboot()";

    QString result = mSettings.value(SETTING_FASTBOOT_FILE).toString();

    QFileInfo settingsFileInfo(result);
    if (!settingsFileInfo.exists() || !settingsFileInfo.isExecutable()) {
        //get the contents of the PATH environment variable
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        QString pathEnvVar = env.value("PATH");

        //split the path environment variable up into individual paths
        QStringList pathList = pathEnvVar.split(PATH_ENVVAR_SEP);

        //iterate over the list of paths to search for the fastboot executable
        foreach (QString path, pathList) {
            QString fastbootWithPath = path + QDir::separator() + FASTBOOT;
            QFileInfo fileInfo(fastbootWithPath);
            if (fileInfo.exists() && fileInfo.isExecutable()) {
                result = fastbootWithPath;
                break;
            }
        }
    }

    return result;
}

bool DialogFastboot::validateFastbootFile(const QString& fastbootFile) {
    QFileInfo fileInfo(fastbootFile.trimmed());
    if ((fileInfo.fileName().compare(FASTBOOT) == 0) && fileInfo.exists() && fileInfo.isExecutable()) {
        mGotFastbootFile = true;
    } else {
        mGotFastbootFile = false;
    }
    return mGotFastbootFile;
}

bool DialogFastboot::validateImageFile(const QString& imageFile) {
    QFileInfo fileInfo(imageFile);
    if (fileInfo.exists()) {
        mGotImageFile = true;
    } else {
        mGotImageFile = false;
    }
    return mGotImageFile;
}

void DialogFastboot::execFastboot(const QStringList& args) {
    if (mProcess == NULL) {
        QString fastbootFilename = mUi->lineFastbootFile->text();

        //create fastboot process and setup signals
        mProcess = new QProcess(this);
        connect(mProcess, SIGNAL(readyReadStandardOutput()), this, SLOT(on_processStdOutput()));
        connect(mProcess, SIGNAL(readyReadStandardError()), this, SLOT(on_processStdError()));
        connect(mProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(on_processFinished(int, QProcess::ExitStatus)));

        //show command in output
        QString flatCmd = args.join(" ");
        log("Executing: " + FASTBOOT_NOEXT + " " + flatCmd);

        //start process
        mProcess->start(fastbootFilename, args);
        if (!mProcess->waitForStarted(FASTBOOT_EXEC_TIMEOUT)) {
            log("Failed to start fastboot");
            delete mProcess;
            mProcess = NULL;
        }
    }

    analyzeSelections();
}

void DialogFastboot::log(const QString& text) {
    mUi->textFastbootLog->setTextColor(Qt::blue);
    mUi->textFastbootLog->append(" -- " + text);
}
