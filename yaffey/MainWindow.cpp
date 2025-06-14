/*
 * yaffey: Utility for reading, editing and writing YAFFS2 images
 * Copyright (C) 2012 David Place <david.t.place@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 */

#include <QDebug>

#include <QFile>
#include <QMessageBox>
#include <QFileDialog>
#include <QListView>

#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "DialogEditProperties.h"
#include "DialogFastboot.h"
#include "DialogImport.h"
#include "YaffsManager.h"
#include "YaffsTreeView.h"
#include "Utils.h"

static const QString AUTHOR = "DavidPlace";
static const QString APPNAME = "Yaffey";
static const QString VERSION = "0.3";

//xml tag names and attributes
static const char* TAG_MENUITEM = "menuitem";
static const char* TAG_FILE = "file";
static const char* TAG_SYMLINK = "symlink";
static const char* ATTR_NAME = "name";
static const char* ATTR_ALIAS = "alias";
static const char* ATTR_DEST = "dest";
static const char* ATTR_PERMISSIONS = "permissions";
static const char* ATTR_USER = "user";
static const char* ATTR_GROUP = "group";

MainWindow::MainWindow(QWidget* parent, QString imageFilename) : QMainWindow(parent),
                                                                 mUi(new Ui::MainWindow),
                                                                 mContextMenu(this),
                                                                 mSettings(AUTHOR, APPNAME) {
    mUi->setupUi(this);

    //create and connect the signal mapper for dynamic actions and parse dynamic menu xml
    mSignalMapper = new QSignalMapper(this);
    connect(mSignalMapper, SIGNAL(mapped(const QString&)), this, SLOT(on_dynamicActionTriggered(const QString&)));
    mDoc = NULL;
    parseDynamicMenuXml("files/android-menu.xml");

    //setup context menu for the treeview
    mContextMenu.addAction(mUi->actionImport);
    mContextMenu.addAction(mUi->actionExport);
    mContextMenu.addSeparator();
    mContextMenu.addAction(mUi->actionRename);
    mContextMenu.addAction(mUi->actionDelete);
    mContextMenu.addSeparator();
    mContextMenu.addAction(mUi->actionEditProperties);

    //setup context menu for the header
    mHeaderContextMenu.addAction(mUi->actionColumnName);
    mHeaderContextMenu.addAction(mUi->actionColumnSize);
    mHeaderContextMenu.addAction(mUi->actionColumnPermissions);
    mHeaderContextMenu.addAction(mUi->actionColumnAlias);
    mHeaderContextMenu.addAction(mUi->actionColumnDateAccessed);
    mHeaderContextMenu.addAction(mUi->actionColumnDateCreated);
    mHeaderContextMenu.addAction(mUi->actionColumnDateModified);
    mHeaderContextMenu.addAction(mUi->actionColumnUser);
    mHeaderContextMenu.addAction(mUi->actionColumnGroup);

    //get YaffsManager instance and create model
    mYaffsManager = YaffsManager::getInstance();
    newModel();
    updateWindowTitle();

    QHeaderView* headerView = mUi->treeView->header();
    headerView->setContextMenuPolicy(Qt::CustomContextMenu);
    headerView->setSectionResizeMode(YaffsItem::NAME, QHeaderView::Stretch);
    headerView->setSectionResizeMode(YaffsItem::SIZE, QHeaderView::ResizeToContents);
    headerView->setSectionResizeMode(YaffsItem::PERMISSIONS, QHeaderView::ResizeToContents);
    headerView->setSectionResizeMode(YaffsItem::ALIAS, QHeaderView::ResizeToContents);
    headerView->setSectionResizeMode(YaffsItem::DATE_ACCESSED, QHeaderView::ResizeToContents);
    headerView->setSectionResizeMode(YaffsItem::DATE_CREATED, QHeaderView::ResizeToContents);
    headerView->setSectionResizeMode(YaffsItem::DATE_MODIFIED, QHeaderView::ResizeToContents);
    headerView->setSectionResizeMode(YaffsItem::USER, QHeaderView::ResizeToContents);
    headerView->setSectionResizeMode(YaffsItem::GROUP, QHeaderView::ResizeToContents);
#ifdef QT_DEBUG
    headerView->setSectionResizeMode(YaffsItem::OBJECTID, QHeaderView::ResizeToContents);
    headerView->setSectionResizeMode(YaffsItem::PARENTID, QHeaderView::ResizeToContents);
    headerView->setSectionResizeMode(YaffsItem::HEADERPOS, QHeaderView::ResizeToContents);
#endif  //QT_DEBUG

    mUi->treeView->hideColumn(YaffsItem::DATE_CREATED);
    mUi->treeView->hideColumn(YaffsItem::DATE_ACCESSED);

    mUi->actionColumnName->setEnabled(false);
    mUi->actionColumnName->setChecked(!mUi->treeView->isColumnHidden(YaffsItem::NAME));
    mUi->actionColumnSize->setChecked(!mUi->treeView->isColumnHidden(YaffsItem::SIZE));
    mUi->actionColumnPermissions->setChecked(!mUi->treeView->isColumnHidden(YaffsItem::PERMISSIONS));
    mUi->actionColumnAlias->setChecked(!mUi->treeView->isColumnHidden(YaffsItem::ALIAS));
    mUi->actionColumnDateAccessed->setChecked(!mUi->treeView->isColumnHidden(YaffsItem::DATE_ACCESSED));
    mUi->actionColumnDateCreated->setChecked(!mUi->treeView->isColumnHidden(YaffsItem::DATE_CREATED));
    mUi->actionColumnDateModified->setChecked(!mUi->treeView->isColumnHidden(YaffsItem::DATE_MODIFIED));
    mUi->actionColumnUser->setChecked(!mUi->treeView->isColumnHidden(YaffsItem::USER));
    mUi->actionColumnGroup->setChecked(!mUi->treeView->isColumnHidden(YaffsItem::GROUP));

    connect(headerView, SIGNAL(customContextMenuRequested(QPoint)), SLOT(on_treeViewHeader_customContextMenuRequested(QPoint)));
    connect(mUi->actionExpandAll, SIGNAL(triggered()), mUi->treeView, SLOT(expandAll()));
    connect(mUi->actionCollapseAll, SIGNAL(triggered()), mUi->treeView, SLOT(collapseAll()));
    connect(mUi->treeView, SIGNAL(selectionChanged()), SLOT(on_treeView_selectionChanged()));

    if (imageFilename.length() > 0) {
        show();
        openImage(imageFilename);
    } else {
        mUi->statusBar->showMessage(windowTitle() + " v" + VERSION);
    }

    mFastbootDialog = NULL;

    setupActions();
}

MainWindow::~MainWindow() {
    delete mUi;
    delete mFastbootDialog;
    delete mSignalMapper;
    delete mDoc;
}

void MainWindow::newModel() {
    mYaffsModel = mYaffsManager->newModel();
    mUi->treeView->setModel(mYaffsModel);
    connect(mYaffsManager, SIGNAL(modelChanged()), SLOT(on_modelChanged()));
}

void MainWindow::on_treeView_doubleClicked(const QModelIndex& itemIndex) {
    YaffsItem* item = static_cast<YaffsItem*>(itemIndex.internalPointer());
    if (item) {
        if (item->isFile() || item->isSymLink()) {
            mUi->actionEditProperties->trigger();
        }
    }
}

void MainWindow::on_actionNew_triggered() {
    bool doNew = !mYaffsModel->isDirty();
    if (!doNew) {
        QMessageBox::StandardButton result = QMessageBox::question(this,
                                                                   "New",
                                                                   "Unsaved changes. Are you sure you want to create a new image?",
                                                                   QMessageBox::Yes | QMessageBox::No,
                                                                   QMessageBox::No);
        doNew = (result == QMessageBox::Yes);
    }

    if (doNew) {
        newModel();
        mYaffsModel->newImage("new-yaffs2.img");
        mUi->statusBar->showMessage("Created new YAFFS2 image");
    }
}

void MainWindow::on_actionOpen_triggered() {
    QString imageFilename = QFileDialog::getOpenFileName(this, "Open File", ".");

    if (imageFilename.length() > 0) {
        bool doOpen = !mYaffsModel->isDirty();
        if (!doOpen) {
            QMessageBox::StandardButton result = QMessageBox::question(this,
                                                                       "Open",
                                                                       "Unsaved changes. Are you sure you want to open the image?",
                                                                       QMessageBox::Yes | QMessageBox::No,
                                                                       QMessageBox::No);
            doOpen = (result == QMessageBox::Yes);
        }

        if (doOpen) {
            newModel();
            openImage(imageFilename);
        }
    }
}

void MainWindow::updateWindowTitle() {
    QString modelFilename = mYaffsModel->getImageFilename();
    if (modelFilename.length() > 0) {
        if (mYaffsModel->isDirty()) {
            setWindowTitle(APPNAME + " - " + modelFilename + "*");
        } else {
            setWindowTitle(APPNAME + " - " + modelFilename);
        }
    } else {
        setWindowTitle(APPNAME);
    }
}

void MainWindow::openImage(const QString& imageFilename) {
    if (imageFilename.length() > 0) {
        YaffsReadInfo readInfo = mYaffsModel->openImage(imageFilename);
        if (readInfo.result) {
            QModelIndex rootIndex = mYaffsModel->index(0, 0);
            mUi->treeView->expand(rootIndex);
            mUi->statusBar->showMessage("Opened image: " + imageFilename);

            updateWindowTitle();
            QString summary("<table>" \
                            "<tr><td width=120>Files:</td><td>" + QString::number(readInfo.numFiles) + "</td></tr>" +
                            "<tr><td width=120>Directories:</td><td>" + QString::number(readInfo.numDirs) + "</td></tr>" +
                            "<tr><td width=120>SymLinks:</td><td>" + QString::number(readInfo.numSymLinks) + "</td></tr>" +
                            "<tr><td colspan=2><hr/></td></tr>" +
                            "<tr><td width=120>HardLinks:</td><td>" + QString::number(readInfo.numHardLinks) + "</td></tr>" +
                            "<tr><td width=120>Specials:</td><td>" + QString::number(readInfo.numSpecials) + "</td></tr>" +
                            "<tr><td width=120>Unknowns:</td><td>" + QString::number(readInfo.numUnknowns) + "</td></tr>" +
                            "<tr><td colspan=2><hr/></td></tr>" +
                            "<tr><td width=120>Errors:</td><td>" + QString::number(readInfo.numErrorousObjects) + "</td></tr></table>");

            if (readInfo.eofHasIncompletePage) {
                summary += "<br/><br/>Warning:<br/>Incomplete page found at end of file";
            }
            QMessageBox::information(this, "Summary", summary);
        } else {
            QString msg = "Error opening image: " + imageFilename;
            mUi->statusBar->showMessage(msg);
            QMessageBox::critical(this, "Error", msg);
        }
        setupActions();
    }
}

void MainWindow::on_actionClose_triggered() {
    QString imageFile = mYaffsModel->getImageFilename();
    bool doClose = !mYaffsModel->isDirty();

    if (!doClose) {
        QMessageBox::StandardButton result = QMessageBox::question(this,
                                                                   "Close",
                                                                   "Unsaved changes. Are you sure you want to close the image?",
                                                                   QMessageBox::Yes | QMessageBox::No,
                                                                   QMessageBox::No);
        doClose = (result == QMessageBox::Yes);
    }

    if (doClose) {
        if (imageFile.length() > 0) {
            newModel();
            mUi->statusBar->showMessage("Closed image file: " + imageFile);
        }
        mUi->linePath->clear();
        updateWindowTitle();
        setupActions();
    }
}

void MainWindow::on_actionSaveAs_triggered() {
    if (mYaffsModel->isImageOpen()) {
        QString imgName = mYaffsModel->getImageFilename();
        QString saveAsFilename = QFileDialog::getSaveFileName(this, "Save Image As", "./" + imgName);
        if (saveAsFilename.length() > 0) {
            if (saveAsFilename.compare(mYaffsModel->getImageFilename()) != 0) {
                YaffsSaveInfo saveInfo;
                bool result = mYaffsModel->saveAs(saveAsFilename, saveInfo);
                updateWindowTitle();

                QString summary("<table>" \
                                "<tr><td width=120>Files:</td><td>" + QString::number(saveInfo.numFilesSaved) + "</td></tr>" +
                                "<tr><td width=120>Directories:</td><td>" + QString::number(saveInfo.numDirsSaved) + "</td></tr>" +
                                "<tr><td width=120>SymLinks:</td><td>" + QString::number(saveInfo.numSymLinksSaved) + "</td></tr>" +
                                "<tr><td colspan=2><hr/></td></tr>" +
                                "<tr><td width=120>Files Failed:</td><td>" + QString::number(saveInfo.numFilesFailed) + "</td></tr>" +
                                "<tr><td width=120>Directories Failed:</td><td>" + QString::number(saveInfo.numDirsFailed) + "</td></tr>" +
                                "<tr><td width=120>SymLinks Failed:</td><td>" + QString::number(saveInfo.numSymLinksFailed) + "</td></tr></td></tr></table>");

                if (result) {
                    mUi->statusBar->showMessage("Image saved: " + saveAsFilename);
                    QMessageBox::information(this, "Image saved", summary);
                } else {
                    mUi->statusBar->showMessage("Error saving image: " + saveAsFilename);
                    QMessageBox::critical(this, "Error saving image", summary);
                }
            } else {
                mUi->statusBar->showMessage("Error saving image: " + saveAsFilename);
                QMessageBox::critical(this, "Error saving image", "Can't save over current image, choose another filename.");
            }
        }
    }
}

void MainWindow::on_actionImport_triggered() {
    DialogImport import(this);
    int result = import.exec();

    if (result == DialogImport::RESULT_FILE) {
        QModelIndex parentIndex = mUi->treeView->selectionModel()->currentIndex();
        YaffsItem* parentItem = static_cast<YaffsItem*>(parentIndex.internalPointer());
        if (parentItem && parentItem->isDir()) {
            QStringList fileNames = QFileDialog::getOpenFileNames(this, "Select file(s) to import...");
            foreach (QString importFilename, fileNames) {
                importFilename.replace('\\', '/');
                mYaffsModel->importFile(parentItem, importFilename);
            }
        }
    } else if (result == DialogImport::RESULT_DIRECTORY) {
        QModelIndex parentIndex = mUi->treeView->selectionModel()->currentIndex();
        YaffsItem* parentItem = static_cast<YaffsItem*>(parentIndex.internalPointer());
        if (parentItem && parentItem->isDir()) {
            QString directoryName = QFileDialog::getExistingDirectory(this, "Select directory to import...");
            if (directoryName.length() > 0) {
                directoryName.replace('\\', '/');
                mYaffsModel->importDirectory(parentItem, directoryName);
            }
        }
    }
}

void MainWindow::on_actionExport_triggered() {
    QModelIndexList selectedRows = mUi->treeView->selectionModel()->selectedRows();
    if (selectedRows.size() > 0) {
        QString path = QFileDialog::getExistingDirectory(this);
        if (path.length() > 0) {
            exportSelectedItems(path);
        } else {
            mUi->statusBar->showMessage("Export cancelled");
        }
    } else {
        mUi->statusBar->showMessage("Nothing selected to export");
    }
}

void MainWindow::on_actionExit_triggered() {
    close();
}

void MainWindow::on_actionRename_triggered() {
    QModelIndex index = mUi->treeView->selectionModel()->currentIndex();
    YaffsItem* item = static_cast<YaffsItem*>(index.internalPointer());
    if (item && !item->isRoot()) {
        if (index.column() == YaffsItem::NAME) {
            mUi->treeView->edit(index);
        }
    }
}

void MainWindow::on_actionDelete_triggered() {
    QModelIndexList selectedRows = mUi->treeView->selectionModel()->selectedRows();
    int numRowsDeleted = mYaffsModel->removeRows(selectedRows);
    mUi->statusBar->showMessage("Deleted " + QString::number(numRowsDeleted) + " items");
}

void MainWindow::on_actionEditProperties_triggered() {
    QModelIndexList selectedRows = mUi->treeView->selectionModel()->selectedRows();
    if (selectedRows.size() > 0) {
        QDialog* dialog = new DialogEditProperties(*mYaffsModel, selectedRows, this);
        dialog->exec();
    }
    setupActions();
}

void MainWindow::on_actionAndroidFastboot_triggered() {
    if (mFastbootDialog) {
        mFastbootDialog->show();
    } else {
        mFastbootDialog = new DialogFastboot(this, mSettings);
        mFastbootDialog->exec();
    }
}

void MainWindow::on_actionAbout_triggered() {
    static const QString about("<b>" + APPNAME + " v" + VERSION + "</b><br/>" \
                               "Yet Another Flash File (System) Editor YEAH!<br/><br/>" \
                               "Built on " + QString(__DATE__) + " at " + QString(__TIME__) + "<br/><br/>" \
                               "Written by David Place<br/><br/>" \
                               "Special thanks to Dan Lawrence");
    QMessageBox::information(this, "About " + APPNAME, about);
}

void MainWindow::on_actionColumnName_triggered() {
    if (mUi->actionColumnName->isChecked()) {
        mUi->treeView->showColumn(YaffsItem::NAME);
    } else {
        mUi->treeView->hideColumn(YaffsItem::NAME);
    }
}

void MainWindow::on_actionColumnSize_triggered() {
    if (mUi->actionColumnSize->isChecked()) {
        mUi->treeView->showColumn(YaffsItem::SIZE);
    } else {
        mUi->treeView->hideColumn(YaffsItem::SIZE);
    }
}

void MainWindow::on_actionColumnPermissions_triggered() {
    if (mUi->actionColumnPermissions->isChecked()) {
        mUi->treeView->showColumn(YaffsItem::PERMISSIONS);
    } else {
        mUi->treeView->hideColumn(YaffsItem::PERMISSIONS);
    }
}

void MainWindow::on_actionColumnAlias_triggered() {
    if (mUi->actionColumnAlias->isChecked()) {
        mUi->treeView->showColumn(YaffsItem::ALIAS);
    } else {
        mUi->treeView->hideColumn(YaffsItem::ALIAS);
    }
}

void MainWindow::on_actionColumnDateAccessed_triggered() {
    if (mUi->actionColumnDateAccessed->isChecked()) {
        mUi->treeView->showColumn(YaffsItem::DATE_ACCESSED);
    } else {
        mUi->treeView->hideColumn(YaffsItem::DATE_ACCESSED);
    }
}

void MainWindow::on_actionColumnDateCreated_triggered() {
    if (mUi->actionColumnDateCreated->isChecked()) {
        mUi->treeView->showColumn(YaffsItem::DATE_CREATED);
    } else {
        mUi->treeView->hideColumn(YaffsItem::DATE_CREATED);
    }
}

void MainWindow::on_actionColumnDateModified_triggered() {
    if (mUi->actionColumnDateModified->isChecked()) {
        mUi->treeView->showColumn(YaffsItem::DATE_MODIFIED);
    } else {
        mUi->treeView->hideColumn(YaffsItem::DATE_MODIFIED);
    }
}

void MainWindow::on_actionColumnUser_triggered() {
    if (mUi->actionColumnUser->isChecked()) {
        mUi->treeView->showColumn(YaffsItem::USER);
    } else {
        mUi->treeView->hideColumn(YaffsItem::USER);
    }
}

void MainWindow::on_actionColumnGroup_triggered() {
    if (mUi->actionColumnGroup->isChecked()) {
        mUi->treeView->showColumn(YaffsItem::GROUP);
    } else {
        mUi->treeView->hideColumn(YaffsItem::GROUP);
    }
}

void MainWindow::on_treeViewHeader_customContextMenuRequested(const QPoint& pos) {
    QPoint p(mUi->treeView->mapToGlobal(pos));
    mHeaderContextMenu.exec(p);
}

void MainWindow::on_treeView_customContextMenuRequested(const QPoint& pos) {
    setupActions();

    QPoint p(mUi->treeView->mapToGlobal(pos));
    p.setY(p.y() + mUi->treeView->header()->height());
    mContextMenu.exec(p);
}

void MainWindow::on_treeView_selectionChanged() {
    setupActions();
}

void MainWindow::on_modelChanged() {
    setupActions();
}

void MainWindow::on_dynamicActionTriggered(const QString& menuText) {
    QDomElement docElem = mDoc->documentElement();
    QDomElement* menuItem = NULL;

    if (mYaffsModel->isImageOpen()) {
        QDomNode node = docElem.firstChild();
        while (!node.isNull()) {
            QDomElement element = node.toElement();
            if (!element.isNull() && element.tagName() == TAG_MENUITEM && element.hasAttribute(ATTR_NAME)) {
                if (element.attribute(ATTR_NAME) == menuText) {
                    menuItem = &element;
                    break;
                }
            }
            node = node.nextSibling();
        }

        //if the menu item element was found and is valid then we're ready to start creating stuff
        if (menuItem != NULL) {
            int failCount = 0;
            int xmlErrorCount = 0;

            QDomNode node = menuItem->firstChild();
            while (!node.isNull()) {
                QDomElement element = node.toElement();
                if (!element.isNull()) {
                    QString tag = element.tagName();
                    QString dest = element.attribute(ATTR_DEST);
                    QString permissions = element.attribute(ATTR_PERMISSIONS);
                    QString user = element.attribute(ATTR_USER);
                    QString group = element.attribute(ATTR_GROUP);

                    if (dest.length() > 0 && permissions.length() > 0 && user.length() > 0 && group.length() > 0) {
                        uint uid = user.toUInt();
                        uint gid = group.toUInt();
                        uint perms = permissions.toUInt(0, 8);

                        if (tag == TAG_FILE) {
                            QString name = element.attribute(ATTR_NAME);

                            if (name.length() > 0) {
                                YaffsItem* importedFile = mYaffsModel->importFile("files/" + name, dest, uid, gid, perms);
                                if (importedFile == NULL) {
                                    failCount++;
                                }
                            } else {
                                xmlErrorCount++;
                            }
                        } else if (tag == TAG_SYMLINK) {
                            QString alias = element.attribute(ATTR_ALIAS);

                            if (alias.length() > 0) {
                                YaffsItem* newSymLink = mYaffsModel->createSymLink(dest, alias, uid, gid, perms);
                                if (newSymLink == NULL) {
                                    failCount++;
                                }
                            } else {
                                xmlErrorCount++;
                            }
                        }
                    } else {
                        xmlErrorCount++;
                    }
                }
                node = node.nextSibling();
            }

            if (failCount > 0) {
                QMessageBox::critical(this, menuText, "Failed to import " + QString::number(failCount) + " items");
            }

            if (xmlErrorCount > 0) {
                QMessageBox::critical(this, menuText, QString::number(xmlErrorCount) + " error(s) found in xml");
            }
        }
    } else {
        QMessageBox::critical(this, menuText, "An open image is required for this action");
    }
}

void MainWindow::closeEvent(QCloseEvent* closeEvent) {
    bool doClose = !mYaffsModel->isDirty();
    if (!doClose) {
        QMessageBox::StandardButton result = QMessageBox::question(this,
                                                                   "Exit",
                                                                   "Unsaved changes. Are you sure you want to exit?",
                                                                   QMessageBox::Yes | QMessageBox::No,
                                                                   QMessageBox::No);
        doClose = (result == QMessageBox::Yes);
    }

    if (doClose) {
        closeEvent->accept();
    } else {
        closeEvent->ignore();
    }
}

void MainWindow::parseDynamicMenuXml(const QString& xmlFilename) {
    QFile file(xmlFilename);
    if (file.open(QIODevice::ReadOnly)) {
        mDoc = new QDomDocument(xmlFilename);
        if (mDoc->setContent(&file)) {
            QDomElement docElem = mDoc->documentElement();
            if (docElem.hasAttribute(ATTR_NAME)) {
                QMenu* menu = mUi->menuAndroid->addMenu(docElem.attribute(ATTR_NAME));

                QDomNode node = docElem.firstChild();
                while (!node.isNull()) {
                    QDomElement element = node.toElement();
                    if (!element.isNull() && element.tagName() == TAG_MENUITEM && element.hasAttribute(ATTR_NAME)) {
                        //create new action, add it to the new menu and map the triggered signal
                        QAction* action = new QAction(element.attribute(ATTR_NAME), NULL);
                        menu->addAction(action);
                        connect(action, SIGNAL(triggered()), mSignalMapper, SLOT(map()));
                        mSignalMapper->setMapping(action, action->text());
                    }
                    node = node.nextSibling();
                }
            }
        }

        file.close();
    }
}

void MainWindow::exportSelectedItems(const QString& path) {
    QModelIndexList selectedRows = mUi->treeView->selectionModel()->selectedRows();
    if (selectedRows.size() > 0) {
        YaffsExportInfo* exportInfo = mYaffsManager->exportItems(selectedRows, path);

        QString status = "Exported " + QString::number(exportInfo->numDirsExported) + " dir(s) and " +
                                       QString::number(exportInfo->numFilesExported) + " file(s).";
        mUi->statusBar->showMessage(status);

        int dirFails = exportInfo->listDirExportFailures.size();
        int fileFails = exportInfo->listFileExportFailures.size();
        if (dirFails + fileFails > 0) {
            QString msg;

            if (dirFails > 0) {
                static const int MAXDIRS = 10;
                QString items;
                int max = (dirFails > MAXDIRS ? MAXDIRS : dirFails);
                for (int i = 0; i < max; ++i) {
                    const YaffsItem* item = exportInfo->listDirExportFailures.at(i);
                    items += item->getFullPath() + "\n";
                }
                msg += "Failed to export directories:\n" + items;

                if (dirFails > MAXDIRS) {
                    msg += "... plus " + QString::number(dirFails - MAXDIRS) + " more";
                }
            }

            if (fileFails > 0) {
                if (dirFails > 0) {
                    msg += "\n";
                }

                static const int MAXFILES = 10;
                QString items;
                int max = (fileFails > MAXFILES ? MAXFILES : fileFails);
                for (int i = 0; i < max; ++i) {
                    const YaffsItem* item = exportInfo->listFileExportFailures.at(i);
                    items += item->getFullPath() + "\n";
                }
                msg += "Failed to export files:\n" + items;

                if (fileFails > MAXFILES) {
                    msg += "... plus " + QString::number(fileFails - MAXFILES) + " more";
                }
            }

            QMessageBox::critical(this, "Export", msg);
        }

        delete exportInfo;
    }
}

void MainWindow::setupActions() {
    updateWindowTitle();

    QModelIndexList selectedRows = mUi->treeView->selectionModel()->selectedRows();
    int selectionFlags = Utils::identifySelectedRows(selectedRows);
    int selectionSize = selectedRows.size();

    mUi->actionClose->setEnabled(mYaffsModel->isImageOpen());

    if (mYaffsModel->index(0, 0).isValid()) {
        mUi->actionExpandAll->setEnabled(true);
        mUi->actionCollapseAll->setEnabled(true);
        mUi->actionSaveAs->setEnabled(true);
    } else {
        mUi->actionExpandAll->setEnabled(false);
        mUi->actionCollapseAll->setEnabled(false);
        mUi->actionSaveAs->setEnabled(false);
    }

    mUi->actionEditProperties->setEnabled(false);
    mUi->actionImport->setEnabled(false);
    mUi->actionExport->setEnabled(false);
    mUi->actionRename->setEnabled(false);
    mUi->actionDelete->setEnabled(false);

    //if only a single item is selected
    if (selectionSize == 1) {
        mUi->actionRename->setEnabled(!(selectionFlags & SELECTED_ROOT));
        mUi->actionImport->setEnabled(  selectionFlags & SELECTED_DIR);

        QModelIndex itemIndex = selectedRows.at(0);
        YaffsItem* item = static_cast<YaffsItem*>(itemIndex.internalPointer());
        if (item) {
            mUi->linePath->setText(item->getFullPath());
        }
    }

    if (selectionSize >= 1) {
        mUi->actionDelete->setEnabled(!(selectionFlags & SELECTED_ROOT));
        mUi->actionEditProperties->setEnabled(true);
        mUi->actionExport->setEnabled((selectionFlags & (SELECTED_DIR | SELECTED_FILE) && !(selectionFlags & SELECTED_SYMLINK)));

        mUi->statusBar->showMessage("Selected " + QString::number(selectedRows.size()) + " items");
    } else if (selectionSize == 0) {
        mUi->statusBar->showMessage("");
    }
}
