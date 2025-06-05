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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStandardItemModel>
#include <QMenu>
#include <QSignalMapper>
#include <QDomDocument>
#include <QCloseEvent>
#include <QSettings>

#include "YaffsModel.h"
#include "YaffsManager.h"

namespace Ui {
    class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent, QString imageFilename);
    ~MainWindow();

private slots:
    void on_treeView_doubleClicked(const QModelIndex& index);
    void on_actionNew_triggered();
    void on_actionOpen_triggered();
    void on_actionClose_triggered();
    void on_actionSaveAs_triggered();
    void on_actionImport_triggered();
    void on_actionExport_triggered();
    void on_actionExit_triggered();
    void on_actionRename_triggered();
    void on_actionDelete_triggered();
    void on_actionEditProperties_triggered();
    void on_actionAndroidFastboot_triggered();
    void on_actionAbout_triggered();
    void on_actionColumnName_triggered();
    void on_actionColumnSize_triggered();
    void on_actionColumnPermissions_triggered();
    void on_actionColumnAlias_triggered();
    void on_actionColumnDateAccessed_triggered();
    void on_actionColumnDateCreated_triggered();
    void on_actionColumnDateModified_triggered();
    void on_actionColumnUser_triggered();
    void on_actionColumnGroup_triggered();
    void on_treeViewHeader_customContextMenuRequested(const QPoint& pos);
    void on_treeView_customContextMenuRequested(const QPoint& pos);
    void on_treeView_selectionChanged();
    void on_modelChanged();
    void on_dynamicActionTriggered(const QString& menuText);

protected:
    void closeEvent(QCloseEvent* closeEvent);

private:
    void parseDynamicMenuXml(const QString& xmlFilename);
    void newModel();
    void openImage(const QString& imageFilename);
    void closeImage();
    void exportSelectedItems(const QString& path);
    void setupActions();
    void updateWindowTitle();

private:
    Ui::MainWindow* mUi;                //owned
    YaffsModel* mYaffsModel;            //not owned
    YaffsManager* mYaffsManager;        //not owned - singleton
    QMenu mContextMenu;
    QMenu mHeaderContextMenu;
    QDialog* mFastbootDialog;           //owned
    QSignalMapper* mSignalMapper;       //owned
    QDomDocument* mDoc;                 //owned
    QSettings mSettings;
};

#endif  //MAINWINDOW_H
