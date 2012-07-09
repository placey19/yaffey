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

#include <QFile>

#include "Utils.h"
#include "YaffsItem.h"

int Utils::identifySelectedRows(const QModelIndexList& selectedRows) {
    int selectionFlags = (selectedRows.size() == 1 ? SELECTED_SINGLE : 0);

    //iterate through the list of items to see what we have selected
    YaffsItem* item = NULL;
    foreach (QModelIndex index, selectedRows) {
        item = static_cast<YaffsItem*>(index.internalPointer());
        if (item) {
            selectionFlags |= (item->isRoot() ? SELECTED_ROOT : 0);
            selectionFlags |= (item->isDir() ? SELECTED_DIR : 0);
            selectionFlags |= (item->isFile() ? SELECTED_FILE : 0);
            selectionFlags |= (item->isSymLink() ? SELECTED_SYMLINK : 0);
        }
    }

    return selectionFlags;
}

bool Utils::saveDataToFile(const QString& filename, const char* data, size_t length) {
    bool result = false;
    QFile file(filename);
    bool open = file.open(QIODevice::WriteOnly);
    if (open) {
        if (length > 0) {
            result = (file.write(data, length) == length);
        } else if (data == NULL) {
            result = true;
        }
        file.close();
    }
    return result;
}
