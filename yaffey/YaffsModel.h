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

#ifndef YAFFSMODEL_H
#define YAFFSMODEL_H

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QMap>

#include "YaffsControl.h"
#include "YaffsItem.h"

struct YaffsSaveInfo {
    int numFilesSaved;
    int numFilesFailed;
    int numDirsSaved;
    int numDirsFailed;
    int numSymLinksSaved;
    int numSymLinksFailed;
};

class YaffsModel : public QAbstractItemModel,
                   public YaffsControlObserver {
    Q_OBJECT

public:
    YaffsModel(QObject* parent = 0);
    ~YaffsModel();

    // prevent compiler warning about the removeRows() method below by explicitly stating it's not meant to be an override
    using QAbstractItemModel::removeRows;

    void newImage(const QString& newImageName);
    YaffsReadInfo openImage(const QString& imageFilename);
    YaffsItem* importFile(const QString& externalFilenameWithPath, const QString& internalFilenameWithPath, uint uid, uint gid, uint permissions);
    YaffsItem* importFile(YaffsItem* parentItem, const QString& filenameWithPath);
    void importDirectory(YaffsItem* parentItem, const QString& dirNameWithPath);
    YaffsItem* createSymLink(const QString& internalFilenameWithPath, const QString& alias, uint uid, uint gid, uint permissions);
    bool saveAs(const QString& filename, YaffsSaveInfo& saveInfo);
    QString getImageFilename() const { return mImageFilename; }
    bool isDirty() const { return (mItemsDirty + mItemsDeleted + mItemsNew); }
    bool isImageOpen() const { return (mYaffsRoot != NULL); }

    //from QAbstractItemModel
    QVariant data(const QModelIndex& itemIndex, int role) const;
    bool setData(const QModelIndex& itemIndex, const QVariant& value, int role = Qt::EditRole);
    Qt::ItemFlags flags(const QModelIndex& itemIndex) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    QModelIndex index(int row, int column, const QModelIndex& parentIndex = QModelIndex()) const;
    QModelIndex parent(const QModelIndex& itemIndex) const;
    int rowCount(const QModelIndex& parentIndex = QModelIndex()) const;
    int columnCount(const QModelIndex& parentIndex = QModelIndex()) const;
    int removeRows(const QModelIndexList& selectedRows);

protected:
    //from YaffsControlObserver
    void newItem(int yaffsObjectId, const yaffs_obj_hdr* yaffsObjectHeader, int fileOffset);
    void readComplete();

private:
    void saveDirectory(YaffsItem* dirItem);
    void saveFile(YaffsItem* dirItem);
    void saveSymLink(YaffsItem* dirItem);
    int processChildItemsForDelete(YaffsItem* item);
    int calculateAndDeleteContiguousRows(QList<int>& rows, YaffsItem* parentItem);
    int deleteRows(int row, int count, const QModelIndex& parentIndex);
    YaffsItem* pathToItem(const QString& path);

private:
    QString mImageFilename;
    YaffsItem* mYaffsRoot;
    QMap<int, YaffsItem*> mYaffsObjectsItemMap;
    QList<YaffsItem*> mYaffsObjectsWithoutParent;
    YaffsControl* mYaffsSaveControl;
    int mItemsNew;
    int mItemsDirty;
    int mItemsDeleted;

    YaffsSaveInfo* mSaveInfo;
};

#endif  //YAFFSMODEL_H
