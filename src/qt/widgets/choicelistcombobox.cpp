/**********************************************************************
 *                                                                    *
 * Voreen - The Volume Rendering Engine                               *
 *                                                                    *
 * Copyright (C) 2005-2009 Visualization and Computer Graphics Group, *
 * Department of Computer Science, University of Muenster, Germany.   *
 * <http://viscg.uni-muenster.de>                                     *
 *                                                                    *
 * This file is part of the Voreen software package. Voreen is free   *
 * software: you can redistribute it and/or modify it under the terms *
 * of the GNU General Public License version 2 as published by the    *
 * Free Software Foundation.                                          *
 *                                                                    *
 * Voreen is distributed in the hope that it will be useful,          *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of     *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the       *
 * GNU General Public License for more details.                       *
 *                                                                    *
 * You should have received a copy of the GNU General Public License  *
 * in the file "LICENSE.txt" along with this program.                 *
 * If not, see <http://www.gnu.org/licenses/>.                        *
 *                                                                    *
 * The authors reserve all rights not expressly granted herein. For   *
 * non-commercial academic use see the license exception specified in *
 * the file "LICENSE-academic.txt". To get information about          *
 * commercial licensing please contact the authors.                   *
 *                                                                    *
 **********************************************************************/

#include "voreen/qt/widgets/choicelistcombobox.h"

namespace voreen {

ChoiceListComboBox::ChoiceListComboBox(const std::vector<std::string>& list, bool editable, QWidget* parent)
    : QComboBox(parent)
    , ignore_(false)
    , list_(list)
{
    setEditable(editable);
    setInsertPolicy(QComboBox::NoInsert);
}

void ChoiceListComboBox::showPopup() {
    ignore_ = true;
    QString text = currentText();
    clear();

    if (isEditable())
        addItem("");

    for (size_t i=0; i < list_.size(); i++)
        addItem(list_[i].c_str());

    // restore what was previously selected
    if (isEditable()) {
        setCurrentIndex(-1);
        setEditText(text);
    } else {
        for (int i=0; i < count(); i++)
            if (itemText(i) == text) {
                setCurrentIndex(i);
                break;
            }
    }

    QComboBox::showPopup();
    ignore_ = false;
}

} // namespace