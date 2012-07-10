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

#ifndef UTILS_H
#define UTILS_H

#include <QModelIndexList>

#define SELECTED_ROOT               0x1
#define SELECTED_DIR                0x2
#define SELECTED_FILE               0x4
#define SELECTED_SYMLINK            0x8
#define SELECTED_SINGLE             0x10

class Utils {
public:
    static int identifySelectedRows(const QModelIndexList& selectedRows);
    static bool saveDataToFile(const QString& filename, const char* data, size_t length);
    static QString randomString(int length);
};

#endif  //UTILS_H
