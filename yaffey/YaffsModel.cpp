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

#include <QtGui>

#include "YaffsModel.h"
#include "Utils.h"

YaffsModel::YaffsModel(QObject* parent) : QAbstractItemModel(parent) {
    mYaffsRoot = NULL;
    mYaffsSaveControl = NULL;
    mSaveInfo = NULL;

    mItemsNew = 0;
    mItemsDirty = 0;
    mItemsDeleted = 0;
}

YaffsModel::~YaffsModel() {
    delete mYaffsRoot;
}

void YaffsModel::newImage(const QString& newImageName) {
    mYaffsRoot = YaffsItem::createRoot();
    mItemsNew++;
    mImageFilename = newImageName;

    emit layoutChanged();
}

YaffsReadInfo YaffsModel::openImage(const QString& imageFilename) {
    mImageFilename = imageFilename;

    YaffsReadInfo readInfo;
    memset(&readInfo, 0, sizeof(YaffsReadInfo));

    if (mYaffsRoot == NULL) {
        YaffsControl yaffsControl(mImageFilename.toStdString().c_str(), this);
        if (yaffsControl.open(YaffsControl::OPEN_READ)) {
            if (yaffsControl.readImage()) {
                readInfo = yaffsControl.getReadInfo();

                mItemsNew = 0;
                mItemsDirty = 0;
                mItemsDeleted = 0;

                emit layoutChanged();
            }
        }
    }

    return readInfo;
}

//get the YaffsItem at the given internal path or create the path and return a new item.
//will return null if root doesn't exist
YaffsItem* YaffsModel::pathToItem(const QString& path) {
    YaffsItem* parentItem = mYaffsRoot;
    if (parentItem != NULL) {
        QStringList parentDirNames = path.split('/');
        for (int i = 0; i < parentDirNames.length(); ++i) {
            QString dirName = parentDirNames[i];
            if (dirName.length() > 0) {
                YaffsItem* childItem = parentItem->findItemWithName(dirName);
                if (childItem != NULL) {
                    //found item
                    parentItem = childItem;
                } else {
                    //need to create directory
                    YaffsItem* newDir = YaffsItem::createDirectory(parentItem, dirName);
                    parentItem->appendChild(newDir);
                    mItemsNew++;
                    parentItem = newDir;
                }
            }
        }
    }
    return parentItem;
}

YaffsItem* YaffsModel::importFile(const QString& externalFilenameWithPath, const QString& internalFilenameWithPath, uint uid, uint gid, uint permissions) {
    qDebug() << "importFile(), externalFilenameWithPath: " << externalFilenameWithPath << ", internalFilenameWithPath: " << internalFilenameWithPath;

    YaffsItem* importedFile = NULL;
    QString path = "/";
    QString fileName;
    int slash = internalFilenameWithPath.lastIndexOf('/');
    if (slash >= 0) {
        path = internalFilenameWithPath.left(slash);
        int len = internalFilenameWithPath.length();
        fileName = internalFilenameWithPath.right(len - (slash + 1));
    }

    YaffsItem* parentItem = pathToItem(path);
    if (parentItem != NULL) {
        //check to make sure an item of the same name doesn't already exist
        if (parentItem->findItemWithName(fileName) == NULL) {
            importedFile = importFile(parentItem, externalFilenameWithPath);
            importedFile->setName(fileName);
            importedFile->setUserId(uid);
            importedFile->setGroupId(gid);
            importedFile->setPermissions(permissions);
        }
    }

    return importedFile;
}

YaffsItem* YaffsModel::importFile(YaffsItem* parentItem, const QString& filenameWithPath) {
    qDebug() << "importFile(), parentItem: " << parentItem << ", filenameWithPath: " << filenameWithPath;

    YaffsItem* importedFile = NULL;
    if (parentItem && filenameWithPath.length() > 0) {
        QFileInfo fileInfo(filenameWithPath);
        if (fileInfo.exists()) {
            importedFile = YaffsItem::createFile(parentItem, filenameWithPath, fileInfo.size());
            parentItem->appendChild(importedFile);
            mItemsNew++;

            emit layoutChanged();
        }
    }
    return importedFile;
}

void YaffsModel::importDirectory(YaffsItem* parentItem, const QString& externalDirNameWithPath) {
    qDebug() << "importDirectory(), parentItem: " << parentItem << ", externalDirNameWithPath: " << externalDirNameWithPath;

    if (parentItem && externalDirNameWithPath.length() > 0) {
        int slashPos = externalDirNameWithPath.lastIndexOf('/');
        QString dirName = externalDirNameWithPath.mid(slashPos + 1);

        YaffsItem* newDir = YaffsItem::createDirectory(parentItem, dirName);
        parentItem->appendChild(newDir);
        mItemsNew++;

        QDirIterator dirs(externalDirNameWithPath, QDirIterator::NoIteratorFlags);
        while (dirs.hasNext()) {
            QFileInfo fileInfo(dirs.next());
            QString fileName = fileInfo.fileName();
            QString fileNameWithPath = fileInfo.absoluteFilePath();

            if (fileInfo.isDir()) {
                if (fileName != "." && fileName != "..") {
                    importDirectory(newDir, fileNameWithPath);
                }
            } else if (fileInfo.isFile()) {
                importFile(newDir, fileNameWithPath);
            }
        }

        emit layoutChanged();
    }
}

YaffsItem* YaffsModel::createSymLink(const QString& internalFilenameWithPath, const QString& alias, uint uid, uint gid, uint permissions) {
    qDebug() << "createSymLink(), internalFilenameWithPath: " << internalFilenameWithPath << ", alias: " << alias;

    YaffsItem* newSymLink = NULL;
    QString path = "/";
    QString fileName;
    int slash = internalFilenameWithPath.lastIndexOf('/');
    if (slash >= 0) {
        path = internalFilenameWithPath.left(slash);
        int len = internalFilenameWithPath.length();
        fileName = internalFilenameWithPath.right(len - (slash + 1));
    }

    YaffsItem* parentItem = pathToItem(path);
    if (parentItem != NULL) {
        //check to make sure an item of the same name doesn't already exist
        if (parentItem->findItemWithName(fileName) == NULL) {
            newSymLink = YaffsItem::createSymLink(parentItem, fileName, alias, uid, gid, permissions);
            parentItem->appendChild(newSymLink);
            mItemsNew++;
            emit layoutChanged();
        }
    }

    return newSymLink;
}

bool YaffsModel::saveAs(const QString& filename, YaffsSaveInfo& saveInfo) {
    memset(&saveInfo, 0, sizeof(YaffsSaveInfo));
    bool result = false;

    if (filename.compare(mImageFilename) != 0) {
        mSaveInfo = &saveInfo;
        QString tmpFilename = filename + "." + Utils::randomString(4) + ".tmp";

        //make sure tmp file doesn't already exist
        QFileInfo tmpFileInfo(tmpFilename);
        if (!tmpFileInfo.exists()) {
            mYaffsSaveControl = new YaffsControl(tmpFilename.toStdString().c_str(), NULL);
            if (mYaffsSaveControl->open(YaffsControl::OPEN_NEW)) {
                saveDirectory(mYaffsRoot);
                result = (mSaveInfo->numDirsFailed + mSaveInfo->numFilesFailed + mSaveInfo->numSymLinksFailed == 0);
            }
            delete mYaffsSaveControl;
            mYaffsSaveControl = NULL;
            mSaveInfo = NULL;

            if (result) {
                //make sure no file with the goal filename exists
                QFileInfo fileInfo(filename);
                if (fileInfo.exists()) {
                    result = QFile::remove(filename);
                }

                if (result) {
                    //rename the tmp file to the goal filename
                    result = QFile::rename(tmpFilename, filename);

                    mItemsNew = 0;
                    mItemsDirty = 0;
                    mItemsDeleted = 0;
                    mImageFilename = filename;
                }
            }

            //delete the tmp file (if it exists)
            QFile::remove(tmpFilename);
        }
    }

    return result;
}

void YaffsModel::saveDirectory(YaffsItem* dirItem) {
    if (dirItem) {
        YaffsItem* parentItem = dirItem->parent();
        if (parentItem) {
            qDebug() << "d: " << dirItem->getFullPath() << ", Parent: " << parentItem->getFullPath();
        } else {
            qDebug() << "d: " << dirItem->getFullPath() << ", Parent: NULL";
        }

        int newObjectId = -1;
        int newHeaderPos = -1;
        bool isRoot = (parentItem == NULL);
        if (isRoot) {
            newObjectId = mYaffsSaveControl->addRoot(dirItem->getHeader(), newHeaderPos);
        } else {
            newObjectId = mYaffsSaveControl->addDirectory(dirItem->getHeader(), newHeaderPos);
        }

        if (newObjectId != -1) {
            dirItem->setHeaderPosition(newHeaderPos);
            dirItem->setObjectId(newObjectId);

            int childCount = dirItem->childCount();
            for (int i = 0; i < childCount; ++i) {
                YaffsItem* childItem = dirItem->child(i);
                childItem->setParentObjectId(newObjectId);

                if (childItem->isDir()) {
                    saveDirectory(childItem);
                } else if (childItem->isFile()) {
                    saveFile(childItem);
                } else if (childItem->isSymLink()) {
                    saveSymLink(childItem);
                }
            }

            if (!isRoot) {
                mSaveInfo->numDirsSaved++;
            }
            dirItem->setCondition(YaffsItem::CLEAN);
        } else {
            if (!isRoot) {
                mSaveInfo->numDirsFailed++;
            }
            dirItem->setCondition(YaffsItem::ERR);
        }
    }
}

void YaffsModel::saveFile(YaffsItem* fileItem) {
    if (fileItem) {
        YaffsItem* parentItem = fileItem->parent();
        qDebug() << "f: " << fileItem->getFullPath() << ", Parent: " << parentItem->getFullPath();

        if (fileItem->isFile()) {
            YaffsItem::Condition condition = fileItem->getCondition();
            size_t filesize = fileItem->getFileSize();
            int newObjectId = -1;
            int newHeaderPos = -1;

            //if item is new then get the data from the local file system
            if (condition == YaffsItem::NEW) {
                char* data = new char[filesize];
                QString filename = fileItem->getExternalFilename();
                FILE* file = fopen(filename.toStdString().c_str(), "rb");
                if (file) {
                    size_t bytesRead = fread(data, 1, filesize, file);
                    if (bytesRead == filesize) {
                        newObjectId = mYaffsSaveControl->addFile(fileItem->getHeader(), newHeaderPos, data, static_cast<int>(filesize));
                    }
                    fclose(file);
                }
                delete [] data;
            //the data is in the opened image so get the it from there
            } else {
                int headerPosition = fileItem->getHeaderPosition();
                YaffsControl yaffsControl(mImageFilename.toStdString().c_str(), NULL);
                if (yaffsControl.open(YaffsControl::OPEN_READ)) {
                    size_t bytesExtracted = 0;
                    char* data = yaffsControl.extractFile(headerPosition, bytesExtracted);
                    if (bytesExtracted == filesize) {
                        newObjectId = mYaffsSaveControl->addFile(fileItem->getHeader(), newHeaderPos, data, static_cast<int>(filesize));
                    }
                }
            }

            if (newObjectId != -1) {
                fileItem->setHeaderPosition(newHeaderPos);
                fileItem->setObjectId(newObjectId);

                mSaveInfo->numFilesSaved++;
                fileItem->setCondition(YaffsItem::CLEAN);
            } else {
                mSaveInfo->numFilesFailed++;
                fileItem->setCondition(YaffsItem::ERR);
            }
        }
    }
}

void YaffsModel::saveSymLink(YaffsItem* symLinkItem) {
    if (symLinkItem) {
        YaffsItem* parentItem = symLinkItem->parent();
        if (parentItem) {
            qDebug() << "s: " << symLinkItem->getFullPath() << ", Parent: " << parentItem->getFullPath();
            int newHeaderPos = -1;
            int newObjectId = mYaffsSaveControl->addSymLink(symLinkItem->getHeader(), newHeaderPos);

            if (newObjectId != -1) {
                symLinkItem->setHeaderPosition(newHeaderPos);
                symLinkItem->setObjectId(newObjectId);

                mSaveInfo->numSymLinksSaved++;
                symLinkItem->setCondition(YaffsItem::CLEAN);
            } else {
                mSaveInfo->numSymLinksFailed++;
                symLinkItem->setCondition(YaffsItem::ERR);
            }
        }
    }
}

QVariant YaffsModel::data(const QModelIndex& itemIndex, int role) const {
    QVariant result = QVariant();
    YaffsItem* item = static_cast<YaffsItem*>(itemIndex.internalPointer());
    if (itemIndex.isValid() && item) {
        if (role == Qt::DisplayRole) {
            result = item->data(itemIndex.column());
        } else if (role == Qt::ForegroundRole) {
            if (itemIndex.column() == YaffsItem::NAME) {
                if (item->isDir()) {
                    result = QVariant(QColor(Qt::blue));
                } else if (item->isFile()) {
                    result = QVariant(QColor(Qt::black));
                } else if (item->isSymLink()) {
                    result = QVariant(QColor(Qt::darkGreen));
                }
            }
        } else if (role == Qt::BackgroundRole) {
/*            static const QColor orange(255, 165, 0);

            switch (item->getCondition()) {
            case YaffsItem::CLEAN:
                break;
            case YaffsItem::DIRTY:
                result = orange;
                break;
            case YaffsItem::NEW:
                result = Qt::green;
                break;
            case YaffsItem::MOVED:
                break;
            }*/
        } else if (role == Qt::FontRole) {
            if (itemIndex.column() == YaffsItem::PERMISSIONS) {
                result = QFont("Courier");
            }
        } else if (role == Qt::EditRole) {
            switch (itemIndex.column()) {
            case YaffsItem::NAME:
                result = item->getName();
                break;
            case YaffsItem::PERMISSIONS:
                result = item->getPermissions();
                break;
            case YaffsItem::ALIAS:
                result = item->getAlias();
                break;
            case YaffsItem::USER:
                result = item->getUserId();
                break;
            case YaffsItem::GROUP:
                result = item->getGroupId();
                break;
            }
        }
    }
    return result;
}

bool YaffsModel::setData(const QModelIndex& itemIndex, const QVariant& value, int role) {
    bool result = false;
    if (role == Qt::EditRole) {
        YaffsItem* item = static_cast<YaffsItem*>(itemIndex.internalPointer());
        if (item) {
            switch (itemIndex.column()) {
            case YaffsItem::NAME:
                item->setName(value.toString());
                result = true;
                break;
            case YaffsItem::PERMISSIONS:
                item->setPermissions(value.toUInt());
                result = true;
                break;
            case YaffsItem::ALIAS:
                item->setAlias(value.toString());
                result = true;
                break;
            case YaffsItem::USER:
                item->setUserId(value.toUInt());
                result = true;
                break;
            case YaffsItem::GROUP:
                item->setGroupId(value.toUInt());
                result = true;
                break;
            }
        }
        mItemsDirty += (item->getCondition() == YaffsItem::DIRTY ? 1 : 0);
    }

    if (result) {
        emit dataChanged(itemIndex, itemIndex);
    }

    return result;
}

Qt::ItemFlags YaffsModel::flags(const QModelIndex& itemIndex) const {
    Qt::ItemFlags flags = 0;
    if (itemIndex.isValid()) {
        flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
        YaffsItem* item = static_cast<YaffsItem*>(itemIndex.internalPointer());
        if (!item->isRoot() && itemIndex.column() == YaffsItem::NAME) {
            flags |= Qt::ItemIsEditable;
        }
    }
    return flags;
}

QVariant YaffsModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role == Qt::DisplayRole) {
        if (orientation == Qt::Horizontal) {
            if (section == YaffsItem::NAME) {
                return "Name";
            } else if (section == YaffsItem::SIZE) {
                return "Size";
            } else if (section == YaffsItem::PERMISSIONS) {
                return "Permissions";
            } else if (section == YaffsItem::ALIAS) {
                return "Alias";
            } else if (section == YaffsItem::DATE_ACCESSED) {
                return "Date Accessed";
            } else if (section == YaffsItem::DATE_CREATED) {
                return "Date Created";
            } else if (section == YaffsItem::DATE_MODIFIED) {
                return "Date Modified";
            } else if (section == YaffsItem::USER) {
                return "User";
            } else if (section == YaffsItem::GROUP) {
                return "Group";
            }
#ifdef QT_DEBUG
            else if (section == YaffsItem::OBJECTID) {
                return "ObjectId";
            } else if (section == YaffsItem::PARENTID) {
                return "ParentId";
            } else if (section == YaffsItem::HEADERPOS) {
                return "HeaderPos";
            }
#endif  //QT_DEBUG
        }
    }
    return QVariant();
}

QModelIndex YaffsModel::index(int row, int column, const QModelIndex& parentIndex) const {
    YaffsItem* parent = NULL;

    if (mYaffsRoot && (!parentIndex.isValid() || parentIndex == QModelIndex())) {
        if (row == 0) {
            return createIndex(row, column, mYaffsRoot);
        }
    } else {
        parent = static_cast<YaffsItem*>(parentIndex.internalPointer());
    }

    if (parent) {
        YaffsItem* item = parent->child(row);
        if (item) {
            return createIndex(row, column, item);
        }
    }

    return QModelIndex();
}

QModelIndex YaffsModel::parent(const QModelIndex& itemIndex) const {
    YaffsItem* item = static_cast<YaffsItem*>(itemIndex.internalPointer());
    if (item && item != mYaffsRoot) {
        YaffsItem* parent = item->parent();
        if (parent) {
            return createIndex(parent->row(), 0, parent);
        }
    }
    return QModelIndex();
}

int YaffsModel::rowCount(const QModelIndex& parentIndex) const {
    YaffsItem* parent = NULL;
    int count = 0;

    if (!parentIndex.isValid() || parentIndex == QModelIndex()) {
        if (mYaffsRoot) {
            count = 1;
        }
    } else {
        parent = static_cast<YaffsItem*>(parentIndex.internalPointer());
    }

    if (parent) {
        count = parent->childCount();
    }

    return count;
}

int YaffsModel::columnCount(const QModelIndex& /*parentIndex*/) const {
    return YaffsItem::COLUMN_COUNT;
}

int YaffsModel::removeRows(const QModelIndexList& selectedRows) {
    //mark all selected items for delete
    foreach (QModelIndex index, selectedRows) {
        YaffsItem* item = static_cast<YaffsItem*>(index.internalPointer());
        if (item) {
            item->markForDelete();
        }
    }

    //iterate through ALL items and process the marked ones
    return processChildItemsForDelete(mYaffsRoot);
}

int YaffsModel::processChildItemsForDelete(YaffsItem* item) {
    int itemsDeleted = 0;
    if (item->hasChildMarkedForDelete()) {
        QList<int> rowsToDelete;

        //iterate through child items to build up list of items to delete
        for (int i = 0; i < item->childCount(); ++i) {
            YaffsItem* childItem = item->child(i);
            if (childItem->isMarkedForDelete()) {
                rowsToDelete.append(childItem->row());
            } else if (childItem->hasChildMarkedForDelete()) {
                itemsDeleted += processChildItemsForDelete(childItem);
                item->setHasChildMarkedForDelete(false);
            }
        }

        itemsDeleted += calculateAndDeleteContiguousRows(rowsToDelete, item);
    }
    return itemsDeleted;
}

int YaffsModel::calculateAndDeleteContiguousRows(QList<int>& rows, YaffsItem* parentItem) {
    int itemsDeleted = 0;

    qSort(rows);

    int size = rows.size();
    int thisRow, nextRow;
    int count = 1;

    for (int i = size - 1; i >= 0; --i) {
        thisRow = rows.at(i);
        bool lastRow = (i == 0);

        if (!lastRow) {
            nextRow = rows.at(i - 1);
        }

        if (lastRow || nextRow != thisRow - 1) {
            qDebug() << "Removing rows (start, count): (" << thisRow << ", " << count << ")";
            QModelIndex parentIndex = createIndex(parentItem->row(), 0, parentItem);
            itemsDeleted += deleteRows(thisRow, count, parentIndex);
            count = 0;

            if (lastRow) {
                break;
            }
        }

        ++count;
    }

    return itemsDeleted;
}

int YaffsModel::deleteRows(int row, int count, const QModelIndex& parentIndex) {
    int itemsDeleted = 0;

    if (parentIndex.isValid()) {
        beginRemoveRows(parentIndex, row, row + (count - 1));
        for (int i = row + (count - 1); i >= row; --i) {
            YaffsItem* parentItem = static_cast<YaffsItem*>(parentIndex.internalPointer());
            parentItem->removeChild(row);
            itemsDeleted++;
        }
        endRemoveRows();
        emit layoutChanged();
    }

    mItemsDeleted += itemsDeleted;
    return itemsDeleted;
}

//from YaffsReaderObserver
void YaffsModel::newItem(int yaffsObjectId, const yaffs_obj_hdr* yaffsObjectHeader, int fileOffset) {
    if (yaffsObjectId == YAFFS_OBJECTID_ROOT) {
        mYaffsRoot = new YaffsItem(NULL, yaffsObjectHeader, fileOffset, yaffsObjectId);
        mYaffsRoot->setName("/");
        mYaffsObjectsItemMap.insert(YAFFS_OBJECTID_ROOT, mYaffsRoot);
        return;
    }

    //get childs parent
    YaffsItem* parent = mYaffsObjectsItemMap.value(yaffsObjectHeader->parent_obj_id);

    //create item and map it
    YaffsItem* child = new YaffsItem(parent, yaffsObjectHeader, fileOffset, yaffsObjectId);
    mYaffsObjectsItemMap.insert(yaffsObjectId, child);

    if (parent) {
        //add child to parent
        parent->appendChild(child);
    } else {
        qDebug() << "error, parent not found, id: " << yaffsObjectHeader->parent_obj_id;
        mYaffsObjectsWithoutParent.append(child);
    }
}

void YaffsModel::readComplete() {
    //if image didn't contain a root but did contain other stuff, give model a root
    if (mYaffsRoot == NULL && mYaffsObjectsItemMap.size() > 0) {
        mYaffsRoot = YaffsItem::createRoot();
        mYaffsObjectsItemMap.insert(YAFFS_OBJECTID_ROOT, mYaffsRoot);
    }

    if (mYaffsObjectsWithoutParent.size() > 0) {
        //child objects might have been before parent
        foreach (YaffsItem* child, mYaffsObjectsWithoutParent) {
            YaffsItem* parent = mYaffsObjectsItemMap.value(child->getHeader().parent_obj_id);
            if (parent) {
                parent->appendChild(child);
                qDebug() << "child came before parent in file, parent id: " << child->getHeader().parent_obj_id;
            } else {
                qDebug() << "parent still not found, item name: " << child->getName();
            }
        }
    }
    mYaffsObjectsWithoutParent.clear();
}
