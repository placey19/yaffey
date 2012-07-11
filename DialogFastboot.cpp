#include <QDebug>

#include <QFileDialog>
#include <QFileInfo>
#include <QCloseEvent>
#include <QProcessEnvironment>

#include "DialogFastboot.h"
#include "ui_DialogFastboot.h"

#ifdef _WIN32
    static const QString FASTBOOT = "fastboot.exe";
    static const QChar PATH_ENVVAR_SEP = ';';
#else
    static const QString FASTBOOT = "fastboot";
    static const QChar PATH_ENVVAR_SEP = ':';
#endif  //_WIN32

static const QString SETTING_FASTBOOT_FILE = "FASTBOOT";
static const QString SETTING_IMAGE_FILE = "IMAGE";
static const QString SETTING_PARTITION = "PARTITION";

static const char* RED_CROSS = ":/icons/icons/red_cross.png";
static const char* GREEN_TICK = ":/icons/icons/green_tick.png";

DialogFastboot::DialogFastboot(QWidget* parent, QSettings& settings) : QDialog(parent),
                                                                       mUi(new Ui::DialogFastboot),
                                                                       mSettings(settings) {
    mUi->setupUi(this);

    mGotFastbootFile = false;
    mGotImageFile = false;
    mGotDeviceSelected = false;

    mUi->boxPartition->addItem("boot");
    mUi->boxPartition->addItem("system");
    mUi->boxPartition->addItem("recovery");

    //setup fastboot file editbox
    QString fastboot = findFastboot();
    if (fastboot.length() > 0) {
        mUi->lineFastbootFile->setText(fastboot);
        checkForConnectedDevices();
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
    QString filename = QFileDialog::getOpenFileName(this, "Get fastboot location", ".", FASTBOOT);
    if (filename.length() > 0) {
        mUi->lineFastbootFile->setText(filename);
    }
}

void DialogFastboot::on_pushBrowseImage_clicked() {
    QString filename = QFileDialog::getOpenFileName(this, "Get fastboot location", ".");
    if (filename.length() > 0) {
        mUi->lineImageFile->setText(filename);
    }
}

void DialogFastboot::on_lineFastbootFile_textChanged(const QString& text) {
    if (validateFastbootFile(text)) {
        checkForConnectedDevices();
    }
    analyzeSelections();
}

void DialogFastboot::on_lineImageFile_textChanged(const QString& text) {
    validateImageFile(text);
    analyzeSelections();
}

void DialogFastboot::on_pushFlash_clicked() {
    qDebug() << "Flashing...";

    analyzeSelections();

    QString fastbootFilename = mUi->lineFastbootFile->text();

    //create fastboot process and setup signals
    mProcess = new QProcess(this);
    connect(mProcess, SIGNAL(readyReadStandardOutput()), this, SLOT(on_processStdOutput()));
    connect(mProcess, SIGNAL(readyReadStandardError()), this, SLOT(on_processStdError()));
    connect(mProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(on_processFinished(int, QProcess::ExitStatus)));

    //setup arguments
    QString partition = mUi->boxPartition->currentText();
    QStringList args;
    args.append("flash");
    args.append(partition);

    //start process
    mProcess->start(fastbootFilename, args);
    if (mProcess->waitForStarted()) {

    } else {
/*        QMessageBox::critical(this, windowTitle(), KFailedToStart.arg(KProgram));
        ui->statusBar->showMessage(KAnalysisFailed);*/
    }
}

void DialogFastboot::on_pushClose_clicked() {
    close();
}

void DialogFastboot::on_listDevices_itemChanged(QListWidgetItem* item) {
    qDebug() << "on_listDevices_itemChanged";
    if (item) {
        mGotDeviceSelected = true;
    } else {
        mGotDeviceSelected = false;
    }

    analyzeSelections();
}

void DialogFastboot::on_processStdOutput() {
    qDebug() << "on_processStdOutput";

    QByteArray output = mProcess->readAllStandardOutput();
    mUi->textOutput->setTextColor(Qt::black);
    mUi->textOutput->append(output);
}

void DialogFastboot::on_processStdError() {
    qDebug() << "on_processStdError";

    QByteArray output = mProcess->readAllStandardError();
    mUi->textOutput->setTextColor(Qt::red);
    mUi->textOutput->append(output);
}

void DialogFastboot::on_processFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    qDebug() << "on_processFinished";

    mUi->textOutput->setTextColor(Qt::blue);
    if (exitStatus == QProcess::NormalExit) {
        mUi->textOutput->append(" -- process finished, code: " + QString::number(exitCode) + ", status: NormalExit");
    } else {
        mUi->textOutput->append(" -- process finished, code: " + QString::number(exitCode) + ", status: CrashExit");
    }
}

void DialogFastboot::on_boxPartition_currentIndexChanged(const QString& text) {
    analyzeSelections();
}

void DialogFastboot::analyzeSelections() {
    QString commandLine;

    if (mGotFastbootFile) {
        //execute fastboot with devices parameter and parse output

        mUi->labelFastbootFileStatus->setPixmap(QPixmap(QString::fromUtf8(GREEN_TICK)));
        commandLine += FASTBOOT;
    } else {
        mUi->labelFastbootFileStatus->setPixmap(QPixmap(QString::fromUtf8(RED_CROSS)));
    }

    commandLine += " flash ";
    commandLine += mUi->boxPartition->currentText() + " ";

    if (mGotImageFile) {
        mUi->labelImageFileStatus->setPixmap(QPixmap(QString::fromUtf8(GREEN_TICK)));
        commandLine += mUi->lineImageFile->text();
    } else {
        mUi->labelImageFileStatus->setPixmap(QPixmap(QString::fromUtf8(RED_CROSS)));
    }

    mUi->lineCommandLine->setText(commandLine);
}

QString DialogFastboot::findFastboot() {
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

void DialogFastboot::checkForConnectedDevices() {
    QString fastbootFilename = mUi->lineFastbootFile->text();
    if (validateFastbootFile(fastbootFilename)) {
        //create fastboot process and setup signals
        mProcess = new QProcess(this);
        connect(mProcess, SIGNAL(readyReadStandardOutput()), this, SLOT(on_processStdOutput()));
        connect(mProcess, SIGNAL(readyReadStandardError()), this, SLOT(on_processStdError()));
        connect(mProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(on_processFinished(int, QProcess::ExitStatus)));

        //setup arguments
        QStringList args;
        args.append("devices");

        //start process
        mProcess->start(fastbootFilename, args);
        mProcess->waitForStarted();
    }
}
